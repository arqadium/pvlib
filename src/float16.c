#include "float16.h"

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

#define PV_F16_INFC ( PV_F16_INFN >> PV_F16_SHIFT )
/* min float16 NaN as a float32 */
#define PV_F16_NANN ( ( PV_F16_INFC + 1 ) << PV_F16_SHIFT )
#define PV_F16_MAXC ( PV_F16_MAXN >> PV_F16_SHIFT )
#define PV_F16_MINC ( PV_F16_MINN >> PV_F16_SHIFT )
/* float16 sign */
#define PV_F16_SIGNC ( PV_F16_SIGNN >> PV_F16_SHIFT_SIGN )

#define PV_F16_MULN 0x52000000
#define PV_F16_MULC 0x33800000
#define PV_F16_SUBC 0x3FF
#define PV_F16_NORC 0x400
#define PV_F16_MAXD ( PV_F16_INFC - PV_F16_MAXC - 1 )
#define PV_F16_MIND ( PV_F16_MINC - PV_F16_SUBC - 1 )

struct pv_float16_impl
{
	union
	{
		float f;
		int si;
		unsigned ui;
	};
};

unsigned short pv_f16_32to16( float input )
{
	struct pv_float16_impl v, s;
	unsigned sign;

	v.f  = input;
	sign = v.si & PV_F16_SIGNN;
	v.si ^= sign;
	sign >>= PV_F16_SHIFT_SIGN;
	s.si = PV_F16_MULN;
	s.si = s.f * v.f; /* correct subnormals */
	v.si ^= ( s.si ^ v.si ) & -( PV_F16_MINN > v.si );
	v.si ^= ( PV_F16_INFN ^ v.si ) &
		-( ( PV_F16_INFN > v.si ) & ( v.si > PV_F16_MAXN ) );
	v.si ^= ( PV_F16_NANN ^ v.si ) &
		-( ( PV_F16_NANN > v.si ) & ( v.si > PV_F16_INFN ) );
	v.ui >>= PV_F16_SHIFT;
	v.si ^= ( ( v.si - PV_F16_MAXD ) ^ v.si ) & -( v.si > PV_F16_MAXC );
	v.si ^= ( ( v.si - PV_F16_MIND ) ^ v.si ) & -( v.si > PV_F16_SUBC );

	return v.ui | sign;
}

float pv_f16_16to32( unsigned short input )
{
	struct pv_float16_impl v, s;
	int sign, mask;

	v.ui = input;
	sign = v.si & PV_F16_SIGNC;
	v.si ^= sign;
	sign <<= PV_F16_SHIFT_SIGN;
	v.si ^= ( ( v.si + PV_F16_MIND ) ^ v.si ) & -( v.si > PV_F16_SUBC );
	v.si ^= ( ( v.si + PV_F16_MAXD ) ^ v.si ) & -( v.si > PV_F16_MAXC );

	s.si = PV_F16_MULC;
	s.f *= v.si;
	mask = -( PV_F16_NORC > v.si );
	v.si <<= PV_F16_SHIFT;
	v.si ^= ( s.si ^ v.si ) & mask;
	v.si |= sign;

	return v.f;
}
