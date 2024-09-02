// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use super::data::raw_iters::ref_iter::*;
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

    fn device_info(&self) -> &DeviceInfo {
        &self.device_info
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
        })
    }
}
