// Copyright 2019, aw32
// SPDX-License-Identifier: GPL-3.0-only

#include <stdio.h>
#include <string.h>
#include "rogitfs_head.h"
#include "rogitfs_common.h"




int rogitfs_head_readlink(const char *path, char *buf, size_t size) {

	struct fuse_context *context = fuse_get_context();
	struct rogitfs_private *private = (struct rogitfs_private*)context->private_data;

	git_reference *head = NULL;
	int error = git_repository_head(&head, private->repo);
	if (error != 0) {
		const git_error *giterr = git_error_last();
		fprintf(stderr, "git_odb_read %d %s\n", giterr->klass, giterr->message);
		return -1;
	}

	const git_oid *oid = NULL;
	oid = git_reference_target(head);
	if (oid == NULL) {
		git_reference_free(head);
		return -1;
	}
	size_t towrite = size-1;
	char *cur = buf;
	if (towrite >= 7) {
		strncpy(cur, "commit/", 7);
		cur = cur+7;
		towrite = towrite - 7;
	}
	git_oid_tostr(cur, towrite, oid);
	if (towrite < GIT_OID_HEXSZ) {
		cur = cur+towrite;
		towrite = 0;
	} else {
		cur = cur+GIT_OID_HEXSZ;
		towrite = towrite - GIT_OID_HEXSZ;
	}
	cur[0] = 0;
	git_reference_free(head);

	return 0;
}

