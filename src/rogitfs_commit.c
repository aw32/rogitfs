// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "rogitfs_common.h"
#include "rogitfs_commit.h"

int rogitfs_commit_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_object *obj = NULL;
	int res = 0;
	git_filemode_t mode = 0;
	res = rogitfs_get_path_object((char *)path, &obj, &mode, private->repo, private->odb);
	if (res != 0) {
		return -1;
	}
	if (git_object_type(obj) != GIT_OBJECT_BLOB) {
		return -1;
	}
	const git_oid *oid = git_object_id(obj);
	git_odb_object *odb_obj = NULL;
	int error = git_odb_read(&odb_obj, private->odb, oid);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_odb_read %d %s\n", giterr->klass, giterr->message);
		git_object_free(obj);
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

int rogitfs_commit_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_object *obj = NULL;
	int res = 0;
	git_filemode_t mode = 0;
	res = rogitfs_get_path_object((char *)path, &obj, &mode, private->repo, private->odb);
	if (res != 0) {
		return -ENOENT;
	}

	struct stat obj_stat = {};

	git_odb_object *odb_obj = NULL;
	const git_oid *oid = NULL;
	int error = 0;
	switch(git_object_type(obj)) {
	case GIT_OBJECT_COMMIT:
		obj_stat.st_mtim.tv_sec= git_commit_time((git_commit *)obj);

		obj_stat.st_mode = S_IFDIR | 0755;

		git_object_free(obj);
	break;
	case GIT_OBJECT_TREE:
		obj_stat.st_mode = S_IFDIR | 0755;

		git_object_free(obj);
	break;
	case GIT_OBJECT_BLOB:
		if ((mode & GIT_FILEMODE_LINK) == GIT_FILEMODE_LINK) {
			obj_stat.st_mode = S_IFLNK | 0644;
		} else {

			obj_stat.st_mode = S_IFREG | 0644;

			oid = git_object_id(obj);
			error = git_odb_read(&odb_obj, private->odb, oid);
			if (error != 0) {
				const git_error *giterr = git_error_last();
				fprintf(stderr, "git_odb_read %d %s\n", giterr->klass, giterr->message);
				git_object_free(obj);
				return -ENOENT;
			}

			obj_stat.st_size = git_odb_object_size(odb_obj);

			git_odb_object_free(odb_obj);

		}
		git_object_free(obj);
	break;
	default:
		git_object_free(obj);
		return -ENOENT;
	break;
	}

	*stbuf = obj_stat;
	return 0;
}


int rogitfs_commit_readlink(const char *path, char *buf, size_t size) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_object *obj = NULL;
	int res = 0;
	git_filemode_t mode = 0;
	res = rogitfs_get_path_object((char *)path, &obj, &mode, private->repo, private->odb);
	if (res != 0) {
		return -1;
	}
	if (git_object_type(obj) != GIT_OBJECT_BLOB) {
		return -1;
	}
	if ((mode & GIT_FILEMODE_LINK) != GIT_FILEMODE_LINK) {
		return -1;
	}
	const git_oid *oid = git_object_id(obj);
	git_odb_object *odb_obj = NULL;
	int error = git_odb_read(&odb_obj, private->odb, oid);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_odb_read %d %s\n", giterr->klass, giterr->message);
		git_object_free(obj);
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
	size_t towrite = objlen+1;
	if (size < towrite) {
		towrite = size-1;
	}
	memcpy(buf, data, towrite);
	buf[size-1] = 0;
	git_object_free(obj);
	git_odb_object_free(odb_obj);
	return 0;
}


int rogitfs_commit_readdir_commits(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_object *obj = NULL;
	git_filemode_t mode = 0;
	int res = 0;
	res = rogitfs_get_path_object((char *)path, &obj, &mode, private->repo, private->odb);
	if (res != 0) {
		return -ENOENT;
	}

	git_tree *tree = NULL;
	int error = 0;
	switch(git_object_type(obj)) {
	case GIT_OBJECT_COMMIT:
		error = git_commit_tree(&tree, (git_commit *)obj);
		if (error != 0) {
			const git_error *giterr = git_error_last();
			fprintf(stderr, "git_commit_tree %d %s\n", giterr->klass, giterr->message);
			git_object_free(obj);
			return -ENOENT;
		}

		res = rogitfs_readdir_tree_fill(buf, filler, tree, private->odb);
		git_tree_free(tree);
		git_object_free(obj);
	break;
	case GIT_OBJECT_TREE:
		res = rogitfs_readdir_tree_fill(buf, filler, (git_tree *)obj, private->odb);
		git_object_free(obj);
		if (res != 0) {
			return -ENOENT;
		}
	break;
	case GIT_OBJECT_BLOB:
		git_object_free(obj);
		return -ENOENT;
	break;
	default:
		git_object_free(obj);
		return -ENOENT;
	break;
	}

	return 0;
}


int rogitfs_commit_readdir_root(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

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


int rogitfs_commit_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	if (path[0] == '/')
	{
		return rogitfs_commit_readdir_commits(path+1, buf, filler, offset, fi, flags);
	}
	else
	{
		return rogitfs_commit_readdir_root(path, buf, filler, offset, fi, flags);
	}

	return -1;
}
