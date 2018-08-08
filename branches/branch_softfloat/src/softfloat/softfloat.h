
/*============================================================================

This C header file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
Package, Release 2b.

Written by John R. Hauser.  This work was made possible in part by the
International Computer Science Institute, located at Suite 600, 1947 Center
Street, Berkeley, California 94704.  Funding was partially provided by the
National Science Foundation under grant MIP-9311980.  The original version
of this code was written as part of a project to build a fixed-point vector
processor in collaboration with the University of California at Berkeley,
overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
is available through the Web page `http://www.cs.berkeley.edu/~jhauser/
arithmetic/SoftFloat.html'.

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

#ifndef SOFTFLOAT_H
#define SOFTFLOAT_H
/*----------------------------------------------------------------------------
| The macro `FLOATX80' must be defined to enable the extended double-precision
| floating-point format `floatx80'.  If this macro is not defined, the
| `floatx80' type will not be defined, and none of the functions that either
| input or output the `floatx80' type will be defined.  The same applies to
| the `FLOAT128' macro and the quadruple-precision format `float128'.
*----------------------------------------------------------------------------*/
#define FLOATX80
#define FLOAT128

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point types.
*----------------------------------------------------------------------------*/

#include "mamesf.h"

typedef bits32 float32;
typedef bits64 float64;
#ifdef FLOATX80
typedef struct {
	bits16 high;
	bits64 low;
} floatx80;
#endif
#ifdef FLOAT128
typedef struct {
	bits64 high, low;
} float128;
#endif

/*----------------------------------------------------------------------------
| Primitive arithmetic functions, including multi-word arithmetic, and
| division and square root approximations.  (Can be specialized to target if
| desired.)
*----------------------------------------------------------------------------*/
#include "softfloat-macros.h"

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point underflow tininess-detection mode.
*----------------------------------------------------------------------------*/
#if 0
extern int8 float_detect_tininess;
#endif
enum {
	float_tininess_after_rounding  = 0,
	float_tininess_before_rounding = 1
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point rounding mode.
*----------------------------------------------------------------------------*/
#if 0
extern int8 float_rounding_mode;
#ifdef SOFTFLOAT_I860
extern int8 float_rounding_mode2;
#endif
#endif
enum {
	float_round_nearest_even = 0,
	float_round_to_zero      = 1,
	float_round_down         = 2,
	float_round_up           = 3
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point exception flags.
*----------------------------------------------------------------------------*/
#if 0
extern int8 float_exception_flags;
#ifdef SOFTFLOAT_I860
extern int8 float_exception_flags2;
#endif
#endif
enum {
	float_flag_invalid = 0x01, float_flag_denormal = 0x02, float_flag_divbyzero = 0x04, float_flag_overflow = 0x08,
    float_flag_underflow = 0x10, float_flag_inexact = 0x20, float_flag_signaling = 0x40, float_flag_decimal = 0x80
};

/*----------------------------------------------------------------------------
 | Variables for storing sign, exponent and significand of internal extended 
 | double-precision floating-point value for external use.
 *----------------------------------------------------------------------------*/
#if 0
extern flag floatx80_internal_sign;
extern int32 floatx80_internal_exp;
extern bits64 floatx80_internal_sig0;
extern bits64 floatx80_internal_sig1;
extern int8 floatx80_internal_precision;
extern int8 floatx80_internal_mode;
#endif

/*----------------------------------------------------------------------------
 | Special flags for indicating some unique behavior is required.
 *----------------------------------------------------------------------------*/
enum {
    cmp_signed_nan = 0x01, addsub_swap_inf = 0x02, infinity_clear_intbit = 0x04
};

typedef struct {
    int8 float_detect_tininess;
    int8 float_exception_flags;
    int8 float_rounding_mode;
#ifdef FLOATX80
    int8 floatx80_rounding_precision;
    flag floatx80_special_flags;
    flag floatx80_internal_sign;
    int32 floatx80_internal_exp;
    bits64 floatx80_internal_sig0;
    bits64 floatx80_internal_sig1;
    int8 floatx80_internal_precision;
    int8 floatx80_internal_mode;
#endif
} float_ctrl;

void float_init( float_ctrl* c );
int8 get_float_rounding_mode( float_ctrl* c );
void set_float_rounding_mode( int8 mode, float_ctrl* c );
int8 get_float_rounding_precision( float_ctrl* c );
void set_float_rounding_precision( int8 precision, float_ctrl* c );
int8 get_float_exception_flags( float_ctrl* c );
void set_float_exception_flags( int8 flags, float_ctrl* c );
int8 get_float_detect_tininess( float_ctrl* c );
void set_float_detect_tininess( int8 mode, float_ctrl* c );

void set_special_flags( int8 flags, float_ctrl* c);

/*----------------------------------------------------------------------------
 | Function for getting sign, exponent and significand of extended
 | double-precision floating-point intermediate result for external use.
 *----------------------------------------------------------------------------*/
floatx80 getFloatInternalOverflow( float_ctrl* c );
floatx80 getFloatInternalUnderflow( float_ctrl* c );
floatx80 getFloatInternalRoundedAll( float_ctrl* c );
floatx80 getFloatInternalRoundedSome( float_ctrl* c );
floatx80 getFloatInternalUnrounded( float_ctrl* c );
floatx80 getFloatInternalFloatx80( float_ctrl* c );
bits64 getFloatInternalGRS( float_ctrl* c );

/*----------------------------------------------------------------------------
| Routine to raise any or all of the software IEC/IEEE floating-point
| exception flags.
*----------------------------------------------------------------------------*/
void float_raise( int8, float_ctrl* c );
#if 0
#ifdef SOFTFLOAT_I860
void float_raise2( int8 );
#endif
#endif

/*----------------------------------------------------------------------------
 | The pattern for a default generated single-precision NaN.
 *----------------------------------------------------------------------------*/
#define float32_default_nan 0x7FFFFFFF

/*----------------------------------------------------------------------------
 | The pattern for a default generated double-precision NaN.
 *----------------------------------------------------------------------------*/
#define float64_default_nan LIT64( 0x7FFFFFFFFFFFFFFF )

#ifdef FLOATX80
/*----------------------------------------------------------------------------
 | The pattern for a default generated extended double-precision NaN.  The
 | `high' and `low' values hold the most- and least-significant bits,
 | respectively.
 *----------------------------------------------------------------------------*/
#define floatx80_default_nan_high 0x7FFF
#define floatx80_default_nan_low  LIT64( 0xFFFFFFFFFFFFFFFF )
#endif
#ifdef FLOAT128
/*----------------------------------------------------------------------------
 | The pattern for a default generated quadruple-precision NaN.  The `high' and
 | `low' values hold the most- and least-significant bits, respectively.
 *----------------------------------------------------------------------------*/
#define float128_default_nan_high LIT64( 0x7FFFFFFFFFFFFFFF )
#define float128_default_nan_low  LIT64( 0xFFFFFFFFFFFFFFFF )
#endif
#ifdef FLOATX80
/*----------------------------------------------------------------------------
 | The pattern for a default generated extended double-precision infinity.
 *----------------------------------------------------------------------------*/
#define floatx80_default_infinity_high 0x7FFF
#define floatx80_default_infinity_low  LIT64( 0x0000000000000000 )
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE integer-to-floating-point conversion routines.
*----------------------------------------------------------------------------*/
float32 int32_to_float32( int32, float_ctrl* );
float64 int32_to_float64( int32 );
#ifdef FLOATX80
floatx80 int32_to_floatx80( int32 );
#endif
#ifdef FLOAT128
float128 int32_to_float128( int32 );
#endif
float32 int64_to_float32( int64, float_ctrl* );
float64 int64_to_float64( int64, float_ctrl* );
#ifdef FLOATX80
floatx80 int64_to_floatx80( int64 );
#endif
#ifdef FLOAT128
float128 int64_to_float128( int64 );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision conversion routines.
*----------------------------------------------------------------------------*/
int32 float32_to_int32( float32, float_ctrl* );
int32 float32_to_int32_round_to_zero( float32, float_ctrl* );
int64 float32_to_int64( float32, float_ctrl* );
int64 float32_to_int64_round_to_zero( float32, float_ctrl* );
float64 float32_to_float64( float32, float_ctrl* );
#ifdef FLOATX80
floatx80 float32_to_floatx80( float32, float_ctrl* );
floatx80 float32_to_floatx80_allowunnormal( float32 );
#endif
#ifdef FLOAT128
float128 float32_to_float128( float32, float_ctrl* );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision operations.
*----------------------------------------------------------------------------*/
float32 float32_round_to_int( float32, float_ctrl* );
float32 float32_add( float32, float32, float_ctrl* );
float32 float32_sub( float32, float32, float_ctrl* );
float32 float32_mul( float32, float32, float_ctrl* );
float32 float32_div( float32, float32, float_ctrl* );
float32 float32_rem( float32, float32, float_ctrl* );
float32 float32_sqrt( float32, float_ctrl* );
flag float32_eq( float32, float32, float_ctrl* );
flag float32_le( float32, float32, float_ctrl* );
flag float32_lt( float32, float32, float_ctrl* );
#ifdef SOFTFLOAT_I860
flag float32_gt( float32, float32, float_ctrl* );
#endif
flag float32_eq_signaling( float32, float32, float_ctrl* );
flag float32_le_quiet( float32, float32, float_ctrl* );
flag float32_lt_quiet( float32, float32, float_ctrl* );
flag float32_is_signaling_nan( float32 );

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision conversion routines.
*----------------------------------------------------------------------------*/
int32 float64_to_int32( float64, float_ctrl* );
int32 float64_to_int32_round_to_zero( float64, float_ctrl* );
int64 float64_to_int64( float64, float_ctrl* );
int64 float64_to_int64_round_to_zero( float64, float_ctrl* );
float32 float64_to_float32( float64, float_ctrl* );
#ifdef FLOATX80
floatx80 float64_to_floatx80( float64, float_ctrl* );
floatx80 float64_to_floatx80_allowunnormal( float64 );
#endif
#ifdef FLOAT128
float128 float64_to_float128( float64, float_ctrl* );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision operations.
*----------------------------------------------------------------------------*/
float64 float64_round_to_int( float64, float_ctrl* );
float64 float64_add( float64, float64, float_ctrl* );
float64 float64_sub( float64, float64, float_ctrl* );
float64 float64_mul( float64, float64, float_ctrl* );
float64 float64_div( float64, float64, float_ctrl* );
float64 float64_rem( float64, float64, float_ctrl* );
float64 float64_sqrt( float64, float_ctrl* );
flag float64_eq( float64, float64, float_ctrl* );
flag float64_le( float64, float64, float_ctrl* );
flag float64_lt( float64, float64, float_ctrl* );
#ifdef SOFTFLOAT_I860
flag float64_gt( float64, float64, float_ctrl* );
#endif
flag float64_eq_signaling( float64, float64, float_ctrl* );
flag float64_le_quiet( float64, float64, float_ctrl* );
flag float64_lt_quiet( float64, float64, float_ctrl* );
flag float64_is_signaling_nan( float64 );

#ifdef FLOATX80

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision conversion routines.
*----------------------------------------------------------------------------*/
int32 floatx80_to_int32( floatx80, float_ctrl* );
#ifdef SOFTFLOAT_68K
int16 floatx80_to_int16( floatx80, float_ctrl* );
int8 floatx80_to_int8( floatx80, float_ctrl* );
#endif
int32 floatx80_to_int32_round_to_zero( floatx80, float_ctrl* );
int64 floatx80_to_int64( floatx80, float_ctrl* );
int64 floatx80_to_int64_round_to_zero( floatx80, float_ctrl* );
float32 floatx80_to_float32( floatx80, float_ctrl* );
float64 floatx80_to_float64( floatx80, float_ctrl* );
#ifdef SOFTFLOAT_68K
floatx80 floatx80_to_floatx80( floatx80, float_ctrl* );
floatx80 floatdecimal_to_floatx80( floatx80, float_ctrl* );
floatx80 floatx80_to_floatdecimal( floatx80, int32*, float_ctrl* );
#endif
#ifdef FLOAT128
float128 floatx80_to_float128( floatx80, float_ctrl* );
#endif
bits64 extractFloatx80Frac( floatx80 a );
int32 extractFloatx80Exp( floatx80 a );
flag extractFloatx80Sign( floatx80 a );

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision rounding precision.  Valid
| values are 32, 64, and 80.
*----------------------------------------------------------------------------*/
#if 0
extern int8 floatx80_rounding_precision;
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision operations.
*----------------------------------------------------------------------------*/
floatx80 floatx80_round_to_int( floatx80, float_ctrl* );
#ifdef SOFTFLOAT_68K
floatx80 floatx80_round_to_int_toward_zero( floatx80, float_ctrl* );
floatx80 floatx80_round_to_float32( floatx80, float_ctrl* );
floatx80 floatx80_round_to_float64( floatx80, float_ctrl* );
floatx80 floatx80_round32( floatx80, float_ctrl* );
floatx80 floatx80_round64( floatx80, float_ctrl* );
floatx80 floatx80_normalize( floatx80 );
floatx80 floatx80_denormalize( floatx80, flag );
#endif
floatx80 floatx80_add( floatx80, floatx80, float_ctrl* );
floatx80 floatx80_sub( floatx80, floatx80, float_ctrl* );
floatx80 floatx80_mul( floatx80, floatx80, float_ctrl* );
floatx80 floatx80_div( floatx80, floatx80, float_ctrl* );
#ifndef SOFTFLOAT_68K
floatx80 floatx80_rem( floatx80, floatx80, float_ctrl* );
#endif
floatx80 floatx80_sqrt( floatx80, float_ctrl* );

flag floatx80_eq( floatx80, floatx80, float_ctrl* );
flag floatx80_le( floatx80, floatx80, float_ctrl* );
flag floatx80_lt( floatx80, floatx80, float_ctrl* );
flag floatx80_eq_signaling( floatx80, floatx80, float_ctrl* );
flag floatx80_le_quiet( floatx80, floatx80, float_ctrl* );
flag floatx80_lt_quiet( floatx80, floatx80, float_ctrl* );

flag floatx80_is_signaling_nan( floatx80 );
flag floatx80_is_nan( floatx80 );
#ifdef SOFTFLOAT_68K
flag floatx80_is_zero( floatx80 );
flag floatx80_is_infinity( floatx80 );
flag floatx80_is_negative( floatx80 );
flag floatx80_is_denormal( floatx80 );
flag floatx80_is_unnormal( floatx80 );
flag floatx80_is_normal( floatx80 );

// functions are in softfloat.c
floatx80 floatx80_move( floatx80 a, float_ctrl* c );
floatx80 floatx80_abs( floatx80 a, float_ctrl* c );
floatx80 floatx80_neg( floatx80 a, float_ctrl* c );
floatx80 floatx80_getexp( floatx80 a, float_ctrl* c );
floatx80 floatx80_getman( floatx80 a, float_ctrl* c );
floatx80 floatx80_scale(floatx80 a, floatx80 b, float_ctrl* c );
floatx80 floatx80_rem( floatx80 a, floatx80 b, bits64 *q, flag *s, float_ctrl* c );
floatx80 floatx80_mod( floatx80 a, floatx80 b, bits64 *q, flag *s, float_ctrl* c );
floatx80 floatx80_sglmul( floatx80 a, floatx80 b, float_ctrl* c );
floatx80 floatx80_sgldiv( floatx80 a, floatx80 b, float_ctrl* c );
floatx80 floatx80_cmp( floatx80 a, floatx80 b, float_ctrl* c );
floatx80 floatx80_tst( floatx80 a, float_ctrl* c );

// functions are in softfloat_fpsp.c
floatx80 floatx80_acos(floatx80 a, float_ctrl* c);
floatx80 floatx80_asin(floatx80 a, float_ctrl* c);
floatx80 floatx80_atan(floatx80 a, float_ctrl* c);
floatx80 floatx80_atanh(floatx80 a, float_ctrl* c);
floatx80 floatx80_cos(floatx80 a, float_ctrl* c);
floatx80 floatx80_cosh(floatx80 a, float_ctrl* c);
floatx80 floatx80_etox(floatx80 a, float_ctrl* c);
floatx80 floatx80_etoxm1(floatx80 a, float_ctrl* c);
floatx80 floatx80_log10(floatx80 a, float_ctrl* c);
floatx80 floatx80_log2(floatx80 a, float_ctrl* c);
floatx80 floatx80_logn(floatx80 a, float_ctrl* c);
floatx80 floatx80_lognp1(floatx80 a, float_ctrl* c);
floatx80 floatx80_sin(floatx80 a, float_ctrl* c);
floatx80 floatx80_sinh(floatx80 a, float_ctrl* c);
floatx80 floatx80_tan(floatx80 a, float_ctrl* c);
floatx80 floatx80_tanh(floatx80 a, float_ctrl* c);
floatx80 floatx80_tentox(floatx80 a, float_ctrl* c);
floatx80 floatx80_twotox(floatx80 a, float_ctrl* c);
#endif

// functions originally internal to softfloat.c
void normalizeFloatx80Subnormal( bits64 aSig, int32 *zExpPtr, bits64 *zSigPtr );
floatx80 packFloatx80( flag zSign, int32 zExp, bits64 zSig );
floatx80 roundAndPackFloatx80( int8 roundingPrecision, flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1, float_ctrl* c );

// functions are in softfloat-specialize.h
floatx80 propagateFloatx80NaNOneArg( floatx80 a, float_ctrl* c );
floatx80 propagateFloatx80NaN( floatx80 a, floatx80 b, float_ctrl* c );

#endif

#ifdef FLOAT128

/*----------------------------------------------------------------------------
| Software IEC/IEEE quadruple-precision conversion routines.
*----------------------------------------------------------------------------*/
int32 float128_to_int32( float128, float_ctrl* );
int32 float128_to_int32_round_to_zero( float128, float_ctrl* );
int64 float128_to_int64( float128, float_ctrl* );
int64 float128_to_int64_round_to_zero( float128, float_ctrl* );
float32 float128_to_float32( float128, float_ctrl* );
float64 float128_to_float64( float128, float_ctrl* );
#ifdef FLOATX80
floatx80 float128_to_floatx80( float128, float_ctrl* );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE quadruple-precision operations.
*----------------------------------------------------------------------------*/
float128 float128_round_to_int( float128, float_ctrl* );
float128 float128_add( float128, float128, float_ctrl* );
float128 float128_sub( float128, float128, float_ctrl* );
float128 float128_mul( float128, float128, float_ctrl* );
float128 float128_div( float128, float128, float_ctrl* );
float128 float128_rem( float128, float128, float_ctrl* );
float128 float128_sqrt( float128, float_ctrl* );
flag float128_eq( float128, float128, float_ctrl* );
flag float128_le( float128, float128, float_ctrl* );
flag float128_lt( float128, float128, float_ctrl* );
flag float128_eq_signaling( float128, float128, float_ctrl* );
flag float128_le_quiet( float128, float128, float_ctrl* );
flag float128_lt_quiet( float128, float128, float_ctrl* );
flag float128_is_signaling_nan( float128 );

// functions originally internal to softfloat.c
float128 packFloat128( flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1 );
float128 roundAndPackFloat128( flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1, bits64 zSig2, float_ctrl* c );
float128 normalizeRoundAndPackFloat128( flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1, float_ctrl* c );

#endif

#endif //SOFTFLOAT_H