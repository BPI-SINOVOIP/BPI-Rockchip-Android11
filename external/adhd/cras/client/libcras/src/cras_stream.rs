// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use std::cmp::min;
use std::io;
use std::marker::PhantomData;
use std::mem;
use std::{error, fmt};

use audio_streams::{
    capture::{CaptureBuffer, CaptureBufferStream},
    BufferDrop, PlaybackBuffer, PlaybackBufferStream,
};
use cras_sys::gen::{
    cras_disconnect_stream_message, cras_server_message, snd_pcm_format_t, CRAS_AUDIO_MESSAGE_ID,
    CRAS_SERVER_MESSAGE_ID, CRAS_STREAM_DIRECTION,
};
use sys_util::error;

use crate::audio_socket::{AudioMessage, AudioSocket};
use crate::cras_server_socket::CrasServerSocket;
use crate::cras_shm::*;

#[derive(Debug)]
pub enum ErrorType {
    IoError(io::Error),
    MessageTypeError,
    NoShmError,
}

#[derive(Debug)]
pub struct Error {
    error_type: ErrorType,
}

impl Error {
    fn new(error_type: ErrorType) -> Error {
        Error { error_type }
    }
}

impl error::Error for Error {}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.error_type {
            ErrorType::IoError(ref err) => err.fmt(f),
            ErrorType::MessageTypeError => write!(f, "Message type error"),
            ErrorType::NoShmError => write!(f, "Shared memory area is not created"),
        }
    }
}

impl From<io::Error> for Error {
    fn from(io_err: io::Error) -> Error {
        Error::new(ErrorType::IoError(io_err))
    }
}

/// A trait controls the state of `CrasAudioHeader` and
/// interacts with server's audio thread through `AudioSocket`.
pub trait CrasStreamData<'a>: Send {
    // Creates `CrasStreamData` with only `AudioSocket`.
    fn new(audio_sock: AudioSocket) -> Self;
    fn set_header(&mut self, header: CrasAudioHeader<'a>);
    fn header_mut(&mut self) -> &mut Option<CrasAudioHeader<'a>>;
    fn audio_sock_mut(&mut self) -> &mut AudioSocket;
}

/// `CrasStreamData` implementation for `PlaybackBufferStream`.
pub struct CrasPlaybackData<'a> {
    audio_sock: AudioSocket,
    header: Option<CrasAudioHeader<'a>>,
}

impl<'a> CrasStreamData<'a> for CrasPlaybackData<'a> {
    fn new(audio_sock: AudioSocket) -> Self {
        Self {
            audio_sock,
            header: None,
        }
    }

    fn set_header(&mut self, header: CrasAudioHeader<'a>) {
        self.header = Some(header);
    }

    fn header_mut(&mut self) -> &mut Option<CrasAudioHeader<'a>> {
        &mut self.header
    }

    fn audio_sock_mut(&mut self) -> &mut AudioSocket {
        &mut self.audio_sock
    }
}

impl<'a> BufferDrop for CrasPlaybackData<'a> {
    fn trigger(&mut self, nframes: usize) {
        let log_err = |e| error!("BufferDrop error: {}", e);
        if let Some(header) = &mut self.header {
            if let Err(e) = header.commit_written_frames(nframes as u32) {
                log_err(e);
            }
        }
        if let Err(e) = self.audio_sock.data_ready(nframes as u32) {
            log_err(e);
        }
    }
}

/// `CrasStreamData` implementation for `CaptureBufferStream`.
pub struct CrasCaptureData<'a> {
    audio_sock: AudioSocket,
    header: Option<CrasAudioHeader<'a>>,
}

impl<'a> CrasStreamData<'a> for CrasCaptureData<'a> {
    fn new(audio_sock: AudioSocket) -> Self {
        Self {
            audio_sock,
            header: None,
        }
    }

    fn set_header(&mut self, header: CrasAudioHeader<'a>) {
        self.header = Some(header);
    }

    fn header_mut(&mut self) -> &mut Option<CrasAudioHeader<'a>> {
        &mut self.header
    }

    fn audio_sock_mut(&mut self) -> &mut AudioSocket {
        &mut self.audio_sock
    }
}

impl<'a> BufferDrop for CrasCaptureData<'a> {
    fn trigger(&mut self, nframes: usize) {
        let log_err = |e| error!("BufferDrop error: {}", e);
        if let Some(header) = &mut self.header {
            if let Err(e) = header.commit_read_frames(nframes as u32) {
                log_err(e);
            }
        }
        if let Err(e) = self.audio_sock.capture_ready(nframes as u32) {
            log_err(e);
        }
    }
}

#[allow(dead_code)]
pub struct CrasStream<'a, T: CrasStreamData<'a> + BufferDrop> {
    stream_id: u32,
    server_socket: CrasServerSocket,
    block_size: u32,
    direction: CRAS_STREAM_DIRECTION,
    rate: usize,
    num_channels: usize,
    format: snd_pcm_format_t,
    /// A structure for stream to interact with server audio thread.
    controls: T,
    /// The `PhantomData` is used by `controls: T`
    phantom: PhantomData<CrasAudioHeader<'a>>,
    audio_buffer: Option<CrasAudioBuffer>,
}

impl<'a, T: CrasStreamData<'a> + BufferDrop> CrasStream<'a, T> {
    /// Creates a CrasStream by given arguments.
    ///
    /// # Returns
    /// `CrasStream` - CRAS client stream.
    pub fn new(
        stream_id: u32,
        server_socket: CrasServerSocket,
        block_size: u32,
        direction: CRAS_STREAM_DIRECTION,
        rate: usize,
        num_channels: usize,
        format: snd_pcm_format_t,
        audio_sock: AudioSocket,
    ) -> Self {
        Self {
            stream_id,
            server_socket,
            block_size,
            direction,
            rate,
            num_channels,
            format,
            controls: T::new(audio_sock),
            phantom: PhantomData,
            audio_buffer: None,
        }
    }

    /// Receives shared memory fd and initialize stream audio shared memory area
    pub fn init_shm(
        &mut self,
        header_fd: CrasAudioShmHeaderFd,
        samples_fd: CrasShmFd,
    ) -> Result<(), Error> {
        let (header, buffer) = create_header_and_buffers(header_fd, samples_fd)?;
        self.controls.set_header(header);
        self.audio_buffer = Some(buffer);
        Ok(())
    }

    fn wait_request_data(&mut self) -> Result<(), Error> {
        match self.controls.audio_sock_mut().read_audio_message()? {
            AudioMessage::Success { id, .. } => match id {
                CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_REQUEST_DATA => Ok(()),
                _ => Err(Error::new(ErrorType::MessageTypeError)),
            },
            _ => Err(Error::new(ErrorType::MessageTypeError)),
        }
    }

    fn wait_data_ready(&mut self) -> Result<u32, Error> {
        match self.controls.audio_sock_mut().read_audio_message()? {
            AudioMessage::Success { id, frames } => match id {
                CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_DATA_READY => Ok(frames),
                _ => Err(Error::new(ErrorType::MessageTypeError)),
            },
            _ => Err(Error::new(ErrorType::MessageTypeError)),
        }
    }
}

impl<'a, T: CrasStreamData<'a> + BufferDrop> Drop for CrasStream<'a, T> {
    /// A blocking drop function, sends the disconnect message to `CrasClient` and waits for
    /// the return message.
    /// Logs an error message to stderr if the method fails.
    fn drop(&mut self) {
        // Send stream disconnect message
        let msg_header = cras_server_message {
            length: mem::size_of::<cras_disconnect_stream_message>() as u32,
            id: CRAS_SERVER_MESSAGE_ID::CRAS_SERVER_DISCONNECT_STREAM,
        };
        let server_cmsg = cras_disconnect_stream_message {
            header: msg_header,
            stream_id: self.stream_id,
        };
        if let Err(e) = self
            .server_socket
            .send_server_message_with_fds(&server_cmsg, &[])
        {
            error!("CrasStream::Drop error: {}", e);
        }
    }
}

impl<'a, T: CrasStreamData<'a> + BufferDrop> PlaybackBufferStream for CrasStream<'a, T> {
    fn next_playback_buffer(&mut self) -> Result<PlaybackBuffer, Box<dyn error::Error>> {
        // Wait for request audio message
        self.wait_request_data()?;
        let (frame_size, (offset, len)) = match self.controls.header_mut() {
            None => return Err(Error::new(ErrorType::NoShmError).into()),
            Some(header) => (header.get_frame_size(), header.get_write_offset_and_len()?),
        };
        let buf = match self.audio_buffer.as_mut() {
            None => return Err(Error::new(ErrorType::NoShmError).into()),
            Some(audio_buffer) => &mut audio_buffer.get_buffer()[offset..offset + len],
        };
        PlaybackBuffer::new(frame_size, buf, &mut self.controls).map_err(Box::from)
    }
}

impl<'a, T: CrasStreamData<'a> + BufferDrop> CaptureBufferStream for CrasStream<'a, T> {
    fn next_capture_buffer(&mut self) -> Result<CaptureBuffer, Box<dyn error::Error>> {
        // Wait for data ready message
        let frames = self.wait_data_ready()?;
        let (frame_size, shm_frames, offset) = match self.controls.header_mut() {
            None => return Err(Error::new(ErrorType::NoShmError).into()),
            Some(header) => (
                header.get_frame_size(),
                header.get_readable_frames()?,
                header.get_read_buffer_offset()?,
            ),
        };
        let len = min(shm_frames, frames as usize) * frame_size;
        let buf = match self.audio_buffer.as_mut() {
            None => return Err(Error::new(ErrorType::NoShmError).into()),
            Some(audio_buffer) => &mut audio_buffer.get_buffer()[offset..offset + len],
        };
        CaptureBuffer::new(frame_size, buf, &mut self.controls).map_err(Box::from)
    }
}
