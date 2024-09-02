#![allow(dead_code)]
// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

//! A pure Rust implementation of the EROFS filesystem.
//! Technical Details are documented in the [EROFS Documentation](https://erofs.docs.kernel.org/en/latest/)

// It's unavoidable to import alloc here. Since there are so many backends there and if we want to
// to use trait object to export Filesystem pointer. The alloc crate here is necessary.

#[cfg(not(CONFIG_EROFS_FS = "y"))]
extern crate alloc;

/// Erofs requires block index to a 32 bit unsigned integer.
pub(crate) type Blk = u32;
/// Erofs requires normal offset to be a 64bit unsigned integer.
pub(crate) type Off = u64;
/// Erofs requires inode nid to be a 64bit unsigned integer.
pub(crate) type Nid = u64;
/// Erofs Super Offset to read the ondisk superblock
pub(crate) const EROFS_SUPER_OFFSET: Off = 1024;
pub(crate) mod superblock;
