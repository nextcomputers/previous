/*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68881 emulation
  *
  * Conversion routines for hosts knowing floating point format.
  *
  * Copyright 1996 Herman ten Brugge
  * Modified 2005 Peter Keunecke
  */

#ifndef FPP_H
#define FPP_H

#define __USE_ISOC9X  /* We might be able to pick up a NaN */

#include <math.h>
#include <float.h>
#include <fenv.h>

//#pragma STDC FENV_ACCESS on

#define USE_HOST_ROUNDING

#define	FPCR_ROUNDING_MODE	0x00000030
#define	FPCR_ROUND_NEAR		0x00000000
#define	FPCR_ROUND_ZERO		0x00000010
#define	FPCR_ROUND_MINF		0x00000020
#define	FPCR_ROUND_PINF		0x00000030

#define	FPCR_ROUNDING_PRECISION	0x000000c0
#define	FPCR_PRECISION_SINGLE	0x00000040
#define	FPCR_PRECISION_DOUBLE	0x00000080
#define FPCR_PRECISION_EXTENDED	0x00000000

extern uae_u32 fpp_get_fpsr (void);
extern void to_single(fptype *fp, uae_u32 wrd1);
extern void to_double(fptype *fp, uae_u32 wrd1, uae_u32 wrd2);
extern void to_exten(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3);
extern void normalize_exten (uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3);


/* Functions for setting host/library modes and getting status */
STATIC_INLINE void set_fp_mode(uae_u32 mode_control)
{
    switch(mode_control & FPCR_ROUNDING_PRECISION) {
        case FPCR_PRECISION_EXTENDED: // X
            break;
        case FPCR_PRECISION_SINGLE:   // S
            break;
        case FPCR_PRECISION_DOUBLE:   // D
        default:                      // undefined
            break;
    }
#ifdef USE_HOST_ROUNDING
    switch(mode_control & FPCR_ROUNDING_MODE) {
        case FPCR_ROUND_NEAR: // to neareset
            fesetround(FE_TONEAREST);
            break;
        case FPCR_ROUND_ZERO: // to zero
            fesetround(FE_TOWARDZERO);
            break;
        case FPCR_ROUND_MINF: // to minus
            fesetround(FE_DOWNWARD);
            break;
        case FPCR_ROUND_PINF: // to plus
            fesetround(FE_UPWARD);
            break;
    }
    return;
#endif
}
STATIC_INLINE void get_fp_status(uae_u32 *status)
{
    int exp_flags = fetestexcept(FE_ALL_EXCEPT);
    if (exp_flags) {
        if (exp_flags & FE_INEXACT)
            *status |= 0x0200;
        if (exp_flags & FE_DIVBYZERO)
            *status |= 0x0400;
        if (exp_flags & FE_UNDERFLOW)
            *status |= 0x0800;
        if (exp_flags & FE_OVERFLOW)
            *status |= 0x1000;
        if (exp_flags & FE_INVALID)
            *status |= 0x2000;
    }
}
STATIC_INLINE void clear_fp_status(void)
{
    feclearexcept (FE_ALL_EXCEPT);
}

/* Helper functions */
STATIC_INLINE const char *fp_print(fptype *fx)
{
    static char fs[32];
    bool n, d;
    
    n = signbit(*fx) ? 1 : 0;
    d = isnormal(*fx) ? 0 : 1;
    
    if (isinf(*fx)) {
        sprintf(fs, "%c%s", n?'-':'+', "inf");
    } else if (isnan(*fx)) {
        sprintf(fs, "%c%s", n?'-':'+', "nan");
    } else {
        if (n)
            *fx *= -1.0;
#ifdef USE_LONG_DOUBLE
        sprintf(fs, "%c%#.17Le%s%s", n?'-':'+', *fx, "", d?"D":"");
#else
        sprintf(fs, "%c%#.17e%s%s", n?'-':'+', *fx, "", d?"D":"");
#endif
    }
    
    return fs;
}

/* Functions for detecting float type */
STATIC_INLINE bool fp_is_snan(fptype *fp)
{
    return 0; /* FIXME: how to detect SNAN */
}
STATIC_INLINE void fp_unset_snan(fptype *fp)
{
    /* FIXME: how to unset SNAN */
}
STATIC_INLINE bool fp_is_nan (fptype *fp)
{
    return isnan(*fp) != 0;
}
STATIC_INLINE bool fp_is_infinity (fptype *fp)
{
    return isinf(*fp) != 0;
}
STATIC_INLINE bool fp_is_zero(fptype *fp)
{
    return (*fp == 0.0);
}
STATIC_INLINE bool fp_is_neg(fptype *fp)
{
    return signbit(*fp) != 0;
}
STATIC_INLINE bool fp_is_denormal(fptype *fp)
{
    return false;
    //return (isnormal(*fp) == 0); /* FIXME: how to differ denormal/unnormal? */
}
STATIC_INLINE bool fp_is_unnormal(fptype *fp)
{
    return false;
    //return (isnormal(*fp) == 0); /* FIXME: how to differ denormal/unnormal? */
}

/* Function for normalizing unnormals FIXME: how to do this with native floats? */
STATIC_INLINE void fp_normalize(fptype *fp)
{
}

/* Functions for converting between float formats */
/* FIXME: how to preserve/fix denormals and unnormals? */

STATIC_INLINE void to_native(long double *fp, fptype fpx)
{
    *fp = fpx;
}
STATIC_INLINE void from_native(long double fp, fptype *fpx)
{
    *fpx = fp;
}

STATIC_INLINE void to_single_xn(fptype *fp, uae_u32 wrd1)
{
    union {
        float f;
        uae_u32 u;
    } val;
    
    val.u = wrd1;
    *fp = (fptype) val.f;
}
STATIC_INLINE void to_single_x(fptype *fp, uae_u32 wrd1)
{
    union {
        float f;
        uae_u32 u;
    } val;
    
    val.u = wrd1;
    *fp = (fptype) val.f;
}
STATIC_INLINE uae_u32 from_single_x(fptype *fp)
{
    union {
        float f;
        uae_u32 u;
    } val;
    
    val.f = (float) *fp;
    return val.u;
}

STATIC_INLINE void to_double_xn(fptype *fp, uae_u32 wrd1, uae_u32 wrd2)
{
    union {
        double d;
        uae_u32 u[2];
    } val;
    
#ifdef WORDS_BIGENDIAN
    val.u[0] = wrd1;
    val.u[1] = wrd2;
#else
    val.u[1] = wrd1;
    val.u[0] = wrd2;
#endif
    *fp = (fptype) val.d;
}
STATIC_INLINE void to_double_x(fptype *fp, uae_u32 wrd1, uae_u32 wrd2)
{
    union {
        double d;
        uae_u32 u[2];
    } val;
    
#ifdef WORDS_BIGENDIAN
    val.u[0] = wrd1;
    val.u[1] = wrd2;
#else
    val.u[1] = wrd1;
    val.u[0] = wrd2;
#endif
    *fp = (fptype) val.d;
}
STATIC_INLINE void from_double_x(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2)
{
    union {
        double d;
        uae_u32 u[2];
    } val;
    
    val.d = (double) *fp;
#ifdef WORDS_BIGENDIAN
    *wrd1 = val.u[0];
    *wrd2 = val.u[1];
#else
    *wrd1 = val.u[1];
    *wrd2 = val.u[0];
#endif
}
#ifdef USE_LONG_DOUBLE
STATIC_INLINE void to_exten_x(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    union {
        long double ld;
        uae_u32 u[3];
    } val;

#if WORDS_BIGENDIAN
    val.u[0] = (wrd1 & 0xffff0000) | ((wrd2 & 0xffff0000) >> 16);
    val.u[1] = (wrd2 & 0x0000ffff) | ((wrd3 & 0xffff0000) >> 16);
    val.u[2] = (wrd3 & 0x0000ffff) << 16;
#else
    val.u[0] = wrd3;
    val.u[1] = wrd2;
    val.u[2] = wrd1 >> 16;
#endif
    *fp = val.ld;
}
STATIC_INLINE void from_exten_x(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
{
    union {
        long double ld;
        uae_u32 u[3];
    } val;
    
    val.ld = *fp;
#if WORDS_BIGENDIAN
    *wrd1 = val.u[0] & 0xffff0000;
    *wrd2 = ((val.u[0] & 0x0000ffff) << 16) | ((val.u[1] & 0xffff0000) >> 16);
    *wrd3 = ((val.u[1] & 0x0000ffff) << 16) | ((val.u[2] & 0xffff0000) >> 16);
#else
    *wrd3 = val.u[0];
    *wrd2 = val.u[1];
    *wrd1 = val.u[2] << 16;
#endif
}
#else // if !USE_LONG_DOUBLE
static const double twoto32 = 4294967296.0;
STATIC_INLINE void to_exten_x(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    double frac;
    if ((wrd1 & 0x7fff0000) == 0 && wrd2 == 0 && wrd3 == 0) {
        *fp = (wrd1 & 0x80000000) ? -0.0 : +0.0;
        return;
    }
    frac = ((double)wrd2 + ((double)wrd3 / twoto32)) / 2147483648.0;
    if (wrd1 & 0x80000000)
        frac = -frac;
    *fp = ldexp (frac, ((wrd1 >> 16) & 0x7fff) - 16383);
}
STATIC_INLINE void from_exten_x(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
{
    int expon;
    double frac;
    fptype v;
    
    v = *fp;
    if (v == 0.0) {
        *wrd1 = signbit(v) ? 0x80000000 : 0;
        *wrd2 = 0;
        *wrd3 = 0;
        return;
    }
    if (v < 0) {
        *wrd1 = 0x80000000;
        v = -v;
    } else {
        *wrd1 = 0;
    }
    frac = frexp (v, &expon);
    frac += 0.5 / (twoto32 * twoto32);
    if (frac >= 1.0) {
        frac /= 2.0;
        expon++;
    }
    *wrd1 |= (((expon + 16383 - 1) & 0x7fff) << 16);
    *wrd2 = (uae_u32) (frac * twoto32);
    *wrd3 = (uae_u32) ((frac * twoto32 - *wrd2) * twoto32);
}
#endif // !USE_LONG_DOUBLE

#ifndef USE_HOST_ROUNDING
#ifdef USE_LONG_DOUBLE
#define fp_round_to_minus_infinity(x) floorl(x)
#define fp_round_to_plus_infinity(x) ceill(x)
#define fp_round_to_zero(x)	((x) >= 0.0 ? floorl(x) : ceill(x))
#define fp_round_to_nearest(x) roundl(x)
#else // if !USE_LONG_DOUBLE
#define fp_round_to_minus_infinity(x) floor(x)
#define fp_round_to_plus_infinity(x) ceil(x)
#define fp_round_to_zero(x)	((x) >= 0.0 ? floor(x) : ceil(x))
#define fp_round_to_nearest(x) round(x)
#endif // !USE_LONG_DOUBLE
#endif // USE_HOST_ROUNDING

STATIC_INLINE uae_s64 to_int(fptype *src, int size)
{
    static fptype fxsizes[6] =
    {
               -128.0,        127.0,
             -32768.0,      32767.0,
        -2147483648.0, 2147483647.0
    };
    
    if (*src < fxsizes[size * 2 + 0])
        *src = fxsizes[size * 2 + 0];
    if (*src > fxsizes[size * 2 + 1])
        *src = fxsizes[size * 2 + 1];
#ifdef USE_HOST_ROUNDING
#ifdef USE_LONG_DOUBLE
    return lrintl(*src);
#else
    return lrint(*src);
#endif
#else
    switch (regs.fpcr & FPCR_ROUNDING_MODE)
    {
        case FPCR_ROUND_ZERO:
            return fp_round_to_zero (*src);
        case FPCR_ROUND_MINF:
            return fp_round_to_minus_infinity (*src);
        case FPCR_ROUND_NEAR:
            return fp_round_to_nearest (*src);
        case FPCR_ROUND_PINF:
            return fp_round_to_plus_infinity (*src);
        default:
            return (int) *src;
    }
#endif
}
STATIC_INLINE fptype from_int(uae_s32 src)
{
    return (fptype) src;
}

/* Functions for rounding */

// round to float with extended precision exponent
STATIC_INLINE void fp_roundsgl(fptype *fp)
{
    int expon;
    float mant;
#ifdef USE_LONG_DOUBLE
    mant = (float)(frexpl(*fp, &expon) * 2.0);
    *fp = ldexpl((fptype)mant, expon - 1);
#else
    mant = (float)(frexp(*fp, &expon) * 2.0);
    *fp = ldexp((fptype)mant, expon - 1);
#endif
}

// round to double with extended precision exponent
STATIC_INLINE void fp_rounddbl(fptype *fp)
{
    int expon;
    double mant;
#ifdef USE_LONG_DOUBLE
    mant = (double)(frexpl(*fp, &expon) * 2.0);
    *fp = ldexpl((fptype)mant, expon - 1);
#else
    mant = (double)(frexp(*fp, &expon) * 2.0);
    *fp = ldexp((fptype)mant, expon - 1);
#endif
}

// round to float
STATIC_INLINE void fp_round32(fptype *fp)
{
    *fp = (float) *fp;
}

// round to double
STATIC_INLINE void fp_round64(fptype *fp)
{
    *fp = (double) *fp;
}

/* Arithmetic functions */

#ifdef USE_LONG_DOUBLE

STATIC_INLINE fptype fp_move(fptype a)
{
    return a;
}
STATIC_INLINE fptype fp_int(fptype a)
{
#ifdef USE_HOST_ROUNDING
    return rintl(a);
#else
    switch (regs.fpcr & FPCR_ROUNDING_MODE)
    {
        case FPCR_ROUND_NEAR:
            return fp_round_to_nearest(a);
        case FPCR_ROUND_ZERO:
            return fp_round_to_zero(a);
        case FPCR_ROUND_MINF:
            return fp_round_to_minus_infinity(a);
        case FPCR_ROUND_PINF:
            return fp_round_to_plus_infinity(a);
        default: /* never reached */
            return a;
    }
#endif
}
STATIC_INLINE fptype fp_sinh(fptype a)
{
    return sinhl(a);
}
STATIC_INLINE fptype fp_intrz(fptype a)
{
#ifdef USE_HOST_ROUNDING
    return truncl(a);
#else
    return fp_round_to_zero (a);
#endif
}
STATIC_INLINE fptype fp_sqrt(fptype a)
{
    return sqrtl(a);
}
STATIC_INLINE fptype fp_lognp1(fptype a)
{
    return logl(a + 1.0);
}
STATIC_INLINE fptype fp_etoxm1(fptype a)
{
    return expl(a) - 1.0;
}
STATIC_INLINE fptype fp_tanh(fptype a)
{
    return tanhl(a);
}
STATIC_INLINE fptype fp_atan(fptype a)
{
    return atanl(a);
}
STATIC_INLINE fptype fp_asin(fptype a)
{
    return asinl(a);
}
STATIC_INLINE fptype fp_atanh(fptype a)
{
    return atanhl(a);
}
STATIC_INLINE fptype fp_sin(fptype a)
{
    return sinl(a);
}
STATIC_INLINE fptype fp_tan(fptype a)
{
    return tanl(a);
}
STATIC_INLINE fptype fp_etox(fptype a)
{
    return expl(a);
}
STATIC_INLINE fptype fp_twotox(fptype a)
{
    return powl(2.0, a);
}
STATIC_INLINE fptype fp_tentox(fptype a)
{
    return powl(10.0, a);
}
STATIC_INLINE fptype fp_logn(fptype a)
{
    return logl(a);
}
STATIC_INLINE fptype fp_log10(fptype a)
{
    return log10l(a);
}
STATIC_INLINE fptype fp_log2(fptype a)
{
    return log2l(a);
}
STATIC_INLINE fptype fp_abs(fptype a)
{
    return a < 0.0 ? -a : a;
}
STATIC_INLINE fptype fp_cosh(fptype a)
{
    return coshl(a);
}
STATIC_INLINE fptype fp_neg(fptype a)
{
    return -a;
}
STATIC_INLINE fptype fp_acos(fptype a)
{
    return acosl(a);
}
STATIC_INLINE fptype fp_cos(fptype a)
{
    return cosl(a);
}
STATIC_INLINE fptype fp_getexp(fptype a)
{
    int expon;
    frexpl(a, &expon);
    return (long double) (expon - 1);
}
STATIC_INLINE fptype fp_getman(fptype a)
{
    int expon;
    return frexpl(a, &expon) * 2.0;
}
STATIC_INLINE fptype fp_div(fptype a, fptype b)
{
    return (a / b);
}
STATIC_INLINE fptype fp_mod(fptype a, fptype b, uae_u64 *q, uae_s8 *s)
{
    fptype quot;
#ifdef USE_HOST_ROUNDING
    quot = truncl(a / b);
#else
    quot = fp_round_to_zero(a / b);
#endif
    if (quot < 0.0) {
        *s = 1;
        quot = -quot;
    } else {
        *s = 0;
    }
    *q = (uae_u64)quot;
    return fmodl(a, b);
}
STATIC_INLINE fptype fp_add(fptype a, fptype b)
{
    return (a + b);
}
STATIC_INLINE fptype fp_mul(fptype a, fptype b)
{
    return (a * b);
}
STATIC_INLINE fptype fp_sgldiv(fptype a, fptype b)
{
    fptype z;
    float mant;
    int expon;
    z = a / b;
    
    mant = (float)(frexpl(z, &expon) * 2.0);
    return ldexpl((fptype)mant, expon - 1);
}
STATIC_INLINE fptype fp_rem(fptype a, fptype b, uae_u64 *q, uae_s8 *s)
{
    fptype quot;
#ifdef USE_HOST_ROUNDING
    quot = roundl(a / b);
#else
    quot = fp_round_to_nearest(a / b);
#endif
    if (quot < 0.0) {
        *s = 1;
        quot = -quot;
    } else {
        *s = 0;
    }
    *q = (uae_u64)quot;
    return remainderl(a, b);
}
STATIC_INLINE fptype fp_scale(fptype a, fptype b)
{
    return ldexpl(a, (int) b);
}
STATIC_INLINE fptype fp_sglmul(fptype a, fptype b)
{
    fptype z;
    float mant;
    int expon;
    /* FIXME: truncate mantissa of a and b to single precision */
    z = a * b;

    mant = (float)(frexpl(z, &expon) * 2.0);
    return ldexpl((fptype)mant, expon - 1);
}
STATIC_INLINE fptype fp_sub(fptype a, fptype b)
{
    return (a - b);
}
STATIC_INLINE fptype fp_cmp(fptype a, fptype b)
{
    return (a - b); /* FIXME: comparing is different from subtraction */
}

#else // if !USE_LONG_DOUBLE

#define fp_move(a) (a)
STATIC_INLINE fptype fp_int(fptype a)
{
#ifdef USE_HOST_ROUNDING
    return rint(a);
#else
    switch (regs.fpcr & FPCR_ROUNDING_MODE)
    {
        case FPCR_ROUND_NEAR:
            return fp_round_to_nearest(a);
        case FPCR_ROUND_ZERO:
            return fp_round_to_zero(a);
        case FPCR_ROUND_MINF:
            return fp_round_to_minus_infinity(a);
        case FPCR_ROUND_PINF:
            return fp_round_to_plus_infinity(a);
        default: /* never reached */
            return a;
    }
#endif
}
#define fp_sinh(a) sinh(a)
STATIC_INLINE fptype fp_intrz(fptype a)
{
#ifdef USE_HOST_ROUNDING
    return trunc(a);
#else
    return fp_round_to_zero (a);
#endif
}
#define fp_sqrt(a)      sqrt(a)
#define fp_lognp1(a)    log((a) + 1.0)
#define fp_etoxm1(a)    (exp(a) - 1.0)
#define fp_tanh(a)      tanh(a)
#define fp_atan(a)      atan(a)
#define fp_asin(a)      asin(a)
#define fp_atanh(a)     atanh(a)
#define fp_sin(a)       sin(a)
#define fp_tan(a)       tan(a)
#define fp_etox(a)      exp(a)
#define fp_twotox(a)    pow(2.0, a)
#define fp_tentox(a)    pow(10.0, a)
#define fp_logn(a)      log(a)
#define fp_log10(a)     log10(a)
#define fp_log2(a)      log2(a)
#define fp_abs(a)       ((a) < 0.0 ? -(a) : (a))
#define fp_cosh(a)      cosh(a)
#define fp_neg(a)       (-(a))
#define fp_acos(a)      acos(a)
#define fp_cos(a)       cos(a)

STATIC_INLINE fptype fp_getexp(fptype a)
{
    int expon;
    frexp(a, &expon);
    return (double) (expon - 1);
}
STATIC_INLINE fptype fp_getman(fptype a)
{
    int expon;
    return frexp(a, &expon) * 2.0;
}
#define fp_div(a, b)    ((a) / (b))

STATIC_INLINE fptype fp_mod(fptype a, fptype b, uae_u64 *q, uae_s8 *s)
{
    fptype quot;
#ifdef USE_HOST_ROUNDING
    quot = trunc(a / b);
#else
    quot = fp_round_to_zero(a / b);
#endif
    if (quot < 0.0) {
        *s = 1;
        quot = -quot;
    } else {
        *s = 0;
    }
    *q = (uae_u64)quot;
    return fmod(a, b);
}
#define fp_add(a, b)    ((a) + (b))
#define fp_mul(a, b)    ((a) * (b))

STATIC_INLINE fptype fp_sgldiv(fptype a, fptype b)
{
    fptype z;
    float mant;
    int expon;
    z = a / b;
    
    mant = (float)(frexp(z, &expon) * 2.0);
    return ldexp((fptype)mant, expon - 1);
}
STATIC_INLINE fptype fp_rem(fptype a, fptype b, uae_u64 *q, uae_s8 *s)
{
    fptype quot;
#ifdef USE_HOST_ROUNDING
    quot = round(a / b);
#else
    quot = fp_round_to_nearest(a / b);
#endif
    if (quot < 0.0) {
        *s = 1;
        quot = -quot;
    } else {
        *s = 0;
    }
    *q = (uae_u64)quot;
    return remainder(a, b);
}
#define fp_scale(a, b)  ldexp(a, (int)(b))

STATIC_INLINE fptype fp_sglmul(fptype a, fptype b)
{
    fptype z;
    float mant;
    int expon;
    /* FIXME: truncate mantissa of a and b to single precision */
    z = a * b;
    
    mant = (float)(frexp(z, &expon) * 2.0);
    return ldexp((fptype)mant, expon - 1);
}
#define fp_sub(a, b)    ((a) - (b))
#define fp_cmp(a, b)    ((a) - (b)) /* FIXME: comparing is different from subtraction */

#endif // !USE_LONG_DOUBLE

#endif
