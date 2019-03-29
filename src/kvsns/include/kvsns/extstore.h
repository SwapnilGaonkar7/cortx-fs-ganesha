/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) CEA, 2016
 * Author: Philippe Deniel  philippe.deniel@cea.fr
 *
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * -------------
 */

/* extstore.h
 * KVSNS/extstore: header file for external storage interface
 */

#ifndef _POSIX_STORE_H
#define _POSIX_STORE_H

#include <libgen.h>		/* used for 'dirname' */
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <ini_config.h>
#include <kvsns/kvsal.h>
#include <kvsns/kvsns.h>
#include <kvsns/common.h>

int extstore_init(struct collection_item *cfg_items);
int extstore_create(kvsns_ino_t object);
int extstore2_create(void *ctx,
		     kvsns_ino_t object,
		     kvsns_fid_t *fid);
int extstore_read(kvsns_ino_t *ino,
		  off_t offset,
		  size_t buffer_size,
		  void *buffer,
		  bool *end_of_file,
		  struct stat *stat);
int extstore2_read(void *ctx, kvsns_fid_t *kfid, off_t offset,
		   size_t buffer_size, void *buffer, bool *end_of_file,
		   struct stat *stat);
int extstore_write(kvsns_ino_t *ino,
		   off_t offset,
		   size_t buffer_size,
		   void *buffer,
		   bool *fsal_stable,
		   struct stat *stat);
int extstore2_write(void *ctx, kvsns_fid_t *kfid, off_t offset, size_t buffer_size,
		    void *buffer, bool *fsal_stable, struct stat *stat);
int extstore_del(kvsns_ino_t *ino);
int extstore2_del(void *ctx, kvsns_ino_t *ino, kvsns_fid_t *fid);
int extstore_truncate(kvsns_ino_t *ino,
		      off_t filesize,
		      bool on_obj_store,
		      struct stat *stat);
int extstore_attach(kvsns_ino_t *ino,
		    char *objid, int objid_len);
int extstore_get_fid(kvsns_ino_t object,
		     kvsns_fid_t *fid);
int extstore_ino_to_fid(void *ctx, kvsns_ino_t object, kvsns_fid_t *fid);
#endif
