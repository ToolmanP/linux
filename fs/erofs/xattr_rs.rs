// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

//! EROFS Rust Kernel Module Helpers Implementation
//! This is only for experimental purpose. Feedback is always welcome.

#[allow(dead_code)]
#[allow(missing_docs)]
pub(crate) mod rust;
use core::ffi::*;
use core::ptr::NonNull;

use kernel::bindings::{dentry, inode};
use kernel::container_of;

use rust::{erofs_sys::xattrs::*, kinode::*, ksuperblock::*};

/// Used as a replacement for erofs_getattr.
#[no_mangle]
pub unsafe extern "C" fn erofs_getxattr_rust(
    k_inode: NonNull<inode>,
    index: c_uint,
    name: NonNull<u8>,
    buffer: NonNull<u8>,
    size: usize,
) -> c_int {
    // SAFETY: super_block and superblockinfo is always initialized in k_inode.
    let sbi = erofs_sbi(unsafe { NonNull::new(k_inode.as_ref().i_sb).unwrap() });
    // SAFETY: We are sure that the inode is a Kernel Inode since alloc_inode is called
    let erofs_inode = unsafe { &*container_of!(k_inode.as_ptr(), KernelInode, k_inode) };
    // SAFETY: buffer is always initialized in the caller and name is null terminated C string.
    unsafe {
        match sbi.filesystem.get_xattr(
            erofs_inode,
            index,
            core::ffi::CStr::from_ptr(name.as_ptr().cast()).to_bytes(),
            &mut Some(core::slice::from_raw_parts_mut(
                buffer.as_ptr().cast(),
                size,
            )),
        ) {
            Ok(value) => match value {
                XAttrValue::Buffer(x) => x as c_int,
                _ => unreachable!(),
            },
            Err(e) => i32::from(e) as c_int,
        }
    }
}

/// Used as a replacement for erofs_getattr_nobuf.
#[no_mangle]
pub unsafe extern "C" fn erofs_getxattr_nobuf_rust(
    k_inode: NonNull<inode>,
    index: u32,
    name: NonNull<u8>,
    mut value: NonNull<*mut u8>,
) -> c_int {
    // SAFETY: super_block and superblockinfo is always initialized in k_inode.
    let sbi = erofs_sbi(unsafe { NonNull::new(k_inode.as_ref().i_sb).unwrap() });
    // SAFETY: We are sure that the inode is a Kernel Inode since alloc_inode is called
    let erofs_inode = unsafe { &*container_of!(k_inode.as_ptr(), KernelInode, k_inode) };
    // SAFETY: buffer is always initialized in the caller and name is null terminated C string.
    unsafe {
        match sbi.filesystem.get_xattr(
            erofs_inode,
            index,
            core::ffi::CStr::from_ptr(name.as_ptr().cast()).to_bytes(),
            &mut None,
        ) {
            Ok(xattr_value) => match xattr_value {
                XAttrValue::Vec(v) => {
                    let rc = v.len() as c_int;
                    *value.as_mut() = v.leak().as_mut_ptr().cast();
                    rc
                }

                _ => unreachable!(),
            },
            Err(e) => i32::from(e) as c_int,
        }
    }
}

/// Used as a replacement for erofs_getattr.
#[no_mangle]
pub unsafe extern "C" fn erofs_listxattr_rust(
    dentry: NonNull<dentry>,
    buffer: NonNull<u8>,
    size: usize,
) -> c_long {
    // SAFETY: dentry is always initialized in the caller.
    let k_inode = unsafe { dentry.as_ref().d_inode };
    // SAFETY: We are sure that the inode is a Kernel Inode since alloc_inode is called.
    let erofs_inode = unsafe { &*container_of!(k_inode, KernelInode, k_inode) };
    // SAFETY: The super_block is initialized when the erofs_alloc_sbi_rust is called.
    let sbi = erofs_sbi(unsafe { NonNull::new((*k_inode).i_sb).unwrap() });
    match sbi.filesystem.list_xattrs(
        erofs_inode,
        // SAFETY: buffer is always initialized in the caller.
        unsafe { core::slice::from_raw_parts_mut(buffer.as_ptr().cast(), size) },
    ) {
        Ok(value) => value as c_long,
        Err(e) => i32::from(e) as c_long,
    }
}
