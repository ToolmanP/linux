// SPDX-License-Identifier: GPL-2.0-later
// EROFS Rust Bindings Before VFS Patch Sets for Rust

#ifndef __EROFS_RUST_BINDINGS_H
#define __EROFS_RUST_BINDINGS_H

#include <linux/fs.h>

extern const unsigned long EROFS_SB_INFO_OFFSET_RUST;
extern void *erofs_alloc_sbi_rust(struct super_block *sb);
extern void *erofs_free_sbi_rust(struct super_block *sb);
#endif
