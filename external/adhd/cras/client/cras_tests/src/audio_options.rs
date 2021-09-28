// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use std::fmt;
use std::io;
use std::path::PathBuf;

use getopts::Options;

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

#[derive(Debug, PartialEq)]
pub enum Subcommand {
    Capture,
    Playback,
}

impl fmt::Display for Subcommand {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Subcommand::Capture => write!(f, "capture"),
            Subcommand::Playback => write!(f, "playback"),
        }
    }
}

fn show_usage<T: AsRef<str>>(program_name: T) {
    println!(
        "Usage: {} [subcommand] <subcommand args>",
        program_name.as_ref()
    );
    println!("\nSubcommands:\n");
    println!("capture - Capture to a file from CRAS");
    println!("playback - Playback to CRAS from a file");
    println!("\nhelp - Print help message");
}

fn show_subcommand_usage<T: AsRef<str>>(program_name: T, subcommand: &Subcommand, opts: &Options) {
    let brief = format!(
        "Usage: {} {} [options] [filename]",
        program_name.as_ref(),
        subcommand
    );
    print!("{}", opts.usage(&brief));
}

pub struct AudioOptions {
    pub subcommand: Subcommand,
    pub file_name: PathBuf,
    pub buffer_size: Option<usize>,
    pub num_channels: Option<usize>,
    pub frame_rate: Option<usize>,
}

impl AudioOptions {
    pub fn parse_from_args<T: AsRef<str>>(args: &[T]) -> Result<Option<Self>> {
        let mut opts = Options::new();
        opts.optopt("b", "buffer_size", "Buffer size in frames", "SIZE")
            .optopt("c", "", "Number of channels", "NUM")
            .optopt("r", "rate", "Audio frame rate (Hz)", "RATE")
            .optflag("h", "help", "Print help message");

        let mut args = args.into_iter().map(|s| s.as_ref());

        let program_name = args.next().ok_or_else(|| {
            Box::new(io::Error::new(
                std::io::ErrorKind::InvalidInput,
                "Program name must be specified",
            ))
        })?;

        let subcommand = match args.next() {
            None => {
                println!("Must specify a subcommand.");
                show_usage(program_name);
                return Err(Box::new(std::io::Error::new(
                    std::io::ErrorKind::InvalidInput,
                    "No subcommand",
                )));
            }
            Some("help") => {
                show_usage(&program_name);
                return Ok(None);
            }
            Some("capture") => Subcommand::Capture,
            Some("playback") => Subcommand::Playback,
            Some(s) => {
                println!("Subcommand \"{}\" does not exist.", s);
                show_usage(&program_name);
                return Err(Box::new(std::io::Error::new(
                    std::io::ErrorKind::InvalidInput,
                    "Subcommand does not exist",
                )));
            }
        };

        let matches = match opts.parse(args) {
            Ok(m) => m,
            Err(e) => {
                show_subcommand_usage(&program_name, &subcommand, &opts);
                return Err(Box::new(e));
            }
        };
        if matches.opt_present("h") {
            show_subcommand_usage(&program_name, &subcommand, &opts);
            return Ok(None);
        }
        let file_name = match matches.free.get(0) {
            None => {
                println!("Must provide file name.");
                show_subcommand_usage(&program_name, &subcommand, &opts);
                return Err(Box::new(std::io::Error::new(
                    std::io::ErrorKind::InvalidInput,
                    "Must provide file name.",
                )));
            }
            Some(file_name) => PathBuf::from(file_name),
        };
        let buffer_size = matches.opt_get::<usize>("b")?;
        let num_channels = matches.opt_get::<usize>("c")?;
        let frame_rate = matches.opt_get::<usize>("r")?;

        Ok(Some(AudioOptions {
            subcommand,
            file_name,
            buffer_size,
            num_channels,
            frame_rate,
        }))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::OsString;

    #[test]
    fn parse_from_args() {
        let opts = AudioOptions::parse_from_args(&["cras_tests", "playback", "output.wav"])
            .unwrap()
            .unwrap();
        assert_eq!(opts.subcommand, Subcommand::Playback);
        assert_eq!(opts.file_name, OsString::from("output.wav"));
        assert_eq!(opts.frame_rate, None);
        assert_eq!(opts.num_channels, None);
        assert_eq!(opts.buffer_size, None);

        let opts = AudioOptions::parse_from_args(&["cras_tests", "capture", "input.flac"])
            .unwrap()
            .unwrap();
        assert_eq!(opts.subcommand, Subcommand::Capture);
        assert_eq!(opts.file_name, OsString::from("input.flac"));
        assert_eq!(opts.frame_rate, None);
        assert_eq!(opts.num_channels, None);
        assert_eq!(opts.buffer_size, None);

        let opts = AudioOptions::parse_from_args(&[
            "cras_tests",
            "playback",
            "-r",
            "44100",
            "output.wav",
            "-c",
            "2",
        ])
        .unwrap()
        .unwrap();
        assert_eq!(opts.subcommand, Subcommand::Playback);
        assert_eq!(opts.file_name, OsString::from("output.wav"));
        assert_eq!(opts.frame_rate, Some(44100));
        assert_eq!(opts.num_channels, Some(2));
        assert_eq!(opts.buffer_size, None);

        assert!(AudioOptions::parse_from_args(&["cras_tests"]).is_err());
        assert!(AudioOptions::parse_from_args(&["cras_tests", "capture"]).is_err());
        assert!(AudioOptions::parse_from_args(&["cras_tests", "playback"]).is_err());
        assert!(AudioOptions::parse_from_args(&["cras_tests", "loopback"]).is_err());
        assert!(AudioOptions::parse_from_args(&["cras_tests", "loopback", "file.ogg"]).is_err());
        assert!(AudioOptions::parse_from_args(&["cras_tests", "filename.wav"]).is_err());
        assert!(AudioOptions::parse_from_args(&["cras_tests", "filename.wav", "capture"]).is_err());
        assert!(AudioOptions::parse_from_args(&["cras_tests", "help"]).is_ok());
        assert!(AudioOptions::parse_from_args(&[
            "cras_tests",
            "-c",
            "2",
            "playback",
            "output.wav",
            "-r",
            "44100"
        ])
        .is_err());
    }
}
