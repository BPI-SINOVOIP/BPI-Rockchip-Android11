// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
extern crate bindgen;

use bindgen::builder;

use std::fs::{self, File};
use std::io::Write;
use std::path::Path;
use std::str;

fn copy_headers(src_dir: &Path, dst_dir: &Path) -> Result<(), String> {
    if dst_dir.is_file() {
        fs::remove_file(&dst_dir).or_else(|e| {
            Err(format!(
                "failed to remove existing file at {:?}: {}",
                dst_dir, e
            ))
        })?;
    }

    if !dst_dir.is_dir() {
        fs::create_dir(&dst_dir).or_else(|e| {
            Err(format!(
                "failed to create destination directory: {:?}: {}",
                dst_dir, e
            ))
        })?;
    }

    let header_files = vec![
        "cras_audio_format.h",
        "cras_iodev_info.h",
        "cras_messages.h",
        "cras_shm.h",
        "cras_types.h",
        "cras_util.h",
    ];

    for header in &header_files {
        let src = src_dir.join(&header);
        let dst = dst_dir.join(&header);
        fs::copy(&src, &dst).or_else(|e| {
            Err(format!(
                "failed to copy header file {:?} to {:?}: {}",
                src, dst, e
            ))
        })?;
    }
    Ok(())
}

/*
 * If we use both `packed` and `align(4)` for a struct, bindgen will generate
 * it as an opaque struct.
 *
 * `cras_server_state` is created from C with `packed` and `aligned(4)` and
 * shared through a shared memory area.
 *
 * Structs with `packed` and `align(4)` have the same memory layout as those
 * with `packed` except for some extra alignment bytes at the end.
 *
 * Therefore, using only `packed` for `cras_server_state` from Rust side is safe.
 *
 * This function modifies `cras_server_state` from
 * `__attribute__ ((packed, aligned(4)))` to `__attribute__ ((packed))`
 */
fn modify_server_state_attributes(dir: &Path) -> Result<(), String> {
    let cras_types_path = dir.join("cras_types.h");
    let bytes = fs::read(&cras_types_path)
        .or_else(|e| Err(format!("failed to read {:?}: {}", cras_types_path, e)))?;

    let old = str::from_utf8(&bytes).or_else(|e| {
        Err(format!(
            "failed to parse {:?} as utf8: {}",
            cras_types_path, e
        ))
    })?;

    let new = old.replacen(
        "struct __attribute__((packed, aligned(4))) cras_server_state {",
        "struct __attribute__((packed)) cras_server_state {",
        1,
    );

    if new.len() >= old.len() {
        return Err("failed to remove 'aligned(4)' from cras_server_state".to_string());
    }

    fs::write(&cras_types_path, new).or_else(|e| {
        Err(format!(
            "failed to write updated contents to {:?}: {}",
            cras_types_path, e
        ))
    })?;

    Ok(())
}

fn gen() -> String {
    let name = "cras_gen";
    let bindings = builder()
        .header("c_headers/cras_messages.h")
        .header("c_headers/cras_types.h")
        .header("c_headers/cras_audio_format.h")
        .header("c_headers/cras_shm.h")
        .whitelist_type("cras_.*")
        .whitelist_var("cras_.*")
        .whitelist_type("CRAS_.*")
        .whitelist_var("CRAS_.*")
        .whitelist_type("audio_message")
        .rustified_enum("CRAS_.*")
        .rustified_enum("_snd_pcm_.*")
        .generate()
        .expect(format!("Unable to generate {} code", name).as_str());

    bindings.to_string()
}

fn write_output(output_path: &Path, output: String) -> std::io::Result<()> {
    let header = b"// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * generated from files in cras/src/common in adhd:
 * cras_audio_format.h
 * cras_iodev_info.h
 * cras_messages.h
 * cras_shm.h
 * cras_types.h
 * cras_util.h
 */
";

    let mut output_file = File::create(output_path)?;
    output_file.write_all(header)?;
    output_file.write_all(output.as_bytes())?;
    Ok(())
}

fn main() {
    let src_header_dir = Path::new("../../../src/common");
    let dst_header_dir = Path::new("./c_headers");

    copy_headers(src_header_dir, dst_header_dir).expect("failed to copy C headers");
    modify_server_state_attributes(dst_header_dir)
        .expect("failed to modify cras_server_state's attributes");
    let generated_code = gen();
    write_output(Path::new("lib_gen.rs"), generated_code).expect("failed to write generated code");
}
