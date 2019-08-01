/*
 * PACK VECTOR LIBRARY – “pvlib”, libpv.{a,so}
 * Copyright © 2019 Alexander Nicholi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * -----
 *
 * this format primarily serialises struct fields from the corresponding
 * nanoSVG header. IMPLEMENTORS: please read through the top parts of
 * nanosvg.h as well.
 *
 * -----
 *
 * FILE FORMAT.
 *
 * Offs | Size | Description
 * -----+------+-------------
 * 0x00 | 0x01 | const 0x8A (high bit set a la PNG)
 * 0x01 | 0x02 | const ASCII("PV")
 * 0x03 | 0x01 | const 0x00 (version code)
 * 0x04 | 0x02 | const ASCII("\r\n")
 * 0x06 | 0x01 | const ASCII(EOF)
 * 0x07 | 0x01 | const ASCII("\n")
 * 0x08 | 0x04 | float32 (canvas width)
 * 0x0C | 0x04 | float32 (canvas height)
 * 0x10 | 0x04 | number of shapes (uint32)
 * 0x14 | 0x02 | number of gradients (uint16)
 * 0x16 | .... | (shapes)
 * .... | .... | (gradients)
 *
 * -----
 *
 * SHAPE FORMAT. an image is composed of a collection of shapes, encoded as
 * a vectoresque array in the pv file format, ordered ascending by z-index
 * (topmost shapes come last).
 *
 * Offs | Size | Description
 * -----+------+-------------
 * 0x00 | 0x01 | field sentinel (see below)
 * .... | 0x03 | fill colour (RRGGBB) [if bit0 set & bit4 clear]
 * .... | 0x02 | fill gradient ID [if bit0 set & bit4 set]
 * .... | 0x03 | stroke colour (RRGGBB) [if bit1 set & bit5 clear]
 * .... | 0x02 | stroke gradient ID [if bit1 set & bit5 set]
 * .... | 0x01 | opacity (0-255) [if bit3 set] [100% if clear]
 * .... | 0x02 | float16: stroke width [if bit1 set]
 * .... | 0x02 | float16: stroke dash offset [if bit1 & bit2 set]
 * .... | 0x01 | stroke dash array length [if bit1 & bit2 set]
 * .... | .... | stroke dash array (float16[length]) [if bit1 & bit2 set]
 * .... | 0x01 | stroke join type (bits 0-1), cap type (bits 2-3) [if bit1 set]
 * .... | 0x02 | float16: miter limit
 * .... | 0x10 | float32[4], tight bounding box of shape
 * .... | 0x04 | path count (uint32)
 * .... | .... | (paths)
 *
 * the bounding box field is a tuple containing (min_x, min_y, max_x, max_y),
 * which specifies the boundaries of the shape to be rendered.
 *
 * -----
 *
 * FIELD SENTINEL. each shape field has a 1-byte (8-bit) bitfield at offset 0
 * of its structure, the bits of which dictate which fields are present:
 *
 * Bit | Description
 * ----+-------------
 *  0  | shape has a fill
 *  1  | shape has a stroke
 *  2  | shape stroke is dashed
 *  3  | shape has less than 100% opacity
 *  4  | fill content is gradient
 *  5  | stroke content is gradient
 *  6  | (from `enum NSVGfillRule`) fill rule == even odd
 *  7  | unused, must be zero
 *
 * if a bit is one, the field is present, and it is encoded in the structural
 * order set above. if the bit is zero, the field is absent, and it is skipped
 * over entirely to whatever fields may be next. see the bracketed notes for
 * each field on which bits would be set or clear for a field to be included.
 *
 * -----
 *
 * PATH FORMAT. each shape’s composition is specified with an array of paths.
 * each path in the array follows this format:
 *
 * Offs | Size | Description
 * -----+------+-------------
 * 0x00 | 0x04 | Number of elements / 2 (bits 0-30), is closed (bit 31)
 * 0x04 | 0x10 | float32[4], tight bounding box of path
 * 0x14 | .... | Array of cubic bezier elements, either xy coord or a curve;
 *      |      | first element is a coord (all float32)
 *      |      | coord is 2 numbers, curve is 4
 *
 * the array format begins with an (x, y) coordinate pair of where to begin
 * drawing the path. this is followed by a bézier curve quad (four values)
 * that encode the line drawn, followed by an endpoint coordinate pair, then
 * the next bézier curve, and so on. since the array length is always even,
 * one bit in the array size is taken to note whether the path is closed or
 * not, encoding the size as half of its real value.
 *
 * -----
 *
 * GRADIENT FORMAT. pv files have an array of gradients used in shapes, the
 * indices of which are referenced in the shape structures. they follow this
 * format:
 *
 * Offs | Size | Description
 * -----+------+-------------
 * 0x00 | 0x18 | float32[6]: xform
 * 0x18 | 0x08 | float32[2]: fx, fy
 * 0x20 | 0x02 | uint16: number of stops (bits 0-13), spread type (bits 14-15)
 * 0x22 | .... | (stops): { float16 offset, uint24 colour } (sizeof == 5)
 *      |      | offset is relative to previous stop
 *
 * the xform field combines the shape transform and the inverse of the
 * gradient transform, so that given a coordinate in screen space (provided by
 * fx and fy), you can compute a point in gradient space with the formulae:
 *
 * gx = fx * xform[0] + fy * xform[2] + xform[4];
 * gy = fx * xform[1] + fy * xform[3] + xform[5];
 *
 * let (fx, fy) be a position in screen space,
 *     (gx, gy) be a position in gradient space.
 *
 * for a linear gradient, the first point is at (0, 0) and the last is at
 * (0, 1). for a radial gradient, the centre is at (0, 0) and the radius is at
 * 1.
 */

#ifndef INC__PVLIB_PV_H
#define INC__PVLIB_PV_H

#ifndef PV_NO_STDIO
#include <stdio.h>
#endif /* PV_NO_STDIO */

#include <stddef.h> /* size_t */

#define PVLIB_VERSION_MAJOR 0
#define PVLIB_VERSION_MINOR 0
#define PVLIB_VERSION_PATCH 0

#ifdef __cplusplus
#define PVLIB_API extern "C"
#else
#define PVLIB_API extern
#endif /* __cplusplus */

#include "nanosvg.h"

#ifndef PV_NO_STDIO

/**
 * @brief Check signature by file
 * @param f A reference to a standard library FILE object, opened in read mode
 * @return Zero on success, nonzero otherwise
 */
PVLIB_API int pv_fchksig( FILE* );

/**
 * @brief Convert PV file to NSVGimage
 * @param f A reference to a standard library FILE object, opened in read mode
 * @param i A reference to a valid NSVGimage struct to output the data into
 * @return Zero on success, nonzero otherwise
 */
PVLIB_API int pv_fpv2nsvg( FILE*, struct NSVGimage* );

/**
 * @brief Convert NSVGimage to PV file
 * @param i A reference to a valid NSVGimage struct to read the data from
 * @param f A reference to a standard library FILE object, opened in write
 *          mode
 * @return Zero on success, nonzero otherwise
 */
PVLIB_API int pv_fnsvg2pv( struct NSVGimage*, FILE* );

#endif /* PV_NO_STDIO */

/**
 * @brief Check signature by buffer
 * @param b A reference to a buffer in memory at least 16 bytes large
 * @return Zero on success, nonzero otherwise
 */
PVLIB_API int pv_chksig( void* );

/**
 * @brief Convert PV buffer to NSVGimage
 * @param b A reference to a buffer in memory, the size of which is not less
 *          than the value provided in @a s
 * @param s The size of the input memory buffer, in bytes
 * @param i A reference to a valid NSVGimage struct to output the data into
 * @return Zero on success, nonzero otherwise
 */
PVLIB_API int pv_pv2nsvg( void*, size_t, struct NSVGimage* );

/**
 * @brief Convert NSVGimage to PV buffer
 * @param i A reference to a valid NSVGimage struct to read the data from
 * @param b A reference to a buffer in memory, the size of which is not less
 *          than the value provided in @a s
 * @param s The size of the output memory buffer, in bytes
 * @return Zero on success, nonzero otherwise
 */
PVLIB_API int pv_nsvg2pv( struct NSVGimage*, void*, size_t* );

#endif /* INC__PVLIB_PV_H */
