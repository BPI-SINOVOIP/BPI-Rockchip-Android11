/*
 * Copyright 2015 The Android Open Source Project
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
 *
 */

#include <gatekeeper/gatekeeper_messages.h>

#include <string.h>

namespace gatekeeper {

/**
 * Methods for serializing/deserializing SizedBuffers
 */

struct __attribute__((__packed__)) serial_header_t {
    uint32_t error;
    uint32_t user_id;
};

static inline bool fitsBuffer(const uint8_t* begin, const uint8_t* end, uint32_t field_size) {
    uintptr_t dummy;
    return !__builtin_add_overflow(reinterpret_cast<uintptr_t>(begin), field_size, &dummy)
            && dummy <= reinterpret_cast<uintptr_t>(end);
}

static inline uint32_t serialized_buffer_size(const SizedBuffer &buf) {
    return sizeof(decltype(buf.size())) + buf.size();
}

static inline void append_to_buffer(uint8_t **buffer, const SizedBuffer &to_append) {
    uint32_t length = to_append.size();
    memcpy(*buffer, &length, sizeof(length));
    *buffer += sizeof(length);
    if (length != 0 && to_append.Data<uint8_t>() != nullptr) {
        memcpy(*buffer, to_append.Data<uint8_t>(), length);
        *buffer += length;
    }
}

static inline gatekeeper_error_t read_from_buffer(const uint8_t **buffer, const uint8_t *end,
        SizedBuffer *target) {
    if (target == nullptr) return ERROR_INVALID;
    if (!fitsBuffer(*buffer, end, sizeof(uint32_t))) return ERROR_INVALID;

    // read length from incomming buffer
    uint32_t length;
    memcpy(&length, *buffer, sizeof(length));
    // advance out buffer
    *buffer += sizeof(length);

    if (length == 0) {
        *target = {};
    } else {
        // sanitize incoming buffer size
        if (!fitsBuffer(*buffer, end, length)) return ERROR_INVALID;

        uint8_t *target_buffer = new(std::nothrow) uint8_t[length];
        if (target_buffer == nullptr) return ERROR_MEMORY_ALLOCATION_FAILED;

        memcpy(target_buffer, *buffer, length);
        *buffer += length;
        *target = { target_buffer, length };
    }
    return ERROR_NONE;
}


uint32_t GateKeeperMessage::GetSerializedSize() const {
    if (error == ERROR_NONE) {
        uint32_t size = sizeof(serial_header_t) + nonErrorSerializedSize();
        return size;
    } else {
        uint32_t size = sizeof(serial_header_t);
        if (error == ERROR_RETRY) {
            size += sizeof(retry_timeout);
        }
        return size;
    }
}

uint32_t GateKeeperMessage::Serialize(uint8_t *buffer, const uint8_t *end) const {
    uint32_t bytes_written = 0;
    if (!fitsBuffer(buffer, end, GetSerializedSize())) {
        return 0;
    }

    serial_header_t *header = reinterpret_cast<serial_header_t *>(buffer);
    if (!fitsBuffer(buffer, end, sizeof(serial_header_t))) return 0;
    header->error = error;
    header->user_id = user_id;
    bytes_written += sizeof(*header);
    buffer += sizeof(*header);
    if (error == ERROR_RETRY) {
        if (!fitsBuffer(buffer, end, sizeof(retry_timeout))) return 0;
        memcpy(buffer, &retry_timeout, sizeof(retry_timeout));
        bytes_written  += sizeof(retry_timeout);
    } else if (error == ERROR_NONE) {
        uint32_t serialized_size = nonErrorSerializedSize();
        if (!fitsBuffer(buffer, end, serialized_size)) return 0;
        nonErrorSerialize(buffer);
        bytes_written += serialized_size;
    }
    return bytes_written;
}

gatekeeper_error_t GateKeeperMessage::Deserialize(const uint8_t *payload, const uint8_t *end) {
    if (!fitsBuffer(payload, end, sizeof(serial_header_t))) return ERROR_INVALID;
    const serial_header_t *header = reinterpret_cast<const serial_header_t *>(payload);
    error = static_cast<gatekeeper_error_t>(header->error);
    user_id = header->user_id;
    payload += sizeof(*header);
    if (error == ERROR_NONE) {
        return nonErrorDeserialize(payload, end);
    } else {
        retry_timeout = 0;
        if (error == ERROR_RETRY) {
            if (!fitsBuffer(payload, end, sizeof(retry_timeout))) {
                return ERROR_INVALID;
            }
            memcpy(&retry_timeout, payload, sizeof(retry_timeout));
        }
    }

    return ERROR_NONE;
}

void GateKeeperMessage::SetRetryTimeout(uint32_t retry_timeout) {
    this->retry_timeout = retry_timeout;
    this->error = ERROR_RETRY;
}

VerifyRequest::VerifyRequest(uint32_t user_id, uint64_t challenge,
        SizedBuffer enrolled_password_handle, SizedBuffer provided_password_payload) {
    this->user_id = user_id;
    this->challenge = challenge;
    this->password_handle = move(enrolled_password_handle);
    this->provided_password = move(provided_password_payload);
}

uint32_t VerifyRequest::nonErrorSerializedSize() const {
    return sizeof(challenge) + serialized_buffer_size(password_handle)
            + serialized_buffer_size(provided_password);
}

void VerifyRequest::nonErrorSerialize(uint8_t *buffer) const {
    memcpy(buffer, &challenge, sizeof(challenge));
    buffer += sizeof(challenge);
    append_to_buffer(&buffer, password_handle);
    append_to_buffer(&buffer, provided_password);
}

gatekeeper_error_t VerifyRequest::nonErrorDeserialize(const uint8_t *payload, const uint8_t *end) {
    gatekeeper_error_t error = ERROR_NONE;

    password_handle = {};
    provided_password = {};

    if (!fitsBuffer(payload, end, sizeof(challenge))) return ERROR_INVALID;

    memcpy(&challenge, payload, sizeof(challenge));
    payload += sizeof(challenge);

    error = read_from_buffer(&payload, end, &password_handle);
    if (error != ERROR_NONE) return error;

    return read_from_buffer(&payload, end, &provided_password);

}

VerifyResponse::VerifyResponse(uint32_t user_id, SizedBuffer auth_token) {
    this->user_id = user_id;
    this->auth_token = move(auth_token);
    this->request_reenroll = false;
}

VerifyResponse::VerifyResponse() {
    request_reenroll = false;
};

void VerifyResponse::SetVerificationToken(SizedBuffer auth_token) {
    this->auth_token = move(auth_token);
}

uint32_t VerifyResponse::nonErrorSerializedSize() const {
    return serialized_buffer_size(auth_token) + sizeof(request_reenroll);
}

void VerifyResponse::nonErrorSerialize(uint8_t *buffer) const {
    append_to_buffer(&buffer, auth_token);
    memcpy(buffer, &request_reenroll, sizeof(request_reenroll));
}

gatekeeper_error_t VerifyResponse::nonErrorDeserialize(const uint8_t *payload, const uint8_t *end) {

    auth_token = {};

    gatekeeper_error_t err = read_from_buffer(&payload, end, &auth_token);
    if (err != ERROR_NONE) {
        return err;
    }

    if (!fitsBuffer(payload, end, sizeof(request_reenroll))) return ERROR_INVALID;
    memcpy(&request_reenroll, payload, sizeof(request_reenroll));
    return ERROR_NONE;
}

EnrollRequest::EnrollRequest(uint32_t user_id, SizedBuffer password_handle,
        SizedBuffer provided_password,  SizedBuffer enrolled_password) {
    this->user_id = user_id;

    this->provided_password = move(provided_password);
    this->enrolled_password = move(enrolled_password);
    this->password_handle = move(password_handle);
}

uint32_t EnrollRequest::nonErrorSerializedSize() const {
   return serialized_buffer_size(provided_password) + serialized_buffer_size(enrolled_password)
       + serialized_buffer_size(password_handle);
}

void EnrollRequest::nonErrorSerialize(uint8_t *buffer) const {
    append_to_buffer(&buffer, provided_password);
    append_to_buffer(&buffer, enrolled_password);
    append_to_buffer(&buffer, password_handle);
}

gatekeeper_error_t EnrollRequest::nonErrorDeserialize(const uint8_t *payload, const uint8_t *end) {
    gatekeeper_error_t ret;

    provided_password = {};
    enrolled_password = {};
    password_handle = {};

     ret = read_from_buffer(&payload, end, &provided_password);
     if (ret != ERROR_NONE) {
         return ret;
     }

     ret = read_from_buffer(&payload, end, &enrolled_password);
     if (ret != ERROR_NONE) {
         return ret;
     }

     return read_from_buffer(&payload, end, &password_handle);
}

EnrollResponse::EnrollResponse(uint32_t user_id, SizedBuffer enrolled_password_handle) {
    this->user_id = user_id;
    this->enrolled_password_handle = move(enrolled_password_handle);
}

void EnrollResponse::SetEnrolledPasswordHandle(SizedBuffer enrolled_password_handle) {
    this->enrolled_password_handle = move(enrolled_password_handle);
}

uint32_t EnrollResponse::nonErrorSerializedSize() const {
    return serialized_buffer_size(enrolled_password_handle);
}

void EnrollResponse::nonErrorSerialize(uint8_t *buffer) const {
    append_to_buffer(&buffer, enrolled_password_handle);
}

gatekeeper_error_t EnrollResponse::nonErrorDeserialize(const uint8_t *payload, const uint8_t *end) {
    enrolled_password_handle = {};

    return read_from_buffer(&payload, end, &enrolled_password_handle);
}

};

