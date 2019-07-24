#include "pv.h"

#define HEADER_MAGIC_SZ 8

static const char k_header_magic[HEADER_MAGIC_SZ] = {
	0x8A, 'P', 'V', 0, '\r', '\n', 0x1A, '\n'
}

/* begin IEEE float16 impl */

#define PV_F16_SHIFT 13
#define PV_F16_SHIFT_SIGN 16

/* float32 infinity */
#define PV_F16_INFN 0x7F800000

/* max float16 normal as a float32 */
#define PV_F16_MAXN 0x477FE000

/* min float16 normal as a float32 */
#define PV_F16_MINN 0x38800000

/* float32 sign */
#define PV_F16_SIGNN 0x80000000

#define PV_F16_INFC (PV_F16_INFN >> PV_F16_SHIFT)
/* min float16 NaN as a float32 */
#define PV_F16_NANN ((PV_F16_INFC + 1) << PV_F16_SHIFT)
#define PV_F16_MAXC (PV_F16_MAXN >> PV_F16_SHIFT)
#define PV_F16_MINC (PV_F16_MINN >> PV_F16_SHIFT)
/* float16 sign */
#define PV_F16_SIGNC (PV_F16_SIGNN >> PV_F16_SHIFT_SIGN)

#define PV_F16_MULN 0x52000000
#define PV_F16_MULC 0x33800000
#define PV_F16_SUBC 0x3FF
#define PV_F16_NORC 0x400
#define PV_F16_MAXD (PV_F16_INFC - PV_F16_MAXC - 1)
#define PV_F16_MIND (PV_F16_MINC - PV_F16_SUBC - 1)

struct pv_float16_impl
{
	union
	{
		float f;
		int si;
		unsigned ui;
	};
};

static unsigned short pv_f16_32to16( float input )
{
	struct pv_float16_impl v, s;
	unsigned sign;

	v.f = input;
	sign = v.si & PV_F16_SIGNN;
	v.si ^= sign;
	sign >>= PV_F16_SHIFT_SIGN;
	s.si = PV_F16_MULN;
	s.si = s.f * v.f; /* correct subnormals */
	v.si ^= (s.si ^ v.si) & -(PV_F16_MINN > v.si);
	v.si ^= (PV_F16_INFN ^ v.si) & -((PV_F16_INFN > v.si)
		& (v.si > PV_F16_MAXN));
	v.si ^= (PV_F16_NANN ^ v.si) & -((PV_F16_NANN > v.si)
		& (v.si > PV_F16_INFN));
	v.ui >>= PV_F16_SHIFT;
	v.si ^= ((v.si - PV_F16_MAXD) ^ v.si) & -(v.si > PV_F16_MAXC);
	v.si ^= ((v.si - PV_F16_MIND) ^ v.si) & -(v.si > PV_F16_SUBC);

	return v.ui | sign;
}

static float pv_f16_16to32( unsigned short input )
{
	struct pv_float16_impl v, s;
	int sign, mask;

	v.ui = input;
	sign = v.si & PV_F16_SIGNC;
	v.si ^= sign;
	sign <<= PV_F16_SHIFT_SIGN;
	v.si ^= ((v.si + PV_F16_MIND) ^ v.si) & -(v.si > PV_F16_SUBC);
	v.si ^= ((v.si + PV_F16_MAXD) ^ v.si) & -(v.si > PV_F16_MAXC);

	s.si = PV_F16_MULC;
	s.f *= v.si;
	mask = -(PV_F16_NORC > v.si);
	v.si <<= PV_F16_SHIFT;
	v.si ^= (s.si ^ v.si) & mask;
	v.si |= sign;

	return v.f;
}

/* end IEEE float16 impl */

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

static int catalog_gradient( NSVGShape* sh, int is_fill, struct gradient** grads, size_t* grads_sz )
{
	struct gradient* g;
	size_t g_i, i;
	char typ;
	NSVGgradient* og;

	if(!sh || !gradient) {
		return -1;
	}

	/* TODO: Scan existing catalog for duplicates */

	/* Realloc array */
	g = realloc( *grads, (sizeof(struct gradient) * grads_sz) + 1 );

	if(!g) {
		return -2;
	}

	*grads = g;
	g_i = *grads_sz;
	*grads_sz++;

	/* Process the gradient */
	typ = is_fill ? sh->fill.type : sh->stroke.type;
	og = is_fill ? sh->fill.gradient : sh->stroke.gradient;

	for(i = 0; i < 6; ++i)
	{
		g[g_i].xform[i] = og->xform[i];
	}

	g[g_i].fx = og->fx;
	g[g_i].fy = og->fy;
	g[g_i].is_radial = typ == NSVG_PAINT_RADIAL_GRADIANT ? 1 : 0;

	if((og->spread & 0x3) != og->spread){
		return -3;
	}

	if((og->nstops & 0x3FFF) != og->nstops) {
		return -4;
	}

	g[g_i].spread = og->spread & 0x3;
	g[g_i].stops_ct = og->nstops & 0x3FFF;

	/* Allocate for the stops */
	g[g_i].stops = malloc( sizeof(struct stop) * g[g_i].stops_ct);

	if(!(g[g_i].stops)) {
		return -2;
	}

	for(i = 0; i < g[g_i].stops_ct; ++i) {
		g[g_i].stops[i].offs = og->stops[i].offset;
		g[g_i].stops[i].col = og->stops[i].color;
	}

	return 0;
}

#define TMPBUF_CPY_WRITE(OBJ, N) do { \
	memcpy( buf, (OBJ), N); \
	r = fwrite( buf, sizeof(char), N, f ); \
	if(r < N) { return -2;} \
	} while(0)

int pv_fnsvg2pv( NSVGimage* svg, FILE* f )
{
	int r;
	unsigned char buf[4], opts;
	unsigned i, shape_ct, grads_ct;
	NSVGshape* cur_shape;
	struct gradient* grads;

	grads = NULL;
	grads_ct = 0;

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

	TMPBUF_CPY_WRITE( &(svg->width), 4);
	TMPBUF_CPY_WRITE( &(svg->height),4);

	shape_ct = 0;
	cur_shape = svg->shapes;

	while(cur_shape != NULL) {
		long path_ct_pos, cur_pos;
		unsigned short miter;
		NSVGpath* cur_path;
		unsigned path_ct;

		opts = 0;
		shape_ct++;

		switch(cur_shape->fill.type) {
		case NSVG_PAINT_LINEAR_GRADIENT:
		case NSVG_PAINT_RADIAL_GRADIENT:
			opts |= 1 << 4;
			/* goto case; */
		case NSVG_PAINT_COLOR:
			opts |= 1;
			/* goto case; */
		case NSVG_PAINT_NONE:
			break;
		}

		switch(cur_shape->stroke.type) {
		case NSVG_PAINT_LINEAR_GRADIENT:
		case NSVG_PAINT_RADIAL_GRADIENT:
			opts |= 1 << 5;
			/* goto case; */
		case NSVG_PAINT_COLOR:
			opts |= 1 << 1;
			/* goto case; */
		case NSVG_PAINT_NONE:
			break;
		}

		if(cur_shape->opacity < 1.0f) {
			opts |= 1 << 3;
		}

		if(cur_shape->strokeDashOffset > 0 && cur_shape->strokeDashCount > 0)
		{
			opts |= 1 << 2;
		}

		opts |= ((cur_shape->fillRule & 1) << 6)
			| ((cur_shape->flags & NSVG_FLAGS_VISIBLE) ? 1 : 0) << 7;

#define WRITE_COLOURS(N) do { if(opts & (1 << N)) { \
			if(opts & (1 << (4 + N))) {\
				size_t grads_i;\
				grads_i = grads_ct;\
				r = catalog_gradient( cur_shape, N, &grads, &grads_ct);\
				if(r < 0) {\
					return -10 + r;\
				}\
				if(r == 0) {\
					TMPBUF_CPY_WRITE( &(grads_i), 2);\
					/* if r is positive, the gradient already exists */\
				}\
			} else {\
				NSVGpaint p = N == 1 ? cur_shape->fill : cur_shape->stroke;\
				buf[0] = p.color & 0xFF;\
				buf[1] = (p.color >> 8) & 0xFF;\
				buf[2] = (p.color >> 16) & 0xFF;\
				r = fwrite( buf, sizeof(char), 3, f );\
				if(r < 3) { return -2;}\
			}\
		} } while(0)

		WRITE_COLOURS(1); /* fill comes first */
		WRITE_COLOURS(0);

#undef WRITE_COLOURS

		if(opts & (1 << 3)) {
			/* Record opacity as 8-bit fixed point */
			buf[0] = (cur_shape->opacity * ((1 << 8) - 1));

			r = fwrite( buf, sizeof(char), 1, f);

			if(r < 1) { return -2;}
		}

		if(opts & (1 << 1)) {
			/* Record stroke properties */
			unsigned short stroke_w;
			unsigned char join_cap;

			stroke_w = pv_f16_32to16( cur_shape->strokeWidth );

			r = fwrite( &stroke_w, sizeof(char), 2, f );

			if(r < 2) { return -2;}

			if(opts & (1 << 2)) {
				/* Record dash characteristics */
				unsigned short stroke_dash_offs;
				unsigned char dash_ct;

				stroke_dash_offs = pv_f16_32to16( cur_shape->strokeDashOffset );

				r = fwrite( &stroke_dash_offs, sizeof(char), 2, f );

				if(r < 2) { return -2;}

				dash_ct = cur_shape->strokeDashCount;

				r = fwrite( &dash_ct, sizeof(char), 1, f );

				if(r < 1) { return -2;}

				for(i = 0; i < dash_ct; ++i) {
					unsigned short dash;

					dash = pv_f16_32to16( cur_shape->strokeDashArray[i]);

					r = fwrite( &dash, sizeof(char), 2, f);

					if(r < 2) { return -2; }
				}
			}

			/* Record line join and cap styling */
			join_cap = (cur_shape->strokeLineJoin & 0x3)
				| ((cur_shape->strokeLineCap & 0x3) << 2);

			r = fwrite( &join_cap, sizeof(char), 1, f);

			if(r < 1) { return -2;}
		}

		/* Record the miter limit */
		miter = pv_f16_32to16( cur_shape->miterLimit);

		r = fwrite( &miter, sizeof(char), 2, f);

		if(r < 2) { return -2;}

		/* Record shape bounds */
		for(i = 0; i < 4; ++i)
		{
			TMPBUF_CPY_WRITE(&(cur_shape->bounds[i]), 4);
		}

		/* Once all paths are written, we’ll come back to set this */
		path_ct_pos = ftell( f);

		r = fwrite( buf, sizeof(char), 4, f); /* buf is garbage for now */

		if(r < 4) { return -2;}

		/* Record all paths */
		path_ct = 0;
		cur_path = cur_shape->paths;

		while(cur_path != NULL)
		{
			unsigned elem_ct, npts;

			path_ct++;

			elem_ct = cur_path->npts < 0 ? 0 : cur_path->npts;
			npts = elem_ct; /* used as a count later, elem_ct is bitwised */
			elem_ct >>= 1; /* divide by 2 bitwise */
			elem_ct |= (cur_path->closed == 1 ? 1 : 0) << 31;

			r = fwrite( &elem_ct, sizeof(char), 4, f);

			if(r < 4) { return -2;}

			for(i = 0; i < 4; ++i)
			{
				r = fwrite( cur_path->bounds, sizeof(float), 4, f);

				if(r < 4) { return -2;}
			}

			/* Write the beziér */
			r = fwrite( cur_path->pts, sizeof(float), npts, f);

			if(r < npts) { return -2;}

			cur_path = cur_path->next;
		}

		/* Write in the number of paths now */
		cur_pos = ftell(f);

		r = fseek( f, path_ct_pos, SEEK_SET);

		if(r) { return -3;}

		r = fwrite( &path_ct, sizeof(char), 4, f);

		if(r < 4) { return -2;}

		r = fseek( f, cur_pos, SEEK_SET);

		if(r) { return -3;}

		cur_shape = cur_shape->next;
	}

	/* Record the number of shapes */
	r = fseek( f, 0x10, SEEK_SET);

	if(r) { return -3;}

	r = fwrite( &shape_ct, sizeof(char), 4, f);

	if(r < 4) { return -2;}

	/* Record the gradient table now */
	r = fwrite( &grads_ct, sizeof(char), 4, f);

	if(r < 4) { return -2;}

	for(i = 0; i < grads_ct; ++i) {
		unsigned short stops_b;

		r = fwrite( &(grads[i].xform), sizeof(float), 6, f);

		if(r < 6) { return -2;}

		r = fwrite( &(grads[i].fx), sizeof(float), 1, f);

		if(r < 1) { return -2;}

		r = fwrite( &(grads[i].fy), sizeof(float), 1, f);

		if(r < 1) { return -2;}

		stops_b = grads[i].stops_ct | (grads[i].spread << 14)

		r = fwrite( &stops_b, sizeof(char), 2, f);

		if(r < 2) { return -2;}

		/* TODO: Record all stops into a ready-to-write buffer */
	}
}
