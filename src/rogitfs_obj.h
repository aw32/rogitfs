// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#ifndef __ROGITFS_OBJ_H
#define __ROGITFS_OBJ_H

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <git2.h>

int rogitfs_obj_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int rogitfs_obj_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);

int rogitfs_obj_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);

#endif
