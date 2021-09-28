#ifndef GLES_TEXTURE_UTILS_H
#define GLES_TEXTURE_UTILS_H

#include <GLES3/gl31.h>

namespace GLESTextureUtils {

void computeTextureStartEnd(
        GLsizei width, GLsizei height, GLsizei depth,
        GLenum format, GLenum type,
        int unpackAlignment,
        int unpackRowLength,
        int unpackImageHeight,
        int unpackSkipPixels,
        int unpackSkipRows,
        int unpackSkipImages,
        int* start,
        int* end);

int computeTotalImageSize(
        GLsizei width, GLsizei height, GLsizei depth,
        GLenum format, GLenum type,
        int unpackAlignment,
        int unpackRowLength,
        int unpackImageHeight,
        int unpackSkipPixels,
        int unpackSkipRows,
        int unpackSkipImages);

int computeNeededBufferSize(
        GLsizei width, GLsizei height, GLsizei depth,
        GLenum format, GLenum type,
        int unpackAlignment,
        int unpackRowLength,
        int unpackImageHeight,
        int unpackSkipPixels,
        int unpackSkipRows,
        int unpackSkipImages);

// Writes out |height| offsets for glReadPixels to read back
// data in separate rows of pixels. Returns:
// 1. |startOffset|: offset in bytes to apply at the beginning
// 2. |packingPixelRowSize|: the buffer size in bytes that has the actual pixels per row.
// 2. |packingTotalRowSize|: the length in bytes of each row including the padding from row length.
void computePackingOffsets2D(
        GLsizei width, GLsizei height,
        GLenum format, GLenum type,
        int packAlignment,
        int packRowLength,
        int packSkipPixels,
        int packSkipRows,
        int* bpp,
        int* startOffset,
        int* packingPixelRowSize,
        int* packingTotalRowSize);

// For processing 3D textures exactly to the sizes of client buffers.
void computePackingOffsets3D(
        GLsizei width, GLsizei height, GLsizei depth,
        GLenum format, GLenum type,
        int packAlignment,
        int packRowLength,
        int packImageHeight,
        int packSkipPixels,
        int packSkipRows,
        int packSkipImages,
        int* bpp,
        int* startOffset,
        int* packingPixelRowSize,
        int* packingTotalRowSize,
        int* packingPixelImageSize,
        int* packingTotalImageSize);

} // namespace GLESTextureUtils
#endif
