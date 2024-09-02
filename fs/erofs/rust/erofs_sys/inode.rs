use super::xattrs::*;
use super::*;
use core::ffi::*;
use core::mem::size_of;

/// Represents the compact bitfield of the Erofs Inode format.
#[repr(transparent)]
#[derive(Clone, Copy)]
pub(crate) struct Format(u16);

pub(crate) const INODE_VERSION_MASK: u16 = 0x1;
pub(crate) const INODE_VERSION_BIT: u16 = 0;

pub(crate) const INODE_LAYOUT_BIT: u16 = 1;
pub(crate) const INODE_LAYOUT_MASK: u16 = 0x7;

/// Helper macro to extract property from the bitfield.
macro_rules! extract {
    ($name: expr, $bit: expr, $mask: expr) => {
        ($name >> $bit) & ($mask)
    };
}

/// The Version of the Inode which represents whether this inode is extended or compact.
/// Extended inodes have more infos about nlinks + mtime.
/// This is documented in https://erofs.docs.kernel.org/en/latest/core_ondisk.html#inodes
#[repr(C)]
#[derive(Clone, Copy)]
pub(crate) enum Version {
    Compat,
    Extended,
    Unknown,
}

/// Represents the data layout backed by the Inode.
/// As Documented in https://erofs.docs.kernel.org/en/latest/core_ondisk.html#inode-data-layouts
#[repr(C)]
#[derive(Clone, Copy, PartialEq)]
pub(crate) enum Layout {
    FlatPlain,
    CompressedFull,
    FlatInline,
    CompressedCompact,
    Chunk,
    Unknown,
}

#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub(crate) enum Type {
    Regular,
    Directory,
    Link,
    Character,
    Block,
    Fifo,
    Socket,
    Unknown,
}

/// This is format extracted from i_format bit representation.
/// This includes various infos and specs about the inode.
impl Format {
    pub(crate) fn version(&self) -> Version {
        match extract!(self.0, INODE_VERSION_BIT, INODE_VERSION_MASK) {
            0 => Version::Compat,
            1 => Version::Extended,
            _ => Version::Unknown,
        }
    }

    pub(crate) fn layout(&self) -> Layout {
        match extract!(self.0, INODE_LAYOUT_BIT, INODE_LAYOUT_MASK) {
            0 => Layout::FlatPlain,
            1 => Layout::CompressedFull,
            2 => Layout::FlatInline,
            3 => Layout::CompressedCompact,
            4 => Layout::Chunk,
            _ => Layout::Unknown,
        }
    }
}

/// Represents the compact inode which resides on-disk.
/// This is documented in https://erofs.docs.kernel.org/en/latest/core_ondisk.html#inodes
#[repr(C)]
#[derive(Clone, Copy)]
pub(crate) struct CompactInodeInfo {
    pub(crate) i_format: Format,
    pub(crate) i_xattr_icount: u16,
    pub(crate) i_mode: u16,
    pub(crate) i_nlink: u16,
    pub(crate) i_size: u32,
    pub(crate) i_reserved: [u8; 4],
    pub(crate) i_u: [u8; 4],
    pub(crate) i_ino: u32,
    pub(crate) i_uid: u16,
    pub(crate) i_gid: u16,
    pub(crate) i_reserved2: [u8; 4],
}

/// Represents the extended inode which resides on-disk.
/// This is documented in https://erofs.docs.kernel.org/en/latest/core_ondisk.html#inodes
#[repr(C)]
#[derive(Clone, Copy)]
pub(crate) struct ExtendedInodeInfo {
    pub(crate) i_format: Format,
    pub(crate) i_xattr_icount: u16,
    pub(crate) i_mode: u16,
    pub(crate) i_reserved: [u8; 2],
    pub(crate) i_size: u64,
    pub(crate) i_u: [u8; 4],
    pub(crate) i_ino: u32,
    pub(crate) i_uid: u32,
    pub(crate) i_gid: u32,
    pub(crate) i_mtime: u64,
    pub(crate) i_mtime_nsec: u32,
    pub(crate) i_nlink: u32,
    pub(crate) i_reserved2: [u8; 16],
}

/// Represents the inode info which is either compact or extended.
#[derive(Clone, Copy)]
pub(crate) enum InodeInfo {
    Extended(ExtendedInodeInfo),
    Compact(CompactInodeInfo),
}

pub(crate) const CHUNK_BLKBITS_MASK: u16 = 0x1f;
pub(crate) const CHUNK_FORMAT_INDEX_BIT: u16 = 0x20;

/// Represents on-disk chunk index of the file backing inode.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub(crate) struct ChunkIndex {
    pub(crate) advise: u16,
    pub(crate) device_id: u16,
    pub(crate) blkaddr: u32,
}

impl From<[u8; 8]> for ChunkIndex {
    fn from(u: [u8; 8]) -> Self {
        let advise = u16::from_le_bytes([u[0], u[1]]);
        let device_id = u16::from_le_bytes([u[2], u[3]]);
        let blkaddr = u32::from_le_bytes([u[4], u[5], u[6], u[7]]);
        ChunkIndex {
            advise,
            device_id,
            blkaddr,
        }
    }
}

/// Chunk format used for indicating the chunkbits and chunkindex.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub(crate) struct ChunkFormat(pub(crate) u16);

impl ChunkFormat {
    pub(crate) fn is_chunkindex(&self) -> bool {
        self.0 & CHUNK_FORMAT_INDEX_BIT != 0
    }
    pub(crate) fn chunkbits(&self) -> u16 {
        self.0 & CHUNK_BLKBITS_MASK
    }
}

/// Represents the inode spec which is either data or device.
#[derive(Clone, Copy, Debug)]
#[repr(u32)]
pub(crate) enum Spec {
    Chunk(ChunkFormat),
    RawBlk(u32),
    Device(u32),
    CompressedBlocks(u32),
    Unknown,
}

/// Convert the spec from the format of the inode based on the layout.
impl From<(&[u8; 4], Layout)> for Spec {
    fn from(value: (&[u8; 4], Layout)) -> Self {
        match value.1 {
            Layout::FlatInline | Layout::FlatPlain => Spec::RawBlk(u32::from_le_bytes(*value.0)),
            Layout::CompressedFull | Layout::CompressedCompact => {
                Spec::CompressedBlocks(u32::from_le_bytes(*value.0))
            }
            Layout::Chunk => Self::Chunk(ChunkFormat(u16::from_le_bytes([value.0[0], value.0[1]]))),
            // We don't support compressed inlines or compressed chunks currently.
            _ => Spec::Unknown,
        }
    }
}

/// Helper functions for Inode Info.
impl InodeInfo {
    const S_IFMT: u16 = 0o170000;
    const S_IFSOCK: u16 = 0o140000;
    const S_IFLNK: u16 = 0o120000;
    const S_IFREG: u16 = 0o100000;
    const S_IFBLK: u16 = 0o60000;
    const S_IFDIR: u16 = 0o40000;
    const S_IFCHR: u16 = 0o20000;
    const S_IFIFO: u16 = 0o10000;
    const S_ISUID: u16 = 0o4000;
    const S_ISGID: u16 = 0o2000;
    const S_ISVTX: u16 = 0o1000;
    pub(crate) fn ino(&self) -> u32 {
        match self {
            Self::Extended(extended) => extended.i_ino,
            Self::Compact(compact) => compact.i_ino,
        }
    }

    pub(crate) fn format(&self) -> Format {
        match self {
            Self::Extended(extended) => extended.i_format,
            Self::Compact(compact) => compact.i_format,
        }
    }

    pub(crate) fn file_size(&self) -> Off {
        match self {
            Self::Extended(extended) => extended.i_size,
            Self::Compact(compact) => compact.i_size as u64,
        }
    }

    pub(crate) fn inode_size(&self) -> Off {
        match self {
            Self::Extended(_) => 64,
            Self::Compact(_) => 32,
        }
    }

    pub(crate) fn spec(&self) -> Spec {
        let mode = match self {
            Self::Extended(extended) => extended.i_mode,
            Self::Compact(compact) => compact.i_mode,
        };

        let u = match self {
            Self::Extended(extended) => &extended.i_u,
            Self::Compact(compact) => &compact.i_u,
        };

        match mode & 0o170000 {
            0o40000 | 0o100000 | 0o120000 => Spec::from((u, self.format().layout())),
            // We don't support device inodes currently.
            _ => Spec::Unknown,
        }
    }

    pub(crate) fn inode_type(&self) -> Type {
        let mode = match self {
            Self::Extended(extended) => extended.i_mode,
            Self::Compact(compact) => compact.i_mode,
        };
        match mode & Self::S_IFMT {
            Self::S_IFDIR => Type::Directory, // Directory
            Self::S_IFREG => Type::Regular,   // Regular File
            Self::S_IFLNK => Type::Link,      // Symbolic Link
            Self::S_IFIFO => Type::Fifo,      // FIFO
            Self::S_IFSOCK => Type::Socket,   // Socket
            Self::S_IFBLK => Type::Block,     // Block
            Self::S_IFCHR => Type::Character, // Character
            _ => Type::Unknown,
        }
    }

    pub(crate) fn xattr_size(&self) -> Off {
        match self {
            Self::Extended(extended) => {
                if extended.i_xattr_icount == 0 {
                    0
                } else {
                    size_of::<XAttrSharedEntrySummary>() as Off
                        + (size_of::<c_int>() as Off) * (extended.i_xattr_icount as Off - 1)
                }
            }
            Self::Compact(_) => 0,
        }
    }

    pub(crate) fn xattr_count(&self) -> u16 {
        match self {
            Self::Extended(extended) => extended.i_xattr_icount,
            Self::Compact(compact) => compact.i_xattr_icount,
        }
    }
}

pub(crate) type CompactInodeInfoBuf = [u8; size_of::<CompactInodeInfo>()];
pub(crate) type ExtendedInodeInfoBuf = [u8; size_of::<ExtendedInodeInfo>()];
pub(crate) const DEFAULT_INODE_BUF: ExtendedInodeInfoBuf = [0; size_of::<ExtendedInodeInfo>()];
