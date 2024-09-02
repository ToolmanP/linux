// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use super::*;
use core::mem::size_of;

/// The ondisk superblock structure.
#[derive(Debug, Clone, Copy, Default)]
#[repr(C)]
pub(crate) struct SuperBlock {
    pub(crate) magic: u32,
    pub(crate) checksum: i32,
    pub(crate) feature_compat: i32,
    pub(crate) blkszbits: u8,
    pub(crate) sb_extslots: u8,
    pub(crate) root_nid: i16,
    pub(crate) inos: i64,
    pub(crate) build_time: i64,
    pub(crate) build_time_nsec: i32,
    pub(crate) blocks: i32,
    pub(crate) meta_blkaddr: u32,
    pub(crate) xattr_blkaddr: u32,
    pub(crate) uuid: [u8; 16],
    pub(crate) volume_name: [u8; 16],
    pub(crate) feature_incompat: i32,
    pub(crate) compression: i16,
    pub(crate) extra_devices: i16,
    pub(crate) devt_slotoff: i16,
    pub(crate) dirblkbits: u8,
    pub(crate) xattr_prefix_count: u8,
    pub(crate) xattr_prefix_start: i32,
    pub(crate) packed_nid: i64,
    pub(crate) xattr_filter_reserved: u8,
    pub(crate) reserved: [u8; 23],
}

impl TryFrom<&[u8]> for SuperBlock {
    type Error = core::array::TryFromSliceError;
    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        value[0..128].try_into()
    }
}

impl From<[u8; 128]> for SuperBlock {
    fn from(value: [u8; 128]) -> Self {
        Self {
            magic: u32::from_le_bytes([value[0], value[1], value[2], value[3]]),
            checksum: i32::from_le_bytes([value[4], value[5], value[6], value[7]]),
            feature_compat: i32::from_le_bytes([value[8], value[9], value[10], value[11]]),
            blkszbits: value[12],
            sb_extslots: value[13],
            root_nid: i16::from_le_bytes([value[14], value[15]]),
            inos: i64::from_le_bytes([
                value[16], value[17], value[18], value[19], value[20], value[21], value[22],
                value[23],
            ]),
            build_time: i64::from_le_bytes([
                value[24], value[25], value[26], value[27], value[28], value[29], value[30],
                value[31],
            ]),
            build_time_nsec: i32::from_le_bytes([value[32], value[33], value[34], value[35]]),
            blocks: i32::from_le_bytes([value[36], value[37], value[38], value[39]]),
            meta_blkaddr: u32::from_le_bytes([value[40], value[41], value[42], value[43]]),
            xattr_blkaddr: u32::from_le_bytes([value[44], value[45], value[46], value[47]]),
            uuid: value[48..64].try_into().unwrap(),
            volume_name: value[64..80].try_into().unwrap(),
            feature_incompat: i32::from_le_bytes([value[80], value[81], value[82], value[83]]),
            compression: i16::from_le_bytes([value[84], value[85]]),
            extra_devices: i16::from_le_bytes([value[86], value[87]]),
            devt_slotoff: i16::from_le_bytes([value[88], value[89]]),
            dirblkbits: value[90],
            xattr_prefix_count: value[91],
            xattr_prefix_start: i32::from_le_bytes([value[92], value[93], value[94], value[95]]),
            packed_nid: i64::from_le_bytes([
                value[96], value[97], value[98], value[99], value[100], value[101], value[102],
                value[103],
            ]),
            xattr_filter_reserved: value[104],
            reserved: value[105..128].try_into().unwrap(),
        }
    }
}

pub(crate) type SuperBlockBuf = [u8; size_of::<SuperBlock>()];
pub(crate) const SUPERBLOCK_EMPTY_BUF: SuperBlockBuf = [0; size_of::<SuperBlock>()];

/// Used for external address calculation.
pub(crate) struct Accessor {
    pub(crate) base: Off,
    pub(crate) off: Off,
    pub(crate) len: Off,
    pub(crate) nr: Off,
}

impl Accessor {
    pub(crate) fn new(address: Off, bits: Off) -> Self {
        let sz = 1 << bits;
        let mask = sz - 1;
        Accessor {
            base: (address >> bits) << bits,
            off: address & mask,
            len: sz - (address & mask),
            nr: address >> bits,
        }
    }
}

impl SuperBlock {
    pub(crate) fn blk_access(&self, address: Off) -> Accessor {
        Accessor::new(address, self.blkszbits as Off)
    }

    pub(crate) fn blknr(&self, pos: Off) -> Blk {
        (pos >> self.blkszbits) as Blk
    }

    pub(crate) fn blkpos(&self, blk: Blk) -> Off {
        (blk as Off) << self.blkszbits
    }

    pub(crate) fn blksz(&self) -> Off {
        1 << self.blkszbits
    }

    pub(crate) fn blk_round_up(&self, addr: Off) -> Blk {
        ((addr + self.blksz() - 1) >> self.blkszbits) as Blk
    }

    pub(crate) fn iloc(&self, nid: Nid) -> Off {
        self.blkpos(self.meta_blkaddr) + ((nid as Off) << (5 as Off))
    }
}
