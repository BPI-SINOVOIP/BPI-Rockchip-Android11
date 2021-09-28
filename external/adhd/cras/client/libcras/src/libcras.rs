// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//! Provides an interface for playing and recording audio through CRAS server.
//!
//! `CrasClient` implements `StreamSource` trait and it can create playback or capture
//! stream - `CrasStream` which can be a
//! - `PlaybackBufferStream` for audio playback or
//! - `CaptureBufferStream` for audio capture.
//!
//! # Example of file audio playback
//!
//! `PlaybackBuffer`s to be filled with audio samples are obtained by calling
//! `next_playback_buffer` from `CrasStream`.
//!
//! Users playing audio fill the provided buffers with audio. When a `PlaybackBuffer` is dropped,
//! the samples written to it are committed to the `CrasStream` it came from.
//!
//!
//! ```
//! // An example of playing raw audio data from a given file
//! use std::env;
//! use std::fs::File;
//! use std::io::{Read, Write};
//! use std::thread::{spawn, JoinHandle};
//! type Result<T> = std::result::Result<T, Box<std::error::Error>>;
//!
//! use libcras::{CrasClient, CrasClientType};
//! use audio_streams::StreamSource;
//!
//! const BUFFER_SIZE: usize = 256;
//! const FRAME_RATE: usize = 44100;
//! const NUM_CHANNELS: usize = 2;
//!
//! # fn main() -> Result<()> {
//! #    let args: Vec<String> = env::args().collect();
//! #    match args.len() {
//! #        2 => {
//!              let mut cras_client = CrasClient::new()?;
//!              cras_client.set_client_type(CrasClientType::CRAS_CLIENT_TYPE_TEST);
//!              let (_control, mut stream) = cras_client
//!                  .new_playback_stream(NUM_CHANNELS, FRAME_RATE, BUFFER_SIZE)?;
//!
//!              // Plays 1000 * BUFFER_SIZE samples from the given file
//!              let mut file = File::open(&args[1])?;
//!              let mut local_buffer = [0u8; BUFFER_SIZE * NUM_CHANNELS * 2];
//!              for _i in 0..1000 {
//!                  // Reads data to local buffer
//!                  let _read_count = file.read(&mut local_buffer)?;
//!
//!                  // Gets writable buffer from stream and
//!                  let mut buffer = stream.next_playback_buffer()?;
//!                  // Writes data to stream buffer
//!                  let _write_frames = buffer.write(&local_buffer)?;
//!              }
//!              // Stream and client should gracefully be closed out of this scope
//! #        }
//! #        _ => {
//! #            println!("{} /path/to/playback_file.raw", args[0]);
//! #        }
//! #    };
//! #    Ok(())
//! # }
//! ```
//!
//! # Example of file audio capture
//!
//! `CaptureBuffer`s which contain audio samples are obtained by calling
//! `next_capture_buffer` from `CrasStream`.
//!
//! Users get captured audio samples from the provided buffers. When a `CaptureBuffer` is dropped,
//! the number of read samples will be committed to the `CrasStream` it came from.
//! ```
//! use std::env;
//! use std::fs::File;
//! use std::io::{Read, Write};
//! use std::thread::{spawn, JoinHandle};
//! type Result<T> = std::result::Result<T, Box<std::error::Error>>;
//!
//! use libcras::{CrasClient, CrasClientType};
//! use audio_streams::StreamSource;
//!
//! const BUFFER_SIZE: usize = 256;
//! const FRAME_RATE: usize = 44100;
//! const NUM_CHANNELS: usize = 2;
//!
//! # fn main() -> Result<()> {
//! #    let args: Vec<String> = env::args().collect();
//! #    match args.len() {
//! #        2 => {
//!              let mut cras_client = CrasClient::new()?;
//!              cras_client.set_client_type(CrasClientType::CRAS_CLIENT_TYPE_TEST);
//!              let (_control, mut stream) = cras_client
//!                  .new_capture_stream(NUM_CHANNELS, FRAME_RATE, BUFFER_SIZE)?;
//!
//!              // Capture 1000 * BUFFER_SIZE samples to the given file
//!              let mut file = File::create(&args[1])?;
//!              let mut local_buffer = [0u8; BUFFER_SIZE * NUM_CHANNELS * 2];
//!              for _i in 0..1000 {
//!
//!                  // Gets readable buffer from stream and
//!                  let mut buffer = stream.next_capture_buffer()?;
//!                  // Reads data to local buffer
//!                  let read_count = buffer.read(&mut local_buffer)?;
//!                  // Writes data to file
//!                  let _read_frames = file.write(&local_buffer[..read_count])?;
//!              }
//!              // Stream and client should gracefully be closed out of this scope
//! #        }
//! #        _ => {
//! #            println!("{} /path/to/capture_file.raw", args[0]);
//! #        }
//! #    };
//! #    Ok(())
//! # }
//! ```
use std::io;
use std::mem;
use std::os::unix::{
    io::{AsRawFd, RawFd},
    net::UnixStream,
};
use std::{error, fmt};

use audio_streams::{
    capture::{CaptureBufferStream, DummyCaptureStream},
    BufferDrop, DummyStreamControl, PlaybackBufferStream, StreamControl, StreamSource,
};
pub use cras_sys::gen::CRAS_CLIENT_TYPE as CrasClientType;
use cras_sys::gen::*;
use sys_util::{PollContext, PollToken};

mod audio_socket;
use crate::audio_socket::AudioSocket;
mod cras_server_socket;
use crate::cras_server_socket::CrasServerSocket;
mod cras_shm;
mod cras_stream;
use crate::cras_stream::{CrasCaptureData, CrasPlaybackData, CrasStream, CrasStreamData};
mod cras_client_message;
use crate::cras_client_message::*;

#[derive(Debug)]
pub enum ErrorType {
    CrasClientMessageError(cras_client_message::Error),
    CrasStreamError(cras_stream::Error),
    IoError(io::Error),
    SysUtilError(sys_util::Error),
    MessageTypeError,
    UnexpectedExit,
}

#[derive(Debug)]
pub struct Error {
    error_type: ErrorType,
}

impl Error {
    fn new(error_type: ErrorType) -> Self {
        Self { error_type }
    }
}

impl error::Error for Error {}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.error_type {
            ErrorType::CrasClientMessageError(ref err) => err.fmt(f),
            ErrorType::CrasStreamError(ref err) => err.fmt(f),
            ErrorType::IoError(ref err) => err.fmt(f),
            ErrorType::SysUtilError(ref err) => err.fmt(f),
            ErrorType::MessageTypeError => write!(f, "Message type error"),
            ErrorType::UnexpectedExit => write!(f, "Unexpected exit"),
        }
    }
}

type Result<T> = std::result::Result<T, Error>;

impl From<io::Error> for Error {
    fn from(io_err: io::Error) -> Self {
        Self::new(ErrorType::IoError(io_err))
    }
}

impl From<sys_util::Error> for Error {
    fn from(sys_util_err: sys_util::Error) -> Self {
        Self::new(ErrorType::SysUtilError(sys_util_err))
    }
}

impl From<cras_stream::Error> for Error {
    fn from(err: cras_stream::Error) -> Self {
        Self::new(ErrorType::CrasStreamError(err))
    }
}

impl From<cras_client_message::Error> for Error {
    fn from(err: cras_client_message::Error) -> Self {
        Self::new(ErrorType::CrasClientMessageError(err))
    }
}

/// A CRAS server client, which implements StreamSource. It can create audio streams connecting
/// to CRAS server.
pub struct CrasClient {
    server_socket: CrasServerSocket,
    client_id: u32,
    next_stream_id: u32,
    cras_capture: bool,
    client_type: CRAS_CLIENT_TYPE,
}

impl CrasClient {
    /// Blocks creating a `CrasClient` with registered `client_id`
    ///
    /// # Results
    ///
    /// * `CrasClient` - A client to interact with CRAS server
    ///
    /// # Errors
    ///
    /// Returns error if error occurs while handling server message or message
    /// type is incorrect
    pub fn new() -> Result<Self> {
        // Create a connection to the server.
        let mut server_socket = CrasServerSocket::new()?;

        // Gets client ID from server
        let client_id = {
            match CrasClient::wait_for_message(&mut server_socket)? {
                ServerResult::Connected(res, _server_state_fd) => res as u32,
                _ => {
                    return Err(Error::new(ErrorType::MessageTypeError));
                }
            }
        };

        Ok(Self {
            server_socket,
            client_id,
            next_stream_id: 0,
            cras_capture: false,
            client_type: CRAS_CLIENT_TYPE::CRAS_CLIENT_TYPE_UNKNOWN,
        })
    }

    /// Enables capturing audio through CRAS server.
    pub fn enable_cras_capture(&mut self) {
        self.cras_capture = true;
    }

    /// Set the type of this client to report to CRAS when connecting streams.
    pub fn set_client_type(&mut self, client_type: CRAS_CLIENT_TYPE) {
        self.client_type = client_type;
    }

    // Gets next server_stream_id from client and increment stream_id counter.
    fn next_server_stream_id(&mut self) -> Result<u32> {
        let res = self.next_stream_id;
        self.next_stream_id += 1;
        self.server_stream_id(&res)
    }

    // Gets server_stream_id from given stream_id
    fn server_stream_id(&self, stream_id: &u32) -> Result<u32> {
        Ok((self.client_id << 16) | stream_id)
    }

    // Creates general stream with given parameters
    fn create_stream<'a, T: BufferDrop + CrasStreamData<'a>>(
        &mut self,
        block_size: u32,
        direction: CRAS_STREAM_DIRECTION,
        rate: usize,
        channel_num: usize,
        format: snd_pcm_format_t,
    ) -> Result<CrasStream<'a, T>> {
        let stream_id = self.next_server_stream_id()?;

        // Prepares server message
        let audio_format = cras_audio_format_packed::new(format, rate, channel_num);
        let msg_header = cras_server_message {
            length: mem::size_of::<cras_connect_message>() as u32,
            id: CRAS_SERVER_MESSAGE_ID::CRAS_SERVER_CONNECT_STREAM,
        };
        let server_cmsg = cras_connect_message {
            header: msg_header,
            proto_version: CRAS_PROTO_VER,
            direction,
            stream_id,
            stream_type: CRAS_STREAM_TYPE::CRAS_STREAM_TYPE_DEFAULT,
            buffer_frames: block_size,
            cb_threshold: block_size,
            flags: 0,
            format: audio_format,
            dev_idx: CRAS_SPECIAL_DEVICE::NO_DEVICE as u32,
            effects: 0,
            client_type: self.client_type,
            client_shm_size: 0,
        };

        // Creates AudioSocket pair
        let (sock1, sock2) = UnixStream::pair()?;

        // Sends `CRAS_SERVER_CONNECT_STREAM` message
        let socks = [sock2.as_raw_fd()];
        self.server_socket
            .send_server_message_with_fds(&server_cmsg, &socks)?;

        let audio_socket = AudioSocket::new(sock1);
        let mut stream = CrasStream::new(
            stream_id,
            self.server_socket.try_clone()?,
            block_size,
            direction,
            rate,
            channel_num,
            format,
            audio_socket,
        );

        loop {
            let result = CrasClient::wait_for_message(&mut self.server_socket)?;
            if let ServerResult::StreamConnected(_stream_id, header_fd, samples_fd) = result {
                stream.init_shm(header_fd, samples_fd)?;
                break;
            }
        }

        Ok(stream)
    }

    // Blocks handling the first server message received from `socket`.
    fn wait_for_message(socket: &mut CrasServerSocket) -> Result<ServerResult> {
        #[derive(PollToken)]
        enum Token {
            ServerMsg,
        }
        let poll_ctx: PollContext<Token> =
            PollContext::new().and_then(|pc| pc.add(socket, Token::ServerMsg).and(Ok(pc)))?;

        let events = poll_ctx.wait()?;
        // Check the first readable message
        let tokens: Vec<Token> = events.iter_readable().map(|e| e.token()).collect();
        tokens
            .get(0)
            .ok_or_else(|| Error::new(ErrorType::UnexpectedExit))
            .and_then(|ref token| {
                match token {
                    Token::ServerMsg => ServerResult::handle_server_message(socket),
                }
                .map_err(Into::into)
            })
    }
}

impl StreamSource for CrasClient {
    fn new_playback_stream(
        &mut self,
        num_channels: usize,
        frame_rate: usize,
        buffer_size: usize,
    ) -> std::result::Result<
        (Box<dyn StreamControl>, Box<dyn PlaybackBufferStream>),
        Box<dyn error::Error>,
    > {
        Ok((
            Box::new(DummyStreamControl::new()),
            Box::new(self.create_stream::<CrasPlaybackData>(
                buffer_size as u32,
                CRAS_STREAM_DIRECTION::CRAS_STREAM_OUTPUT,
                frame_rate,
                num_channels,
                _snd_pcm_format::SND_PCM_FORMAT_S16_LE,
            )?),
        ))
    }

    fn new_capture_stream(
        &mut self,
        num_channels: usize,
        frame_rate: usize,
        buffer_size: usize,
    ) -> std::result::Result<
        (Box<dyn StreamControl>, Box<dyn CaptureBufferStream>),
        Box<dyn error::Error>,
    > {
        if self.cras_capture {
            Ok((
                Box::new(DummyStreamControl::new()),
                Box::new(self.create_stream::<CrasCaptureData>(
                    buffer_size as u32,
                    CRAS_STREAM_DIRECTION::CRAS_STREAM_INPUT,
                    frame_rate,
                    num_channels,
                    _snd_pcm_format::SND_PCM_FORMAT_S16_LE,
                )?),
            ))
        } else {
            Ok((
                Box::new(DummyStreamControl::new()),
                Box::new(DummyCaptureStream::new(
                    num_channels,
                    frame_rate,
                    buffer_size,
                )),
            ))
        }
    }

    fn keep_fds(&self) -> Option<Vec<RawFd>> {
        Some(vec![self.server_socket.as_raw_fd()])
    }
}
