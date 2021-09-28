// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use std::io;
use std::io::{Read, Write};
use std::mem;
use std::os::unix::{
    io::{AsRawFd, RawFd},
    net::UnixStream,
};
use std::time::Duration;

use cras_sys::gen::{audio_message, CRAS_AUDIO_MESSAGE_ID};
use data_model::DataInit;
use sys_util::{PollContext, PollToken};

/// A structure for interacting with the CRAS server audio thread through a `UnixStream::pair`.
pub struct AudioSocket {
    socket: UnixStream,
}

/// Audio message results which are exchanged by `CrasStream` and CRAS audio server.
/// through an audio socket.
#[allow(dead_code)]
#[derive(Debug)]
pub enum AudioMessage {
    /// * `id` - Audio message id, which is a `enum CRAS_AUDIO_MESSAGE_ID`.
    /// * `frames` - A `u32` indicating the read or written frame count.
    Success {
        id: CRAS_AUDIO_MESSAGE_ID,
        frames: u32,
    },
    /// * `error` - Error code when a error occurs.
    Error(i32),
}

/// Converts AudioMessage to raw audio_message for CRAS audio server.
impl Into<audio_message> for AudioMessage {
    fn into(self) -> audio_message {
        match self {
            AudioMessage::Success { id, frames } => audio_message {
                id,
                error: 0,
                frames,
            },
            AudioMessage::Error(error) => audio_message {
                id: CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_REQUEST_DATA,
                error,
                frames: 0,
            },
        }
    }
}

/// Converts AudioMessage from raw audio_message from CRAS audio server.
impl From<audio_message> for AudioMessage {
    fn from(message: audio_message) -> Self {
        match message.error {
            0 => AudioMessage::Success {
                id: message.id as CRAS_AUDIO_MESSAGE_ID,
                frames: message.frames,
            },
            error => AudioMessage::Error(error),
        }
    }
}

impl AudioSocket {
    /// Creates `AudioSocket` from a `UnixStream`.
    ///
    /// # Arguments
    /// `socket` - A `UnixStream`.
    pub fn new(socket: UnixStream) -> Self {
        AudioSocket { socket }
    }

    fn read_from_socket<T>(&mut self) -> io::Result<T>
    where
        T: Sized + DataInit + Default,
    {
        let mut message: T = Default::default();
        let rc = self.socket.read(message.as_mut_slice())?;
        if rc == mem::size_of::<T>() {
            Ok(message)
        } else {
            Err(io::Error::new(io::ErrorKind::Other, "Read truncated data."))
        }
    }

    /// Blocks reading an `audio message`.
    ///
    /// # Returns
    /// `AudioMessage` - AudioMessage enum.
    ///
    /// # Errors
    /// Returns io::Error if error occurs.
    pub fn read_audio_message(&mut self) -> io::Result<AudioMessage> {
        match self.read_audio_message_with_timeout(None)? {
            None => Err(io::Error::new(io::ErrorKind::Other, "Unexpected exit")),
            Some(message) => Ok(message),
        }
    }

    /// Blocks waiting for an `audio message` until `timeout` occurs. If `timeout`
    /// is None, blocks indefinitely.
    ///
    /// # Returns
    /// Some(AudioMessage) - AudioMessage enum if we receive a message before timeout.
    /// None - If the timeout expires.
    ///
    /// # Errors
    /// Returns io::Error if error occurs.
    pub fn read_audio_message_with_timeout(
        &mut self,
        timeout: Option<Duration>,
    ) -> io::Result<Option<AudioMessage>> {
        #[derive(PollToken)]
        enum Token {
            AudioMsg,
        }
        let poll_ctx: PollContext<Token> =
            match PollContext::new().and_then(|pc| pc.add(self, Token::AudioMsg).and(Ok(pc))) {
                Ok(pc) => pc,
                Err(e) => {
                    return Err(io::Error::new(
                        io::ErrorKind::Other,
                        format!("Failed to create PollContext: {}", e),
                    ));
                }
            };
        let events = {
            let result = match timeout {
                None => poll_ctx.wait(),
                Some(duration) => poll_ctx.wait_timeout(duration),
            };
            match result {
                Ok(v) => v,
                Err(e) => {
                    return Err(io::Error::new(
                        io::ErrorKind::Other,
                        format!("Failed to poll: {:?}", e),
                    ));
                }
            }
        };

        // Check the first readable message
        let tokens: Vec<Token> = events.iter_readable().map(|e| e.token()).collect();
        match tokens.get(0) {
            None => Ok(None),
            Some(&Token::AudioMsg) => {
                let raw_msg: audio_message = self.read_from_socket()?;
                Ok(Some(AudioMessage::from(raw_msg)))
            }
        }
    }

    /// Sends raw audio message with given AudioMessage enum.
    ///
    /// # Arguments
    /// * `msg` - enum AudioMessage, which could be `Success` with message id
    /// and frames or `Error` with error code.
    ///
    /// # Errors
    /// Returns error if `libc::write` fails.
    fn send_audio_message(&mut self, msg: AudioMessage) -> io::Result<()> {
        let msg: audio_message = msg.into();
        let rc = self.socket.write(msg.as_slice())?;
        if rc < mem::size_of::<audio_message>() {
            Err(io::Error::new(io::ErrorKind::Other, "Sent truncated data."))
        } else {
            Ok(())
        }
    }

    /// Sends the data ready message with written frame count.
    ///
    /// # Arguments
    /// * `frames` - An `u32` indicating the written frame count.
    pub fn data_ready(&mut self, frames: u32) -> io::Result<()> {
        self.send_audio_message(AudioMessage::Success {
            id: CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_DATA_READY,
            frames,
        })
    }

    /// Sends the capture ready message with read frame count.
    ///
    /// # Arguments
    ///
    /// * `frames` - An `u32` indicating the number of read frames.
    pub fn capture_ready(&mut self, frames: u32) -> io::Result<()> {
        self.send_audio_message(AudioMessage::Success {
            id: CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_DATA_CAPTURED,
            frames,
        })
    }
}

impl AsRawFd for AudioSocket {
    fn as_raw_fd(&self) -> RawFd {
        self.socket.as_raw_fd()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // PartialEq for comparing AudioMessage in tests
    impl PartialEq for AudioMessage {
        fn eq(&self, other: &Self) -> bool {
            match (self, other) {
                (
                    AudioMessage::Success { id, frames },
                    AudioMessage::Success {
                        id: other_id,
                        frames: other_frames,
                    },
                ) => id == other_id && frames == other_frames,
                (AudioMessage::Error(err), AudioMessage::Error(other_err)) => err == other_err,
                _ => false,
            }
        }
    }

    fn init_audio_socket_pair() -> (AudioSocket, AudioSocket) {
        let (sock1, sock2) = UnixStream::pair().unwrap();
        let sender = AudioSocket::new(sock1);
        let receiver = AudioSocket::new(sock2);
        (sender, receiver)
    }

    #[test]
    fn audio_socket_send_and_recv_audio_message() {
        let (mut sender, mut receiver) = init_audio_socket_pair();
        let message_succ = AudioMessage::Success {
            id: CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_REQUEST_DATA,
            frames: 0,
        };
        sender.send_audio_message(message_succ).unwrap();
        let res = receiver.read_audio_message().unwrap();
        assert_eq!(
            res,
            AudioMessage::Success {
                id: CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_REQUEST_DATA,
                frames: 0
            }
        );

        let message_err = AudioMessage::Error(123);
        sender.send_audio_message(message_err).unwrap();
        let res = receiver.read_audio_message().unwrap();
        assert_eq!(res, AudioMessage::Error(123));
    }

    #[test]
    fn audio_socket_data_ready_send_and_recv() {
        let (sock1, sock2) = UnixStream::pair().unwrap();
        let mut audio_socket_send = AudioSocket::new(sock1);
        let mut audio_socket_recv = AudioSocket::new(sock2);
        audio_socket_send.data_ready(256).unwrap();

        // Test receiving by using raw audio_message since CRAS audio server use this.
        let audio_msg: audio_message = audio_socket_recv.read_from_socket().unwrap();
        let ref_audio_msg = audio_message {
            id: CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_DATA_READY,
            error: 0,
            frames: 256,
        };
        // Use brace to copy unaligned data locally
        assert_eq!({ audio_msg.id }, { ref_audio_msg.id });
        assert_eq!({ audio_msg.error }, { ref_audio_msg.error });
        assert_eq!({ audio_msg.frames }, { ref_audio_msg.frames });
    }

    #[test]
    fn audio_socket_capture_ready() {
        let (sock1, sock2) = UnixStream::pair().unwrap();
        let mut audio_socket_send = AudioSocket::new(sock1);
        let mut audio_socket_recv = AudioSocket::new(sock2);
        audio_socket_send
            .capture_ready(256)
            .expect("Failed to send capture ready message.");

        // Test receiving by using raw audio_message since CRAS audio server use this.
        let audio_msg: audio_message = audio_socket_recv
            .read_from_socket()
            .expect("Failed to read audio message from AudioSocket.");
        let ref_audio_msg = audio_message {
            id: CRAS_AUDIO_MESSAGE_ID::AUDIO_MESSAGE_DATA_CAPTURED,
            error: 0,
            frames: 256,
        };
        // Use brace to copy unaligned data locally
        assert_eq!({ audio_msg.id }, { ref_audio_msg.id });
        assert_eq!({ audio_msg.error }, { ref_audio_msg.error });
        assert_eq!({ audio_msg.frames }, { ref_audio_msg.frames });
    }

    #[test]
    fn audio_socket_send_when_broken_pipe() {
        let sock1 = {
            let (sock1, _) = UnixStream::pair().unwrap();
            sock1
        };
        let mut audio_socket = AudioSocket::new(sock1);
        let res = audio_socket.data_ready(256);
        //Broken pipe
        assert_eq!(
            res.expect_err("Result should be an error.").kind(),
            io::Error::from_raw_os_error(32).kind(),
            "Error should be broken pipe.",
        );
    }
}
