// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
extern crate data_model;

#[allow(dead_code)]
#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
pub mod gen;
use gen::{
    _snd_pcm_format, audio_message, cras_audio_format_packed, CRAS_AUDIO_MESSAGE_ID, CRAS_CHANNEL,
};

unsafe impl data_model::DataInit for gen::audio_message {}
unsafe impl data_model::DataInit for gen::cras_client_connected {}
unsafe impl data_model::DataInit for gen::cras_client_stream_connected {}
unsafe impl data_model::DataInit for gen::cras_connect_message {}
unsafe impl data_model::DataInit for gen::cras_disconnect_stream_message {}
unsafe impl data_model::DataInit for gen::cras_server_state {}

impl cras_audio_format_packed {
    /// Initializes `cras_audio_format_packed` from input parameters.
    ///
    /// # Arguments
    /// * `format` - Format in used.
    /// * `rate` - Rate in used.
    /// * `num_channels` - Number of channels in used.
    ///
    /// # Returns
    /// Structure `cras_audio_format_packed`
    pub fn new(format: _snd_pcm_format, rate: usize, num_channels: usize) -> Self {
        let mut audio_format = Self {
            format: format as i32,
            frame_rate: rate as u32,
            num_channels: num_channels as u32,
            channel_layout: [-1; CRAS_CHANNEL::CRAS_CH_MAX as usize],
        };
        for i in 0..CRAS_CHANNEL::CRAS_CH_MAX as usize {
            if i < num_channels {
                audio_format.channel_layout[i] = i as i8;
            } else {
                break;
            }
        }
        audio_format
    }
}

impl Default for audio_message {
    fn default() -> Self {
        Self {
            error: 0,
            frames: 0,
            id: CRAS_AUDIO_MESSAGE_ID::NUM_AUDIO_MESSAGES,
        }
    }
}
