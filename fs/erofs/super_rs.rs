// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

//! EROFS Rust Kernel Module Helpers Implementation
//! This is only for experimental purpose. Feedback is always welcome.

#[allow(dead_code)]
#[allow(missing_docs)]
pub(crate) mod rust;

use core::ffi::*;
use core::mem::offset_of;
use core::ptr::NonNull;
use kernel::{bindings::super_block, types::ForeignOwnable};
use rust::{
    erofs_sys::{
        alloc_helper::*,
        data::backends::uncompressed::*,
        superblock::{mem::*, *},
        *,
    },
    kinode::*,
    ksources::*,
    ksuperblock::*,
};

fn try_alloc_sbi(sb: NonNull<super_block>) -> PosixResult<*const c_void> {
    //  We have to use heap_alloc here to erase the signature of MemFileSystem
    let sbi = heap_alloc(SuperblockInfo::new(
        heap_alloc(KernelFileSystem::try_new(UncompressedBackend::new(
            MetabufSource::new(sb, unsafe { NonNull::new_unchecked(sb.as_ref().s_fs_info) }),
        ))?)?,
        KernelInodeCollection::new(sb),
        // SAFETY: The super_block is initialized when the erofs_alloc_sbi_rust is called.
        unsafe { NonNull::new_unchecked(sb.as_ref().s_fs_info) },
    ))?;
    Ok(sbi.into_foreign())
}
/// Allocating a rust implementation of super_block_info c_void when calling from fill_super
/// operations. Though we still need to embed original superblock info inside rust implementation
/// for compatibility. This is left as it is for now.
#[no_mangle]
pub unsafe extern "C" fn erofs_alloc_sbi_rust(sb: NonNull<super_block>) -> *const c_void {
    try_alloc_sbi(sb).unwrap_or_else(|err| err.into())
}

/// Freeing a rust implementation of super_block_info c_void when calling from kill_super
/// Returning the original c_void pointer for outer C code to free.
#[no_mangle]
pub unsafe extern "C" fn erofs_free_sbi_rust(sb: NonNull<super_block>) -> *const c_void {
    let opaque: *const c_void = erofs_sbi(sb).opaque.as_ptr().cast();
    // This will be freed as it goes out of the scope.
    free_sbi(sb);
    opaque
}

/// Used as a hint offset to be exported so that EROFS_SB can find the correct the s_fs_info.
#[no_mangle]
pub static EROFS_SB_INFO_OFFSET_RUST: c_ulong = offset_of!(KernelSuperblockInfo, opaque) as c_ulong;
