// SPDX-License-Identifier: GPL-2.0-later
// EROFS Rust Bindings Before VFS Patch Sets for Rust

#ifndef __EROFS_RUST_BINDINGS_H
#define __EROFS_RUST_BINDINGS_H

#include <linux/fs.h>

typedef u64 erofs_nid_t;
typedef u64 erofs_off_t;
/* data type for filesystem-wide blocks number */
typedef u32 erofs_blk_t;

extern const unsigned long EROFS_SB_INFO_OFFSET_RUST;
extern const unsigned int EROFS_INODE_SIZE_RUST;
extern const unsigned long EROFS_VFS_INODE_OFFSET_RUST;
extern const long EROFS_I_OFFSET_RUST;

extern void *erofs_alloc_sbi_rust(struct super_block *sb);
extern void *erofs_free_sbi_rust(struct super_block *sb);
extern int erofs_iget5_eq_rust(struct inode *inode, void *opaque);
extern struct inode *erofs_iget_rust(struct super_block *sb, erofs_nid_t nid);
extern struct dentry *erofs_lookup_rust(struct inode *inode, struct dentry *dentry,
			      unsigned int flags);
extern struct dentry *erofs_get_parent_rust(struct dentry *dentry);
extern int erofs_readdir_rust(struct file *file, struct dir_context *ctx);
#endif
