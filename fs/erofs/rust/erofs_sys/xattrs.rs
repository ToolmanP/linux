// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use alloc::vec::Vec;

/// The header of the xattr entry index.
/// This is used to describe the superblock's xattrs collection.
#[derive(Clone, Copy)]
#[repr(C)]
pub(crate) struct XAttrSharedEntrySummary {
    pub(crate) name_filter: u32,
    pub(crate) shared_count: u8,
    pub(crate) reserved: [u8; 7],
}

impl From<[u8; 12]> for XAttrSharedEntrySummary {
    fn from(value: [u8; 12]) -> Self {
        Self {
            name_filter: u32::from_le_bytes([value[0], value[1], value[2], value[3]]),
            shared_count: value[4],
            reserved: value[5..12].try_into().unwrap(),
        }
    }
}

pub(crate) const XATTR_ENTRY_SUMMARY_BUF: [u8; 12] = [0u8; 12];

/// Represented as a inmemory memory entry index header used by SuperBlockInfo.
pub(crate) struct XAttrSharedEntries {
    pub(crate) name_filter: u32,
    pub(crate) shared_indexes: Vec<u32>,
}

/// Represents the name index for infixes or prefixes.
#[repr(C)]
#[derive(Clone, Copy)]
pub(crate) struct XattrNameIndex(u8);

impl core::cmp::PartialEq<u8> for XattrNameIndex {
    fn eq(&self, other: &u8) -> bool {
        if self.0 & EROFS_XATTR_LONG_PREFIX != 0 {
            self.0 & EROFS_XATTR_LONG_MASK == *other
        } else {
            self.0 == *other
        }
    }
}

impl XattrNameIndex {
    pub(crate) fn is_long(&self) -> bool {
        self.0 & EROFS_XATTR_LONG_PREFIX != 0
    }
}

impl From<u8> for XattrNameIndex {
    fn from(value: u8) -> Self {
        Self(value)
    }
}

#[allow(clippy::from_over_into)]
impl Into<usize> for XattrNameIndex {
    fn into(self) -> usize {
        if self.0 & EROFS_XATTR_LONG_PREFIX != 0 {
            (self.0 & EROFS_XATTR_LONG_MASK) as usize
        } else {
            self.0 as usize
        }
    }
}

/// This is on-disk representation of xattrs entry header.
/// This is used to describe one extended attribute.
#[repr(C)]
#[derive(Clone, Copy)]
pub(crate) struct XAttrEntryHeader {
    pub(crate) suffix_len: u8,
    pub(crate) name_index: XattrNameIndex,
    pub(crate) value_len: u16,
}

impl From<[u8; 4]> for XAttrEntryHeader {
    fn from(value: [u8; 4]) -> Self {
        Self {
            suffix_len: value[0],
            name_index: value[1].into(),
            value_len: u16::from_le_bytes(value[2..4].try_into().unwrap()),
        }
    }
}

/// Xattr Common Infix holds the prefix index in the first byte and all the common infix data in
/// the rest of the bytes.
pub(crate) struct XAttrInfix(pub(crate) Vec<u8>);

impl XAttrInfix {
    fn prefix_index(&self) -> u8 {
        self.0[0]
    }
    fn name(&self) -> &[u8] {
        &self.0[1..]
    }
}

pub(crate) const EROFS_XATTR_LONG_PREFIX: u8 = 0x80;
pub(crate) const EROFS_XATTR_LONG_MASK: u8 = EROFS_XATTR_LONG_PREFIX - 1;

/// Supported xattr prefixes
pub(crate) const EROFS_XATTRS_PREFIXS: [&[u8]; 7] = [
    b"",
    b"user.",
    b"system.posix_acl_access",
    b"system.posix_acl_default",
    b"trusted.",
    b"",
    b"security.",
];

/// Represents the value of an xattr entry or the size of it if the buffer is present in the query.
#[derive(Debug)]
pub(crate) enum XAttrValue {
    Buffer(usize),
    Vec(Vec<u8>),
}
