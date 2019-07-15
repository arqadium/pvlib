#include "pv.h"

#define HEADER_MAGIC_SZ 8

static const char k_header_magic[HEADER_MAGIC_SZ] = {
	0x8A, 'P', 'V', 0, '\r', '\n', 0x1A, '\n'
}

#ifndef PV_NO_STDIO

int pv_fchksig( FILE* f )
{
	int i, r;
	char buf[0x16];

	if(!f) { return -2; }

	/* Make sure there are enough bytes for a header */
	r = fseek( f, 0x16, SEEK_SET );

	if(r) { return -3; }

	r = fseek( f, 0, SEEK_SET );

	if(r) { return -3; }

	r = fread( (void*)(buf), sizeof(char), 0x16, f );

	if(r < 0x16 && feof( f )) {
		return -1;
	}

	return pv_chksig( (void*)(buf) );
}

#endif /* PV_NO_STDIO */

int pv_chksig( void* b )
{
	char* c;

	c = (char*)(b);

	for(i = 0; i < 8; ++i) {
		if(c[i] != k_header_magic[i]) {
			return -1;
		}
	}

	float canvas_w, canvas_h;

	memcpy( &canvas_w, &(c[0x8]), 4 );
	memcpy( &canvas_h, &(c[0xC]), 4 );

	if(!canvas_w || !canvas_h) {
		return -1;
	}

	return 0;
}

struct stop
{
	float offs;
	unsigned col;
};

struct gradient
{
	float xform[6];
	float fx, fy;
	int is_radial;
	short stops_ct : 14;
	short spread : 2;
	struct stop* stops;
};

static int catalog_gradient( NSVGShape* sh, int is_radial, struct gradient** grads, size_t* grads_sz )
{
	struct gradient* g;
	size_t g_i;

	if(!sh || !gradient) {
		return -1;
	}

	/* Realloc array */
	g = realloc( *grads, (sizeof(struct gradient) * grads_sz) + 1 );

	if(!g) {
		return -2;
	}

	*grads = g;
	g_i = *grads_sz;
	*grads_sz++;

	/* Process the gradient */
}

#define TMPBUF_CPY_WRITE(OBJ) do { \
	memcpy( buf, (OBJ), 4); \
	r = fwrite( buf, sizeof(char), 4, f ); \
	if(r < 4) { return -2;} \
	} while(0)

int pv_fnsvg2pv( NSVGimage* svg, FILE* f )
{
	int r;
	char buf[4], opts;
	size_t i, shape_ct;
	NSVGshape* cur_shape;

	if(!svg || !f) {
		return -1;
	}

	r = fseek( f, 0, SEEK_SET );

	if(r) {
		return -2;
	}

	r = fwrite( k_header_magic, sizeof(char), HEADER_MAGIC_SZ, f );

	if(r < HEADER_MAGIC_SZ) {
		return -2;
	}

	TMPBUF_CPY_WRITE( &(svg->width));
	TMPBUF_CPY_WRITE( &(svg->height));

	shape_ct = 0;
	cur_shape = svg->shapes;

	while(cur_shape != NULL) {
		opts = 0;
		switch(cur_shape->fill.type) {
		case NSVG_PAINT_NONE:
			break;
		case NSVG_PAINT_LINEAR_GRADIENT:
		case NSVG_PAINT_RADIAL_GRADIENT:
			opts |= 1 << 4;
		case NSVG_PAINT_COLOR:
			opts |= 1;
			break;
		}
	}
}
