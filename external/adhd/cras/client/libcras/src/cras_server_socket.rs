// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use std::io;
use std::os::unix::io::{AsRawFd, RawFd};

use sys_util::{net::UnixSeqpacket, ScmSocket};

use data_model::DataInit;

const CRAS_SERVER_SOCKET_PATH: &str = "/run/cras/.cras_socket";
/// A socket connecting to the CRAS audio server.
pub struct CrasServerSocket {
    socket: UnixSeqpacket,
}

impl CrasServerSocket {
    pub fn new() -> io::Result<CrasServerSocket> {
        let socket = UnixSeqpacket::connect(CRAS_SERVER_SOCKET_PATH)?;
        Ok(CrasServerSocket { socket })
    }

    /// Sends a sized and packed server messge to the server socket. The message
    /// must implement `Sized` and `DataInit`.
    /// # Arguments
    /// * `message` - A sized and packed message.
    /// * `fds` - A slice of fds to send.
    ///
    /// # Returns
    /// * Length of written bytes in `usize`.
    ///
    /// # Errors
    /// Return error if the socket fails to write message to server.
    pub fn send_server_message_with_fds<M: Sized + DataInit>(
        &self,
        message: &M,
        fds: &[RawFd],
    ) -> io::Result<usize> {
        match fds.len() {
            0 => self.socket.send(message.as_slice()),
            _ => match self.send_with_fds(message.as_slice(), fds) {
                Ok(len) => Ok(len),
                Err(err) => Err(io::Error::new(io::ErrorKind::Other, format!("{}", err))),
            },
        }
    }

    /// Creates a clone of the underlying socket. The returned clone can also be
    /// used to communicate with the cras server.
    pub fn try_clone(&self) -> io::Result<CrasServerSocket> {
        let new_sock = self.socket.try_clone()?;
        Ok(CrasServerSocket { socket: new_sock })
    }
}

// For using `recv_with_fds` and `send_with_fds`.
impl ScmSocket for CrasServerSocket {
    fn socket_fd(&self) -> RawFd {
        self.socket.as_raw_fd()
    }
}

// For using `PollContex`.
impl AsRawFd for CrasServerSocket {
    fn as_raw_fd(&self) -> RawFd {
        self.socket.as_raw_fd()
    }
}
