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

/**
 * PackVector binary file format
 * Offs | Size | Description
 * -----+------+-------------
 * 0x00 | 0x01 | const 0x8A (high bit a la PNG)
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
 * the beginning of each shape field is an 8-bit sentinel, with a collection
 * of flags marking the presence or absence of various fields. if a flag is
 * absent, it is discluded entirely (100% compression). the conditions of each
 * field’s inclusion are annotated in brackets.
 *
 * this format primarily serialises struct fields from the corresponding
 * nanoSVG header. IMPLEMENTORS: please read through the top parts of
 * nanosvg.h as well.
 *
 * Shape format
 * Offs | Size | Description
 * -----+------+-------------
 * 0x00 | 0x01 | bitfield of present fields (bit6 = fill rule, bit7 = visible)
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
 * .... | .... | paths
 *
 * Path format
 * Offs | Size | Description
 * -----+------+-------------
 * 0x00 | 0x04 | Number of elements / 2 (bits 0-30), is closed (bit 31)
 * 0x04 | 0x10 | float32[4], tight bounding box of path
 * 0x14 | .... | Array of cubic bezier elements, either xy coord or a curve;
 *      |      | first element is a coord (all float32)
 *      |      | coord is 2 numbers, curve is 4
 *
 * Gradient format
 * Offs | Size | Description
 * -----+------+-------------
 * 0x00 | 0x18 | float32[6]: xform
 * 0x18 | 0x08 | float32[2]: fx, fy
 * 0x20 | 0x02 | uint16: number of stops (bits 0-13), spread type (bits 14-15)
 * 0x22 | .... | (stops): { float16 offset, uint24 colour } (sizeof == 5)
 *      |      | offset is relative to previous stop
 */

#include "nanosvg.h"

#ifndef PV_NO_STDIO
PVLIB_API int pv_fchksig( FILE* );
PVLIB_API int pv_fpv2nsvg( FILE*, NSVGimage* );
PVLIB_API int pv_fnsvg2pv( NSVGimage*, FILE* );
#endif

PVLIB_API int pv_chksig( void* );
PVLIB_API int pv_pv2nsvg( void*, size_t, NSVGimage* );
PVLIB_API int pv_nsvg2pv( NSVGimage*, void*, size_t* );

#endif /* INC__PVLIB_PV_H */
