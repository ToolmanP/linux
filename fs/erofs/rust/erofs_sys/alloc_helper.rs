// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

/// This module provides helper functions for the alloc crate
/// Note that in linux kernel, the allocation is fallible however in userland it is not.
/// Since most of the functions depend on infallible allocation, here we provide helper functions
/// so that most of codes don't need to be changed.

#[cfg(CONFIG_EROFS_FS = "y")]
use kernel::prelude::*;

#[cfg(not(CONFIG_EROFS_FS = "y"))]
use alloc::vec;

use super::*;
use alloc::boxed::Box;
use alloc::vec::Vec;

pub(crate) fn push_vec<T>(v: &mut Vec<T>, value: T) -> PosixResult<()> {
    v.push(value, GFP_KERNEL)
        .map_or_else(|_| Err(Errno::ENOMEM), |_| Ok(()))
}

pub(crate) fn extend_from_slice<T: Clone>(v: &mut Vec<T>, slice: &[T]) -> PosixResult<()> {
    v.extend_from_slice(slice, GFP_KERNEL)
        .map_or_else(|_| Err(Errno::ENOMEM), |_| Ok(()))
}

pub(crate) fn heap_alloc<T>(value: T) -> PosixResult<Box<T>> {
    Box::new(value, GFP_KERNEL).map_or_else(|_| Err(Errno::ENOMEM), |v| Ok(v))
}

pub(crate) fn vec_with_capacity<T: Default + Clone>(capacity: usize) -> PosixResult<Vec<T>> {
    Vec::with_capacity(capacity, GFP_KERNEL).map_or_else(|_| Err(Errno::ENOMEM), |v| Ok(v))
}
