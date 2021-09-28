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

#include <OpenGLESDispatch/GLESv1Dispatch.h>

#include "GLESv1.h"

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

// Stubs (ES 1.1)

static void glBindVertexArrayOES(GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glCurrentPaletteMatrixOES(GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glDeleteVertexArraysOES(GLsizei, const GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGenVertexArraysOES(GLsizei, GLuint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetBufferPointervOES(GLenum, GLenum, GLvoid**) {
    printf("%s: not implemented\n", __func__);
}

static void glGetTexGenfvOES(GLenum, GLenum, GLfloat*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetTexGenivOES(GLenum, GLenum, GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glGetTexGenxvOES(GLenum, GLenum, GLfixed*) {
    printf("%s: not implemented\n", __func__);
}

static GLboolean glIsVertexArrayOES(GLuint) {
    printf("%s: not implemented\n", __func__);
    return GL_FALSE;
}

static void glLoadPaletteFromModelViewMatrixOES() {
    printf("%s: not implemented\n", __func__);
}

static void glMatrixIndexPointerData(GLint, GLenum, GLsizei, void*, GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glMatrixIndexPointerOffset(GLint, GLenum, GLsizei, GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glMultiDrawArraysSUN(GLenum, GLint*, GLsizei*, GLsizei) {
    printf("%s: not implemented\n", __func__);
}

static void glMultiDrawElementsSUN(GLenum, const GLsizei*, GLenum, const GLvoid**, GLsizei) {
    printf("%s: not implemented\n", __func__);
}

static GLbitfield glQueryMatrixxOES(GLfixed*, GLint*) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static void glTexGenfOES(GLenum, GLenum, GLfloat) {
    printf("%s: not implemented\n", __func__);
}

static void glTexGenfvOES(GLenum, GLenum, const GLfloat*) {
    printf("%s: not implemented\n", __func__);
}

static void glTexGeniOES(GLenum, GLenum, GLint) {
    printf("%s: not implemented\n", __func__);
}

static void glTexGenivOES(GLenum, GLenum, const GLint*) {
    printf("%s: not implemented\n", __func__);
}

static void glTexGenxOES(GLenum, GLenum, GLfixed) {
    printf("%s: not implemented\n", __func__);
}

static void glTexGenxvOES(GLenum, GLenum, const GLfixed*) {
    printf("%s: not implemented\n", __func__);
}

static GLboolean glUnmapBufferOES(GLenum) {
    printf("%s: not implemented\n", __func__);
    return GL_FALSE;
}

static void glWeightPointerData(GLint, GLenum, GLsizei, void*, GLuint) {
    printf("%s: not implemented\n", __func__);
}

static void glWeightPointerOffset(GLint, GLenum, GLsizei, GLuint) {
    printf("%s: not implemented\n", __func__);
}

// Non-stubs (common)

static void glDrawElementsData(GLenum mode, GLsizei count, GLenum type, void* indices, GLuint) {
    s_gles1.glDrawElements(mode, count, type, indices);
}

static void glDrawElementsOffset(GLenum mode, GLsizei count, GLenum type, GLuint offset) {
    s_gles1.glDrawElements(mode, count, type, reinterpret_cast<const GLvoid*>(offset));
}

static GLint glFinishRoundTrip() {
    s_gles1.glFinish();
    return 0;
}

static void glGetCompressedTextureFormats(int count, GLint* formats) {
    int nFormats;
    s_gles1.glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &nFormats);
    if (nFormats <= count)
        s_gles1.glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats);
}

// Non-stubs (ES 1.1)

static void glColorPointerData(GLint size, GLenum type, GLsizei, void* data, GLuint) {
    s_gles1.glColorPointer(size, type, 0, data);
}

static void glColorPointerOffset(GLint size, GLenum type, GLsizei, GLuint offset) {
    s_gles1.glColorPointer(size, type, 0, reinterpret_cast<GLvoid*>(offset));
}

static void glNormalPointerData(GLenum type, GLsizei, void* data, GLuint) {
    s_gles1.glNormalPointer(type, 0, data);
}

static void glNormalPointerOffset(GLenum type, GLsizei, GLuint offset) {
    s_gles1.glNormalPointer(type, 0, reinterpret_cast<GLvoid*>(offset));
}

static void glPointSizePointerData(GLenum type, GLsizei, void* data, GLuint) {
    s_gles1.glPointSizePointerOES(type, 0, data);
}

static void glPointSizePointerOffset(GLenum type, GLsizei, GLuint offset) {
    s_gles1.glPointSizePointerOES(type, 0, reinterpret_cast<GLvoid*>(offset));
}

static void glTexCoordPointerData(GLint, GLint size, GLenum type, GLsizei, void* data, GLuint) {
    // FIXME: unit?
    s_gles1.glTexCoordPointer(size, type, 0, data);
}

static void glTexCoordPointerOffset(GLint size, GLenum type, GLsizei, GLuint offset) {
    s_gles1.glTexCoordPointer(size, type, 0, reinterpret_cast<GLvoid*>(offset));
}

static void glVertexPointerData(GLint size, GLenum type, GLsizei, void* data, GLuint) {
    s_gles1.glVertexPointer(size, type, 0, data);
}

static void glVertexPointerOffset(GLint size, GLenum type, GLsizei, GLuint offset) {
    s_gles1.glVertexPointer(size, type, 0, reinterpret_cast<GLvoid*>(offset));
}

#define KNIT(return_type, function_name, signature, callargs) function_name = s_gles1.function_name;

GLESv1::GLESv1() {
    LIST_GLES1_FUNCTIONS(KNIT, KNIT)

    // Remap some ES 1.0 extensions that become core in ES 1.1
    glAlphaFuncxOES = glAlphaFuncx;
    glClearColorxOES = glClearColorx;
    glClearDepthfOES = glClearDepthf;
    glClearDepthxOES = glClearDepthx;
    glClipPlanefIMG = glClipPlanef;
    glClipPlanefOES = glClipPlanef;
    glClipPlanexIMG = glClipPlanex;
    glClipPlanexOES = glClipPlanex;
    glColor4xOES = glColor4x;
    glDepthRangefOES = glDepthRangef;
    glDepthRangexOES = glDepthRangex;
    glFogxOES = glFogx;
    glFogxvOES = glFogxv;
    glFrustumfOES = glFrustumf;
    glFrustumxOES = glFrustumx;
    glGetClipPlanefOES = glGetClipPlanef;
    glGetClipPlanexOES = glGetClipPlanex;
    glGetFixedvOES = glGetFixedv;
    glGetLightxvOES = glGetLightxv;
    glGetMaterialxvOES = glGetMaterialxv;
    glGetTexEnvxvOES = glGetTexEnvxv;
    glGetTexParameterxvOES = glGetTexParameterxv;
    glLightModelxOES = glLightModelx;
    glLightModelxvOES = glLightModelxv;
    glLightxOES = glLightx;
    glLightxvOES = glLightxv;
    glLineWidthxOES = glLineWidthx;
    glLoadMatrixxOES = glLoadMatrixx;
    glMaterialxOES = glMaterialx;
    glMaterialxvOES = glMaterialxv;
    glMultiTexCoord4xOES = glMultiTexCoord4x;
    glMultMatrixxOES = glMultMatrixx;
    glNormal3xOES = glNormal3x;
    glOrthofOES = glOrthof;
    glOrthoxOES = glOrthox;
    glPointParameterxOES = glPointParameterx;
    glPointParameterxvOES = glPointParameterxv;
    glPointSizexOES = glPointSizex;
    glPolygonOffsetxOES = glPolygonOffsetx;
    glRotatexOES = glRotatex;
    glSampleCoveragexOES = glSampleCoveragex;
    glScalexOES = glScalex;
    glTexEnvxOES = glTexEnvx;
    glTexEnvxvOES = glTexEnvxv;
    glTexParameterxOES = glTexParameterx;
    glTexParameterxvOES = glTexParameterxv;
    glTranslatexOES = glTranslatex;

    // Entrypoints requiring custom wrappers (common)
    glDrawElementsData = ::glDrawElementsData;
    glDrawElementsOffset = ::glDrawElementsOffset;
    glFinishRoundTrip = ::glFinishRoundTrip;
    glGetCompressedTextureFormats = ::glGetCompressedTextureFormats;

    // Entrypoints requiring custom wrappers (ES 1.1)
    glColorPointerData = ::glColorPointerData;
    glColorPointerOffset = ::glColorPointerOffset;
    glNormalPointerData = ::glNormalPointerData;
    glNormalPointerOffset = ::glNormalPointerOffset;
    glPointSizePointerData = ::glPointSizePointerData;
    glPointSizePointerOffset = ::glPointSizePointerOffset;
    glTexCoordPointerData = ::glTexCoordPointerData;
    glTexCoordPointerOffset = ::glTexCoordPointerOffset;
    glVertexPointerData = ::glVertexPointerData;
    glVertexPointerOffset = ::glVertexPointerOffset;

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

    // Stub some extensions we will never implement (ES 1.1)
    glBindVertexArrayOES = ::glBindVertexArrayOES;
    glCurrentPaletteMatrixOES = ::glCurrentPaletteMatrixOES;
    glDeleteVertexArraysOES = ::glDeleteVertexArraysOES;
    glGenVertexArraysOES = ::glGenVertexArraysOES;
    glGetBufferPointervOES = ::glGetBufferPointervOES;
    glGetTexGenfvOES = ::glGetTexGenfvOES;
    glGetTexGenivOES = ::glGetTexGenivOES;
    glGetTexGenxvOES = ::glGetTexGenxvOES;
    glIsVertexArrayOES = ::glIsVertexArrayOES;
    glLoadPaletteFromModelViewMatrixOES = ::glLoadPaletteFromModelViewMatrixOES;
    glMatrixIndexPointerData = ::glMatrixIndexPointerData;
    glMatrixIndexPointerOffset = ::glMatrixIndexPointerOffset;
    glMultiDrawArraysSUN = ::glMultiDrawArraysSUN;
    glMultiDrawElementsSUN = ::glMultiDrawElementsSUN;
    glQueryMatrixxOES = ::glQueryMatrixxOES;
    glTexGenfOES = ::glTexGenfOES;
    glTexGenfvOES = ::glTexGenfvOES;
    glTexGeniOES = ::glTexGeniOES;
    glTexGenivOES = ::glTexGenivOES;
    glTexGenxOES = ::glTexGenxOES;
    glTexGenxvOES = ::glTexGenxvOES;
    glUnmapBufferOES = ::glUnmapBufferOES;
    glWeightPointerData = ::glWeightPointerData;
    glWeightPointerOffset = ::glWeightPointerOffset;
}
