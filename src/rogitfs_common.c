// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "rogitfs_common.h"

int rogitfs_readdir_tree_fill(void *buf, fuse_fill_dir_t filler, git_tree *tree, git_odb *odb) {

	size_t entry_count = git_tree_entrycount(tree);
	if (entry_count == 0) {
		return 0;
	}

	for (unsigned int i = 0; i < entry_count; i++) {
		const git_tree_entry *entry = git_tree_entry_byindex(tree, i);
		if (entry == NULL) {
			return -1;
		}

		const char *name = git_tree_entry_name(entry);
		struct stat entry_stat = {};
		switch(git_tree_entry_type(entry)) {
		case GIT_OBJECT_TREE:
			entry_stat.st_mode = S_IFDIR | 0755;
		break;
		case GIT_OBJECT_BLOB:

			if ((git_tree_entry_filemode(entry) & GIT_FILEMODE_LINK) == GIT_FILEMODE_LINK) {
				entry_stat.st_mode = S_IFLNK | 0644;
			} else {

				entry_stat.st_mode = S_IFREG | 0644;

				const git_oid *oid = git_tree_entry_id(entry);
				git_odb_object *odb_obj = NULL;
				int error = git_odb_read(&odb_obj, odb, oid);
				if (error != 0) {
					const git_error *giterr = git_error_last();
					fprintf(stderr, "git_odb_read %d %s\n", giterr->klass, giterr->message);
					return -1;
				}
				entry_stat.st_size = git_odb_object_size(odb_obj);
				git_odb_object_free(odb_obj);
			}

		break;
		default:
			continue;
		break;
		}

		int res = filler(buf, name, &entry_stat, 0, 0);
		if (res != 0) {
			return -1;
		}
	}

	return 0;
}

int rogitfs_readdir_odb_fill(const git_oid *id, void *fill_payload) {

	if (fill_payload == NULL) {
		return 1;
	}

	struct odb_fill_payload *payload = (struct odb_fill_payload *) fill_payload;

	git_object *obj = NULL;
	int error = git_object_lookup(&obj, payload->private->repo, id, payload->type);
	if (error == GIT_ENOTFOUND) {
		return 0;
	}

	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_odb_foreach %d %d %s\n", error, giterr->klass, giterr->message);
		git_object_free(obj);
		return 0;
	}

	char buffer[GIT_OID_HEXSZ+1] = {};
	char *hash = git_oid_tostr(buffer, GIT_OID_HEXSZ+1, id);

	int res = payload->filler(payload->buf, hash, NULL, 0, 0);
	git_object_free(obj);

	return res;
}

int rogitfs_get_path_object(const char *path, git_object **result_obj, git_filemode_t *result_mode, git_repository *repo, git_odb *odb) {

	char *rest = (char *)path;
	git_object *obj = NULL;
	git_filemode_t obj_mode = 0;
	size_t rest_len = strlen(rest);

	while(rest_len > 0) {

		// cut off next component
		char *next_end = index(rest, '/');
		size_t comp_len = 0;
		if (next_end == NULL) {
			comp_len = rest_len;
		} else {
			comp_len = next_end-rest;
		}
		char comp_buffer[comp_len + 1];
		memset(comp_buffer, 0, comp_len + 1);
		strncpy(comp_buffer, rest, comp_len);

		// lookup next component
		git_object *new_obj = NULL;
		git_filemode_t new_mode = 0;
		int error = rogitfs_check_path_component(obj, comp_buffer, &new_obj, &new_mode, repo, odb);
		if (obj != NULL) {
			git_object_free(obj);
		}
		if (error != 0) {
			return -ENOENT;
		}
		obj = new_obj;
		obj_mode = new_mode;

		// advance rest
		if (next_end == NULL) {
			rest = rest+rest_len;
		} else {
			rest = next_end+1;
		}
		rest_len = strlen(rest);
	}
	if (obj == NULL) {
		return -ENOENT;
	}

	*result_obj = obj;
	*result_mode = obj_mode;
	return 0;
}

int rogitfs_check_path_component(git_object *parent_obj, const char *component, git_object **result_obj, git_filemode_t *result_mode, git_repository *repo, git_odb *odb) {

	int error = 0;

	if (parent_obj == NULL) {
		// assume component is an object hash

		size_t comp_len = strlen(component);
		if (comp_len != GIT_OID_HEXSZ) {
			return -1;
		}

		git_oid oid = {};
		error = git_oid_fromstr(&oid, component);
		if (error != 0) {
			const git_error *giterr = git_error_last();
			fprintf(stderr, "git_oid_fromstr %d %s\n", giterr->klass, giterr->message);
			return -1;
		}

		git_object *obj = NULL;
		error = git_object_lookup(&obj, repo, &oid, GIT_OBJECT_ANY);
		if (error != 0) {
			const git_error *giterr = git_error_last();
			fprintf(stderr, "git_object_lookup %d %s\n", giterr->klass, giterr->message);
			return -1;
		}

		*result_obj = obj;

		return 0;

	} else {

		git_object_t parent_type = git_object_type(parent_obj);

		git_tree *tree = NULL;

		switch(parent_type) {
		case GIT_OBJECT_COMMIT:
			error = git_commit_tree(&tree, (git_commit *)parent_obj);
			if (error != 0) {
				const git_error *giterr = git_error_last();
				fprintf(stderr, "git_commit_tree %d %s\n", giterr->klass, giterr->message);
				return -1;
			}
		break;
		case GIT_OBJECT_TREE:
			tree = (git_tree *)parent_obj;
		break;
		default:
			// wrong object type
			return -1;
		break;
		}

		const git_tree_entry *entry = git_tree_entry_byname(tree, component);
		if (entry == NULL) {
			if ((git_tree *)parent_obj != tree) {
				git_tree_free(tree);
			}
			return -1;
		}

		git_object *obj = NULL;
		error = git_tree_entry_to_object(&obj, repo, entry);
		if (error != 0) {
			const git_error *giterr = git_error_last();
			fprintf(stderr, "git_tree_entry_to_object %d %s\n", giterr->klass, giterr->message);
			if ((git_tree *)parent_obj != tree) {
				git_tree_free(tree);
			}
			return -1;
		}
		*result_mode = git_tree_entry_filemode(entry);

		if ((git_tree *)parent_obj != tree) {
			git_tree_free(tree);
		}

		*result_obj = obj;
		return 0;
	}
}

int path_component(const char *path, unsigned int find_index, const char **result_comp, unsigned int *result_comp_size) {

	const char *start = path;
	if (start[0] == 0) {
		return -1;
	}
	unsigned int comp_index = 0;
	const char *comp_start = start;
	const char *comp_end = NULL;
	while (comp_index < find_index) {
		while (comp_start[0] == '/') {
			comp_start++;
		}
		if (comp_start[0] == 0) {
			return -1;
		}
		comp_end = index(comp_start, '/');
		if (comp_end == NULL) {
			break;
		}
		comp_index++;
		comp_start = comp_end+1;
		comp_end = NULL;
	}
	if (comp_index != find_index) {
		return -1;
	}
	comp_end = index(comp_start, '/');
	unsigned int comp_size = 0;
	if (comp_end == NULL) {
		comp_size = strlen(comp_start);
	} else {
		comp_size = comp_end-comp_start;
	}
	if (comp_size == 0) {
		return -1;
	}
	*result_comp = comp_start;
	*result_comp_size = comp_size;
	return 0;
}

int path_component_count(const char *path, unsigned int *result_count) {

	const char *start = path;
	if (start[0] == 0) {
		*result_count = 0;
		return 0;
	}
	unsigned int comp_index = 0;
	const char *comp_start = start;
	const char *comp_end = NULL;
	while (comp_start[0] != 0) {
		while (comp_start[0] == '/') {
			comp_start++;
		}
		if (comp_start[0] == 0) {
			*result_count = comp_index;
			return 0;
		}
		comp_end = index(comp_start, '/');
		if (comp_end == NULL) {
			break;
		}
		comp_index++;
		comp_start = comp_end+1;
		comp_end = NULL;
	}
	if (strlen(comp_start) > 0) {
		comp_index++;
	}
	*result_count = comp_index;
	return 0;
}
