
/*============================================================================

This C source file is an extension to the SoftFloat IEC/IEEE Floating-point 
Arithmetic Package, Release 2a.

=============================================================================*/

#include "softfloat.h"
#include "softfloat_fpsp_tables.h"

// prototypes: move to softfloat.h once all is done
floatx80 floatx80_acos(floatx80 a);
floatx80 floatx80_asin(floatx80 a);
floatx80 floatx80_atan(floatx80 a);

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


/* Helpers */

int32 floatx80_make_compact(int32 aExp, bits64 aSig)
{
    return (aExp<<16)|(aSig>>48);
}


/*----------------------------------------------------------------------------
 | Arc cosine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_acos(floatx80 a)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    int8 user_rnd_mode, user_rnd_prec;
    
    int32 compact;
    floatx80 fp0, fp1, one;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF && (bits64) (aSig<<1)) {
        return propagateFloatx80NaNOneArg(a);
    }
    
    if (aExp == 0 && (aSig & one_sig) == 0) { // zero and denormal return pi/2
        float_raise(float_flag_inexact);
        return roundAndPackFloatx80(floatx80_rounding_precision, 0, piby2_exp, pi_sig, 0);
    }
    
    compact = floatx80_make_compact(aExp, aSig);
    
    if (compact >= 0x3FFF8000) { // |X| >= 1
        if (aExp == one_exp && aSig == one_sig) { // |X| == 1
            if (aSign) { // X == -1
                float_raise(float_flag_inexact);
                return roundAndPackFloatx80(floatx80_rounding_precision, 0, pi_exp, pi_sig, 0);
            } else { // X == +1
                return packFloatx80(0, 0, 0);
            }
        } else { // |X| > 1
            float_raise(float_flag_invalid);
            a.low = floatx80_default_nan_low;
            a.high = floatx80_default_nan_high;
            return a;
        }
    } // |X| < 1
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;
    
    one = packFloatx80(0, one_exp, one_sig);
    fp0 = a;
    
    fp1 = floatx80_add(one, fp0);   // 1 + X
    fp0 = floatx80_sub(one, fp0);   // 1 - X
    fp0 = floatx80_div(fp0, fp1);   // (1-X)/(1+X)
    fp0 = floatx80_sqrt(fp0);       // SQRT((1-X)/(1+X))
    fp0 = floatx80_atan(fp0);       // ATAN(SQRT((1-X)/(1+X)))
    
    float_rounding_mode = user_rnd_mode;
    floatx80_rounding_precision = user_rnd_prec;
    
    a = floatx80_add(fp0, fp0);     // 2 * ATAN(SQRT((1-X)/(1+X)))
    
    float_raise(float_flag_inexact);
    
    return a;
}

/*----------------------------------------------------------------------------
 | Arc sine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_asin(floatx80 a)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    int8 user_rnd_mode, user_rnd_prec;
    
    int32 compact;
    floatx80 fp0, fp1, fp2, one;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF && (bits64) (aSig<<1)) {
        return propagateFloatx80NaNOneArg(a);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0); // zero
        if ((aSig & one_sig) == 0) { // denormal
            normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
            return roundAndPackFloatx80(floatx80_rounding_precision, aSign, aExp, aSig, 0);
        }
    }

    compact = floatx80_make_compact(aExp, aSig);
    
    if (compact >= 0x3FFF8000) { // |X| >= 1
        if (aExp == one_exp && aSig == one_sig) { // |X| == 1
            float_raise(float_flag_inexact);
            return roundAndPackFloatx80(floatx80_rounding_precision, aSign, piby2_exp, pi_sig, 0);
        } else { // |X| > 1
            float_raise(float_flag_invalid);
            a.low = floatx80_default_nan_low;
            a.high = floatx80_default_nan_high;
            return a;
        }

    } // |X| < 1
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;
    
    one = packFloatx80(0, one_exp, one_sig);
    fp0 = a;

    fp1 = floatx80_sub(one, fp0);   // 1 - X
    fp2 = floatx80_add(one, fp0);   // 1 + X
    fp1 = floatx80_mul(fp2, fp1);   // (1+X)*(1-X)
    fp1 = floatx80_sqrt(fp1);       // SQRT((1+X)*(1-X))
    fp0 = floatx80_div(fp0, fp1);   // X/SQRT((1+X)*(1-X))
    
    float_rounding_mode = user_rnd_mode;
    floatx80_rounding_precision = user_rnd_prec;
    
    a = floatx80_atan(fp0);         // ATAN(X/SQRT((1+X)*(1-X)))
    
    float_raise(float_flag_inexact);

    return a;
}

/*----------------------------------------------------------------------------
 | Arc tangent
 *----------------------------------------------------------------------------*/

floatx80 floatx80_atan(floatx80 a)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    int8 user_rnd_mode, user_rnd_prec;
    
    int32 compact, tbl_index;
    floatx80 fp0, fp1, fp2, fp3, xsave;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        return roundAndPackFloatx80(floatx80_rounding_precision,
                                    aSign, piby2_exp, pi_sig0, pi_sig1);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0); // zero
        if ((aSig & one_sig) == 0) { // denormal
            normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
            return roundAndPackFloatx80(floatx80_rounding_precision, aSign, aExp, aSig, 0);
        }
    }
    
    compact = floatx80_make_compact(aExp, aSig);
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;

    if (compact < 0x3FFB8000 || compact > 0x4002FFFF) { // |X| >= 16 or |X| < 1/16
        if (compact > 0x3FFF8000) { // |X| >= 16
            if (compact > 0x40638000) { // |X| > 2^(100)
                fp0 = packFloatx80(aSign, piby2_exp, pi_sig);
                fp1 = packFloatx80(aSign, 0x0001, one_sig);
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;
                a = floatx80_sub(fp0, fp1);
                float_raise(float_flag_inexact);
                return a;
            } else {
                fp0 = a;
                fp1 = packFloatx80(1, one_exp, one_sig); // -1
                fp1 = floatx80_div(fp1, fp0); // X' = -1/X
                xsave = fp1;
                fp0 = floatx80_mul(fp1, fp1); // Y = X'*X'
                fp1 = floatx80_mul(fp0, fp0); // Z = Y*Y
                fp3 = float64_to_floatx80(LIT64(0xBFB70BF398539E6A)); // C5
                fp2 = float64_to_floatx80(LIT64(0x3FBC7187962D1D7D)); // C4
                fp3 = floatx80_mul(fp3, fp1); // Z*C5
                fp2 = floatx80_mul(fp2, fp1); // Z*C4
                fp3 = floatx80_add(fp3, float64_to_floatx80(LIT64(0xBFC24924827107B8))); // C3+Z*C5
                fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FC999999996263E))); // C2+Z*C4
                fp1 = floatx80_mul(fp1, fp3); // Z*(C3+Z*C5)
                fp2 = floatx80_mul(fp2, fp0); // Y*(C2+Z*C4)
                fp1 = floatx80_add(fp1, float64_to_floatx80(LIT64(0xBFD5555555555536))); // C1+Z*(C3+Z*C5)
                fp0 = floatx80_mul(fp0, xsave); // X'*Y
                fp1 = floatx80_add(fp1, fp2); // [Y*(C2+Z*C4)]+[C1+Z*(C3+Z*C5)]
                fp0 = floatx80_mul(fp0, fp1); // X'*Y*([B1+Z*(B3+Z*B5)]+[Y*(B2+Z*(B4+Z*B6))]) ??
                fp0 = floatx80_add(fp0, xsave);
                fp1 = packFloatx80(aSign, piby2_exp, pi_sig);
                
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;

                a = floatx80_add(fp0, fp1);
                
                float_raise(float_flag_inexact);
                
                return a;
            }
        } else { // |X| < 1/16
            if (compact < 0x3FD78000) { // |X| < 2^(-40)
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;
                
                a = floatx80_move(a);
                
                float_raise(float_flag_inexact);
                
                return a;
            } else {
                fp0 = a;
                xsave = a;
                fp0 = floatx80_mul(fp0, fp0); // Y = X*X
                fp1 = floatx80_mul(fp0, fp0); // Z = Y*Y
                fp2 = float64_to_floatx80(LIT64(0x3FB344447F876989)); // B6
                fp3 = float64_to_floatx80(LIT64(0xBFB744EE7FAF45DB)); // B5
                fp2 = floatx80_mul(fp2, fp1); // Z*B6
                fp3 = floatx80_mul(fp3, fp1); // Z*B5
                fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FBC71C646940220))); // B4+Z*B6
                fp3 = floatx80_add(fp3, float64_to_floatx80(LIT64(0xBFC24924921872F9))); // B3+Z*B5
                fp2 = floatx80_mul(fp2, fp1); // Z*(B4+Z*B6)
                fp1 = floatx80_mul(fp1, fp3); // Z*(B3+Z*B5)
                fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FC9999999998FA9))); // B2+Z*(B4+Z*B6)
                fp1 = floatx80_add(fp1, float64_to_floatx80(LIT64(0xBFD5555555555555))); // B1+Z*(B3+Z*B5)
                fp2 = floatx80_mul(fp2, fp0); // Y*(B2+Z*(B4+Z*B6))
                fp0 = floatx80_mul(fp0, xsave); // X*Y
                fp1 = floatx80_add(fp1, fp2); // [B1+Z*(B3+Z*B5)]+[Y*(B2+Z*(B4+Z*B6))]
                fp0 = floatx80_mul(fp0, fp1); // X*Y*([B1+Z*(B3+Z*B5)]+[Y*(B2+Z*(B4+Z*B6))])
                
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;
                
                a = floatx80_add(fp0, xsave);
                
                float_raise(float_flag_inexact);
                
                return a;
            }
        }
    } else {
        aSig &= LIT64(0xF800000000000000);
        aSig |= LIT64(0x0400000000000000);
        
        xsave = packFloatx80(aSign, aExp, aSig); // F
        fp0 = a;
        fp1 = a; // X
        fp2 = packFloatx80(0, one_exp, one_sig); // 1
        fp1 = floatx80_mul(fp1, xsave); // X*F
        fp0 = floatx80_sub(fp0, xsave); // X-F
        fp1 = floatx80_add(fp1, fp2); // 1 + X*F
        fp0 = floatx80_div(fp0, fp1); // U = (X-F)/(1+X*F)
        
        tbl_index = compact;
        
        tbl_index &= 0x7FFF0000;
        tbl_index -= 0x3FFB0000;
        tbl_index >>= 1;
        tbl_index += compact&0x00007800;
        tbl_index >>= 11;
        
        fp3 = atan_tbl[tbl_index];
        
        fp3.high |= aSign ? 0x8000 : 0; // ATAN(F)
        
        fp1 = floatx80_mul(fp0, fp0); // V = U*U
        fp2 = float64_to_floatx80(LIT64(0xBFF6687E314987D8)); // A3
        fp2 = floatx80_add(fp2, fp1); // A3+V
        fp2 = floatx80_mul(fp2, fp1); // V*(A3+V)
        fp1 = floatx80_mul(fp1, fp0); // U*V
        fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x4002AC6934A26DB3))); // A2+V*(A3+V)
        fp1 = floatx80_mul(fp1, float64_to_floatx80(LIT64(0xBFC2476F4E1DA28E))); // A1+U*V
        fp1 = floatx80_mul(fp1, fp2); // A1*U*V*(A2+V*(A3+V))
        fp0 = floatx80_add(fp0, fp1); // ATAN(U)
        
        float_rounding_mode = user_rnd_mode;
        floatx80_rounding_precision = user_rnd_prec;
        
        a = floatx80_add(fp0, fp3); // ATAN(X)
        
        float_raise(float_flag_inexact);
        
        return a;
    }
}
#if 0
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
#endif
