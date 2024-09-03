// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use core::ffi::c_void;
use core::mem::MaybeUninit;
use core::ptr::NonNull;

use kernel::bindings::{inode, super_block};

use super::erofs_sys::inode::*;
use super::erofs_sys::superblock::*;
use super::erofs_sys::*;

#[repr(C)]
pub(crate) struct KernelInode {
    pub(crate) info: MaybeUninit<InodeInfo>,
    pub(crate) nid: MaybeUninit<Nid>,
    pub(crate) k_inode: MaybeUninit<inode>,
    pub(crate) k_opaque: MaybeUninit<*mut c_void>,
}

impl Inode for KernelInode {
    fn new(_sb: &SuperBlock, _info: InodeInfo, _nid: Nid) -> Self {
        unimplemented!();
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
    fn iget(&mut self, _nid: Nid, _f: &dyn FileSystem<Self::I>) -> PosixResult<&mut Self::I> {
        unimplemented!();
    }
}

impl KernelInodeCollection {
    pub(crate) fn new(sb: NonNull<super_block>) -> Self {
        Self { sb }
    }
}
