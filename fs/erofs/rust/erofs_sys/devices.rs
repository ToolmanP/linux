// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use alloc::vec::Vec;

/// Device specification.
#[derive(Copy, Clone, Debug)]
pub(crate) struct DeviceSpec {
    pub(crate) tags: [u8; 64],
    pub(crate) blocks: u32,
    pub(crate) mapped_blocks: u32,
}

/// Device slot.
#[derive(Copy, Clone, Debug)]
#[repr(C)]
pub(crate) struct DeviceSlot {
    tags: [u8; 64],
    blocks: u32,
    mapped_blocks: u32,
    reserved: [u8; 56],
}

/// Device information.
pub(crate) struct DeviceInfo {
    pub(crate) mask: u16,
    pub(crate) specs: Vec<DeviceSpec>,
}
