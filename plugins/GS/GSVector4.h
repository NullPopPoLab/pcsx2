/*
 *	Copyright (C) 2007-2017 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "Pcsx2Types.h"

#include "stdafx.h"

class alignas(16) GSVector4
{
public:
	union
	{
		struct {float x, y, z, w;};
		struct {float r, g, b, a;};
		struct {float left, top, right, bottom;};
		float v[4];
		float F32[4];
		s8    I8[16];
		s16   I16[8];
		s32   I32[4];
		s64   I64[2];
		u8    U8[16];
		u16   U16[8];
		u32   U32[4];
		u64   U64[2];
		__m128 m;
	};

	static GSVector4 m_ps0123;
	static GSVector4 m_ps4567;
	static GSVector4 m_half;
	static GSVector4 m_one;
	static GSVector4 m_two;
	static GSVector4 m_four;
	static GSVector4 m_x4b000000;
	static GSVector4 m_x4f800000;
	static GSVector4 m_max;
	static GSVector4 m_min;

	static void InitVectors();

	__forceinline GSVector4()
	{
	}

	constexpr GSVector4(const GSVector4&) = default;

	__forceinline GSVector4(float x, float y, float z, float w)
	{
		m = _mm_set_ps(w, z, y, x);
	}

	__forceinline GSVector4(float x, float y)
	{
		m = _mm_unpacklo_ps(_mm_load_ss(&x), _mm_load_ss(&y));
	}

	__forceinline GSVector4(int x, int y, int z, int w)
	{
		GSVector4i v(x, y, z, w);

		m = _mm_cvtepi32_ps(v.m);
	}

	__forceinline GSVector4(int x, int y)
	{
		m = _mm_cvtepi32_ps(_mm_unpacklo_epi32(_mm_cvtsi32_si128(x), _mm_cvtsi32_si128(y)));
	}

	__forceinline explicit GSVector4(const GSVector2& v)
	{
		m = _mm_castsi128_ps(_mm_loadl_epi64((__m128i*)&v));
	}

	__forceinline explicit GSVector4(const GSVector2i& v)
	{
		m = _mm_cvtepi32_ps(_mm_loadl_epi64((__m128i*)&v));
	}

	__forceinline explicit GSVector4(__m128 m)
	{
		this->m = m;
	}

	__forceinline explicit GSVector4(float f)
	{
		*this = f;
	}

	__forceinline explicit GSVector4(int i)
	{
		#if _M_SSE >= 0x501

		m = _mm_cvtepi32_ps(_mm_broadcastd_epi32(_mm_cvtsi32_si128(i)));

		#else

		GSVector4i v((int)i);

		*this = GSVector4(v);

		#endif
	}
	
	__forceinline explicit GSVector4(u32 u)
	{
		GSVector4i v((int)u);

		*this = GSVector4(v) + (m_x4f800000 & GSVector4::cast(v.sra32(31)));
	}

	__forceinline explicit GSVector4(const GSVector4i& v);

	__forceinline static GSVector4 cast(const GSVector4i& v);

	#if _M_SSE >= 0x500

	__forceinline static GSVector4 cast(const GSVector8& v);

	#endif

	#if _M_SSE >= 0x501

	__forceinline static GSVector4 cast(const GSVector8i& v);

	#endif

	__forceinline void operator = (const GSVector4& v)
	{
		m = v.m;
	}

	__forceinline void operator = (float f)
	{
		#if _M_SSE >= 0x501

		m =  _mm_broadcastss_ps(_mm_load_ss(&f));

		#else

		m = _mm_set1_ps(f);

		#endif
	}

	__forceinline void operator = (__m128 m)
	{
		this->m = m;
	}

	__forceinline operator __m128() const
	{
		return m;
	}

	__forceinline u32 rgba32() const
	{
		return GSVector4i(*this).rgba32();
	}

	__forceinline static GSVector4 rgba32(u32 rgba)
	{
		return GSVector4(GSVector4i::load((int)rgba).u8to32());
	}

	__forceinline static GSVector4 rgba32(u32 rgba, int shift)
	{
		return GSVector4(GSVector4i::load((int)rgba).u8to32() << shift);
	}

	__forceinline GSVector4 abs() const
	{
		return *this & cast(GSVector4i::x7fffffff());
	}

	__forceinline GSVector4 neg() const
	{
		return *this ^ cast(GSVector4i::x80000000());
	}

	__forceinline GSVector4 rcp() const
	{
		return GSVector4(_mm_rcp_ps(m));
	}

	__forceinline GSVector4 rcpnr() const
	{
		GSVector4 v = rcp();

		return (v + v) - (v * v) * *this;
	}

	template<int mode> __forceinline GSVector4 round() const
	{
		#if _M_SSE >= 0x401

		return GSVector4(_mm_round_ps(m, mode));

		#else

		GSVector4 a = *this;

		GSVector4 b = (a & cast(GSVector4i::x80000000())) | m_x4b000000;

		b = a + b - b;

		if((mode & 7) == (Round_NegInf & 7))
		{
			return b - ((a < b) & m_one);
		}

		if((mode & 7) == (Round_PosInf & 7))
		{
			return b + ((a > b) & m_one);
		}

		ASSERT((mode & 7) == (Round_NearestInt & 7)); // other modes aren't implemented

		return b;

		#endif
	}

	__forceinline GSVector4 floor() const
	{
		return round<Round_NegInf>();
	}

	__forceinline GSVector4 ceil() const
	{
		return round<Round_PosInf>();
	}

	// http://jrfonseca.blogspot.com/2008/09/fast-sse2-pow-tables-or-polynomials.html

	#define LOG_POLY0(x, c0) GSVector4(c0)
	#define LOG_POLY1(x, c0, c1) (LOG_POLY0(x, c1).madd(x, GSVector4(c0)))
	#define LOG_POLY2(x, c0, c1, c2) (LOG_POLY1(x, c1, c2).madd(x, GSVector4(c0)))
	#define LOG_POLY3(x, c0, c1, c2, c3) (LOG_POLY2(x, c1, c2, c3).madd(x, GSVector4(c0)))
	#define LOG_POLY4(x, c0, c1, c2, c3, c4) (LOG_POLY3(x, c1, c2, c3, c4).madd(x, GSVector4(c0)))
	#define LOG_POLY5(x, c0, c1, c2, c3, c4, c5) (LOG_POLY4(x, c1, c2, c3, c4, c5).madd(x, GSVector4(c0)))

	__forceinline GSVector4 log2(int precision = 5) const
	{
		// NOTE: sign bit ignored, safe to pass negative numbers

		// The idea behind this algorithm is to split the float into two parts, log2(m * 2^e) => log2(m) + log2(2^e) => log2(m) + e, 
		// and then approximate the logarithm of the mantissa (it's 1.x when normalized, a nice short range).

		GSVector4 one = m_one;

		GSVector4i i = GSVector4i::cast(*this);

		GSVector4 e = GSVector4(((i << 1) >> 24) - GSVector4i::x0000007f());
		GSVector4 m = GSVector4::cast((i << 9) >> 9) | one;

		GSVector4 p;

		// Minimax polynomial fit of log2(x)/(x - 1), for x in range [1, 2[

		switch(precision)
		{
		case 3:
			p = LOG_POLY2(m, 2.28330284476918490682f, -1.04913055217340124191f, 0.204446009836232697516f);
			break;
		case 4:
			p = LOG_POLY3(m, 2.61761038894603480148f, -1.75647175389045657003f, 0.688243882994381274313f, -0.107254423828329604454f);
			break;
		default:
		case 5:
			p = LOG_POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);
			break;
		case 6:
			p = LOG_POLY5(m, 3.1157899f, -3.3241990f, 2.5988452f, -1.2315303f,  3.1821337e-1f, -3.4436006e-2f);
			break;
		}

		// This effectively increases the polynomial degree by one, but ensures that log2(1) == 0

		p = p * (m - one);

		return p + e;
	}

	__forceinline GSVector4 madd(const GSVector4& a, const GSVector4& b) const
	{
		return *this * a + b;
	}

	__forceinline GSVector4 msub(const GSVector4& a, const GSVector4& b) const
	{
		return *this * a - b;
	}

	__forceinline GSVector4 nmadd(const GSVector4& a, const GSVector4& b) const
	{
		return b - *this * a;
	}

	__forceinline GSVector4 nmsub(const GSVector4& a, const GSVector4& b) const
	{
		return -b - *this * a;
	}

	__forceinline GSVector4 addm(const GSVector4& a, const GSVector4& b) const
	{
		return a.madd(b, *this); // *this + a * b
	}

	__forceinline GSVector4 subm(const GSVector4& a, const GSVector4& b) const
	{
		return a.nmadd(b, *this); // *this - a * b
	}

	__forceinline GSVector4 hadd() const
	{
		#if _M_SSE >= 0x300
		
		return GSVector4(_mm_hadd_ps(m, m));
		
		#else
		
		return xzxz() + ywyw();
		
		#endif
	}

	__forceinline GSVector4 hadd(const GSVector4& v) const
	{
		#if _M_SSE >= 0x300
		
		return GSVector4(_mm_hadd_ps(m, v.m));
		
		#else
		
		return xzxz(v) + ywyw(v);
		
		#endif
	}

	__forceinline GSVector4 hsub() const
	{
		#if _M_SSE >= 0x300
		
		return GSVector4(_mm_hsub_ps(m, m));
		
		#else
		
		return xzxz() - ywyw();
		
		#endif
	}

	__forceinline GSVector4 hsub(const GSVector4& v) const
	{
		#if _M_SSE >= 0x300
		
		return GSVector4(_mm_hsub_ps(m, v.m));
		
		#else
		
		return xzxz(v) - ywyw(v);

		#endif
	}

	#if _M_SSE >= 0x401

	template<int i> __forceinline GSVector4 dp(const GSVector4& v) const
	{
		return GSVector4(_mm_dp_ps(m, v.m, i));
	}

	#endif

	__forceinline GSVector4 sat(const GSVector4& a, const GSVector4& b) const
	{
		return GSVector4(_mm_min_ps(_mm_max_ps(m, a), b));
	}

	__forceinline GSVector4 sat(const GSVector4& a) const
	{
		return GSVector4(_mm_min_ps(_mm_max_ps(m, a.xyxy()), a.zwzw()));
	}

	__forceinline GSVector4 sat(const float scale = 255) const
	{
		return sat(zero(), GSVector4(scale));
	}

	__forceinline GSVector4 clamp(const float scale = 255) const
	{
		return min(GSVector4(scale));
	}

	__forceinline GSVector4 min(const GSVector4& a) const
	{
		return GSVector4(_mm_min_ps(m, a));
	}

	__forceinline GSVector4 max(const GSVector4& a) const
	{
		return GSVector4(_mm_max_ps(m, a));
	}

	#if _M_SSE >= 0x401

	template<int mask> __forceinline GSVector4 blend32(const GSVector4& a)  const
	{
		return GSVector4(_mm_blend_ps(m, a, mask));
	}

	#endif

	__forceinline GSVector4 blend32(const GSVector4& a, const GSVector4& mask)  const
	{
		#if _M_SSE >= 0x401

		return GSVector4(_mm_blendv_ps(m, a, mask));

		#else

		return GSVector4(_mm_or_ps(_mm_andnot_ps(mask, m), _mm_and_ps(mask, a)));

		#endif
	}

	__forceinline GSVector4 upl(const GSVector4& a) const
	{
		return GSVector4(_mm_unpacklo_ps(m, a));
	}

	__forceinline GSVector4 uph(const GSVector4& a) const
	{
		return GSVector4(_mm_unpackhi_ps(m, a));
	}

	__forceinline GSVector4 upld(const GSVector4& a) const
	{
		return GSVector4(_mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(m), _mm_castps_pd(a.m))));
	}

	__forceinline GSVector4 uphd(const GSVector4& a) const
	{
		return GSVector4(_mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(m), _mm_castps_pd(a.m))));
	}

	__forceinline GSVector4 l2h(const GSVector4& a) const
	{
		return GSVector4(_mm_movelh_ps(m, a));
	}

	__forceinline GSVector4 h2l(const GSVector4& a) const
	{
		return GSVector4(_mm_movehl_ps(m, a));
	}

	__forceinline GSVector4 andnot(const GSVector4& v) const
	{
		return GSVector4(_mm_andnot_ps(v.m, m));
	}

	__forceinline int mask() const
	{
		return _mm_movemask_ps(m);
	}

	__forceinline bool alltrue() const
	{
		return mask() == 0xf;
	}

	__forceinline bool allfalse() const
	{
		#if _M_SSE >= 0x500

		return _mm_testz_ps(m, m) != 0;

		#elif _M_SSE >= 0x401

		__m128i a = _mm_castps_si128(m);

		return _mm_testz_si128(a, a) != 0;

		#else

		return mask() == 0;

		#endif
	}

	__forceinline GSVector4 replace_nan(const GSVector4& v) const
	{
		return v.blend32(*this, *this == *this);
	}

	template<int src, int dst> __forceinline GSVector4 insert32(const GSVector4& v) const
	{
		switch(dst)
		{
			case 0:
				switch(src)
				{
					case 0:
						return yyxx(v).zxzw(*this);
					case 1:
						return yyyy(v).zxzw(*this);
					case 2:
						return yyzz(v).zxzw(*this);
					case 3:
						return yyww(v).zxzw(*this);
					default:
						__assume(0);
				}
				break;
			case 1:
				switch(src)
				{
					case 0: return xxxx(v).xzzw(*this);
					case 1: return xxyy(v).xzzw(*this);
					case 2: return xxzz(v).xzzw(*this);
					case 3: return xxww(v).xzzw(*this);
					default:
						__assume(0);
				}
				break;
			case 2:
				switch(src)
				{
					case 0: return xyzx(wwxx(v));
					case 1: return xyzx(wwyy(v));
					case 2: return xyzx(wwzz(v));
					case 3: return xyzx(wwww(v));
					default:
						__assume(0);
				}
				break;
			case 3:
				switch(src)
				{
					case 0: return xyxz(zzxx(v));
					case 1: return xyxz(zzyy(v));
					case 2: return xyxz(zzzz(v));
					case 3: return xyxz(zzww(v));
					default:
						__assume(0);
				}
				break;
			default:
				__assume(0);
		}
	}

	template<int i> __forceinline int extract32() const
	{
#if _M_SSE >= 0x401
		return _mm_extract_ps(m, i);
#else
		return I32[i];
#endif
	}

	__forceinline static GSVector4 zero()
	{
		return GSVector4(_mm_setzero_ps());
	}

	__forceinline static GSVector4 xffffffff()
	{
		return zero() == zero();
	}

	__forceinline static GSVector4 ps0123()
	{
		return GSVector4(m_ps0123);
	}

	__forceinline static GSVector4 ps4567()
	{
		return GSVector4(m_ps4567);
	}

	__forceinline static GSVector4 loadl(const void* p)
	{
		return GSVector4(_mm_castpd_ps(_mm_load_sd((double*)p)));
	}

	__forceinline static GSVector4 load(float f)
	{
		return GSVector4(_mm_load_ss(&f));
	}

	__forceinline static GSVector4 load(u32 u)
	{
		GSVector4i v = GSVector4i::load((int)u);

		return GSVector4(v) + (m_x4f800000 & GSVector4::cast(v.sra32(31)));
	}

	template<bool aligned> __forceinline static GSVector4 load(const void* p)
	{
		return GSVector4(aligned ? _mm_load_ps((const float*)p) : _mm_loadu_ps((const float*)p));
	}

	__forceinline static void storent(void* p, const GSVector4& v)
	{
		_mm_stream_ps((float*)p, v.m);
	}

	__forceinline static void storel(void* p, const GSVector4& v)
	{
		_mm_store_sd((double*)p, _mm_castps_pd(v.m));
	}

	__forceinline static void storeh(void* p, const GSVector4& v)
	{
		_mm_storeh_pd((double*)p, _mm_castps_pd(v.m));
	}

	template<bool aligned> __forceinline static void store(void* p, const GSVector4& v)
	{
		if(aligned) _mm_store_ps((float*)p, v.m);
		else _mm_storeu_ps((float*)p, v.m);
	}

	__forceinline static void store(float* p, const GSVector4& v)
	{
		_mm_store_ss(p, v.m);
	}

	__forceinline static void expand(const GSVector4i& v, GSVector4& a, GSVector4& b, GSVector4& c, GSVector4& d)
	{
		GSVector4i mask = GSVector4i::x000000ff();

		a = GSVector4(v & mask);
		b = GSVector4((v >> 8) & mask);
		c = GSVector4((v >> 16) & mask);
		d = GSVector4((v >> 24));
	}

	__forceinline static void transpose(GSVector4& a, GSVector4& b, GSVector4& c, GSVector4& d)
	{
		GSVector4 v0 = a.xyxy(b);
		GSVector4 v1 = c.xyxy(d);

		GSVector4 e = v0.xzxz(v1);
		GSVector4 f = v0.ywyw(v1);

		GSVector4 v2 = a.zwzw(b);
		GSVector4 v3 = c.zwzw(d);

		GSVector4 g = v2.xzxz(v3);
		GSVector4 h = v2.ywyw(v3);

		a = e;
		b = f;
		c = g;
		d = h;
/*
		GSVector4 v0 = a.xyxy(b);
		GSVector4 v1 = c.xyxy(d);
		GSVector4 v2 = a.zwzw(b);
		GSVector4 v3 = c.zwzw(d);

		a = v0.xzxz(v1);
		b = v0.ywyw(v1);
		c = v2.xzxz(v3);
		d = v2.ywyw(v3);
*/
/*
		GSVector4 v0 = a.upl(b);
		GSVector4 v1 = a.uph(b);
		GSVector4 v2 = c.upl(d);
		GSVector4 v3 = c.uph(d);

		a = v0.l2h(v2);
		b = v2.h2l(v0);
		c = v1.l2h(v3);
		d = v3.h2l(v1);
*/	}

	__forceinline GSVector4 operator - () const
	{
		return neg();
	}

	__forceinline void operator += (const GSVector4& v)
	{
		m = _mm_add_ps(m, v);
	}

	__forceinline void operator -= (const GSVector4& v)
	{
		m = _mm_sub_ps(m, v);
	}

	__forceinline void operator *= (const GSVector4& v)
	{
		m = _mm_mul_ps(m, v);
	}

	__forceinline void operator /= (const GSVector4& v)
	{
		m = _mm_div_ps(m, v);
	}

	__forceinline void operator += (float f)
	{
		*this += GSVector4(f);
	}

	__forceinline void operator -= (float f)
	{
		*this -= GSVector4(f);
	}

	__forceinline void operator *= (float f)
	{
		*this *= GSVector4(f);
	}

	__forceinline void operator /= (float f)
	{
		*this /= GSVector4(f);
	}

	__forceinline void operator &= (const GSVector4& v)
	{
		m = _mm_and_ps(m, v);
	}

	__forceinline void operator |= (const GSVector4& v)
	{
		m = _mm_or_ps(m, v);
	}

	__forceinline void operator ^= (const GSVector4& v)
	{
		m = _mm_xor_ps(m, v);
	}

	__forceinline friend GSVector4 operator + (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_add_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator - (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_sub_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator * (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_mul_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator / (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_div_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator + (const GSVector4& v, float f)
	{
		return v + GSVector4(f);
	}

	__forceinline friend GSVector4 operator - (const GSVector4& v, float f)
	{
		return v - GSVector4(f);
	}

	__forceinline friend GSVector4 operator * (const GSVector4& v, float f)
	{
		return v * GSVector4(f);
	}

	__forceinline friend GSVector4 operator / (const GSVector4& v, float f)
	{
		return v / GSVector4(f);
	}

	__forceinline friend GSVector4 operator & (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_and_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator | (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_or_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator ^ (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_xor_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator == (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_cmpeq_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator != (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_cmpneq_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator > (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_cmpgt_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator < (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_cmplt_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator >= (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_cmpge_ps(v1, v2));
	}

	__forceinline friend GSVector4 operator <= (const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(_mm_cmple_ps(v1, v2));
	}

	#define VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, ws, wn) \
		__forceinline GSVector4 xs##ys##zs##ws() const {return GSVector4(_mm_shuffle_ps(m, m, _MM_SHUFFLE(wn, zn, yn, xn)));} \
		__forceinline GSVector4 xs##ys##zs##ws(const GSVector4& v) const {return GSVector4(_mm_shuffle_ps(m, v.m, _MM_SHUFFLE(wn, zn, yn, xn)));} \

	#define VECTOR4_SHUFFLE_3(xs, xn, ys, yn, zs, zn) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, x, 0) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, y, 1) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, z, 2) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, w, 3) \

	#define VECTOR4_SHUFFLE_2(xs, xn, ys, yn) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, x, 0) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, y, 1) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, z, 2) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, w, 3) \

	#define VECTOR4_SHUFFLE_1(xs, xn) \
		VECTOR4_SHUFFLE_2(xs, xn, x, 0) \
		VECTOR4_SHUFFLE_2(xs, xn, y, 1) \
		VECTOR4_SHUFFLE_2(xs, xn, z, 2) \
		VECTOR4_SHUFFLE_2(xs, xn, w, 3) \

	VECTOR4_SHUFFLE_1(x, 0)
	VECTOR4_SHUFFLE_1(y, 1)
	VECTOR4_SHUFFLE_1(z, 2)
	VECTOR4_SHUFFLE_1(w, 3)

	#if _M_SSE >= 0x501

	__forceinline GSVector4 broadcast32() const
	{
		return GSVector4(_mm_broadcastss_ps(m));
	}

	__forceinline static GSVector4 broadcast32(const GSVector4& v)
	{
		return GSVector4(_mm_broadcastss_ps(v.m));
	}

	__forceinline static GSVector4 broadcast32(const void* f)
	{
		return GSVector4(_mm_broadcastss_ps(_mm_load_ss((const float*)f)));
	}

	#endif
};
