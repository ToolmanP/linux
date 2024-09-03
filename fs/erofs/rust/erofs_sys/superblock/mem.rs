// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use super::data::raw_iters::ref_iter::*;
use super::operations::*;
use super::*;

// Memory Mapped Device/File so we need to have some external lifetime on the backend trait.
// Note that we do not want the lifetime to infect the MemFileSystem which may have a impact on
// the content iter below. Just use HRTB to dodge the borrow checker.

pub(crate) struct KernelFileSystem<B>
where
    B: Backend,
{
    backend: B,
    sb: SuperBlock,
    device_info: DeviceInfo,
    infixes: Vec<XAttrInfix>,
}

impl<I, B> FileSystem<I> for KernelFileSystem<B>
where
    B: Backend,
    I: Inode,
{
    fn superblock(&self) -> &SuperBlock {
        &self.sb
    }
    fn backend(&self) -> &dyn Backend {
        &self.backend
    }

    fn as_filesystem(&self) -> &dyn FileSystem<I> {
        self
    }

    fn mapped_iter<'b, 'a: 'b>(
        &'a self,
        inode: &'b I,
        offset: Off,
    ) -> PosixResult<Box<dyn BufferMapIter<'a> + 'b>> {
        heap_alloc(RefMapIter::new(
            &self.sb,
            &self.backend,
            MapIter::new(self, inode, offset),
        ))
        .map(|v| v as Box<dyn BufferMapIter<'a> + 'b>)
    }
    fn continuous_iter<'a>(
        &'a self,
        offset: Off,
        len: Off,
    ) -> PosixResult<Box<dyn ContinuousBufferIter<'a> + 'a>> {
        heap_alloc(ContinuousRefIter::new(&self.sb, &self.backend, offset, len))
            .map(|v| v as Box<dyn ContinuousBufferIter<'a> + 'a>)
    }

    fn device_info(&self) -> &DeviceInfo {
        &self.device_info
    }
    fn xattr_infixes(&self) -> &Vec<XAttrInfix> {
        &self.infixes
    }
}

impl<B> KernelFileSystem<B>
where
    B: Backend,
{
    pub(crate) fn try_new(backend: B) -> PosixResult<Self> {
        let mut buf = SUPERBLOCK_EMPTY_BUF;
        backend.fill(&mut buf, EROFS_SUPER_OFFSET)?;
        let sb: SuperBlock = buf.into();
        let infixes = get_xattr_infixes(&mut ContinuousRefIter::new(
            &sb,
            &backend,
            sb.xattr_prefix_start as Off,
            sb.xattr_prefix_count as Off * 4,
        ))?;
        let device_info = get_device_infos(&mut ContinuousRefIter::new(
            &sb,
            &backend,
            sb.devt_slotoff as Off * 128,
            sb.extra_devices as Off * 128,
        ))?;
        Ok(Self {
            backend,
            sb,
            device_info,
            infixes,
        })
    }
}
