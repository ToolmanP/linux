// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

pub(crate) mod mem;
use alloc::boxed::Box;
use alloc::vec::Vec;
use core::mem::size_of;

use super::alloc_helper::*;
use super::data::raw_iters::*;
use super::data::*;
use super::devices::*;
use super::dir::*;
use super::inode::*;
use super::map::*;
use super::xattrs::*;
use super::*;

use crate::round;

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
    pub(crate) fn chunk_access(&self, format: ChunkFormat, address: Off) -> Accessor {
        let chunkbits = format.chunkbits() + self.blkszbits as u16;
        Accessor::new(address, chunkbits as Off)
    }
}

pub(crate) trait FileSystem<I>
where
    I: Inode,
{
    fn superblock(&self) -> &SuperBlock;
    fn backend(&self) -> &dyn Backend;
    fn as_filesystem(&self) -> &dyn FileSystem<I>;
    fn device_info(&self) -> &DeviceInfo;
    fn flatmap(&self, inode: &I, offset: Off, inline: bool) -> MapResult {
        let sb = self.superblock();
        let nblocks = sb.blk_round_up(inode.info().file_size());
        let blkaddr = match inode.info().spec() {
            Spec::RawBlk(blkaddr) => Ok(blkaddr),
            _ => Err(EUCLEAN),
        }?;

        let lastblk = if inline { nblocks - 1 } else { nblocks };
        if offset < sb.blkpos(lastblk) {
            let len = inode.info().file_size().min(sb.blkpos(lastblk)) - offset;
            Ok(Map {
                logical: Segment { start: offset, len },
                physical: Segment {
                    start: sb.blkpos(blkaddr) + offset,
                    len,
                },
                algorithm_format: 0,
                device_id: 0,
                map_type: MapType::Normal,
            })
        } else if inline {
            let len = inode.info().file_size() - offset;
            let accessor = sb.blk_access(offset);
            Ok(Map {
                logical: Segment { start: offset, len },
                physical: Segment {
                    start: sb.iloc(inode.nid())
                        + inode.info().inode_size()
                        + inode.info().xattr_size()
                        + accessor.off,
                    len,
                },
                algorithm_format: 0,
                device_id: 0,
                map_type: MapType::Meta,
            })
        } else {
            Err(EUCLEAN)
        }
    }

    fn chunk_map(&self, inode: &I, offset: Off) -> MapResult {
        let sb = self.superblock();
        let chunkformat = match inode.info().spec() {
            Spec::Chunk(chunkformat) => Ok(chunkformat),
            _ => Err(EUCLEAN),
        }?;
        let accessor = sb.chunk_access(chunkformat, offset);

        if chunkformat.is_chunkindex() {
            let unit = size_of::<ChunkIndex>() as Off;
            let pos = round!(
                UP,
                self.superblock().iloc(inode.nid())
                    + inode.info().inode_size()
                    + inode.info().xattr_size()
                    + unit * accessor.nr,
                unit
            );
            let mut buf = [0u8; size_of::<ChunkIndex>()];
            self.backend().fill(&mut buf, pos)?;
            let chunk_index = ChunkIndex::from(buf);
            if chunk_index.blkaddr == u32::MAX {
                Err(EUCLEAN)
            } else {
                Ok(Map {
                    logical: Segment {
                        start: accessor.base + accessor.off,
                        len: accessor.len,
                    },
                    physical: Segment {
                        start: sb.blkpos(chunk_index.blkaddr) + accessor.off,
                        len: accessor.len,
                    },
                    algorithm_format: 0,
                    device_id: chunk_index.device_id & self.device_info().mask,
                    map_type: MapType::Normal,
                })
            }
        } else {
            let unit = 4;
            let pos = round!(
                UP,
                sb.iloc(inode.nid())
                    + inode.info().inode_size()
                    + inode.info().xattr_size()
                    + unit * accessor.nr,
                unit
            );
            let mut buf = [0u8; 4];
            self.backend().fill(&mut buf, pos)?;
            let blkaddr = u32::from_le_bytes(buf);
            let len = accessor.len.min(inode.info().file_size() - offset);
            if blkaddr == u32::MAX {
                Err(EUCLEAN)
            } else {
                Ok(Map {
                    logical: Segment {
                        start: accessor.base + accessor.off,
                        len,
                    },
                    physical: Segment {
                        start: sb.blkpos(blkaddr) + accessor.off,
                        len,
                    },
                    algorithm_format: 0,
                    device_id: 0,
                    map_type: MapType::Normal,
                })
            }
        }
    }

    fn map(&self, inode: &I, offset: Off) -> MapResult {
        match inode.info().format().layout() {
            Layout::FlatInline => self.flatmap(inode, offset, true),
            Layout::FlatPlain => self.flatmap(inode, offset, false),
            Layout::Chunk => self.chunk_map(inode, offset),
            _ => todo!(),
        }
    }

    fn mapped_iter<'b, 'a: 'b>(
        &'a self,
        inode: &'b I,
        offset: Off,
    ) -> PosixResult<Box<dyn BufferMapIter<'a> + 'b>>;

    fn continuous_iter<'a>(
        &'a self,
        offset: Off,
        len: Off,
    ) -> PosixResult<Box<dyn ContinuousBufferIter<'a> + 'a>>;

    // Inode related goes here.
    fn read_inode_info(&self, nid: Nid) -> PosixResult<InodeInfo> {
        (self.as_filesystem(), nid).try_into()
    }

    fn find_nid(&self, inode: &I, name: &str) -> PosixResult<Option<Nid>> {
        for buf in self.mapped_iter(inode, 0)? {
            for dirent in buf?.iter_dir() {
                if dirent.dirname() == name.as_bytes() {
                    return Ok(Some(dirent.desc.nid));
                }
            }
        }
        Ok(None)
    }

    // Readdir related goes here.
    fn fill_dentries(
        &self,
        inode: &I,
        offset: Off,
        emitter: &mut dyn FnMut(Dirent<'_>, Off),
    ) -> PosixResult<()> {
        let sb = self.superblock();
        let accessor = sb.blk_access(offset);
        if offset > inode.info().file_size() {
            return Err(EUCLEAN);
        }

        let map_offset = round!(DOWN, offset, sb.blksz());
        let blk_offset = round!(UP, accessor.off, size_of::<DirentDesc>() as Off);

        let mut map_iter = self.mapped_iter(inode, map_offset)?;
        let first_buf = map_iter.next().unwrap()?;
        let mut collection = first_buf.iter_dir();

        let mut pos: Off = map_offset + blk_offset;

        if blk_offset as usize / size_of::<DirentDesc>() <= collection.total() {
            collection.skip_dir(blk_offset as usize / size_of::<DirentDesc>());
            for dirent in collection {
                emitter(dirent, pos);
                pos += size_of::<DirentDesc>() as Off;
            }
        }

        pos = round!(UP, pos, sb.blksz());

        for buf in map_iter {
            for dirent in buf?.iter_dir() {
                emitter(dirent, pos);
                pos += size_of::<DirentDesc>() as Off;
            }
            pos = round!(UP, pos, sb.blksz());
        }
        Ok(())
    }
    // Extended attributes goes here.
    fn xattr_infixes(&self) -> &Vec<XAttrInfix>;
    // Currently we eagerly initialized all xattrs;
    fn read_inode_xattrs_shared_entries(
        &self,
        nid: Nid,
        info: &InodeInfo,
    ) -> PosixResult<XAttrSharedEntries> {
        let sb = self.superblock();
        let mut offset = sb.iloc(nid) + info.inode_size();
        let mut buf = XATTR_ENTRY_SUMMARY_BUF;
        let mut indexes: Vec<u32> = Vec::new();
        self.backend().fill(&mut buf, offset)?;

        let header: XAttrSharedEntrySummary = XAttrSharedEntrySummary::from(buf);
        offset += size_of::<XAttrSharedEntrySummary>() as Off;
        for buf in self.continuous_iter(offset, (header.shared_count << 2) as Off)? {
            let data = buf?;
            extend_from_slice(&mut indexes, unsafe {
                core::slice::from_raw_parts(
                    data.content().as_ptr().cast(),
                    data.content().len() >> 2,
                )
            })?;
        }

        Ok(XAttrSharedEntries {
            name_filter: header.name_filter,
            shared_indexes: indexes,
        })
    }
    /// get_xattr
    fn get_xattr(
        &self,
        inode: &I,
        index: u32,
        name: &[u8],
        buffer: &mut Option<&mut [u8]>,
    ) -> PosixResult<XAttrValue> {
        let sb = self.superblock();
        let shared_count = inode.xattrs_shared_entries().shared_indexes.len();
        let inline_offset = sb.iloc(inode.nid())
            + inode.info().inode_size() as Off
            + size_of::<XAttrSharedEntrySummary>() as Off
            + 4 * shared_count as Off;

        let inline_header_sz =
            size_of::<XAttrSharedEntrySummary>() as Off + shared_count as Off * 4;

        if inline_header_sz <= inode.info().xattr_size() {
            let inline_len = inode.info().xattr_size() - inline_header_sz;
            if let Some(mut inline_provider) =
                SkippableContinuousIter::try_new(self.continuous_iter(inline_offset, inline_len)?)?
            {
                while !inline_provider.eof() {
                    let header = inline_provider.get_entry_header()?;
                    match inline_provider.query_xattr_value(
                        self.xattr_infixes(),
                        &header,
                        name,
                        index,
                        buffer,
                    ) {
                        Ok(value) => return Ok(value),
                        Err(e) => {
                            if e != ENODATA {
                                return Err(e);
                            }
                        }
                    }
                }
            }
        }

        for entry_index in inode.xattrs_shared_entries().shared_indexes.iter() {
            let mut shared_provider = SkippableContinuousIter::try_new(self.continuous_iter(
                sb.blkpos(self.superblock().xattr_blkaddr) + (*entry_index as Off) * 4,
                u64::MAX,
            )?)?
            .unwrap();
            let header = shared_provider.get_entry_header()?;
            match shared_provider.query_xattr_value(
                self.xattr_infixes(),
                &header,
                name,
                index,
                buffer,
            ) {
                Ok(value) => return Ok(value),
                Err(e) => {
                    if e != ENODATA {
                        return Err(e);
                    }
                }
            }
        }

        Err(ENODATA)
    }
    /// list_xattrs
    fn list_xattrs(&self, inode: &I, buffer: &mut [u8]) -> PosixResult<usize> {
        let sb = self.superblock();
        let shared_count = inode.xattrs_shared_entries().shared_indexes.len();
        let inline_offset = sb.iloc(inode.nid())
            + inode.info().inode_size() as Off
            + size_of::<XAttrSharedEntrySummary>() as Off
            + shared_count as Off * 4;
        let inline_header_sz =
            size_of::<XAttrSharedEntrySummary>() as Off + shared_count as Off * 4;
        let mut offset = 0;

        if inline_header_sz <= inode.info().xattr_size() {
            let inline_len = inode.info().xattr_size() - inline_header_sz;
            if let Some(mut inline_provider) =
                SkippableContinuousIter::try_new(self.continuous_iter(inline_offset, inline_len)?)?
            {
                while !inline_provider.eof() {
                    let header = inline_provider.get_entry_header()?;
                    offset += inline_provider.get_xattr_key(
                        self.xattr_infixes(),
                        &header,
                        &mut buffer[offset..],
                    )?;
                    inline_provider.skip_xattr_value(&header)?;
                }
            }
        }

        for index in inode.xattrs_shared_entries().shared_indexes.iter() {
            let mut shared_provider = SkippableContinuousIter::try_new(self.continuous_iter(
                sb.blkpos(self.superblock().xattr_blkaddr) + (*index as Off) * 4,
                u64::MAX,
            )?)?
            .unwrap();
            let header = shared_provider.get_entry_header()?;
            offset += shared_provider.get_xattr_key(
                self.xattr_infixes(),
                &header,
                &mut buffer[offset..],
            )?;
        }
        Ok(offset)
    }
}

pub(crate) struct SuperblockInfo<I, C, T>
where
    I: Inode,
    C: InodeCollection<I = I>,
{
    pub(crate) filesystem: Box<dyn FileSystem<I>>,
    pub(crate) inodes: C,
    pub(crate) opaque: T,
}

impl<I, C, T> SuperblockInfo<I, C, T>
where
    I: Inode,
    C: InodeCollection<I = I>,
{
    pub(crate) fn new(fs: Box<dyn FileSystem<I>>, c: C, opaque: T) -> Self {
        Self {
            filesystem: fs,
            inodes: c,
            opaque,
        }
    }
}
