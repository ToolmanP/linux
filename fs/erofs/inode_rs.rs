// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

//! EROFS Rust Kernel Module Helpers Implementation
//! This is only for experimental purpose. Feedback is always welcome.

#[allow(dead_code)]
#[allow(missing_docs)]
pub(crate) mod rust;

use core::ffi::*;
use core::mem::{offset_of, size_of};
use core::ptr::NonNull;
use kernel::bindings::{inode, super_block};
use kernel::container_of;
use rust::{
    erofs_sys::{operations::*, *},
    kinode::*,
    ksuperblock::erofs_sbi,
};

/// Used as a size hint to be exported to kmem_caceh_create
#[no_mangle]
pub static EROFS_INODE_SIZE_RUST: c_uint = size_of::<KernelInode>() as c_uint;

/// Used as a hint offset to be exported so EROFS_VFS_I to find the embedded the vfs inode.
#[no_mangle]
pub static EROFS_VFS_INODE_OFFSET_RUST: c_ulong = offset_of!(KernelInode, k_inode) as c_ulong;

/// Used as a hint offset to be exported to EROFS_I to find the embedded c side erofs_inode.
#[no_mangle]
pub static EROFS_I_OFFSET_RUST: c_long =
    offset_of!(KernelInode, k_opaque) as c_long - offset_of!(KernelInode, k_inode) as c_long;

/// Exported as iget replacement
#[no_mangle]
pub unsafe extern "C" fn erofs_iget_rust(sb: NonNull<super_block>, nid: Nid) -> *mut c_void {
    // SAFETY: The super_block is initialized when the erofs_alloc_sbi_rust is called.
    let sbi = erofs_sbi(sb);
    read_inode(sbi.filesystem.as_ref(), &mut sbi.inodes, nid)
        .map_or_else(|e| e.into(), |inode| inode.k_inode.as_mut_ptr().cast())
}

fn try_fill_inode(k_inode: NonNull<inode>, nid: Nid) -> PosixResult<()> {
    // SAFETY: The super_block is initialized when the erofs_fill_inode_rust is called.
    let sbi = erofs_sbi(unsafe { NonNull::new(k_inode.as_ref().i_sb).unwrap() });
    // SAFETY: k_inode is a part of KernelInode.
    let erofs_inode: &mut KernelInode = unsafe {
        &mut *(container_of!(k_inode.as_ptr(), KernelInode, k_inode) as *mut KernelInode)
    };
    let info = sbi.filesystem.read_inode_info(nid)?;
    erofs_inode.nid.write(nid);
    erofs_inode.shared_entries.write(
        sbi.filesystem
            .read_inode_xattrs_shared_entries(nid, &info)?,
    );
    erofs_inode.info.write(info);
    Ok(())
}
/// Exported as fill_inode additional fill inode
#[no_mangle]
pub unsafe extern "C" fn erofs_fill_inode_rust(k_inode: NonNull<inode>, nid: Nid) -> c_int {
    try_fill_inode(k_inode, nid).map_or_else(|e| i32::from(e) as c_int, |_| 0)
}
