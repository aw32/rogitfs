// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#ifndef __ROGITFS_H_
#define __ROGITFS_H_

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <fuse3/fuse_opt.h>
#include <git2.h>
#include "rogitfs_common.h"

static struct rogitfs_private rogitfs_private;

static char* repopath = NULL;


static struct options {
    const char *repopath;
    int show_help;
} options;

static struct fuse_operations rogitfs_operations;

int rogitfs_open(const char *path, struct fuse_file_info *file_info);

int rogitfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int rogitfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);

int rogitfs_statfs(const char *path, struct statvfs *buf);

int rogitfs_opendir(const char *path, struct fuse_file_info *file_info);

int rogitfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
	off_t offset, struct fuse_file_info *fi,
	enum fuse_readdir_flags flags);

int rogitfs_releasedir(const char *path, struct fuse_file_info *file_info);

int rogitfs_access(const char *path, int mode);

int rogitfs_readlink(const char *path, char *buf, size_t size);

int rogitfs_getxattr(const char *path, const char *name, char *value, size_t size);


void *rogitfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg);

void rogitfs_destroy(void *private_data);

int main(int argc, char *argv[]);

#endif
