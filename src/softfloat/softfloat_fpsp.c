
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
floatx80 floatx80_atanh(floatx80 a);
floatx80 floatx80_cosh(floatx80 a);
floatx80 floatx80_etox(floatx80 a);
floatx80 floatx80_etoxm1(floatx80 a);
floatx80 floatx80_tentox(floatx80 a);
floatx80 floatx80_twotox(floatx80 a);

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

/*----------------------------------------------------------------------------
 | Hyperbolic arc tangent
 *----------------------------------------------------------------------------*/

floatx80 floatx80_atanh(floatx80 a)
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
            float_raise(float_flag_divbyzero);
            return packFloatx80(aSign, 0x7FFF, floatx80_default_infinity_low);
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
    fp2 = packFloatx80(aSign, 0x3FFE, one_sig); // SIGN(X) * (1/2)
    fp0 = packFloatx80(0, aExp, aSig); // Y = |X|
    fp1 = packFloatx80(1, aExp, aSig); // -Y
    fp0 = floatx80_add(fp0, fp0); // 2Y
    fp1 = floatx80_add(fp1, one); // 1-Y
    fp0 = floatx80_div(fp0, fp1); // Z = 2Y/(1-Y)
    //fp0 = floatx80_lognp1(fp0); // LOG1P(Z)
    
    float_rounding_mode = user_rnd_mode;
    floatx80_rounding_precision = user_rnd_prec;
    
    a = floatx80_add(fp0, fp2); // ATANH(X) = SIGN(X) * (1/2) * LOG1P(Z)
    
    float_raise(float_flag_inexact);
    
    return a;
}
#if 0
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
#endif
/*----------------------------------------------------------------------------
 | Hyperbolic cosine
 *----------------------------------------------------------------------------*/

floatx80 floatx80_cosh(floatx80 a)
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
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(0, one_exp, one_sig); // zero
        if ((aSig & one_sig) == 0) { // denormal
            one = packFloatx80(0, one_exp, one_sig);
            a = floatx80_add(one, float32_to_floatx80(0x00800000));
            float_raise(float_flag_inexact);
            return a;
        }
    }
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;
    
    compact = floatx80_make_compact(aExp, aSig);
    
    if (compact > 0x400CB167) {
        if (compact > 0x400CB2B3) {
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;
            return roundAndPackFloatx80(floatx80_rounding_precision, 0, 0x8000, one_sig, 0);
        } else {
            fp0 = packFloatx80(0, aExp, aSig);
            fp0 = floatx80_sub(fp0, float64_to_floatx80(LIT64(0x40C62D38D3D64634)));
            fp0 = floatx80_sub(fp0, float64_to_floatx80(LIT64(0x3D6F90AEB1E75CC7)));
            fp0 = floatx80_etox(fp0);
            fp1 = packFloatx80(0, 0x7FFB, one_sig);
            
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;

            a = floatx80_mul(fp0, fp1);
            
            float_raise(float_flag_inexact);
            
            return a;
        }
    }
    
    fp0 = packFloatx80(0, aExp, aSig); // |X|
    fp0 = floatx80_etox(fp0); // EXP(|X|)
    fp0 = floatx80_mul(fp0, float32_to_floatx80(0x3F000000)); // (1/2)*EXP(|X|)
    fp1 = float32_to_floatx80(0x3E800000); // 1/4
    fp1 = floatx80_div(fp1, fp0); // 1/(2*EXP(|X|))
    
    float_rounding_mode = user_rnd_mode;
    floatx80_rounding_precision = user_rnd_prec;
    
    a = floatx80_add(fp0, fp1);
    
    float_raise(float_flag_inexact);
    
    return a;
}

/*----------------------------------------------------------------------------
 | e to x
 *----------------------------------------------------------------------------*/

floatx80 floatx80_etox(floatx80 a)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    int8 user_rnd_mode, user_rnd_prec;
    
    int32 compact, n, j, k, m, m1;
    floatx80 fp0, fp1, fp2, fp3, l2, scale, adjscale;
    flag adjflag;

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
        if ((aSig & one_sig) == 0) {
            float_raise(float_flag_inexact);
            return floatx80_add(float32_to_floatx80(0x3F800000),
                                float32_to_floatx80(aSign?0x80800000:0x00800000));
        }
    }
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;
    
    adjflag = 0;
    
    if (aExp >= 0x3FBE) { // |X| >= 2^(-65)
        compact = floatx80_make_compact(aExp, aSig);
        
        if (compact < 0x400CB167) { // |X| < 16380 log2
            fp0 = a;
            fp1 = a;
            fp0 = floatx80_mul(fp0, float32_to_floatx80(0x42B8AA3B)); // 64/log2 * X
            adjflag = 0;
            n = floatx80_to_int32(fp0); // int(64/log2*X)
            fp0 = int32_to_floatx80(n);
            
            j = n & 0x3F; // J = N mod 64
            m = n / 64; // NOTE: this is really arithmetic right shift by 6
            if (n < 0 && j) { // arithmetic right shift is division and round towards minus infinity
                m--;
            }
            m += 0x3FFF; // biased exponent of 2^(M)
            
        expcont1:
            fp2 = fp0; // N
            fp0 = floatx80_mul(fp0, float32_to_floatx80(0xBC317218)); // N * L1, L1 = lead(-log2/64)
            l2 = packFloatx80(0, 0x3FDC, LIT64(0x82E308654361C4C6));
            fp2 = floatx80_mul(fp2, l2); // N * L2, L1+L2 = -log2/64
            fp0 = floatx80_add(fp0, fp1); // X + N*L1
            fp0 = floatx80_add(fp0, fp2); // R
            
            fp1 = floatx80_mul(fp0, fp0); // S = R*R
            fp2 = float32_to_floatx80(0x3AB60B70); // A5
            fp2 = floatx80_mul(fp2, fp1); // fp2 is S*A5
            fp3 = floatx80_mul(float32_to_floatx80(0x3C088895), fp1); // fp3 is S*A4
            fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FA5555555554431))); // fp2 is A3+S*A5
            fp3 = floatx80_add(fp3, float64_to_floatx80(LIT64(0x3FC5555555554018))); // fp3 is A2+S*A4
            fp2 = floatx80_mul(fp2, fp1); // fp2 is S*(A3+S*A5)
            fp3 = floatx80_mul(fp3, fp1); // fp3 is S*(A2+S*A4)
            fp2 = floatx80_add(fp2, float32_to_floatx80(0x3F000000)); // fp2 is A1+S*(A3+S*A5)
            fp3 = floatx80_mul(fp3, fp0); // fp3 IS R*S*(A2+S*A4)
            fp2 = floatx80_mul(fp2, fp1); // fp2 IS S*(A1+S*(A3+S*A5))
            fp0 = floatx80_add(fp0, fp3); // fp0 IS R+R*S*(A2+S*A4)
            fp0 = floatx80_add(fp0, fp2); // fp0 IS EXP(R) - 1
            
            fp1 = exp_tbl[j];
            fp0 = floatx80_mul(fp0, fp1); // 2^(J/64)*(Exp(R)-1)
            fp0 = floatx80_add(fp0, float32_to_floatx80(exp_tbl2[j])); // accurate 2^(J/64)
            fp0 = floatx80_add(fp0, fp1); // 2^(J/64) + 2^(J/64)*(Exp(R)-1)
            
            scale = packFloatx80(0, m, one_sig);
            if (adjflag) {
                adjscale = packFloatx80(0, m1, one_sig);
                fp0 = floatx80_mul(fp0, adjscale);
            }
            
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;
            
            a = floatx80_mul(fp0, scale);
            
            float_raise(float_flag_inexact);
            
            return a;
        } else { // |X| >= 16380 log2
            if (compact > 0x400CB27C) { // |X| >= 16480 log2
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;
                if (aSign) {
                    a = roundAndPackFloatx80(floatx80_rounding_precision, 0, -0x1000, aSig, 0);
                } else {
                    a = roundAndPackFloatx80(floatx80_rounding_precision, 0, 0x8000, aSig, 0);
                }
                float_raise(float_flag_inexact);
                
                return a;
            } else {
                fp0 = a;
                fp1 = a;
                fp0 = floatx80_mul(fp0, float32_to_floatx80(0x42B8AA3B)); // 64/log2 * X
                adjflag = 1;
                n = floatx80_to_int32(fp0); // int(64/log2*X)
                fp0 = int32_to_floatx80(n);
                
                j = n & 0x3F; // J = N mod 64
                k = n / 64; // NOTE: this is really arithmetic right shift by 6
                if (n < 0 && j) { // arithmetic right shift is division and round towards minus infinity
                    k--;
                }
                m1 = k / 2; // NOTE: this is really arithmetic right shift by 1
                if (k < 0 && (k & 1)) { // arithmetic right shift is division and round towards minus infinity
                    m1--;
                }
                m = k - m1;
                m1 += 0x3FFF; // biased exponent of 2^(M1)
                m += 0x3FFF; // biased exponent of 2^(M)
                
                goto expcont1;
            }
        }
    } else { // |X| < 2^(-65)
        float_rounding_mode = user_rnd_mode;
        floatx80_rounding_precision = user_rnd_prec;
        
        a = floatx80_add(a, float32_to_floatx80(0x3F800000)); // 1 + X
        
        float_raise(float_flag_inexact);
        
        return a;
    }
}

/*----------------------------------------------------------------------------
 | e to x minus 1
 *----------------------------------------------------------------------------*/

floatx80 floatx80_etoxm1(floatx80 a)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    int8 user_rnd_mode, user_rnd_prec;
    
    int32 compact, n, j, m, m1;
    floatx80 fp0, fp1, fp2, fp3, l2, sc, onebysc;
    
    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        if (aSign) return packFloatx80(aSign, one_exp, one_sig);
        return packFloatx80(0, 0x7FFF, floatx80_default_infinity_low);
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0); // zero
        if ((aSig & one_sig) == 0) { // denormal
            normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
            return roundAndPackFloatx80(floatx80_rounding_precision, aSign, aExp, aSig, 0);
        }
    }
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;
    
    if (aExp >= 0x3FFD) { // |X| >= 1/4
        compact = floatx80_make_compact(aExp, aSig);
        
        if (compact <= 0x4004C215) { // |X| <= 70 log2
            fp0 = a;
            fp1 = a;
            fp0 = floatx80_mul(fp0, float32_to_floatx80(0x42B8AA3B)); // 64/log2 * X
            n = floatx80_to_int32(fp0); // int(64/log2*X)
            fp0 = int32_to_floatx80(n);
            
            j = n & 0x3F; // J = N mod 64
            m = n / 64; // NOTE: this is really arithmetic right shift by 6
            if (n < 0 && j) { // arithmetic right shift is division and round towards minus infinity
                m--;
            }
            m1 = -m;
            //m += 0x3FFF; // biased exponent of 2^(M)
            //m1 += 0x3FFF; // biased exponent of -2^(-M)
            
            fp2 = fp0; // N
            fp0 = floatx80_mul(fp0, float32_to_floatx80(0xBC317218)); // N * L1, L1 = lead(-log2/64)
            l2 = packFloatx80(0, 0x3FDC, LIT64(0x82E308654361C4C6));
            fp2 = floatx80_mul(fp2, l2); // N * L2, L1+L2 = -log2/64
            fp0 = floatx80_add(fp0, fp1); // X + N*L1
            fp0 = floatx80_add(fp0, fp2); // R
            
            fp1 = floatx80_mul(fp0, fp0); // S = R*R
            fp2 = float32_to_floatx80(0x3950097B); // A6
            fp2 = floatx80_mul(fp2, fp1); // fp2 is S*A6
            fp3 = floatx80_mul(float32_to_floatx80(0x3AB60B6A), fp1); // fp3 is S*A5
            fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3F81111111174385))); // fp2 IS A4+S*A6
            fp3 = floatx80_add(fp3, float64_to_floatx80(LIT64(0x3FA5555555554F5A))); // fp3 is A3+S*A5
            fp2 = floatx80_mul(fp2, fp1); // fp2 IS S*(A4+S*A6)
            fp3 = floatx80_mul(fp3, fp1); // fp3 IS S*(A3+S*A5)
            fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FC5555555555555))); // fp2 IS A2+S*(A4+S*A6)
            fp3 = floatx80_add(fp3, float32_to_floatx80(0x3F000000)); // fp3 IS A1+S*(A3+S*A5)
            fp2 = floatx80_mul(fp2, fp1); // fp2 IS S*(A2+S*(A4+S*A6))
            fp1 = floatx80_mul(fp1, fp3); // fp1 IS S*(A1+S*(A3+S*A5))
            fp2 = floatx80_mul(fp2, fp0); // fp2 IS R*S*(A2+S*(A4+S*A6))
            fp0 = floatx80_add(fp0, fp1); // fp0 IS R+S*(A1+S*(A3+S*A5))
            fp0 = floatx80_add(fp0, fp2); // fp0 IS EXP(R) - 1
            
            fp0 = floatx80_mul(fp0, exp_tbl[j]); // 2^(J/64)*(Exp(R)-1)
            
            if (m >= 64) {
                fp1 = float32_to_floatx80(exp_tbl2[j]);
                onebysc = packFloatx80(1, m1 + 0x3FFF, one_sig); // -2^(-M)
                fp1 = floatx80_add(fp1, onebysc);
                fp0 = floatx80_add(fp0, fp1);
                fp0 = floatx80_add(fp0, exp_tbl[j]);
            } else if (m < -3) {
                fp0 = floatx80_add(fp0, float32_to_floatx80(exp_tbl2[j]));
                fp0 = floatx80_add(fp0, exp_tbl[j]);
                onebysc = packFloatx80(1, m1 + 0x3FFF, one_sig); // -2^(-M)
                fp0 = floatx80_add(fp0, onebysc);
            } else { // -3 <= m <= 63
                fp1 = exp_tbl[j];
                fp0 = floatx80_add(fp0, float32_to_floatx80(exp_tbl2[j]));
                onebysc = packFloatx80(1, m1 + 0x3FFF, one_sig); // -2^(-M)
                fp1 = floatx80_add(fp1, onebysc);
                fp0 = floatx80_add(fp0, fp1);
            }
            
            sc = packFloatx80(0, m + 0x3FFF, one_sig);
            
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;
            
            a = floatx80_mul(fp0, sc);
            
            float_raise(float_flag_inexact);
            
            return a;
        } else { // |X| > 70 log2
            if (aSign) {
                fp0 = float32_to_floatx80(0xBF800000); // -1
                
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;
                
                a = floatx80_add(fp0, float32_to_floatx80(0x00800000)); // -1 + 2^(-126)
                
                float_raise(float_flag_inexact);
                
                return a;
            } else {
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;
                
                return floatx80_etox(a);
            }
        }
    } else { // |X| < 1/4
        if (aExp >= 0x3FBE) {
            fp0 = a;
            fp0 = floatx80_mul(fp0, fp0); // S = X*X
            fp1 = float32_to_floatx80(0x2F30CAA8); // B12
            fp1 = floatx80_mul(fp1, fp0); // S * B12
            fp2 = float32_to_floatx80(0x310F8290); // B11
            fp1 = floatx80_add(fp1, float32_to_floatx80(0x32D73220)); // B10
            fp2 = floatx80_mul(fp2, fp0);
            fp1 = floatx80_mul(fp1, fp0);
            fp2 = floatx80_add(fp2, float32_to_floatx80(0x3493F281)); // B9
            fp1 = floatx80_add(fp1, float64_to_floatx80(LIT64(0x3EC71DE3A5774682))); // B8
            fp2 = floatx80_mul(fp2, fp0);
            fp1 = floatx80_mul(fp1, fp0);
            fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3EFA01A019D7CB68))); // B7
            fp1 = floatx80_add(fp1, float64_to_floatx80(LIT64(0x3F2A01A01A019DF3))); // B6
            fp2 = floatx80_mul(fp2, fp0);
            fp1 = floatx80_mul(fp1, fp0);
            fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3F56C16C16C170E2))); // B5
            fp1 = floatx80_add(fp1, float64_to_floatx80(LIT64(0x3F81111111111111))); // B4
            fp2 = floatx80_mul(fp2, fp0);
            fp1 = floatx80_mul(fp1, fp0);
            fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FA5555555555555))); // B3
            fp3 = packFloatx80(0, 0x3FFC, LIT64(0xAAAAAAAAAAAAAAAB));
            fp1 = floatx80_add(fp1, fp3); // B2
            fp2 = floatx80_mul(fp2, fp0);
            fp1 = floatx80_mul(fp1, fp0);
            
            fp2 = floatx80_mul(fp2, fp0);
            fp1 = floatx80_mul(fp1, a);
            
            fp0 = floatx80_mul(fp0, float32_to_floatx80(0x3F000000)); // S*B1
            fp1 = floatx80_add(fp1, fp2); // Q
            fp0 = floatx80_add(fp0, fp1); // S*B1+Q
            
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;
            
            a = floatx80_add(fp0, a);
            
            float_raise(float_flag_inexact);
            
            return a;
        } else { // |X| < 2^(-65)
            sc = packFloatx80(1, 1, one_sig);
            fp0 = a;

            if (aExp < 0x0033) { // |X| < 2^(-16382)
                fp0 = floatx80_mul(fp0, float64_to_floatx80(LIT64(0x48B0000000000000)));
                fp0 = floatx80_add(fp0, sc);
                
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;

                a = floatx80_mul(fp0, float64_to_floatx80(LIT64(0x3730000000000000)));
            } else {
                float_rounding_mode = user_rnd_mode;
                floatx80_rounding_precision = user_rnd_prec;
                
                a = floatx80_add(fp0, sc);
            }
            
            float_raise(float_flag_inexact);
            
            return a;
        }
    }
}
#if 0
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
#endif
/*----------------------------------------------------------------------------
 | 10 to x
 *----------------------------------------------------------------------------*/

floatx80 floatx80_tentox(floatx80 a)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    int8 user_rnd_mode, user_rnd_prec;
    
    int32 compact, n, j, l, m, m1;
    floatx80 fp0, fp1, fp2, fp3, adjfact, fact1, fact2;
    
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
        if ((aSig & one_sig) == 0) { // denormal
            fp0 = float32_to_floatx80(0x3F800000);
            a = floatx80_add(fp0, float32_to_floatx80(0x00800001|aSign?0x80000000:0));
            float_raise(float_flag_inexact);
            return a;
        }
    }
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;
    
    fp0 = a;
    
    compact = floatx80_make_compact(aExp, aSig);
    
    if (compact < 0x3FB98000 || compact > 0x400B9B07) { // |X| > 16480 LOG2/LOG10 or |X| < 2^(-70)
        if (compact > 0x3FFF8000) { // |X| > 16480
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;
            
            if (aSign) {
                return roundAndPackFloatx80(floatx80_rounding_precision, 0, -0x1000, aSig, 0);
            } else {
                return roundAndPackFloatx80(floatx80_rounding_precision, 0, 0x8000, aSig, 0);
            }
        } else { // |X| < 2^(-70)
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;
            
            a = floatx80_add(fp0, float32_to_floatx80(0x3F800000)); // 1 + X
            
            float_raise(float_flag_inexact);
            
            return a;
        }
    } else { // 2^(-70) <= |X| <= 16480 LOG 2 / LOG 10
        fp1 = fp0; // X
        fp1 = floatx80_mul(fp1, float64_to_floatx80(LIT64(0x406A934F0979A371))); // X*64*LOG10/LOG2
        n = floatx80_to_int32(fp1); // N=INT(X*64*LOG10/LOG2)
        fp1 = int32_to_floatx80(n);

        j = n & 0x3F;
        l = n / 64; // NOTE: this is really arithmetic right shift by 6
        if (n < 0 && j) { // arithmetic right shift is division and round towards minus infinity
            l--;
        }
        m = l / 2; // NOTE: this is really arithmetic right shift by 1
        if (l < 0 && (l & 1)) { // arithmetic right shift is division and round towards minus infinity
            m--;
        }
        m1 = l - m;
        m1 += 0x3FFF; // ADJFACT IS 2^(M')

        adjfact = packFloatx80(0, m1, one_sig);
        fact1 = exp2_tbl[j];
        fact1.high += m;
        fact2.high = exp2_tbl2[j]>>16;
        fact2.high += m;
        fact2.low = (bits64)(exp2_tbl2[j] & 0xFFFF);
        fact2.low <<= 48;
        
        fp2 = fp1; // N
        fp1 = floatx80_mul(fp1, float64_to_floatx80(LIT64(0x3F734413509F8000))); // N*(LOG2/64LOG10)_LEAD
        fp3 = packFloatx80(1, 0x3FCD, LIT64(0xC0219DC1DA994FD2));
        fp2 = floatx80_mul(fp2, fp3); // N*(LOG2/64LOG10)_TRAIL
        fp0 = floatx80_sub(fp0, fp1); // X - N L_LEAD
        fp0 = floatx80_sub(fp0, fp2); // X - N L_TRAIL
        fp2 = packFloatx80(0, 0x4000, LIT64(0x935D8DDDAAA8AC17)); // LOG10
        fp0 = floatx80_mul(fp0, fp2); // R
        
        // EXPR
        fp1 = floatx80_mul(fp0, fp0); // S = R*R
        fp2 = float64_to_floatx80(LIT64(0x3F56C16D6F7BD0B2)); // A5
        fp3 = float64_to_floatx80(LIT64(0x3F811112302C712C)); // A4
        fp2 = floatx80_mul(fp2, fp1); // S*A5
        fp3 = floatx80_mul(fp3, fp1); // S*A4
        fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FA5555555554CC1))); // A3+S*A5
        fp3 = floatx80_add(fp3, float64_to_floatx80(LIT64(0x3FC5555555554A54))); // A2+S*A4
        fp2 = floatx80_mul(fp2, fp1); // S*(A3+S*A5)
        fp3 = floatx80_mul(fp3, fp1); // S*(A2+S*A4)
        fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FE0000000000000))); // A1+S*(A3+S*A5)
        fp3 = floatx80_mul(fp3, fp0); // R*S*(A2+S*A4)
        
        fp2 = floatx80_mul(fp2, fp1); // S*(A1+S*(A3+S*A5))
        fp0 = floatx80_add(fp0, fp3); // R+R*S*(A2+S*A4)
        fp0 = floatx80_add(fp0, fp2); // EXP(R) - 1
        
        fp0 = floatx80_mul(fp0, fact1);
        fp0 = floatx80_add(fp0, fact2);
        fp0 = floatx80_add(fp0, fact1);
        
        float_rounding_mode = user_rnd_mode;
        floatx80_rounding_precision = user_rnd_prec;
        
        a = floatx80_mul(fp0, adjfact);
        
        float_raise(float_flag_inexact);
        
        return a;
    }
}

/*----------------------------------------------------------------------------
 | 2 to x
 *----------------------------------------------------------------------------*/

floatx80 floatx80_twotox(floatx80 a)
{
    flag aSign;
    int32 aExp;
    bits64 aSig;
    
    int8 user_rnd_mode, user_rnd_prec;
    
    int32 compact, n, j, l, m, m1;
    floatx80 fp0, fp1, fp2, fp3, adjfact, fact1, fact2;
    
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
        if ((aSig & one_sig) == 0) { // denormal
            fp0 = float32_to_floatx80(0x3F800000);
            a = floatx80_add(fp0, float32_to_floatx80(0x00800001|aSign?0x80000000:0));
            float_raise(float_flag_inexact);
            return a;
        }
    }
    
    user_rnd_mode = float_rounding_mode;
    user_rnd_prec = floatx80_rounding_precision;
    
    fp0 = a;
    
    compact = floatx80_make_compact(aExp, aSig);
    
    if (compact < 0x3FB98000 || compact > 0x400D80C0) { // |X| > 16480 or |X| < 2^(-70)
        if (compact > 0x3FFF8000) { // |X| > 16480
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;
            
            if (aSign) {
                return roundAndPackFloatx80(floatx80_rounding_precision, 0, -0x1000, aSig, 0);
            } else {
                return roundAndPackFloatx80(floatx80_rounding_precision, 0, 0x8000, aSig, 0);
            }
        } else { // |X| < 2^(-70)
            float_rounding_mode = user_rnd_mode;
            floatx80_rounding_precision = user_rnd_prec;
            
            a = floatx80_add(fp0, float32_to_floatx80(0x3F800000)); // 1 + X
            
            float_raise(float_flag_inexact);
            
            return a;
        }
    } else { // 2^(-70) <= |X| <= 16480
        fp1 = fp0; // X
        fp1 = floatx80_mul(fp1, float32_to_floatx80(0x42800000)); // X * 64
        n = floatx80_to_int32(fp1);
        fp1 = int32_to_floatx80(n);
        j = n & 0x3F;
        l = n / 64; // NOTE: this is really arithmetic right shift by 6
        if (n < 0 && j) { // arithmetic right shift is division and round towards minus infinity
            l--;
        }
        m = l / 2; // NOTE: this is really arithmetic right shift by 1
        if (l < 0 && (l & 1)) { // arithmetic right shift is division and round towards minus infinity
            m--;
        }
        m1 = l - m;
        m1 += 0x3FFF; // ADJFACT IS 2^(M')
        
        adjfact = packFloatx80(0, m1, one_sig);
        fact1 = exp2_tbl[j];
        fact1.high += m;
        fact2.high = exp2_tbl2[j]>>16;
        fact2.high += m;
        fact2.low = (bits64)(exp2_tbl2[j] & 0xFFFF);
        fact2.low <<= 48;
        
        fp1 = floatx80_mul(fp1, float32_to_floatx80(0x3C800000)); // (1/64)*N
        fp0 = floatx80_sub(fp0, fp1); // X - (1/64)*INT(64 X)
        fp2 = packFloatx80(0, 0x3FFE, LIT64(0xB17217F7D1CF79AC)); // LOG2
        fp0 = floatx80_mul(fp0, fp2); // R
        
        // EXPR
        fp1 = floatx80_mul(fp0, fp0); // S = R*R
        fp2 = float64_to_floatx80(LIT64(0x3F56C16D6F7BD0B2)); // A5
        fp3 = float64_to_floatx80(LIT64(0x3F811112302C712C)); // A4
        fp2 = floatx80_mul(fp2, fp1); // S*A5
        fp3 = floatx80_mul(fp3, fp1); // S*A4
        fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FA5555555554CC1))); // A3+S*A5
        fp3 = floatx80_add(fp3, float64_to_floatx80(LIT64(0x3FC5555555554A54))); // A2+S*A4
        fp2 = floatx80_mul(fp2, fp1); // S*(A3+S*A5)
        fp3 = floatx80_mul(fp3, fp1); // S*(A2+S*A4)
        fp2 = floatx80_add(fp2, float64_to_floatx80(LIT64(0x3FE0000000000000))); // A1+S*(A3+S*A5)
        fp3 = floatx80_mul(fp3, fp0); // R*S*(A2+S*A4)
        
        fp2 = floatx80_mul(fp2, fp1); // S*(A1+S*(A3+S*A5))
        fp0 = floatx80_add(fp0, fp3); // R+R*S*(A2+S*A4)
        fp0 = floatx80_add(fp0, fp2); // EXP(R) - 1
        
        fp0 = floatx80_mul(fp0, fact1);
        fp0 = floatx80_add(fp0, fact2);
        fp0 = floatx80_add(fp0, fact1);
        
        float_rounding_mode = user_rnd_mode;
        floatx80_rounding_precision = user_rnd_prec;
        
        a = floatx80_mul(fp0, adjfact);
        
        float_raise(float_flag_inexact);
        
        return a;
    }
}
