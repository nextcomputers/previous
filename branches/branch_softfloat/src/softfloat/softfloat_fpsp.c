
/*============================================================================

This C source file is an extension to the SoftFloat IEC/IEEE Floating-point 
Arithmetic Package, Release 2a.

=============================================================================*/

#include "softfloat.h"

/*----------------------------------------------------------------------------
| Methods for detecting special conditions for mathematical functions
| supported by MC68881 and MC68862 mathematical coprocessor.
*----------------------------------------------------------------------------*/

#define pi_sig      LIT64(0xc90fdaa22168c235)
#define pi_sig0     LIT64(0xc90fdaa22168c234)
#define pi_sig1     LIT64(0xc4c6628b80dc1cd1)

#define pi_exp      0x4000
#define piby2_exp   0x3FFF
#define piby4_exp   0x3FFE

#define one_exp     0x3FFF
#define one_sig     LIT64(0x8000000000000000)


/*----------------------------------------------------------------------------
 | Arc cosine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_acos(floatx80 a)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    int8 user_rnd_mode, user_rnd_prec;
    
    floatx80 fp0, fp1, one;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF && (bits64) (aSig<<1)) {
        return propagateFloatx80NaNOneArg(a);
    }
    
    if (aExp > one_exp || (aExp == one_exp && aSig > one_sig)) {
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    if (aExp == one_exp && aSig == one_sig) {
        if (aSign) {
            float_raise(float_flag_inexact);
            return packFloatx80(0, pi_exp, pi_sig);
        } else {
            return packFloatx80(0, 0, 0);
        }
    }
    
    if (aExp == 0 && (aSig & one_sig) == 0) { // denormalized also returns pi/2
        return packFloatx80(0, piby2_exp, pi_sig);
    }
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;
    
    one = packFloatx80(0, one_exp, one_sig);
    fp0 = a;
    
    fp1 = floatx80_add(one, fp0);   // 1 + X
    fp0 = floatx80_sub(one, fp0);   // 1 - X
    fp0 = floatx80_div(fp0, fp1);   // (1-X)/(1+X)
    fp0 = floatx80_sqrt(fp0);       // SQRT((1-X)/(1+X))
    //fp0 = floatx80_atan(fp0);       // ATAN(SQRT((1-X)/(1+X)))
    
    a = floatx80_add(fp0, fp0);     // 2 * ATAN(SQRT((1-X)/(1+X)))
    
    float_rounding_mode = user_rnd_mode;
    floatx80_rounding_precision = user_rnd_prec;
    
    float_raise(float_flag_inexact);
    
    return a;
}

/*----------------------------------------------------------------------------
 | Arc sine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_asin_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF && (bits64) (aSig<<1)) {
        return propagateFloatx80NaNOneArg(a);
    }
    
    if (aExp > one_exp || (aExp == one_exp && aSig > one_sig)) {
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Arc tangent
 *----------------------------------------------------------------------------*/

floatx80 floatx80_atan_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        return roundAndPackFloatx80(floatx80_rounding_precision,
                                    aSign, piby2_exp, pi_sig0, pi_sig1);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Hyperbolic arc tangent
 *----------------------------------------------------------------------------*/

floatx80 floatx80_atanh_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF && (bits64) (aSig<<1)) {
        return propagateFloatx80NaNOneArg(a);
    }
    
    if (aExp >= one_exp) {
        if (aExp == one_exp && aSig == one_sig) {
            float_raise(float_flag_divbyzero);
            packFloatx80(aSign, 0x7FFF, floatx80_default_infinity_low);
        }
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Cosine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_cos_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(0, one_exp, one_sig);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Hyperbolic cosine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_cosh_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(0, one_exp, one_sig);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | e to x
 *----------------------------------------------------------------------------*/

floatx80 floatx80_etox_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        if (aSign) return packFloatx80(0, 0, 0);
        return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(0, one_exp, one_sig);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | e to x minus 1
 *----------------------------------------------------------------------------*/

floatx80 floatx80_etoxm1_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        if (aSign) return packFloatx80(aSign, one_exp, one_sig);
        return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Log base 10
 *----------------------------------------------------------------------------*/

floatx80 floatx80_log10_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) propagateFloatx80NaNOneArg(a);
        if (aSign == 0)
            return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) {
            float_raise(float_flag_divbyzero);
            return packFloatx80(1, 0x7FFF, floatx80_default_infinity_low);
        }
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    if (aSign) {
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Log base 2
 *----------------------------------------------------------------------------*/

floatx80 floatx80_log2_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) propagateFloatx80NaNOneArg(a);
        if (aSign == 0)
            return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) {
            float_raise(float_flag_divbyzero);
            return packFloatx80(1, 0x7FFF, floatx80_default_infinity_low);
        }
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    if (aSign) {
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Log base e
 *----------------------------------------------------------------------------*/

floatx80 floatx80_logn_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) propagateFloatx80NaNOneArg(a);
        if (aSign == 0)
            return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) {
            float_raise(float_flag_divbyzero);
            return packFloatx80(1, 0x7FFF, floatx80_default_infinity_low);
        }
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    if (aSign) {
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Log base e of x plus 1
 *----------------------------------------------------------------------------*/

floatx80 floatx80_lognp1_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) propagateFloatx80NaNOneArg(a);
        if (aSign) {
            float_raise(float_flag_invalid);
            a.low = floatx80_default_nan_low;
            a.high = floatx80_default_nan_high;
            return a;
        }
        return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    if (aSign && aExp >= one_exp) {
        if (aExp == one_exp && aSig == one_sig) {
            float_raise(float_flag_divbyzero);
            packFloatx80(aSign, 0x7FFF, floatx80_default_infinity_low); /* NaN? */
        }
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Sine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_sin_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Hyperbolic sine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_sinh_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        return packFloatx80(aSign, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Tangent
 *----------------------------------------------------------------------------*/

floatx80 floatx80_tan_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        float_raise(float_flag_invalid);
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | Hyperbolic tangent
 *----------------------------------------------------------------------------*/

floatx80 floatx80_tanh_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        return packFloatx80(aSign, one_exp, one_sig);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | 10 to x
 *----------------------------------------------------------------------------*/

floatx80 floatx80_tentox_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        if (aSign) return packFloatx80(0, 0, 0);
        return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(0, one_exp, one_sig);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}

/*----------------------------------------------------------------------------
 | 2 to x
 *----------------------------------------------------------------------------*/

floatx80 floatx80_twotox_check(floatx80 a, flag *e)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    *e = 1;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        if (aSign) return packFloatx80(0, 0, 0);
        return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(0, one_exp, one_sig);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    
    *e = 0;
    return a;
}
