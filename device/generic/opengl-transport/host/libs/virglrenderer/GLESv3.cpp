/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <OpenGLESDispatch/GLESv3Dispatch.h>

#include "GLESv3.h"

#include <string>
#include <vector>

// Stubs (common)

static void glDeleteFencesNV(GLsizei, const GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glDisableDriverControlQCOM(GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glDiscardFramebufferEXT(GLenum, GLsizei, const GLenum*) {
    printf("%s: not implemented\n", __func__);
}

static void glEnableDriverControlQCOM(GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glEndTilingQCOM(GLbitfield) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetBufferPointervQCOM(GLenum, GLvoid**) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetBuffersQCOM(GLuint*, GLint, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetFramebuffersQCOM(GLuint*, GLint, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetProgramBinarySourceQCOM(GLuint, GLenum, GLchar*, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetProgramsQCOM(GLuint*, GLint, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetRenderbuffersQCOM(GLuint*, GLint, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetShadersQCOM(GLuint*, GLint, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetTexLevelParameterivQCOM(GLuint, GLenum, GLint, GLenum, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetTexSubImageQCOM(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei,
                                    GLenum, GLenum, GLvoid*) {
    printf("%s: not implemented\n", __func__);
}

static void glExtGetTexturesQCOM(GLuint*, GLint, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static GLboolean glExtIsProgramBinaryQCOM(GLuint) {
    printf("%s: not implemented\n", __func__);
    return GL_FALSE;
}

static void glExtTexObjectStateOverrideiQCOM(GLenum, GLenum, GLint) {
    printf("%s: not implemented\n", __func__);
}

static void glFinishFenceNV(GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glFramebufferTexture2DMultisampleIMG(GLenum, GLenum, GLenum, GLuint, GLint, GLsizei) {
    printf("%s: not implemented\n", __func__);
}

static void glGenFencesNV(GLsizei, GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetDriverControlsQCOM(GLint*, GLsizei, GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetDriverControlStringQCOM(GLuint, GLsizei, GLsizei*, GLchar*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetFenceivNV(GLuint, GLenum, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static GLboolean glIsFenceNV(GLuint) {
    printf("%s: not implemented\n", __func__);
    return GL_FALSE;
}

static void* glMapBufferOES(GLenum, GLenum) {
    printf("%s: not implemented\n", __func__);
    return nullptr;
}

static void glMultiDrawArraysEXT(GLenum, const GLint*, const GLsizei*, GLsizei) {
    printf("%s: not implemented\n", __func__);
}

static void glMultiDrawElementsEXT(GLenum, const GLsizei*, GLenum, const GLvoid* const*, GLsizei) {
    printf("%s: not implemented\n", __func__);
}

static void glRenderbufferStorageMultisampleIMG(GLenum, GLsizei, GLenum, GLsizei, GLsizei) {
    printf("%s: not implemented\n", __func__);
}

static void glSetFenceNV(GLuint, GLenum) {
    printf("%s: not implemented\n", __func__);
}

static void glStartTilingQCOM(GLuint, GLuint, GLuint, GLuint, GLbitfield) {
    printf("%s: not implemented\n", __func__);
}

static GLboolean glTestFenceNV(GLuint) {
    printf("%s: not implemented\n", __func__);
    return GL_FALSE;
}

// Stubs (ES 3.1)

static void glBeginPerfMonitorAMD(GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glCoverageMaskNV(GLboolean) {
    printf("%s: not implemented\n", __func__);
}

static void glCoverageOperationNV(GLenum) {
    printf("%s: not implemented\n", __func__);
}

static void glDeletePerfMonitorsAMD(GLsizei, GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glEndPerfMonitorAMD(GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glGenPerfMonitorsAMD(GLsizei, GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetPerfMonitorCounterDataAMD(GLuint, GLenum, GLsizei, GLuint*, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetPerfMonitorCounterInfoAMD(GLuint, GLuint, GLenum, GLvoid*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetPerfMonitorCountersAMD(GLuint, GLint*, GLint*, GLsizei, GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetPerfMonitorCounterStringAMD(GLuint, GLuint, GLsizei, GLsizei*, GLchar*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetPerfMonitorGroupsAMD(GLint*, GLsizei, GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetPerfMonitorGroupStringAMD(GLuint, GLsizei, GLsizei*, GLchar*) {
    printf("%s: not implemented\n", __func__);
}

static void glSelectPerfMonitorCountersAMD(GLuint, GLboolean, GLuint, GLint, GLuint*) {
    printf("%s: not implemented\n", __func__);
}

// Non-stubs (common)

static void glDrawElementsData(GLenum mode, GLsizei count, GLenum type, void* indices, GLuint) {
    s_gles3.glDrawElements(mode, count, type, indices);
}

static void glDrawElementsOffset(GLenum mode, GLsizei count, GLenum type, GLuint offset) {
    s_gles3.glDrawElements(mode, count, type, reinterpret_cast<const GLvoid*>(offset));
}

static GLint glFinishRoundTrip() {
    s_gles3.glFinish();
    return 0;
}

static void glGetCompressedTextureFormats(int count, GLint* formats) {
    int nFormats;
    s_gles3.glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &nFormats);
    if (nFormats <= count)
        s_gles3.glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats);
}

// Non-stubs (ES 3.1)

struct GlSync {
    GlSync(GLESv3* ctx_, GLsync sync_) : sync(sync_), id(ctx_->sync_nextId++), ctx(ctx_) {
        ctx->sync_map.emplace(id, this);
    }

    ~GlSync() {
        ctx->sync_map.erase(id);
    }

    GLsync sync;
    uint64_t id;

  private:
    GLESv3* ctx;
};

static GLenum glClientWaitSyncAEMU(void* ctx_, uint64_t wait_on, GLbitfield flags,
                                   GLuint64 timeout) {
    GLESv3* ctx = static_cast<GLESv3*>(ctx_);

    std::map<uint64_t, GlSync*>::iterator it;
    it = ctx->sync_map.find(wait_on);
    if (it == ctx->sync_map.end())
        return GL_INVALID_VALUE;

    GlSync* sync = it->second;
    return s_gles3.glClientWaitSync(sync->sync, flags, timeout);
}

static void glCompressedTexImage2DOffsetAEMU(GLenum target, GLint level, GLenum internalformat,
                                             GLsizei width, GLsizei height, GLint border,
                                             GLsizei imageSize, GLuint offset) {
    s_gles3.glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize,
                                   reinterpret_cast<const GLvoid*>(offset));
}

static void glCompressedTexImage3DOffsetAEMU(GLenum target, GLint level, GLenum internalformat,
                                             GLsizei width, GLsizei height, GLsizei depth,
                                             GLint border, GLsizei imageSize, GLuint offset) {
    s_gles3.glCompressedTexImage3D(target, level, internalformat, width, height, depth, border,
                                   imageSize, reinterpret_cast<const GLvoid*>(offset));
}

static void glCompressedTexSubImage2DOffsetAEMU(GLenum target, GLint level, GLint xoffset,
                                                GLint yoffset, GLsizei width, GLsizei height,
                                                GLenum format, GLsizei imageSize, GLuint offset) {
    s_gles3.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                      imageSize, reinterpret_cast<const GLvoid*>(offset));
}

static void glCompressedTexSubImage3DOffsetAEMU(GLenum target, GLint level, GLint xoffset,
                                                GLint yoffset, GLint zoffset, GLsizei width,
                                                GLsizei height, GLsizei depth, GLenum format,
                                                GLsizei imageSize, GLuint offset) {
    s_gles3.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth,
                                      format, imageSize, reinterpret_cast<const GLvoid*>(offset));
}

static GLuint glCreateShaderProgramvAEMU(GLenum type, GLsizei, const char* packedStrings, GLuint) {
    return s_gles3.glCreateShaderProgramv(type, 1, &packedStrings);
}

static void glDeleteSyncAEMU(void* ctx_, uint64_t to_delete) {
    GLESv3* ctx = static_cast<GLESv3*>(ctx_);

    std::map<uint64_t, GlSync*>::iterator it;
    it = ctx->sync_map.find(to_delete);
    if (it == ctx->sync_map.end())
        return;

    GlSync* sync = it->second;
    s_gles3.glDeleteSync(sync->sync);
    delete sync;
}

static void glDrawArraysIndirectDataAEMU(GLenum mode, const void* indirect, GLuint) {
    s_gles3.glDrawArraysIndirect(mode, indirect);
}

static void glDrawArraysIndirectOffsetAEMU(GLenum mode, GLuint offset) {
    s_gles3.glDrawArraysIndirect(mode, reinterpret_cast<const void*>(offset));
}

static void glDrawElementsIndirectDataAEMU(GLenum mode, GLenum type, const void* indirect, GLuint) {
    s_gles3.glDrawElementsIndirect(mode, type, indirect);
}

static void glDrawElementsIndirectOffsetAEMU(GLenum mode, GLenum type, GLuint offset) {
    s_gles3.glDrawElementsIndirect(mode, type, reinterpret_cast<const void*>(offset));
}

static void glDrawElementsInstancedDataAEMU(GLenum mode, GLsizei count, GLenum type,
                                            const void* indices, GLsizei primcount, GLsizei) {
    s_gles3.glDrawElementsInstanced(mode, count, type, indices, primcount);
}

static void glDrawElementsInstancedOffsetAEMU(GLenum mode, GLsizei count, GLenum type,
                                              GLuint offset, GLsizei primcount) {
    s_gles3.glDrawElementsInstanced(mode, count, type, reinterpret_cast<const void*>(offset),
                                    primcount);
}

static void glDrawRangeElementsDataAEMU(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                        GLenum type, const GLvoid* indices, GLsizei) {
    s_gles3.glDrawRangeElements(mode, start, end, count, type, indices);
}

static void glDrawRangeElementsOffsetAEMU(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                          GLenum type, GLuint offset) {
    s_gles3.glDrawRangeElements(mode, start, end, count, type,
                                reinterpret_cast<const GLvoid*>(offset));
}

static uint64_t glFenceSyncAEMU(void* ctx_, GLenum condition, GLbitfield flags) {
    GLsync sync_ = s_gles3.glFenceSync(condition, flags);
    if (sync_ == 0)
        return 0U;

    GLESv3* ctx = static_cast<GLESv3*>(ctx_);
    GlSync* sync = new (std::nothrow) GlSync(ctx, sync_);
    if (!sync) {
        s_gles3.glDeleteSync(sync_);
        return 0U;
    }

    return sync->id;
}

static void glFlushMappedBufferRangeAEMU(GLenum target, GLintptr offset, GLsizeiptr length,
                                         GLbitfield access, void* guest_buffer) {
    if (guest_buffer && length) {
        void* gpuPtr = s_gles3.glMapBufferRange(target, offset, length, access);
        if (gpuPtr) {
            memcpy(gpuPtr, guest_buffer, length);
            s_gles3.glFlushMappedBufferRange(target, 0, length);
            s_gles3.glUnmapBuffer(target);
        }
    }
}

static void glGetSyncivAEMU(void* ctx_, uint64_t sync_, GLenum pname, GLsizei bufSize,
                            GLsizei* length, GLint* values) {
    GLESv3* ctx = static_cast<GLESv3*>(ctx_);

    std::map<uint64_t, GlSync*>::iterator it;
    it = ctx->sync_map.find(sync_);
    if (it == ctx->sync_map.end())
        return;

    GlSync* sync = it->second;
    s_gles3.glGetSynciv(sync->sync, pname, bufSize, length, values);
}

static std::vector<std::string> sUnpackVarNames(GLsizei count, const char* packedNames) {
    std::vector<std::string> unpacked;
    GLsizei current = 0;

    while (current < count) {
        const char* delimPos = strstr(packedNames, ";");
        size_t nameLen = delimPos - packedNames;
        std::string next;
        next.resize(nameLen);
        memcpy(&next[0], packedNames, nameLen);
        unpacked.push_back(next);
        packedNames = delimPos + 1;
        current++;
    }

    return unpacked;
}

static void glGetUniformIndicesAEMU(GLuint program, GLsizei uniformCount, const GLchar* packedNames,
                                    GLsizei packedLen, GLuint* uniformIndices) {
    std::vector<std::string> unpacked = sUnpackVarNames(uniformCount, packedNames);
    GLchar** unpackedArray = new GLchar*[unpacked.size()];
    GLsizei i = 0;
    for (auto& elt : unpacked) {
        unpackedArray[i] = (GLchar*)&elt[0];
        i++;
    }

    s_gles3.glGetUniformIndices(program, uniformCount, const_cast<const GLchar**>(unpackedArray),
                                uniformIndices);
    delete[] unpackedArray;
}

static GLboolean glIsSyncAEMU(void* ctx_, uint64_t sync) {
    GLESv3* ctx = static_cast<GLESv3*>(ctx_);
    return ctx->sync_map.count(sync) ? GL_TRUE : GL_FALSE;
}

static void glMapBufferRangeAEMU(GLenum target, GLintptr offset, GLsizeiptr length,
                                 GLbitfield access, void* mapped) {
    if ((access & GL_MAP_READ_BIT) ||
        ((access & GL_MAP_WRITE_BIT) &&
         (!(access & GL_MAP_INVALIDATE_RANGE_BIT) && !(access & GL_MAP_INVALIDATE_BUFFER_BIT)))) {
        void* gpuPtr = s_gles3.glMapBufferRange(target, offset, length, access);
        if (gpuPtr) {
            if (mapped)
                memcpy(mapped, gpuPtr, length);
            s_gles3.glUnmapBuffer(target);
        }
    }
}

static void glReadPixelsOffsetAEMU(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                                   GLenum type, GLuint offset) {
    s_gles3.glReadPixels(x, y, width, height, format, type, reinterpret_cast<GLvoid*>(offset));
}

static void glShaderString(GLuint shader, const GLchar* string, GLsizei) {
    s_gles3.glShaderSource(shader, 1, &string, NULL);
}

static void glTexImage2DOffsetAEMU(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                   GLsizei height, GLint border, GLenum format, GLenum type,
                                   GLuint offset) {
    s_gles3.glTexImage2D(target, level, internalformat, width, height, border, format, type,
                         reinterpret_cast<const GLvoid*>(offset));
}

static void glTexImage3DOffsetAEMU(GLenum target, GLint level, GLint internalFormat, GLsizei width,
                                   GLsizei height, GLsizei depth, GLint border, GLenum format,
                                   GLenum type, GLuint offset) {
    s_gles3.glTexImage3D(target, level, internalFormat, width, height, depth, border, format, type,
                         reinterpret_cast<const GLvoid*>(offset));
}

static void glTexSubImage2DOffsetAEMU(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                      GLsizei width, GLsizei height, GLenum format, GLenum type,
                                      GLuint offset) {
    s_gles3.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
                            reinterpret_cast<const GLvoid*>(offset));
}

static void glTexSubImage3DOffsetAEMU(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                      GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                      GLenum format, GLenum type, GLuint offset) {
    s_gles3.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                            type, reinterpret_cast<const GLvoid*>(offset));
}

static void glTransformFeedbackVaryingsAEMU(GLuint program, GLsizei count,
                                            const char* packedVaryings, GLuint packedVaryingsLen,
                                            GLenum bufferMode) {
    std::vector<std::string> unpacked = sUnpackVarNames(count, packedVaryings);
    char** unpackedArray = new char*[unpacked.size()];
    GLsizei i = 0;
    for (auto& elt : unpacked) {
        unpackedArray[i] = &elt[0];
        i++;
    }

    s_gles3.glTransformFeedbackVaryings(program, count, const_cast<const char**>(unpackedArray),
                                        bufferMode);
    delete[] unpackedArray;
}

static void glUnmapBufferAEMU(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access,
                              void* guest_buffer, GLboolean* out_res) {
    *out_res = GL_TRUE;

    if (access & GL_MAP_WRITE_BIT) {
        if (guest_buffer) {
            void* gpuPtr = s_gles3.glMapBufferRange(target, offset, length, access);
            if (gpuPtr)
                memcpy(gpuPtr, guest_buffer, length);
        }

        *out_res = s_gles3.glUnmapBuffer(target);
    }
}

static void glVertexAttribIPointerDataAEMU(GLuint index, GLint size, GLenum type, GLsizei,
                                           void* data, GLuint) {
    s_gles3.glVertexAttribIPointer(index, size, type, 0, data);
}

static void glVertexAttribIPointerOffsetAEMU(GLuint index, GLint size, GLenum type, GLsizei,
                                             GLuint offset) {
    s_gles3.glVertexAttribIPointer(index, size, type, 0, reinterpret_cast<const GLvoid*>(offset));
}

static void glVertexAttribPointerData(GLuint indx, GLint size, GLenum type, GLboolean normalized,
                                      GLsizei, void* data, GLuint) {
    s_gles3.glVertexAttribPointer(indx, size, type, normalized, 0, data);
}

static void glVertexAttribPointerOffset(GLuint indx, GLint size, GLenum type, GLboolean normalized,
                                        GLsizei, GLuint offset) {
    s_gles3.glVertexAttribPointer(indx, size, type, normalized, 0,
                                  reinterpret_cast<const GLvoid*>(offset));
}

static void glWaitSyncAEMU(void* ctx_, uint64_t wait_on, GLbitfield flags, GLuint64 timeout) {
    GLESv3* ctx = static_cast<GLESv3*>(ctx_);

    std::map<uint64_t, GlSync*>::iterator it;
    it = ctx->sync_map.find(wait_on);
    if (it == ctx->sync_map.end())
        return;

    GlSync* sync = it->second;
    s_gles3.glWaitSync(sync->sync, flags, timeout);
}

#define KNIT(return_type, function_name, signature, callargs) function_name = s_gles3.function_name;

GLESv3::GLESv3() {
    LIST_GLES3_FUNCTIONS(KNIT, KNIT)

    // Remap some ES 2.0 extensions that become core in ES 3.1
    glBindVertexArrayOES = glBindVertexArray;
    glDeleteVertexArraysOES = glDeleteVertexArrays;
    glGenVertexArraysOES = glGenVertexArrays;
    glGetProgramBinaryOES = glGetProgramBinary;
    glIsVertexArrayOES = glIsVertexArray;
    glProgramBinaryOES = glProgramBinary;
    glUnmapBufferOES = glUnmapBuffer;

    // Entrypoints requiring custom wrappers (common)
    glDrawElementsData = ::glDrawElementsData;
    glDrawElementsOffset = ::glDrawElementsOffset;
    glFinishRoundTrip = ::glFinishRoundTrip;
    glGetCompressedTextureFormats = ::glGetCompressedTextureFormats;

    // Entrypoints requiring custom wrappers (ES 3.1)
    glClientWaitSyncAEMU = ::glClientWaitSyncAEMU;
    glCompressedTexImage2DOffsetAEMU = ::glCompressedTexImage2DOffsetAEMU;
    glCompressedTexImage3DOffsetAEMU = ::glCompressedTexImage3DOffsetAEMU;
    glCompressedTexSubImage2DOffsetAEMU = ::glCompressedTexSubImage2DOffsetAEMU;
    glCompressedTexSubImage3DOffsetAEMU = ::glCompressedTexSubImage3DOffsetAEMU;
    glCreateShaderProgramvAEMU = ::glCreateShaderProgramvAEMU;
    glDeleteSyncAEMU = ::glDeleteSyncAEMU;
    glDrawArraysIndirectDataAEMU = ::glDrawArraysIndirectDataAEMU;
    glDrawArraysIndirectOffsetAEMU = ::glDrawArraysIndirectOffsetAEMU;
    glDrawElementsIndirectDataAEMU = ::glDrawElementsIndirectDataAEMU;
    glDrawElementsIndirectOffsetAEMU = ::glDrawElementsIndirectOffsetAEMU;
    glDrawElementsInstancedDataAEMU = ::glDrawElementsInstancedDataAEMU;
    glDrawElementsInstancedOffsetAEMU = ::glDrawElementsInstancedOffsetAEMU;
    glDrawRangeElementsDataAEMU = ::glDrawRangeElementsDataAEMU;
    glDrawRangeElementsOffsetAEMU = ::glDrawRangeElementsOffsetAEMU;
    glFenceSyncAEMU = ::glFenceSyncAEMU;
    glFlushMappedBufferRangeAEMU = ::glFlushMappedBufferRangeAEMU;
    glGetSyncivAEMU = ::glGetSyncivAEMU;
    glGetUniformIndicesAEMU = ::glGetUniformIndicesAEMU;
    glIsSyncAEMU = ::glIsSyncAEMU;
    glMapBufferRangeAEMU = ::glMapBufferRangeAEMU;
    glReadPixelsOffsetAEMU = ::glReadPixelsOffsetAEMU;
    glShaderString = ::glShaderString;
    glTexImage2DOffsetAEMU = ::glTexImage2DOffsetAEMU;
    glTexImage3DOffsetAEMU = ::glTexImage3DOffsetAEMU;
    glTexSubImage2DOffsetAEMU = ::glTexSubImage2DOffsetAEMU;
    glTexSubImage3DOffsetAEMU = ::glTexSubImage3DOffsetAEMU;
    glTransformFeedbackVaryingsAEMU = ::glTransformFeedbackVaryingsAEMU;
    glUnmapBufferAEMU = ::glUnmapBufferAEMU;
    glVertexAttribIPointerDataAEMU = ::glVertexAttribIPointerDataAEMU;
    glVertexAttribIPointerOffsetAEMU = ::glVertexAttribIPointerOffsetAEMU;
    glVertexAttribPointerData = ::glVertexAttribPointerData;
    glVertexAttribPointerOffset = ::glVertexAttribPointerOffset;
    glWaitSyncAEMU = ::glWaitSyncAEMU;

    // Stub some extensions we will never implement (common)
    glDeleteFencesNV = ::glDeleteFencesNV;
    glDisableDriverControlQCOM = ::glDisableDriverControlQCOM;
    glDiscardFramebufferEXT = ::glDiscardFramebufferEXT;
    glEnableDriverControlQCOM = ::glEnableDriverControlQCOM;
    glEndTilingQCOM = ::glEndTilingQCOM;
    glExtGetBufferPointervQCOM = ::glExtGetBufferPointervQCOM;
    glExtGetBuffersQCOM = ::glExtGetBuffersQCOM;
    glExtGetFramebuffersQCOM = ::glExtGetFramebuffersQCOM;
    glExtGetProgramBinarySourceQCOM = ::glExtGetProgramBinarySourceQCOM;
    glExtGetProgramsQCOM = ::glExtGetProgramsQCOM;
    glExtGetRenderbuffersQCOM = ::glExtGetRenderbuffersQCOM;
    glExtGetShadersQCOM = ::glExtGetShadersQCOM;
    glExtGetTexLevelParameterivQCOM = ::glExtGetTexLevelParameterivQCOM;
    glExtGetTexSubImageQCOM = ::glExtGetTexSubImageQCOM;
    glExtGetTexturesQCOM = ::glExtGetTexturesQCOM;
    glExtIsProgramBinaryQCOM = ::glExtIsProgramBinaryQCOM;
    glExtTexObjectStateOverrideiQCOM = ::glExtTexObjectStateOverrideiQCOM;
    glFinishFenceNV = ::glFinishFenceNV;
    glFramebufferTexture2DMultisampleIMG = ::glFramebufferTexture2DMultisampleIMG;
    glGenFencesNV = ::glGenFencesNV;
    glGetDriverControlsQCOM = ::glGetDriverControlsQCOM;
    glGetDriverControlStringQCOM = ::glGetDriverControlStringQCOM;
    glGetFenceivNV = ::glGetFenceivNV;
    glIsFenceNV = ::glIsFenceNV;
    glMapBufferOES = ::glMapBufferOES;
    glMultiDrawArraysEXT = ::glMultiDrawArraysEXT;
    glMultiDrawElementsEXT = ::glMultiDrawElementsEXT;
    glRenderbufferStorageMultisampleIMG = ::glRenderbufferStorageMultisampleIMG;
    glSetFenceNV = ::glSetFenceNV;
    glStartTilingQCOM = ::glStartTilingQCOM;
    glTestFenceNV = ::glTestFenceNV;

    // Stub some extensions we will never implement (ES 3.1)
    glBeginPerfMonitorAMD = ::glBeginPerfMonitorAMD;
    glCoverageMaskNV = ::glCoverageMaskNV;
    glCoverageOperationNV = ::glCoverageOperationNV;
    glDeletePerfMonitorsAMD = ::glDeletePerfMonitorsAMD;
    glEndPerfMonitorAMD = ::glEndPerfMonitorAMD;
    glGenPerfMonitorsAMD = ::glGenPerfMonitorsAMD;
    glGetPerfMonitorCounterDataAMD = ::glGetPerfMonitorCounterDataAMD;
    glGetPerfMonitorCounterInfoAMD = ::glGetPerfMonitorCounterInfoAMD;
    glGetPerfMonitorCountersAMD = ::glGetPerfMonitorCountersAMD;
    glGetPerfMonitorCounterStringAMD = ::glGetPerfMonitorCounterStringAMD;
    glGetPerfMonitorGroupsAMD = ::glGetPerfMonitorGroupsAMD;
    glGetPerfMonitorGroupStringAMD = ::glGetPerfMonitorGroupStringAMD;
    glSelectPerfMonitorCountersAMD = ::glSelectPerfMonitorCountersAMD;
}
