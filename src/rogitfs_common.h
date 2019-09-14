// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#ifndef __ROGITFS_COMMON_H__
#define __ROGITFS_COMMON_H__

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <git2.h>

struct rogitfs_private {
	git_repository *repo;
	git_odb *odb;
};

struct odb_fill_payload {
	void *buf;
	fuse_fill_dir_t filler;
	git_object_t type;
	struct rogitfs_private *private;
};

int rogitfs_readdir_tree_fill(void *buf, fuse_fill_dir_t filler, git_tree *tree, git_odb *odb);

int rogitfs_readdir_odb_fill(const git_oid *id, void *payload);

int rogitfs_get_path_object(const char *path, git_object **result_obj, git_filemode_t *result_mode, git_repository *repo, git_odb *odb);

int rogitfs_check_path_component(git_object *obj, const char *component, git_object **result_obj, git_filemode_t *result_mode, git_repository *repo, git_odb *odb);

int path_component(const char *path, unsigned int index, const char **result_comp, unsigned int *result_comp_size);

int path_component_count(const char *path, unsigned int *result_count);

#endif
