// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later
use super::erofs_sys::superblock::*;
use super::kinode::*;
use alloc::boxed::Box;
use core::{ffi::c_void, ptr::NonNull};
use kernel::bindings::super_block;
use kernel::types::ForeignOwnable;

pub(crate) type KernelOpaque = NonNull<*mut c_void>;
/// KernelSuperblockInfo defined by embedded Kernel Inode
pub(crate) type KernelSuperblockInfo =
    SuperblockInfo<KernelInode, KernelInodeCollection, KernelOpaque>;

/// SAFETY:
/// Cast the c_void back to KernelSuperblockInfo.
/// This seems to be prune to some concurrency issues
/// but the fact is that only KernelInodeCollection field can have mutability.
/// However, it's backed by the original iget_locked5 and it's already preventing
/// any concurrency issues. So it's safe to be casted mutable here even if it's not backed by
/// Arc/Mutex instead of using generic method from Foreign Ownable which only provides
/// immutable reference casting which is not enough.
/// Since the pointer always live as long as this module exists, it's safe to declare it as static.
pub(crate) fn erofs_sbi(sb: NonNull<super_block>) -> &'static mut KernelSuperblockInfo {
    unsafe { &mut *(sb.as_ref().s_fs_info).cast::<KernelSuperblockInfo>() }
}

pub(crate) fn free_sbi(sb: NonNull<super_block>) {
    unsafe { Box::<KernelSuperblockInfo>::from_foreign(sb.as_ref().s_fs_info) };
}
