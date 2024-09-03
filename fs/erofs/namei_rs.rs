// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

//! EROFS Rust Kernel Module Helpers Implementation
//! This is only for experimental purpose. Feedback is always welcome.

#[allow(dead_code)]
#[allow(missing_docs)]
pub(crate) mod rust;
use core::ffi::*;
use core::ptr::NonNull;

use kernel::bindings::{d_obtain_alias, d_splice_alias, dentry, inode};
use kernel::container_of;

use rust::{erofs_sys::operations::*, kinode::*, ksuperblock::*};

/// Lookup function for dentry-inode lookup replacement.
#[no_mangle]
pub unsafe extern "C" fn erofs_lookup_rust(
    k_inode: NonNull<inode>,
    dentry: NonNull<dentry>,
    _flags: c_uint,
) -> *mut c_void {
    // SAFETY: We are sure that the inode is a Kernel Inode since alloc_inode is called
    let erofs_inode = unsafe { &*container_of!(k_inode.as_ptr(), KernelInode, k_inode) };
    // SAFETY: The super_block is initialized when the erofs_alloc_sbi_rust is called.
    let sbi = erofs_sbi(unsafe { NonNull::new(k_inode.as_ref().i_sb).unwrap() });
    // SAFETY: this is backed by qstr which is c representation of a valid slice.
    let name = unsafe {
        core::str::from_utf8_unchecked(core::slice::from_raw_parts(
            dentry.as_ref().d_name.name,
            dentry.as_ref().d_name.__bindgen_anon_1.__bindgen_anon_1.len as usize,
        ))
    };
    let k_inode: *mut inode =
        dir_lookup(sbi.filesystem.as_ref(), &mut sbi.inodes, erofs_inode, name)
            .map_or(core::ptr::null_mut(), |result| result.k_inode.as_mut_ptr());

    // SAFETY: We are sure that the inner k_inode has already been initialized.
    unsafe { d_splice_alias(k_inode, dentry.as_ptr()).cast() }
}

/// Exported as a replacement of erofs_get_parent.
#[no_mangle]
pub unsafe extern "C" fn erofs_get_parent_rust(child: NonNull<dentry>) -> *mut c_void {
    // SAFETY: We are sure that the inode is a Kernel Inode since alloc_inode is called
    let k_inode = unsafe { child.as_ref().d_inode };
    // SAFETY: The super_block is initialized when the erofs_alloc_sbi_rust is called.
    let sbi = erofs_sbi(unsafe { NonNull::new((*k_inode).i_sb).unwrap() }); // SAFETY: We are sure that the inode is a Kernel Inode since alloc_inode is called
    let inode = unsafe { &*container_of!(k_inode, KernelInode, k_inode) };
    let k_inode: *mut inode = dir_lookup(sbi.filesystem.as_ref(), &mut sbi.inodes, inode, "..")
        .map_or(core::ptr::null_mut(), |result| result.k_inode.as_mut_ptr());
    // SAFETY: We are sure that the inner k_inode has already been initialized
    unsafe { d_obtain_alias(k_inode).cast() }
}
