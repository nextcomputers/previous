/*============================================================================
This source file is an extension to the SoftFloat IEC/IEEE Floating-point
Arithmetic Package, Release 2b, written for Bochs (x86 achitecture simulator)
floating point emulation.
float_raise(float_flag_invalid)
THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
EFFECTIVELY INDEMNIFY JOHN HAUSER AND THE INTERNATIONAL COMPUTER SCIENCE
INSTITUTE (possibly via similar legal warning) AGAINST ALL LOSSES, COSTS, OR
OTHER PROBLEMS INCURRED BY THEIR CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) the source code for the derivative work includes prominent notice that
the work is derivative, and (2) the source code includes prominent notice with
these four paragraphs for those parts of this code that are retained.
=============================================================================*/

/*============================================================================
 * Written for Bochs (x86 achitecture simulator) by
 *            Stanislav Shwartsman [sshwarts at sourceforge net]
 * Adapted for lib/softfloat in MESS by Hans Ostermeyer (03/2012)
 * ==========================================================================*/

#define FLOAT128

#define USE_estimateDiv128To64
#include "mamesf.h"
#include "softfloat.h"
#include "fpu_constant.h"

static const floatx80 floatx80_log10_2 = { 0x3ffd, 0x9a209a84fbcff798U };
static const floatx80 floatx80_ln_2 = { 0x3ffe, 0xb17217f7d1cf79acU };
static const floatx80 floatx80_one = { 0x3fff, 0x8000000000000000U };
static const floatx80 floatx80_default_nan = { floatx80_default_nan_high, floatx80_default_nan_low };

#define packFloat_128(zHi, zLo) {(zHi), (zLo)}
#define PACK_FLOAT_128(hi,lo) packFloat_128(LIT64(hi),LIT64(lo))

#define EXP_BIAS 0x3FFF


static const float128 float128_one =
	packFloat_128(0x3fff000000000000U, 0x0000000000000000U);
static const float128 float128_two =
	packFloat_128(0x4000000000000000U, 0x0000000000000000U);

static const float128 float128_ln2inv2 =
	packFloat_128(0x400071547652b82fU, 0xe1777d0ffda0d23aU);

#define SQRT2_HALF_SIG  0xb504f333f9de6484U

extern float128 OddPoly(float128 x, float128 *arr, unsigned n);

#define L2_ARR_SIZE 9

static float128 ln_arr[L2_ARR_SIZE] =
{
	PACK_FLOAT_128(0x3fff000000000000, 0x0000000000000000), /*  1 */
	PACK_FLOAT_128(0x3ffd555555555555, 0x5555555555555555), /*  3 */
	PACK_FLOAT_128(0x3ffc999999999999, 0x999999999999999a), /*  5 */
	PACK_FLOAT_128(0x3ffc249249249249, 0x2492492492492492), /*  7 */
	PACK_FLOAT_128(0x3ffbc71c71c71c71, 0xc71c71c71c71c71c), /*  9 */
	PACK_FLOAT_128(0x3ffb745d1745d174, 0x5d1745d1745d1746), /* 11 */
	PACK_FLOAT_128(0x3ffb3b13b13b13b1, 0x3b13b13b13b13b14), /* 13 */
	PACK_FLOAT_128(0x3ffb111111111111, 0x1111111111111111), /* 15 */
	PACK_FLOAT_128(0x3ffae1e1e1e1e1e1, 0xe1e1e1e1e1e1e1e2)  /* 17 */
};

static float128 poly_ln(float128 x1)
{
/*
    //
    //                     3     5     7     9     11     13     15
    //        1+u         u     u     u     u     u      u      u
    // 1/2 ln ---  ~ u + --- + --- + --- + --- + ---- + ---- + ---- =
    //        1-u         3     5     7     9     11     13     15
    //
    //                     2     4     6     8     10     12     14
    //                    u     u     u     u     u      u      u
    //       = u * [ 1 + --- + --- + --- + --- + ---- + ---- + ---- ] =
    //                    3     5     7     9     11     13     15
    //
    //           3                          3
    //          --       4k                --        4k+2
    //   p(u) = >  C  * u           q(u) = >  C   * u
    //          --  2k                     --  2k+1
    //          k=0                        k=0
    //
    //          1+u                 2
    //   1/2 ln --- ~ u * [ p(u) + u * q(u) ]
    //          1-u
    //
*/
	return OddPoly(x1, ln_arr, L2_ARR_SIZE);
}

/* required sqrt(2)/2 < x < sqrt(2) */
static float128 poly_l2(float128 x)
{
	/* using float128 for approximation */
	float128 x_p1 = float128_add(x, float128_one);
	float128 x_m1 = float128_sub(x, float128_one);
	x = float128_div(x_m1, x_p1);
	x = poly_ln(x);
	x = float128_mul(x, float128_ln2inv2);
	return x;
}

static float128 poly_l2p1(float128 x)
{
	/* using float128 for approximation */
	float128 x_p2 = float128_add(x, float128_two);
	x = float128_div(x, x_p2);
	x = poly_ln(x);
	x = float128_mul(x, float128_ln2inv2);
	return x;
}

// =================================================
// FYL2X                   Compute y * log (x)
//                                        2
// =================================================

//
// Uses the following identities:
//
// 1. ----------------------------------------------------------
//              ln(x)
//   log (x) = -------,  ln (x*y) = ln(x) + ln(y)
//      2       ln(2)
//
// 2. ----------------------------------------------------------
//                1+u             x-1
//   ln (x) = ln -----, when u = -----
//                1-u             x+1
//
// 3. ----------------------------------------------------------
//                        3     5     7           2n+1
//       1+u             u     u     u           u
//   ln ----- = 2 [ u + --- + --- + --- + ... + ------ + ... ]
//       1-u             3     5     7           2n+1
//

static floatx80 fyl2x(floatx80 a, floatx80 b)
{
	uint64_t aSig = extractFloatx80Frac(a);
	int32_t aExp = extractFloatx80Exp(a);
	int aSign = extractFloatx80Sign(a);
	uint64_t bSig = extractFloatx80Frac(b);
	int32_t bExp = extractFloatx80Exp(b);
	int bSign = extractFloatx80Sign(b);

	int zSign = bSign ^ 1;

	if (aExp == 0x7FFF) {
		if ((uint64_t) (aSig<<1)
				|| ((bExp == 0x7FFF) && (uint64_t) (bSig<<1)))
		{
			return propagateFloatx80NaN(a, b);
		}
		if (aSign)
		{
invalid:
			float_raise(float_flag_invalid);
			return floatx80_default_nan;
		}
		else {
			if (bExp == 0) {
				if (bSig == 0) goto invalid;
				float_raise(float_flag_denormal);
			}
			return packFloatx80(bSign, 0x7FFF, floatx80_default_infinity_low);
		}
	}
	if (bExp == 0x7FFF)
	{
		if ((uint64_t) (bSig<<1)) return propagateFloatx80NaN(a, b);
		if (aSign && (uint64_t)(aExp | aSig)) goto invalid;
		if (aSig && (aExp == 0))
			float_raise(float_flag_denormal);
		if (aExp < 0x3FFF) {
			return packFloatx80(zSign, 0x7FFF, floatx80_default_infinity_low);
		}
		if (aExp == 0x3FFF && ((uint64_t) (aSig<<1) == 0)) goto invalid;
		return packFloatx80(bSign, 0x7FFF, floatx80_default_infinity_low);
	}
	if (aExp == 0) {
		if (aSig == 0) {
			if ((bExp | bSig) == 0) goto invalid;
			float_raise(float_flag_divbyzero);
			return packFloatx80(zSign, 0x7FFF, floatx80_default_infinity_low);
		}
		if (aSign) goto invalid;
		float_raise(float_flag_denormal);
		normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
	}
	if (aSign) goto invalid;
	if (bExp == 0) {
		if (bSig == 0) {
			if (aExp < 0x3FFF) return packFloatx80(zSign, 0, 0);
			return packFloatx80(bSign, 0, 0);
		}
		float_raise(float_flag_denormal);
		normalizeFloatx80Subnormal(bSig, &bExp, &bSig);
	}
	if (aExp == 0x3FFF && ((uint64_t) (aSig<<1) == 0))
		return packFloatx80(bSign, 0, 0);

	float_raise(float_flag_inexact);

	int ExpDiff = aExp - 0x3FFF;
	aExp = 0;
	if (aSig >= SQRT2_HALF_SIG) {
		ExpDiff++;
		aExp--;
	}

	/* ******************************** */
	/* using float128 for approximation */
	/* ******************************** */

	uint64_t zSig0, zSig1;
	shift128Right(aSig<<1, 0, 16, &zSig0, &zSig1);
	float128 x = packFloat128(0, aExp+0x3FFF, zSig0, zSig1);
	x = poly_l2(x);
	x = float128_add(x, int64_to_float128((int64_t) ExpDiff));
	return floatx80_mul(b, float128_to_floatx80(x));
}

// =================================================
// FYL2XP1                 Compute y * log (x + 1)
//                                        2
// =================================================

//
// Uses the following identities:
//
// 1. ----------------------------------------------------------
//              ln(x)
//   log (x) = -------
//      2       ln(2)
//
// 2. ----------------------------------------------------------
//                  1+u              x
//   ln (x+1) = ln -----, when u = -----
//                  1-u             x+2
//
// 3. ----------------------------------------------------------
//                        3     5     7           2n+1
//       1+u             u     u     u           u
//   ln ----- = 2 [ u + --- + --- + --- + ... + ------ + ... ]
//       1-u             3     5     7           2n+1
//

floatx80 fyl2xp1(floatx80 a, floatx80 b)
{
	int32_t aExp, bExp;
	uint64_t aSig, bSig, zSig0, zSig1, zSig2;
	int aSign, bSign;

	aSig = extractFloatx80Frac(a);
	aExp = extractFloatx80Exp(a);
	aSign = extractFloatx80Sign(a);
	bSig = extractFloatx80Frac(b);
	bExp = extractFloatx80Exp(b);
	bSign = extractFloatx80Sign(b);
	int zSign = aSign ^ bSign;

	if (aExp == 0x7FFF) {
		if ((uint64_t) (aSig<<1)
				|| ((bExp == 0x7FFF) && (uint64_t) (bSig<<1)))
		{
			return propagateFloatx80NaN(a, b);
		}
		if (aSign)
		{
invalid:
			float_raise(float_flag_invalid);
			return floatx80_default_nan;
		}
			else {
			if (bExp == 0) {
				if (bSig == 0) goto invalid;
				float_raise(float_flag_denormal);
			}
			return packFloatx80(bSign, 0x7FFF, floatx80_default_infinity_low);
		}
	}
	if (bExp == 0x7FFF)
	{
		if ((uint64_t) (bSig<<1))
			return propagateFloatx80NaN(a, b);

		if (aExp == 0) {
			if (aSig == 0) goto invalid;
			float_raise(float_flag_denormal);
		}

		return packFloatx80(zSign, 0x7FFF, floatx80_default_infinity_low);
	}
	if (aExp == 0) {
		if (aSig == 0) {
			if (bSig && (bExp == 0)) float_raise(float_flag_denormal);
			return packFloatx80(zSign, 0, 0);
		}
		float_raise(float_flag_denormal);
		normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
	}
	if (bExp == 0) {
		if (bSig == 0) return packFloatx80(zSign, 0, 0);
		float_raise(float_flag_denormal);
		normalizeFloatx80Subnormal(bSig, &bExp, &bSig);
	}

	float_raise(float_flag_inexact);

	if (aSign && aExp >= 0x3FFF)
		return a;

	if (aExp >= 0x3FFC) // big argument
	{
		return fyl2x(floatx80_add(a, floatx80_one), b);
	}

	// handle tiny argument
	if (aExp < EXP_BIAS-70)
	{
		// first order approximation, return (a*b)/ln(2)
		int32_t zExp = aExp + FLOAT_LN2INV_EXP - 0x3FFE;

	mul128By64To192(FLOAT_LN2INV_HI, FLOAT_LN2INV_LO, aSig, &zSig0, &zSig1, &zSig2);
		if (0 < (int64_t) zSig0) {
			shortShift128Left(zSig0, zSig1, 1, &zSig0, &zSig1);
			--zExp;
		}

		zExp = zExp + bExp - 0x3FFE;
	mul128By64To192(zSig0, zSig1, bSig, &zSig0, &zSig1, &zSig2);
		if (0 < (int64_t) zSig0) {
			shortShift128Left(zSig0, zSig1, 1, &zSig0, &zSig1);
			--zExp;
		}

		return
			roundAndPackFloatx80(80, aSign ^ bSign, zExp, zSig0, zSig1);
	}

	/* ******************************** */
	/* using float128 for approximation */
	/* ******************************** */

	shift128Right(aSig<<1, 0, 16, &zSig0, &zSig1);
	float128 x = packFloat128(aSign, aExp, zSig0, zSig1);
	x = poly_l2p1(x);
	return floatx80_mul(b, float128_to_floatx80(x));
}

floatx80 floatx80_flognp1(floatx80 a)
{
	return fyl2xp1(a, floatx80_ln_2);
}

floatx80 floatx80_flogn(floatx80 a)
{
	return fyl2x(a, floatx80_ln_2);
}

floatx80 floatx80_flog2(floatx80 a)
{
	return fyl2x(a, floatx80_one);
}

floatx80 floatx80_flog10(floatx80 a)
{
	return fyl2x(a, floatx80_log10_2);
}
