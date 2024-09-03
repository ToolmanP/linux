// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use core::ffi::*;
use core::ptr::NonNull;

use super::erofs_sys::data::*;
use super::erofs_sys::errnos::*;
use super::erofs_sys::*;

use kernel::bindings::super_block;

extern "C" {
    #[link_name = "erofs_read_metabuf_rust_helper"]
    pub(crate) fn read_metabuf(
        sb: NonNull<c_void>,
        sbi: NonNull<c_void>,
        offset: c_ulonglong,
    ) -> *mut c_void;
    #[link_name = "erofs_put_metabuf_rust_helper"]
    pub(crate) fn put_metabuf(addr: NonNull<c_void>);
}

fn try_read_metabuf(
    sb: NonNull<super_block>,
    sbi: NonNull<c_void>,
    offset: c_ulonglong,
) -> PosixResult<NonNull<c_void>> {
    let ptr = unsafe { read_metabuf(sb.cast(), sbi.cast(), offset) };
    if ptr.is_null() {
        Err(Errno::ENOMEM)
    } else if is_value_err(ptr) {
        Err(Errno::from(ptr))
    } else {
        Ok(unsafe { NonNull::new_unchecked(ptr) })
    }
}

pub(crate) struct MetabufSource {
    sb: NonNull<super_block>,
    opaque: NonNull<c_void>,
}

impl MetabufSource {
    pub(crate) fn new(sb: NonNull<super_block>, opaque: NonNull<c_void>) -> Self {
        Self { sb, opaque }
    }
}

impl Source for MetabufSource {
    fn fill(&self, data: &mut [u8], offset: Off) -> PosixResult<u64> {
        self.as_buf(offset, data.len() as u64).map(|buf| {
            data[..buf.content().len()].clone_from_slice(buf.content());
            buf.content().len() as Off
        })
    }
    fn as_buf<'a>(&'a self, offset: Off, len: Off) -> PosixResult<RefBuffer<'a>> {
        try_read_metabuf(self.sb.clone(), self.opaque.clone(), offset).map(|ptr| {
            let data: &'a [u8] =
                unsafe { core::slice::from_raw_parts(ptr.as_ptr() as *const u8, len as usize) };
            RefBuffer::new(data, 0, len as usize, |ptr| unsafe {
                put_metabuf(NonNull::new_unchecked(ptr as *mut c_void))
            })
        })
    }
}
