// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "rogitfs_common.h"
#include "rogitfs_inherit.h"


static int rogitfs_inherit_readdir_commits(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	const char *comp = NULL;
	unsigned int comp_size = 0;
	int error = path_component(path, 0, &comp, &comp_size);
	if (error != 0) {
		return -1;
	}
	if (comp_size != GIT_OID_HEXSZ) {
		return -1;
	}
	git_oid oid = {};
	error = git_oid_fromstr(&oid, comp);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_oid_fromstr %d %s\n", giterr->klass, giterr->message);
		return -1;
	}
	git_object *obj = 0;
	error = git_object_lookup(&obj, private->repo, &oid, GIT_OBJECT_COMMIT);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_object_lookup %d %s\n", giterr->klass, giterr->message);
		return -1;
	}
	git_commit *commit = (git_commit *)obj;
	unsigned int parent_count = git_commit_parentcount(commit);
	char buff[100];

	struct stat commit_stat = {
		.st_mode = S_IFDIR | 0755,
		.st_size = 1337
	};

	for (unsigned int i = 0; i < parent_count; i++) {
		memset(buff, 0, sizeof(char) * 100);
		snprintf(buff, 99, "%d", i);
		int res = filler(buf, buff, &commit_stat, 0, 0);
		if (res != 0) {
			return -1;
		}
	}

	git_object_free(obj);

	return 0;
}

static int rogitfs_inherit_readdir_root(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	struct odb_fill_payload payload = {
		.buf = buf,
		.filler = filler,
		.type = GIT_OBJECT_COMMIT,
		.private = private
	};
	int error = git_odb_foreach(private->odb, &rogitfs_readdir_odb_fill, &payload);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_odb_foreach %d %s\n", giterr->klass, giterr->message);
		return -ENOENT;
	}
	return 0;

}

int rogitfs_inherit_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	if (path[0] == '/') {

		return rogitfs_inherit_readdir_commits(path+1, buf, filler, offset, fi, flags);

	} else {

		return rogitfs_inherit_readdir_root(path, buf, filler, offset, fi, flags);

	}

	return -1;
}


int rogitfs_inherit_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {

	unsigned int comp_count = 0;
	int error = path_component_count(path, &comp_count);
	if (error != 0) {
		return -1;
	}

	struct stat obj_stat = {};

	if (comp_count == 1) {

		obj_stat.st_mode = S_IFDIR | 0755;
		*stbuf = obj_stat;
		return 0;

	} else if (comp_count == 2) {

		obj_stat.st_mode = S_IFLNK | 0644;
		*stbuf = obj_stat;
		return 0;

	}

	return -1;
}

int rogitfs_inherit_readlink(const char *path, char *buf, size_t size) {

	unsigned int comp_count = 0;
	int error = path_component_count(path, &comp_count);
	if (error != 0) {
		return -1;
	}

	if (comp_count != 2) {
		return -1;
	}

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	const char *comp = NULL;
	unsigned int comp_size = 0;
	error = path_component(path, 0, &comp, &comp_size);
	if (error != 0) {
		return -1;
	}
	if (comp_size != GIT_OID_HEXSZ) {
		return -1;
	}
	git_oid oid = {};
	error = git_oid_fromstr(&oid, comp);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_oid_fromstr %d %s\n", giterr->klass, giterr->message);
		return -1;
	}
	git_object *obj = 0;
	error = git_object_lookup(&obj, private->repo, &oid, GIT_OBJECT_COMMIT);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_object_lookup %d %s\n", giterr->klass, giterr->message);
		return -1;
	}
	git_commit *commit = (git_commit *)obj;
	unsigned int parent_count = git_commit_parentcount(commit);

	error = path_component(path, 1, &comp, &comp_size);
	if (error != 0) {
		return -1;
	}
	unsigned int parent_index = 0;
	int read = sscanf(comp, "%u", &parent_index);
	if (read == EOF) {
		int error = errno;
		if (error == 0) {}
		errno = 0;
		git_object_free(obj);
		return -1;
	}
	if (read != 1) {
		git_object_free(obj);
		return -1;
	}
	if (parent_index >= parent_count) {
		git_object_free(obj);
		return -1;
	}

	const git_oid *parent = NULL;
	parent = git_commit_parent_id(commit, parent_index);
	if (parent == NULL) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_commit_parent %d %s\n", giterr->klass, giterr->message);
		git_object_free(obj);
		return -1;
	}
	size_t towrite = 3 + GIT_OID_HEXSZ + 1;
	if (size < towrite) {
		towrite = size-1;
	}
	char *cur = buf;
	if (towrite >= 3) {
		strncpy(cur, "../", 3);
		cur = cur+3;
		towrite = towrite - 3;
	}
	git_oid_tostr(cur, towrite, parent);
	if (GIT_OID_HEXSZ > towrite) {
		cur = cur+towrite;
		towrite = 0;
	} else {
		cur = cur+GIT_OID_HEXSZ;
		towrite = towrite - GIT_OID_HEXSZ;
	}
	cur[0] = 0;

	git_object_free(obj);
	return 0;
}

