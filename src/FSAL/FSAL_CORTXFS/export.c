/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) Panasas Inc., 2011
 * Author: Jim Lieb jlieb@panasas.com
 *
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * -------------
 */

/* export.c
 * KVSFS FSAL export object
 */

#include <stdint.h>
#include "common/log.h"
#include <config_parsing.h>
#include <fsal_types.h>
#include <FSAL/fsal_config.h>
#include <FSAL/fsal_commonlib.h>
#include "fsal_internal.h"
#include "kvsfs_methods.h"
#include "cortxfs.h"
#include "nfs_exports.h"
extern struct kvsfs_fsal_module KVSFS;
static int kvsfs_conf_pnfs_commit(void *node,
				  void *link_mem,
				  void *self_struct,
				  struct config_error_type *err_type);

static struct config_item export_params[] = {
	CONF_ITEM_NOOP("name"),
	CONF_ITEM_STR("cortxfs_config", 0, MAXPATHLEN, NULL,
		      kvsfs_fsal_export, cfs_config),
	CONFIG_EOL
};


static struct config_block export_param = {
	.dbus_interface_name = "org.ganesha.nfsd.config.fsal.kvsfs-export",
	.blk_desc.name = "FSAL",
	.blk_desc.type = CONFIG_BLOCK,
	.blk_desc.u.blk.init = noop_conf_init,
	.blk_desc.u.blk.params = export_params,
	.blk_desc.u.blk.commit = noop_conf_commit
};

static void kvsfs_export_ops_init(struct export_ops *ops);

/* create_export
 * Create an export point and return a handle to it to be kept
 * in the export list.
 * First lookup the fsal, then create the export and then put the fsal back.
 * returns the export with one reference taken.
 */

fsal_status_t kvsfs_create_export(struct fsal_module *fsal_hdl,
				void *parse_node,
				struct config_error_type *err_type,
				const struct fsal_up_vector *up_ops)
{
	struct kvsfs_fsal_export *myself = NULL;
	fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
	int retval = 0;
	fsal_errors_t fsal_error = ERR_FSAL_INVAL;
	struct cfs_fs *cfs_fs = NULL;
	uint8_t pnfs_role; 

	uint16_t fsid =	op_ctx->ctx_export->export_id;
	LogEvent(COMPONENT_FSAL, "export id %d", (int)fsid);

	myself = gsh_calloc(1, sizeof(struct kvsfs_fsal_export));

	fsal_export_init(&myself->export);
	kvsfs_export_ops_init(&myself->export.exp_ops);
	
	myself->export.up_ops = up_ops;
	retval = load_config_from_node(parse_node,
				       &export_param,
				       myself,
				       true,
				       err_type);
	if (retval != 0) {
		status = fsalstat(fsal_error, retval);
		goto errout;
	}
	
	/* Get the pNFS Role */
	pnfs_role = get_pnfs_role(&KVSFS);

	retval = cfs_fs_open(op_ctx->ctx_export->fullpath, &cfs_fs);
	if (retval != 0) {
		LogMajor(COMPONENT_FSAL, "FS open failed :%s",
				op_ctx->ctx_export->fullpath);

		if (retval != -ENOENT) {
			cfs_fini();
		}
		status = fsalstat(fsal_error, retval);
		goto errout;
	}

	retval = fsal_attach_export(fsal_hdl, &myself->export.exports);
	if (retval != 0) {
		status = fsalstat(fsal_error, retval);
		goto err_locked;	/* seriously bad */
	}
	myself->export.fsal = fsal_hdl;

	op_ctx->fsal_export = &myself->export;

	if ( pnfs_role == CORTXFS_PNFS_DS || pnfs_role == CORTXFS_PNFS_BOTH ) {
		myself->pnfs_ds_enabled = myself->export.exp_ops.fs_supports(&myself->export,
					  fso_pnfs_ds_supported);
	}


	if ( pnfs_role == CORTXFS_PNFS_MDS || pnfs_role == CORTXFS_PNFS_BOTH ) {
		myself->pnfs_mds_enabled = myself->export.exp_ops.fs_supports(&myself->export,
					   fso_pnfs_mds_supported);
	}

	myself->cfs_fs = cfs_fs;
	myself->fs_id = fsid;

	/* TODO:PORTING: pNFS support */
	if (myself->pnfs_ds_enabled) {
		struct fsal_pnfs_ds *pds = NULL;

		status = fsal_hdl->m_ops.
			fsal_pnfs_ds(fsal_hdl, parse_node, &pds);
		if (status.major != ERR_FSAL_NO_ERROR)
			goto err_locked;

		/* special case: server_id matches export_id */
		pds->id_servers = op_ctx->ctx_export->export_id;
		pds->mds_export = op_ctx->ctx_export;
		pds->mds_fsal_export = op_ctx->fsal_export;

		if (!pnfs_ds_insert(pds)) {
			LogCrit(COMPONENT_CONFIG,
				"Server id %d already in use.",
				pds->id_servers);
			status.major = ERR_FSAL_EXIST;
			fsal_pnfs_ds_fini(pds);
			gsh_free(pds);
			goto err_locked;
		}

		LogCrit(COMPONENT_FSAL,
			"kvsfs_fsal_create: pnfs DS was enabled for [%s]",
			op_ctx->ctx_export->fullpath);
	}

	if (myself->pnfs_mds_enabled) {
		LogCrit(COMPONENT_FSAL,
			"kvsfs_fsal_create: pnfs MDS was enabled for [%s]",
			op_ctx->ctx_export->fullpath);
		export_ops_pnfs(&myself->export.exp_ops);
      		fsal_ops_pnfs(&myself->export.fsal->m_ops);
	}

	return fsalstat(ERR_FSAL_NO_ERROR, 0);

err_locked:
	if (myself->export.fsal != NULL)
		fsal_detach_export(fsal_hdl, &myself->export.exports);
	cfs_fs_close(cfs_fs);

errout:
	/* elvis has left the building */
	gsh_free(myself);

	return status;

}

static int kvsfs_conf_pnfs_commit(void *node,
				  void *link_mem,
				  void *self_struct,
				  struct config_error_type *err_type)
{
	/* struct lustre_pnfs_param *lpp = self_struct; */

	/* Verifications/parameter checking to be added here */

	return 0;
}

/* export object methods
 */

static void export_release(struct fsal_export *exp_hdl)
{
	struct kvsfs_fsal_export *myself;

	myself = container_of(exp_hdl, struct kvsfs_fsal_export, export);

	fsal_detach_export(exp_hdl->fsal, &exp_hdl->exports);
	free_export_ops(exp_hdl);

	gsh_free(myself);		/* elvis has left the building */
}

/* statvfs-like call */
static fsal_status_t get_dynamic_info(struct fsal_export *exp_hdl,
				      struct fsal_obj_handle *obj_hdl,
				      fsal_dynamicfsinfo_t *infop)
{
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/* kvsfs_export_ops_init
 * overwrite vector entries with the methods that we support
 */

void kvsfs_export_ops_init(struct export_ops *ops)
{
	ops->release = export_release;
	ops->lookup_path = kvsfs_lookup_path;
	ops->wire_to_host = kvsfs_extract_handle;
	ops->create_handle = kvsfs_create_handle;
	ops->get_fs_dynamic_info = get_dynamic_info;
	ops->alloc_state = kvsfs_alloc_state;
	ops->free_state = kvsfs_free_state;
}

