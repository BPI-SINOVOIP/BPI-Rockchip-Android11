// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//! ```
//! use audio_streams::{StreamSource, DummyStreamSource};
//! use std::io::Read;
//!
//! const buffer_size: usize = 120;
//! const num_channels: usize = 2;
//! const frame_size: usize = num_channels * 2; // 16-bit samples are two bytes.
//!
//! # fn main() -> std::result::Result<(), Box<std::error::Error>> {
//! let mut stream_source = DummyStreamSource::new();
//!
//! let (_, mut stream) = stream_source
//!     .new_capture_stream(num_channels, 48000, buffer_size)?;
//! // Capture 10 buffers of zeros.
//! let mut cp_bufs = [[0xa5u8; buffer_size * frame_size]; 10];
//! for cp_buf in &mut cp_bufs {
//!     let mut stream_buffer = stream.next_capture_buffer()?;
//!     assert_eq!(stream_buffer.read(cp_buf)?, buffer_size * frame_size);
//! }
//! # Ok (())
//! # }
//! ```

use std::{
    error,
    fmt::{self, Display},
    io::{self, Read, Write},
    time::{Duration, Instant},
};

use super::{AudioBuffer, BufferDrop, DummyBufferDrop};

/// `CaptureBufferStream` provides `CaptureBuffer`s to read with audio samples from capture.
pub trait CaptureBufferStream: Send {
    fn next_capture_buffer<'a>(&'a mut self) -> Result<CaptureBuffer<'a>, Box<dyn error::Error>>;
}

/// `CaptureBuffer` contains a block of audio samples got from capture stream. It provides
/// temporary view to those samples and will notifies capture stream when dropped.
/// Note that it'll always send `buffer.len() / frame_size` to drop function when it got destroyed
/// since `CaptureBufferStream` assumes that users get all the samples from the buffer.
pub struct CaptureBuffer<'a> {
    buffer: AudioBuffer<'a>,
}

/// Errors that are possible from a `CaptureBuffer`.
#[derive(Debug)]
pub enum CaptureBufferError {
    InvalidLength,
}

impl error::Error for CaptureBufferError {}

impl Display for CaptureBufferError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            CaptureBufferError::InvalidLength => write!(f, "Invalid buffer length"),
        }
    }
}

impl<'a> CaptureBuffer<'a> {
    /// Creates a new `CaptureBuffer` that holds a reference to the backing memory specified in
    /// `buffer`.
    pub fn new<F>(
        frame_size: usize,
        buffer: &'a mut [u8],
        drop: &'a mut F,
    ) -> Result<Self, CaptureBufferError>
    where
        F: BufferDrop,
    {
        if buffer.len() % frame_size != 0 {
            return Err(CaptureBufferError::InvalidLength);
        }

        Ok(CaptureBuffer {
            buffer: AudioBuffer {
                buffer,
                frame_size,
                offset: 0,
                drop,
            },
        })
    }

    /// Returns the number of audio frames that fit in the buffer.
    pub fn frame_capacity(&self) -> usize {
        self.buffer.buffer.len() / self.buffer.frame_size
    }

    /// Reads up to `size` bytes directly from this buffer inside of the given callback function.
    pub fn copy_cb<F: FnOnce(&[u8])>(&mut self, size: usize, cb: F) {
        let len = size / self.buffer.frame_size * self.buffer.frame_size;
        cb(&self.buffer.buffer[self.buffer.offset..(self.buffer.offset + len)]);
        self.buffer.offset += len;
    }
}

impl<'a> Read for CaptureBuffer<'a> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let len = buf.len() / self.buffer.frame_size * self.buffer.frame_size;
        let written = (&mut buf[..len]).write(&self.buffer.buffer[self.buffer.offset..])?;
        self.buffer.offset += written;
        Ok(written)
    }
}

/// Always sends `frame_capacity` when it drops.
impl<'a> Drop for CaptureBuffer<'a> {
    fn drop(&mut self) {
        self.buffer.drop.trigger(self.frame_capacity());
    }
}

/// Stream that provides null capture samples.
pub struct DummyCaptureStream {
    buffer: Vec<u8>,
    frame_size: usize,
    interval: Duration,
    next_frame: Duration,
    start_time: Option<Instant>,
    buffer_drop: DummyBufferDrop,
}

impl DummyCaptureStream {
    // TODO(allow other formats)
    pub fn new(num_channels: usize, frame_rate: usize, buffer_size: usize) -> Self {
        const S16LE_SIZE: usize = 2;
        let frame_size = S16LE_SIZE * num_channels;
        let interval = Duration::from_millis(buffer_size as u64 * 1000 / frame_rate as u64);
        DummyCaptureStream {
            buffer: vec![0; buffer_size * frame_size],
            frame_size,
            interval,
            next_frame: interval,
            start_time: None,
            buffer_drop: DummyBufferDrop {
                which_buffer: false,
            },
        }
    }
}

impl CaptureBufferStream for DummyCaptureStream {
    fn next_capture_buffer(&mut self) -> Result<CaptureBuffer, Box<dyn error::Error>> {
        if let Some(start_time) = self.start_time {
            if start_time.elapsed() < self.next_frame {
                std::thread::sleep(self.next_frame - start_time.elapsed());
            }
            self.next_frame += self.interval;
        } else {
            self.start_time = Some(Instant::now());
            self.next_frame = self.interval;
        }
        Ok(CaptureBuffer::new(
            self.frame_size,
            &mut self.buffer,
            &mut self.buffer_drop,
        )?)
    }
}

#[cfg(test)]
mod tests {
    use super::super::*;
    use super::*;

    #[test]
    fn invalid_buffer_length() {
        // Capture buffers can't be created with a size that isn't divisible by the frame size.
        let mut cp_buf = [0xa5u8; 480 * 2 * 2 + 1];
        let mut buffer_drop = DummyBufferDrop {
            which_buffer: false,
        };
        assert!(CaptureBuffer::new(2, &mut cp_buf, &mut buffer_drop).is_err());
    }

    #[test]
    fn trigger() {
        struct TestDrop {
            frame_count: usize,
        };
        impl BufferDrop for TestDrop {
            fn trigger(&mut self, nwritten: usize) {
                self.frame_count += nwritten;
            }
        }
        let mut test_drop = TestDrop { frame_count: 0 };
        {
            const FRAME_SIZE: usize = 4;
            let mut buf = [0u8; 480 * FRAME_SIZE];
            let mut cp_buf = CaptureBuffer::new(FRAME_SIZE, &mut buf, &mut test_drop).unwrap();
            let mut local_buf = [0u8; 240 * FRAME_SIZE];
            assert_eq!(cp_buf.read(&mut local_buf).unwrap(), 240 * FRAME_SIZE);
        }
        // This should be 480 no matter how many samples are read.
        assert_eq!(test_drop.frame_count, 480);
    }

    #[test]
    fn sixteen_bit_stereo() {
        let mut server = DummyStreamSource::new();
        let (_, mut stream) = server.new_capture_stream(2, 48000, 480).unwrap();
        let mut stream_buffer = stream.next_capture_buffer().unwrap();
        assert_eq!(stream_buffer.frame_capacity(), 480);
        let mut pb_buf = [0xa5u8; 480 * 2 * 2];
        assert_eq!(stream_buffer.read(&mut pb_buf).unwrap(), 480 * 2 * 2);
    }

    #[test]
    fn consumption_rate() {
        let mut server = DummyStreamSource::new();
        let (_, mut stream) = server.new_capture_stream(2, 48000, 480).unwrap();
        let start = Instant::now();
        {
            let mut stream_buffer = stream.next_capture_buffer().unwrap();
            let mut cp_buf = [0xa5u8; 480 * 2 * 2];
            assert_eq!(stream_buffer.read(&mut cp_buf).unwrap(), 480 * 2 * 2);
            for i in 0..cp_buf.len() {
                assert_eq!(cp_buf[i], 0, "Read samples should all be zeros.");
            }
        }
        // The second call should block until the first buffer is consumed.
        let _stream_buffer = stream.next_capture_buffer().unwrap();
        let elapsed = start.elapsed();
        assert!(
            elapsed > Duration::from_millis(10),
            "next_capture_buffer didn't block long enough {}",
            elapsed.subsec_millis()
        );
    }

}
