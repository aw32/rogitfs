// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#include <stdio.h>
#include <string.h>
#include "rogitfs_refs.h"
#include "rogitfs_common.h"

int rogitfs_refs_readdir_refs(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_strarray array = {};

	int error = git_reference_list(&array, private->repo);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_reference_list %d %s\n", giterr->klass, giterr->message);
		return -1;
	}

	const char **refs = (const char **) calloc(1, sizeof(char *) * array.count);
	unsigned int *refs_size = (unsigned int *) calloc(1, sizeof(unsigned int) * array.count);
	unsigned int refs_cur = 0;

	size_t pathlen = strlen(path);
	unsigned int path_comp_count = 0;
	error = path_component_count(path, &path_comp_count);
	if (error != 0) {
		return -1;
	}
	const char *path_comp = NULL;
	unsigned int path_comp_size = 0;
	unsigned int comp_index = path_comp_count-1;
	if (comp_index == -1) {
		comp_index = 0;
		path_comp = path;
		path_comp_size = 0;
	} else {
		error = path_component(path, comp_index, &path_comp, &path_comp_size);
		if (error != 0) {
			free(refs);
			free(refs_size);
			git_strarray_free(&array);
			return -1;
		}
	}
	const char *ref_comp = NULL;
	unsigned int ref_comp_size = 0;
	unsigned int ref_comp_count = 0;
	for (unsigned int i = 0; i < array.count; i++) {
		const char *ref = array.strings[i];
		if (strncmp(ref, "refs/", 5) != 0) {
			continue;
		}
		ref = ref+5;
		if (strncmp(path, ref, pathlen) != 0) {
			continue;
		}
		error = path_component_count(ref, &ref_comp_count);
		if (error != 0) {
			continue;
		}
		if (ref_comp_count < path_comp_count) {
			continue;
		}
		error = path_component(ref, comp_index, &ref_comp, &ref_comp_size);
		if (error != 0) {
			continue;
		}
		if (path_comp_size != 0 && path_comp_size != ref_comp_size) {
			continue;
		}
		if (strncmp(path_comp, ref_comp, path_comp_size) != 0) {
			continue;
		}
		// found compatible entry
		if (ref_comp_count <= path_comp_count) {
			continue;
		}
		unsigned int next_ref_comp_index = comp_index+1;
		if (path_comp_count == 0) {
			next_ref_comp_index = 0;
		}
		const char *next_ref_comp = NULL;
		unsigned int next_ref_comp_size = 0;
		error = path_component(ref, next_ref_comp_index, &next_ref_comp, &next_ref_comp_size);
		if (error != 0) {
			continue;
		}
		if (next_ref_comp_size == 0) {
			continue;
		}
		// check if already seen
		int seen = 1;
		for (unsigned int j = 0; j < refs_cur; j++) {
			if (refs_size[j] == next_ref_comp_size && strncmp(refs[j], next_ref_comp, refs_size[j]) == 0) {
				seen = 0;
				break;
			}
		}
		if (seen == 0) {
			continue;
		}

		// add to list
		refs[refs_cur] = next_ref_comp;
		refs_size[refs_cur] = next_ref_comp_size;
		refs_cur++;

		char buffer[next_ref_comp_size+1];
		memset(buffer, 0, next_ref_comp_size+1);
		strncpy(buffer, next_ref_comp, next_ref_comp_size);

		int res = filler(buf, buffer, NULL, 0, 0);
		if (res != 0) {
			free(refs);
			free(refs_size);
			git_strarray_free(&array);
			return -1;
		}

	}

	free(refs);
	free(refs_size);
	git_strarray_free(&array);
	return 0;
}

int rogitfs_refs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	if (path[0] == '/') {

		return rogitfs_refs_readdir_refs(path+1, buf, filler, offset, fi, flags);

	} else {

		return rogitfs_refs_readdir_refs(path, buf, filler, offset, fi, flags);

	}

	return -1;
}

int rogitfs_refs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_strarray array = {};

	int error = git_reference_list(&array, private->repo);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_reference_list %d %s\n", giterr->klass, giterr->message);
		return -1;
	}

	size_t pathlen = strlen(path);
	unsigned int path_comp_count = 0;
	error = path_component_count(path, &path_comp_count);
	if (error != 0) {
		git_strarray_free(&array);
		return -1;
	}
	if (path_comp_count == 0) {
		git_strarray_free(&array);
		return -1;
	}
	const char *path_comp = NULL;
	unsigned int path_comp_size = 0;
	unsigned int comp_index = path_comp_count-1;
	error = path_component(path, comp_index, &path_comp, &path_comp_size);
	if (path_comp_size == 0) {
		git_strarray_free(&array);
		return -1;
	}
	unsigned int array_index = -1;
	const char *ref_comp = NULL;
	unsigned int ref_comp_size = 0;
	unsigned int ref_comp_count = 0;
	for (unsigned int i = 0; i < array.count; i++) {
		const char *ref = array.strings[i];
		if (strncmp(ref, "refs/", 5) != 0) {
			continue;
		}
		ref = ref+5;
		if (strncmp(path, ref, pathlen) != 0) {
			continue;
		}
		error = path_component_count(ref, &ref_comp_count);
		if (error != 0) {
			continue;
		}
		if (ref_comp_count < path_comp_count) {
			continue;
		}
		error = path_component(ref, comp_index, &ref_comp, &ref_comp_size);
		if (error != 0) {
			continue;
		}
		if (path_comp_size != ref_comp_size) {
			continue;
		}
		if (strncmp(path_comp, ref_comp, path_comp_size) == 0) {
			array_index = i;
			break;
		}
	}
	if (array_index == -1) {
		git_strarray_free(&array);
		return -1;
	}
	struct stat commit_stat = {};
	if (ref_comp_count == path_comp_count) {
		commit_stat.st_mode = S_IFLNK | 0644;
	} else {
		commit_stat.st_mode = S_IFDIR | 0755;
		commit_stat.st_size = 1337;
	}

	*stbuf = commit_stat;
	git_strarray_free(&array);
	return 0;
}


int rogitfs_refs_readlink(const char *path, char *buf, size_t size) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_strarray array = {};

	int error = git_reference_list(&array, private->repo);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_reference_list %d %s\n", giterr->klass, giterr->message);
		return -1;
	}

	size_t pathlen = strlen(path);

	for (unsigned int i = 0; i < array.count; i++) {
		const char *orig_ref = array.strings[i];
		const char *ref = orig_ref;
		if (strncmp(ref, "refs/", 5) != 0) {
			continue;
		}
		ref = ref+5;
		if (strncmp(ref, path, pathlen) == 0) {
			git_oid oid = {};
			error = git_reference_name_to_id(&oid, private->repo, orig_ref);
			if (error != 0) {
				const git_error *giterr = git_error_last();
				fprintf(stderr, "git_reference_name_to_id %d %s\n", giterr->klass, giterr->message);
				git_strarray_free(&array);
				return -1;
			}
			unsigned int ref_comp_count = 0;
			error = path_component_count(ref, &ref_comp_count);
			if (error != 0) {
				fprintf(stderr, "path_component_count %d \n", error);
				continue;
			}
			char *cur = buf;
			for (unsigned int j = 0; j < ref_comp_count; j++) {
				if (size - (cur-buf) - 1  >= 3 ) {
					cur[0] = '.';
					cur[1] = '.';
					cur[2] = '/';
					cur = cur+3;
				}
			}
			size_t towrite = size - (cur-buf) - 1;
			if (towrite > 7) {
				strncpy(cur, "commit/", 7);
				cur = cur+7;
			}
			towrite = size - (cur-buf) - 1;
			if (towrite > 0) {
				git_oid_tostr(cur, towrite, &oid);
				cur = cur+towrite;
			}
			cur[0] = 0;
			git_strarray_free(&array);
			return 0;
		}
	}
	git_strarray_free(&array);

	return -1;
}
