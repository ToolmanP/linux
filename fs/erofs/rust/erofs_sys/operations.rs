// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use super::alloc_helper::*;
use super::data::raw_iters::*;
use super::data::*;
use super::inode::*;
use super::superblock::*;
use super::xattrs::*;
use super::*;
use alloc::vec::Vec;

use crate::round;

pub(crate) fn read_inode<'a, I, C>(
    filesystem: &'a dyn FileSystem<I>,
    collection: &'a mut C,
    nid: Nid,
) -> PosixResult<&'a mut I>
where
    I: Inode,
    C: InodeCollection<I = I>,
{
    collection.iget(nid, filesystem)
}

pub(crate) fn dir_lookup<'a, I, C>(
    filesystem: &'a dyn FileSystem<I>,
    collection: &'a mut C,
    inode: &I,
    name: &str,
) -> PosixResult<&'a mut I>
where
    I: Inode,
    C: InodeCollection<I = I>,
{
    filesystem
        .find_nid(inode, name)?
        .map_or(Err(Errno::ENOENT), |nid| {
            read_inode(filesystem, collection, nid)
        })
}

pub(crate) fn get_xattr_infixes<'a>(
    iter: &mut (dyn ContinuousBufferIter<'a> + 'a),
) -> PosixResult<Vec<XAttrInfix>> {
    let mut result: Vec<XAttrInfix> = Vec::new();
    for data in iter {
        let buffer = data?;
        let buf = buffer.content();
        let len = buf.len();
        let mut cur: usize = 0;
        while cur <= len {
            let mut infix: Vec<u8> = Vec::new();
            let size = u16::from_le_bytes([buf[cur], buf[cur + 1]]) as usize;
            extend_from_slice(&mut infix, &buf[cur + 2..cur + 2 + size])?;
            push_vec(&mut result, XAttrInfix(infix))?;
            cur = round!(UP, cur + 2 + size, 4);
        }
    }
    Ok(result)
}
