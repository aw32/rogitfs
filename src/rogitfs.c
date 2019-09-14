// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#include "rogitfs.h"
#include <stdio.h>
#include <fuse3/fuse_opt.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#include "rogitfs_obj.h"
#include "rogitfs_commit.h"
#include "rogitfs_refs.h"
#include "rogitfs_inherit.h"
#include "rogitfs_head.h"

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
    OPTION("--repopath=%s", repopath),
    OPTION("-h", show_help),
    OPTION("--help", show_help),
    FUSE_OPT_END
};

static struct rogitfs_private rogitfs_private = {};

int rogitfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {


	if (strncmp(path, "/obj/", 5) == 0) {

		long len = strlen(path);
		if (len != GIT_OID_HEXSZ + 5)
		{
			fputs("wrong format\n", stderr);
			return -1;
		}

		return rogitfs_obj_read((const char *)path+5, buf, size, offset, fi);

	} else if (strncmp(path, "/commit/", 8) == 0) {

		return rogitfs_commit_read((const char *)path+8, buf, size, offset, fi);

	} else {

		return -1;
	}

	return -1;
}

int rogitfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {

	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 1;
		stbuf->st_size = 1;
		stbuf->st_blocks = 1;
	} else if (strcmp(path, "/commit") == 0) {
		struct stat commit_stat = {
			.st_mode = S_IFDIR | 0755,
			.st_size = 1337
		};
		*stbuf = commit_stat;
	} else if (strcmp(path, "/obj") == 0) {
		struct stat obj_stat = {
			.st_mode = S_IFDIR | 0755,
			.st_size = 1337
		};
		*stbuf = obj_stat;
	} else if (strcmp(path, "/HEAD") == 0) {
		struct stat obj_stat = {
			.st_mode = S_IFLNK | 0644,
		};
		*stbuf = obj_stat;
	} else if (strcmp(path, "/inherit") == 0) {
		struct stat refs_stat = {
			.st_mode = S_IFDIR | 0755,
			.st_size = 1337
		};
		*stbuf = refs_stat;
	} else if (strncmp(path, "/inherit/", 9) == 0) {

		return rogitfs_inherit_getattr(path+9, stbuf, fi);

	} else if (strcmp(path, "/refs") == 0) {
		struct stat refs_stat = {
			.st_mode = S_IFDIR | 0755,
			.st_size = 1337
		};
		*stbuf = refs_stat;
	} else if (strncmp(path, "/refs/", 6) == 0) {

		return rogitfs_refs_getattr(path+6, stbuf, fi);

	} else if (strncmp(path, "/obj/", 5) == 0) {

		long len = strlen(path);
		if (len != GIT_OID_HEXSZ + 5) {
			return -ENOENT;
		}

		return rogitfs_obj_getattr(path+5, stbuf, fi);

	} else if (strncmp(path, "/commit/", 8) == 0) {

		return rogitfs_commit_getattr((const char *)path+8, stbuf, fi);
	} else {
		res = -ENOENT;
	}

	return res;
}

int rogitfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

	if (strcmp(path, "/") == 0) {

		int res = 0;

		res = filler(buf, ".", NULL, 0, 0);
		if (res != 0) {
			return -ENOENT;
		}
		res = filler(buf, "..", NULL, 0, 0);
		if (res != 0) {
			return -ENOENT;
		}
		struct stat commit_stat = {
			.st_mode = S_IFDIR | 0755,
			.st_size = 1337
		};
		res = filler(buf, "commit", &commit_stat, 0, 0);
		if (res != 0) {
			return -ENOENT;
		}
		struct stat obj_stat = {.st_mode = S_IFDIR | 0755};
		res = filler(buf, "obj", &obj_stat, 0, 0);
		if (res != 0) {
			return -ENOENT;
		}
		struct stat refs_stat = {.st_mode = S_IFDIR | 0755};
		res = filler(buf, "refs", &refs_stat, 0, 0);
		if (res != 0) {
			return -ENOENT;
		}
		struct stat inherit_stat = {.st_mode = S_IFDIR | 0755};
		res = filler(buf, "inherit", &inherit_stat, 0, 0);
		if (res != 0) {
			return -ENOENT;
		}
		struct stat head_stat = {.st_mode = S_IFLNK | 0644};
		res = filler(buf, "HEAD", &head_stat, 0, 0);
		if (res != 0) {
			return -ENOENT;
		}

	} else if (strcmp(path, "/obj") == 0) {

		return rogitfs_obj_readdir((const char *)path+4, buf, filler, offset, fi, flags);

	} else if (strncmp(path, "/commit", 7) == 0) {

		return rogitfs_commit_readdir(path+7, buf, filler, offset, fi, flags);

	} else if (strncmp(path, "/refs", 5) == 0) {

		return rogitfs_refs_readdir(path+5, buf, filler, offset, fi, flags);

	} else if (strncmp(path, "/inherit", 8) == 0) {

		return rogitfs_inherit_readdir(path+8, buf, filler, offset, fi, flags);

	} else {
		return -ENOENT;
	}

	return 0;
}

int rogitfs_readlink(const char *path, char *buf, size_t size) {

	if (strncmp(path, "/commit/", 8) == 0)
	{
		return rogitfs_commit_readlink(path+8, buf, size);

	} else if (strncmp(path, "/refs/", 6) == 0) {

		return rogitfs_refs_readlink(path+6, buf, size);

	} else if (strncmp(path, "/inherit/", 9) == 0) {

		return rogitfs_inherit_readlink(path+9, buf, size);

	} else if (strcmp(path, "/HEAD") == 0) {

		return rogitfs_head_readlink(path+5, buf, size);

	} else {
		return -1;
	}

	return -1;
}

void rogitfs_destroy(void *private_data) {

	struct rogitfs_private *private = (struct rogitfs_private *)private_data;

	if (private->odb != NULL) {
		git_odb_free(private->odb);
		private->odb = NULL;
	}

	if (private->repo != NULL) {
		git_repository_free(private->repo);
		private->repo = NULL;
	}


	git_libgit2_shutdown();

}

static void show_help(const char *progname)
{   
    printf("usage: %s [options] <mountpoint>\n\n", progname);
    printf("File-system specific options:\n"
		   "    --repopath=<s>      Path to repository\n"
		   "                        (default: working dir)\n"
           "\n");
}


int main(int argc, char *argv[]) {

	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	options.repopath = strdup(".");
	
	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;
	char *rpath = realpath(options.repopath, NULL);
	if (rpath == NULL) {
		int err = errno;
		errno = 0;
		fprintf(stderr, "realpath %d %s\n", err, strerror(err));
		exit(1);
	}
	repopath = rpath;

	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

	git_libgit2_init();

	int error = git_repository_open(&rogitfs_private.repo, repopath);

	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_error %d %s\n", giterr->klass, giterr->message);
		exit(1);
	}

	error = git_repository_odb(&rogitfs_private.odb, rogitfs_private.repo);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_repository_odb %d %s\n", giterr->klass, giterr->message);
	}

	ret = fuse_main(args.argc, args.argv, &rogitfs_operations, &rogitfs_private);
	fuse_opt_free_args(&args);
	if (repopath != NULL) {
		free(repopath);
		repopath = NULL;
	}
	return ret;
}

static struct fuse_operations rogitfs_operations = {
	.destroy 		= rogitfs_destroy,
	.read			= rogitfs_read,
	.readdir		= rogitfs_readdir,
	.getattr		= rogitfs_getattr,
	.readlink		= rogitfs_readlink,
};

