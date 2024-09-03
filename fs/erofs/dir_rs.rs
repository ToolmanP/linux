// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

//! EROFS Rust Kernel Module Helpers Implementation
//! This is only for experimental purpose. Feedback is always welcome.

#[allow(dead_code)]
#[allow(missing_docs)]
pub(crate) mod rust;
use core::ffi::*;
use core::ptr::NonNull;

use kernel::bindings::{dir_context, file};
use kernel::container_of;

use rust::{
    erofs_sys::{inode::*, *},
    kinode::*,
    ksuperblock::*,
};

/// Exported as a replacement of erofs_readdir.
#[no_mangle]
pub unsafe extern "C" fn erofs_readdir_rust(
    f: NonNull<file>,
    mut ctx: NonNull<dir_context>,
) -> c_int {
    // SAFETY: inode is always initialized in file.
    let k_inode = unsafe { f.as_ref().f_inode };
    // SAFETY: We are sure that the inode is a Kernel Inode since alloc_inode is called
    let erofs_inode = unsafe { &*container_of!(k_inode, KernelInode, k_inode) };
    // SAFETY: The super_block is always initialized when calling iget5_locked.
    let sb = unsafe { (*k_inode).i_sb };
    let sbi = erofs_sbi(NonNull::new(sb).unwrap());
    // SAFETY: ctx is nonnull.
    let offset = unsafe { ctx.as_ref().pos };
    match sbi
        .filesystem
        .fill_dentries(erofs_inode, offset as Off, &mut |dir, pos| unsafe {
            // inline expansion from dir_emit
            ctx.as_ref().actor.unwrap()(
                ctx.as_ptr(),
                dir.name.as_ptr().cast(),
                dir.name.len() as i32,
                pos as i64,
                dir.desc.nid as u64,
                dir.desc.file_type as u32,
            );
            ctx.as_mut().pos = pos as i64;
        }) {
        Ok(_) => {
            unsafe { ctx.as_mut().pos = erofs_inode.info().file_size() as i64 }
            0
        }
        Err(e) => (i32::from(e)) as c_int,
    }
}
