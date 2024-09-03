// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use core::ffi::*;
use core::mem::MaybeUninit;
use core::ptr::NonNull;

use kernel::bindings::{inode, super_block};
use kernel::container_of;

use super::erofs_sys::errnos::*;
use super::erofs_sys::inode::*;
use super::erofs_sys::superblock::*;
use super::erofs_sys::*;

extern "C" {
    #[link_name = "erofs_iget_locked_rust_helper"]
    fn iget_locked(sb: NonNull<c_void>, nid: Nid) -> *mut c_void;
}

#[repr(C)]
pub(crate) struct KernelInode {
    pub(crate) info: MaybeUninit<InodeInfo>,
    pub(crate) nid: MaybeUninit<Nid>,
    pub(crate) k_inode: MaybeUninit<inode>,
    pub(crate) k_opaque: MaybeUninit<*mut c_void>,
}

impl Inode for KernelInode {
    fn new(_sb: &SuperBlock, _info: InodeInfo, _nid: Nid) -> Self {
        Self {
            info: MaybeUninit::uninit(),
            nid: MaybeUninit::uninit(),
            k_inode: MaybeUninit::uninit(),
            k_opaque: MaybeUninit::uninit(),
        }
    }
    fn nid(&self) -> Nid {
        unsafe { self.nid.assume_init() }
    }
    fn info(&self) -> &InodeInfo {
        unsafe { self.info.assume_init_ref() }
    }
}

pub(crate) struct KernelInodeCollection {
    sb: NonNull<super_block>,
}

impl InodeCollection for KernelInodeCollection {
    type I = KernelInode;
    fn iget(&mut self, nid: Nid, _f: &dyn FileSystem<Self::I>) -> PosixResult<&mut Self::I> {
        // SAFETY: iget_locked is safe to call here.
        let k_inode = unsafe { iget_locked(self.sb.cast(), nid) };
        if is_value_err(k_inode.cast()) {
            return Err(Errno::from(k_inode as i32));
        } else {
            let erofs_inode: &mut KernelInode =
                // SAFETY: iget_locked returns a valid pointer to a vfs inode and it's embedded in a KernelInode.
                unsafe { &mut *(container_of!(k_inode, KernelInode, k_inode) as *mut KernelInode) };
            return Ok(erofs_inode);
        }
    }
}

impl KernelInodeCollection {
    pub(crate) fn new(sb: NonNull<super_block>) -> Self {
        Self { sb }
    }
}
