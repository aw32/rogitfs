// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#ifndef __ROGITFS_HEAD_H__
#define __ROGITFS_HEAD_H__

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <git2.h>

int rogitfs_head_readlink(const char *path, char *buf, size_t size);

#endif
