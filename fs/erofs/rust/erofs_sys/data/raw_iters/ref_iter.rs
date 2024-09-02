// Copyright 2024 Yiyang Wu
// SPDX-License-Identifier: MIT or GPL-2.0-or-later

use super::super::*;
use super::*;

/// Continous Ref Buffer Iterator which iterates over a range of disk addresses within the
/// the temp block size. Since the temp block is always the same size as page and it will not
/// overflow.
pub(crate) struct ContinuousRefIter<'a, B>
where
    B: Backend,
{
    sb: &'a SuperBlock,
    backend: &'a B,
    offset: Off,
    len: Off,
}

impl<'a, B> ContinuousRefIter<'a, B>
where
    B: Backend,
{
    pub(crate) fn new(sb: &'a SuperBlock, backend: &'a B, offset: Off, len: Off) -> Self {
        Self {
            sb,
            backend,
            offset,
            len,
        }
    }
}

impl<'a, B> Iterator for ContinuousRefIter<'a, B>
where
    B: Backend,
{
    type Item = PosixResult<RefBuffer<'a>>;
    fn next(&mut self) -> Option<Self::Item> {
        if self.len == 0 {
            return None;
        }
        let accessor = self.sb.blk_access(self.offset);
        let len = accessor.len.min(self.len);
        let result: Option<Self::Item> = self.backend.as_buf(self.offset, len).map_or_else(
            |e| Some(Err(e)),
            |buf| {
                self.offset += len;
                self.len -= len;
                Some(Ok(buf))
            },
        );
        result
    }
}

impl<'a, B> ContinuousBufferIter<'a> for ContinuousRefIter<'a, B>
where
    B: Backend,
{
    fn advance_off(&mut self, offset: Off) {
        self.offset += offset;
        self.len -= offset
    }
    fn eof(&self) -> bool {
        self.len == 0
    }
}
