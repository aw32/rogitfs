// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "rogitfs_obj.h"
#include "rogitfs_common.h"


int rogitfs_obj_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_oid oid = {};
	int error = git_oid_fromstr(&oid, path);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_oid_fromstr %d %s\n", giterr->klass, giterr->message);
		return -1;
	}

	git_object *obj = NULL;
	error = git_object_lookup(&obj, private->repo, &oid, GIT_OBJECT_ANY);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_object_lookup %d %s\n", giterr->klass, giterr->message);
		return -1;

	}

	git_odb_object *odb_obj = NULL;
	error = git_odb_read(&odb_obj, private->odb, &oid);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_odb_read %d %s\n", giterr->klass, giterr->message);
		git_odb_object_free(odb_obj);
		return -1;
	}

	const void *data = git_odb_object_data(odb_obj);
	if (data == NULL) {
		fputs("git_odb_object_data is NULL\n", stderr);
		git_object_free(obj);
		git_odb_object_free(odb_obj);
		return -1;
	}

	size_t objlen = git_odb_object_size(odb_obj);

	size_t toread = size;
	size_t end = offset + toread;
	if (end > objlen) {
		toread = toread-(end-objlen);
	}

	if (toread == 0) {
		git_object_free(obj);
		git_odb_object_free(odb_obj);
		return 0;
	}

	memcpy(buf, data+offset, toread);

	git_object_free(obj);
	git_odb_object_free(odb_obj);

	return toread;
}

int rogitfs_obj_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_oid oid = {};
	int error = git_oid_fromstr(&oid, path);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_oid_fromstr %d %s\n", giterr->klass, giterr->message);
		return -ENOENT;
	}

	git_object *obj = NULL;
	error = git_object_lookup(&obj, private->repo, &oid, GIT_OBJECT_ANY);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_object_lookup %d %s\n", giterr->klass, giterr->message);
		return -ENOENT;
	}

	git_odb_object *odb_obj = NULL;
	error = git_odb_read(&odb_obj, private->odb, &oid);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_odb_read %d %s\n", giterr->klass, giterr->message);
		git_odb_object_free(odb_obj);
		return -ENOENT;
	}


	struct stat obj_stat = {
		.st_mode = S_IFREG | 0444,
		.st_size = git_odb_object_size(odb_obj)
	};

	git_object_free(obj);
	git_odb_object_free(odb_obj);

	*stbuf = obj_stat;

	return 0;
}

int rogitfs_obj_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	struct odb_fill_payload payload = {
		.buf = buf,
		.filler = filler,
		.type = GIT_OBJECT_ANY,
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
