#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

"""Decodes the protobufs described in go/apollo-qa-tracing-design."""

import base64
import binascii
import struct

from acts.controllers.buds_lib.dev_utils.proto.gen import apollo_qa_pb2
from acts.controllers.buds_lib.dev_utils.proto.gen import audiowear_pb2


def to_dictionary(proto):
    proto_dic = {}
    msg = [element.split(':') for element in str(proto).split('\n') if element]
    for element in msg:
        key = element[0].strip()
        value = element[1].strip()
        proto_dic[key] = value
    return proto_dic


def is_automation_protobuf(logline):
    return logline.startswith('QA_MSG|')


def decode(logline):
    """Decode the logline.

    Args:
      logline: String line with the encoded message.

    Returns:
      String value with the decoded message.
    """
    decoded = None
    decoders = {'HEX': binascii.unhexlify, 'B64': base64.decodebytes}
    msgs = {
        apollo_qa_pb2.TRACE:
            apollo_qa_pb2.ApolloQATrace,
        apollo_qa_pb2.GET_VER_RESPONSE:
            apollo_qa_pb2.ApolloQAGetVerResponse,
        apollo_qa_pb2.GET_CODEC_RESPONSE:
            apollo_qa_pb2.ApolloQAGetCodecResponse,
        apollo_qa_pb2.GET_DSP_STATUS_RESPONSE:
            apollo_qa_pb2.ApolloQAGetDspStatusResponse,
    }

    if is_automation_protobuf(logline):
        _, encoding, message = logline.split("|", 2)
        message = message.rstrip()
        if encoding in decoders.keys():
            message = decoders[encoding](message)
            header = message[0:4]
            serialized = message[4:]
            if len(header) == 4 and len(serialized) == len(message) - 4:
                msg_group, msg_type, msg_len = struct.unpack('>BBH', header)
                if (len(serialized) == msg_len and
                        msg_group == audiowear_pb2.APOLLO_QA):
                    proto = msgs[msg_type]()
                    proto.ParseFromString(serialized)
                    decoded = to_dictionary(proto)
    return decoded
