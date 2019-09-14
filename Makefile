# Copyright 2019, aw32
# SPDX-License-Identifier: GPL-3.0-only

CFLAGS = -D_FILE_OFFSET_BITS=64 $(shell pkg-config --cflags fuse3)
LFLAGS = $(shell pkg-config --libs libgit2) $(shell pkg-config --libs fuse3)
SRC = src/rogitfs.c src/rogitfs_head.c src/rogitfs_inherit.c src/rogitfs_refs.c src/rogitfs_commit.c src/rogitfs_obj.c src/rogitfs_common.c

all:
	gcc -O0 -g -Wall -Isrc -o rogitfs ${CFLAGS} ${LFLAGS} ${SRC}
