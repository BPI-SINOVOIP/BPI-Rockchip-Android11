/*
 * Copyright (C) 2015-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

GCSS_KEY_SECTION_START (NA, na, 0x0000)
GCSS_KEY (AE_STATS_SINK, ae_stats_sink)
GCSS_KEY (AF_STATS_SINK, af_stats_sink)
GCSS_KEY (AWB_STATS_SINK, awb_stats_sink)
GCSS_KEY (CONNECTION, connection)
GCSS_KEY (CONTENT_TYPE, content_type)
GCSS_KEY (FORMAT, format)
GCSS_KEY (FORMATS, formats)
GCSS_KEY (GRAPH, graph)
GCSS_KEY (GRAPHS, graphs)
GCSS_KEY (GRAPH_DESCRIPTOR, graph_descriptor)
GCSS_KEY (GRAPH_SETTINGS, graph_settings)
GCSS_KEY (INPUT, input)
GCSS_KEY (ISA, isa)
GCSS_KEY (KERNEL, kernel)
GCSS_KEY (METADATA, metadata)
GCSS_KEY (MODE_ID, mode_id)
GCSS_KEY (NAME, name)
GCSS_KEY (NODE, node)
GCSS_KEY (NODES, nodes)
GCSS_KEY (NON_SCALED_OUTPUT, non_scaled_output)
GCSS_KEY (OUTPUT, output)
GCSS_KEY (PEER, peer)
GCSS_KEY (PLANE, plane)
GCSS_KEY (PORT, port)
GCSS_KEY (PROGRAM_GROUP, program_group)
GCSS_KEY (SCALED_OUTPUT, scaled_output)
GCSS_KEY (SENSOR, sensor)
GCSS_KEY (SENSOR_MODE, sensor_mode)
GCSS_KEY (SENSOR_MODES, sensor_modes)
GCSS_KEY (PIXEL_ARRAY, pixel_array)
GCSS_KEY (BINNER, binner)
GCSS_KEY (SCALER, scaler)
GCSS_KEY (CSI_BE, csi_be)
GCSS_KEY (CSI_BE_SOC, csi_be_soc)
GCSS_KEY (SETTINGS, settings)
GCSS_KEY (SINK, sink)
GCSS_KEY (SOURCE, source)
GCSS_KEY (STATIC, static)
GCSS_KEY (TYPE, type)
GCSS_KEY (VALUE, value)
GCSS_KEY (CIPF, cipf)
GCSS_KEY (ATTRIBUTE, attribute)
GCSS_KEY (OPTIONS, options)
GCSS_KEY (APPLY, apply)
GCSS_KEY (TPG, tpg)
/* add new keys to this section above this line */

/* FORMAT KEY DEFINITIONS */
GCSS_KEY_SECTION_START (FORMAT_START, format_start, 0x1000)
GCSS_KEY (FORMAT_I420_VEC, V420)
GCSS_KEY (FORMAT_YYUV420_V32, y032)
GCSS_KEY (FORMAT_NV12, NV12)
GCSS_KEY (FORMAT_NV21, NV21)
GCSS_KEY (FORMAT_IYUV, IYUV)
GCSS_KEY (FORMAT_YUY2, YUY2)
GCSS_KEY (FORMAT_BA10, BA10)
GCSS_KEY (FORMAT_END, format_end)
/* add new keys to this section above this line */

/* NUMERICAL KEY DEFINITIONS */
GCSS_KEY_SECTION_START (NUMERICAL_START, num_start, 0x2000)
GCSS_KEY (ACTIVE_OUTPUTS, active_outputs)
GCSS_KEY (BINNING_H_FACTOR, h_factor)
GCSS_KEY (BINNING_V_FACTOR, v_factor)
GCSS_KEY (BOTTOM, bottom)
GCSS_KEY (BPP, bpp)
GCSS_KEY (BRANCH_POINT, branch_point)
GCSS_KEY (BYTES_PER_LINE, bpl)
GCSS_KEY (CONCURRENT, concurrent)
GCSS_KEY (DIRECTION, direction)
GCSS_KEY (DVS, dvs)
GCSS_KEY (ENABLED, enabled)
GCSS_KEY (EXEC_CTX_ID, exec_ctx_id)
GCSS_KEY (FPS, fps)
GCSS_KEY (FRAGMENT_COUNT, fragment_count)
GCSS_KEY (HEIGHT, height)
GCSS_KEY (ID, id)
GCSS_KEY (KEY, key)
GCSS_KEY (LEFT, left)
GCSS_KEY (OPMODE, operation_mode)
GCSS_KEY (PACKED, packed)
GCSS_KEY (PAL_UUID, pal_uuid)
GCSS_KEY (PG_ID, pg_id)
GCSS_KEY (PSYS_FREQ, psys_frequency)
GCSS_KEY (RCB, rcb)
GCSS_KEY (RESTORE_POINT, restore_point)
GCSS_KEY (RIGHT, right)
GCSS_KEY (SCALING_FACTOR_NUM, num_factor)
GCSS_KEY (SCALING_FACTOR_DENOM, denom_factor)
GCSS_KEY (SINK_PORT, sink_port)
GCSS_KEY (SOURCE_PORT, source_port)
GCSS_KEY (STREAM_ID, stream_id)
GCSS_KEY (TOP, top)
GCSS_KEY (TUNING_MODE, tuning_mode)
GCSS_KEY (VECTORIZED, vectorized)
GCSS_KEY (WIDTH, width)
GCSS_KEY (NUMERICAL_END, num_end)
/* add new keys to this section above this line */
