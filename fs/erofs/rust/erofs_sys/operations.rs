// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use super::inode::*;
use super::superblock::*;
use super::*;

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
