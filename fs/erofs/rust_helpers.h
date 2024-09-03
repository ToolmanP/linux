// SPDX-License-Identifier: GPL-2.0-later
// This is a helpers collection to dodge the missing macros or inline functions in bindgen

#ifndef __EROFS_RUST_HELPERS_H
#define __EROFS_RUST_HELPERS_H

#include "internal.h"

static inline bool erofs_is_fscache_mode_rust_helper(struct super_block *sb,
						     struct erofs_sb_info *sbi)
{
	return IS_ENABLED(CONFIG_EROFS_FS_ONDEMAND) &&
	       !erofs_is_fileio_mode(sbi) && !sb->s_bdev;
}

void *erofs_read_metabuf_rust_helper(struct super_block *sb,
				     struct erofs_sb_info *sbi,
				     erofs_off_t offset);
void erofs_put_metabuf_rust_helper(void *addr);
extern int erofs_fill_inode_rust(struct inode *inode, erofs_nid_t nid);
struct inode *erofs_iget_locked_rust_helper(struct super_block *sb,
						   erofs_nid_t nid);
#endif
