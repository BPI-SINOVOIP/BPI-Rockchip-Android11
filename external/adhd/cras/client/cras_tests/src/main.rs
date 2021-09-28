// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod audio_options;

use std::fs::File;
use std::io::{self, BufRead, BufReader, Write};
use std::thread::spawn;
use sys_util::{set_rt_prio_limit, set_rt_round_robin};
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

use audio_streams::StreamSource;
use libcras::CrasClient;

use crate::audio_options::{AudioOptions, Subcommand};

fn set_priority_to_realtime() {
    const AUDIO_THREAD_RTPRIO: u16 = 10;
    if set_rt_prio_limit(AUDIO_THREAD_RTPRIO as u64).is_err()
        || set_rt_round_robin(AUDIO_THREAD_RTPRIO as i32).is_err()
    {
        println!("Attempt to use real-time priority failed, running with default scheduler.");
    }
}

fn channel_string(num_channels: usize) -> String {
    match num_channels {
        1 => "Mono".to_string(),
        2 => "Stereo".to_string(),
        _ => format!("{} Channels", num_channels),
    }
}

fn playback(opts: AudioOptions) -> Result<()> {
    let file = File::open(&opts.file_name).expect("failed to open file");
    let mut buffered_file = BufReader::new(file);

    let num_channels = opts.num_channels.unwrap_or(2);
    let frame_rate = opts.frame_rate.unwrap_or(48000);

    println!(
        "Playing raw data '{}' : Signed 16 bit Little Endian, Rate {} Hz, {}",
        opts.file_name.display(),
        frame_rate,
        channel_string(num_channels)
    );

    let mut cras_client = CrasClient::new()?;
    let (_control, mut stream) = cras_client.new_playback_stream(
        num_channels,
        frame_rate,
        opts.buffer_size.unwrap_or(256),
    )?;
    let thread = spawn(move || {
        set_priority_to_realtime();
        loop {
            let local_buffer = buffered_file
                .fill_buf()
                .expect("failed to read from input file");

            // Reached EOF
            if local_buffer.len() == 0 {
                break;
            }

            // Gets writable buffer from stream
            let mut buffer = stream
                .next_playback_buffer()
                .expect("failed to get next playback buffer");

            // Writes data to stream buffer
            let write_frames = buffer
                .write(&local_buffer)
                .expect("failed to write output data to buffer");

            // Mark the file data as written
            buffered_file.consume(write_frames);
        }
    });
    thread.join().expect("Failed to join playback thread");
    // Stream and client should gracefully be closed out of this scope

    Ok(())
}

fn capture(opts: AudioOptions) -> Result<()> {
    let num_channels = opts.num_channels.unwrap_or(2);
    let frame_rate = opts.frame_rate.unwrap_or(48000);

    println!(
        "Recording raw data '{}' : Signed 16 bit Little Endian, Rate {} Hz, {}",
        opts.file_name.display(),
        frame_rate,
        channel_string(num_channels)
    );

    let mut cras_client = CrasClient::new()?;
    cras_client.enable_cras_capture();
    let (_control, mut stream) = cras_client.new_capture_stream(
        num_channels,
        frame_rate,
        opts.buffer_size.unwrap_or(256),
    )?;
    let mut file = File::create(&opts.file_name).unwrap();
    loop {
        let _frames = match stream.next_capture_buffer() {
            Err(e) => {
                return Err(e.into());
            }
            Ok(mut buf) => {
                let written = io::copy(&mut buf, &mut file)?;
                written
            }
        };
    }
}

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    let opts = match AudioOptions::parse_from_args(&args)? {
        None => return Ok(()),
        Some(v) => v,
    };

    match opts.subcommand {
        Subcommand::Capture => capture(opts)?,
        Subcommand::Playback => playback(opts)?,
    };
    Ok(())
}
