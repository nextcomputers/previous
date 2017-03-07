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

static int fp_rnd_prec = 80;

/* Functions for setting host/library modes and getting status */
STATIC_INLINE void set_fp_mode(uae_u32 mode_control)
{
    switch(mode_control & FPCR_ROUNDING_PRECISION) {
        case FPCR_PRECISION_EXTENDED: // X
            fp_rnd_prec = 80;
            break;
        case FPCR_PRECISION_SINGLE:   // S
            fp_rnd_prec = 32;
            break;
        case FPCR_PRECISION_DOUBLE:   // D
        default:                      // undefined
            fp_rnd_prec = 64;
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
    /* FIXME: how to detect SNAN? */
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

STATIC_INLINE void to_single(fptype *fp, uae_u32 wrd1)
{
    union {
        float f;
        uae_u32 u;
    } val;
    
    val.u = wrd1;
    *fp = (fptype) val.f;
}
STATIC_INLINE uae_u32 from_single(fptype *fp)
{
    union {
        float f;
        uae_u32 u;
    } val;
    
    val.f = (float) *fp;
    return val.u;
}

STATIC_INLINE void to_double(fptype *fp, uae_u32 wrd1, uae_u32 wrd2)
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
STATIC_INLINE void from_double(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2)
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
STATIC_INLINE void to_exten(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
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
STATIC_INLINE void from_exten(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
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
STATIC_INLINE void to_exten(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
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
STATIC_INLINE void from_exten(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
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
STATIC_INLINE void to_exten_fmovem(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    to_exten(fp, wrd1, wrd2, wrd3);
}
STATIC_INLINE void from_exten_fmovem(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
{
    from_exten(fp, wrd1, wrd2, wrd3);
}

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

/* Functions for returning exception state data */
/* (almost impossible to emulate using native floats) */
STATIC_INLINE fptype fp_get_internal_overflow(void)
{
    return 0.0;
}
STATIC_INLINE fptype fp_get_internal_underflow(void)
{
    return 0.0;
}
STATIC_INLINE fptype fp_get_internal_weired(void)
{
    return 0.0;
}
STATIC_INLINE fptype fp_get_internal_round_exten(void)
{
    return 0.0;
}
STATIC_INLINE uae_u32 fp_get_internal_grs(void)
{
    return 0.0;
}

/* Functions for rounding */

// round to float with extended precision exponent
STATIC_INLINE void fp_round32(fptype *fp)
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
STATIC_INLINE void fp_round64(fptype *fp)
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
STATIC_INLINE void fp_round_single(fptype *fp)
{
    *fp = (float) *fp;
}

// round to double
STATIC_INLINE void fp_round_double(fptype *fp)
{
    *fp = (double) *fp;
}

// round to selected precision
STATIC_INLINE void fp_round(fptype *fp)
{
    switch(fp_rnd_prec) {
        case 32:
            *fp = (float) *fp;
            break;
        case 64:
            *fp = (double) *fp;
            break;
        default:
            break;
    }
}


/* Arithmetic functions */

#ifdef USE_LONG_DOUBLE

STATIC_INLINE void fp_move(fptype *a, fptype *b)
{
    *a = *b;
    fp_round(a);
}
STATIC_INLINE void fp_move_single(fptype *a, fptype *b)
{
    *a = *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_move_double(fptype *a, fptype *b)
{
    *a = *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_int(fptype *a, fptype *b)
{
#ifdef USE_HOST_ROUNDING
    *a = rintl(*b);
#else
    switch (regs.fpcr & FPCR_ROUNDING_MODE)
    {
        case FPCR_ROUND_NEAR:
            *a = fp_round_to_nearest(*b);
            break;
        case FPCR_ROUND_ZERO:
            *a = fp_round_to_zero(*b);
            break;
        case FPCR_ROUND_MINF:
            *a = fp_round_to_minus_infinity(*b);
            break;
        case FPCR_ROUND_PINF:
            *a = fp_round_to_plus_infinity(*b);
            break;
        default: /* never reached */
            break;
    }
#endif
    fp_round(a);
}
STATIC_INLINE void fp_sinh(fptype *a, fptype *b)
{
    *a = sinhl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_intrz(fptype *a, fptype *b)
{
#ifdef USE_HOST_ROUNDING
    *a = truncl(*b);
#else
    *a = fp_round_to_zero (*b);
#endif
    fp_round(a);
}
STATIC_INLINE void fp_sqrt(fptype *a, fptype *b)
{
    *a = sqrtl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_sqrt_single(fptype *a, fptype *b)
{
    *a = sqrtl(*b);
    fp_round_single(a);
}
STATIC_INLINE void fp_sqrt_double(fptype *a, fptype *b)
{
    *a = sqrtl(*b);
    fp_round_double(a);
}
STATIC_INLINE void fp_lognp1(fptype *a, fptype *b)
{
    *a = log1pl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_etoxm1(fptype *a, fptype *b)
{
    *a = expm1l(*b);
    fp_round(a);
}
STATIC_INLINE void fp_tanh(fptype *a, fptype *b)
{
    *a = tanhl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_atan(fptype *a, fptype *b)
{
    *a = atanl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_asin(fptype *a, fptype *b)
{
    *a = asinl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_atanh(fptype *a, fptype *b)
{
    *a = atanhl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_sin(fptype *a, fptype *b)
{
    *a = sinl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_tan(fptype *a, fptype *b)
{
    *a = tanl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_etox(fptype *a, fptype *b)
{
    *a = expl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_twotox(fptype *a, fptype *b)
{
    *a = powl(2.0, *b);
    fp_round(a);
}
STATIC_INLINE void fp_tentox(fptype *a, fptype *b)
{
    *a = powl(10.0, *b);
    fp_round(a);
}
STATIC_INLINE void fp_logn(fptype *a, fptype *b)
{
    *a = logl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_log10(fptype *a, fptype *b)
{
    *a = log10l(*b);
    fp_round(a);
}
STATIC_INLINE void fp_log2(fptype *a, fptype *b)
{
    *a = log2l(*b);
    fp_round(a);
}
STATIC_INLINE void fp_abs(fptype *a, fptype *b)
{
    *a = fabsl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_abs_single(fptype *a, fptype *b)
{
    *a = fabsl(*b);
    fp_round_single(a);
}
STATIC_INLINE void fp_abs_double(fptype *a, fptype *b)
{
    *a = fabsl(*b);
    fp_round_double(a);
}
STATIC_INLINE void fp_cosh(fptype *a, fptype *b)
{
    *a = coshl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_neg(fptype *a, fptype *b)
{
    *a = -(*b);
    fp_round(a);
}
STATIC_INLINE void fp_neg_single(fptype *a, fptype *b)
{
    *a = -(*b);
    fp_round_single(a);
}
STATIC_INLINE void fp_neg_double(fptype *a, fptype *b)
{
    *a = -(*b);
    fp_round_double(a);
}
STATIC_INLINE void fp_acos(fptype *a, fptype *b)
{
    *a = acosl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_cos(fptype *a, fptype *b)
{
    *a = cosl(*b);
    fp_round(a);
}
STATIC_INLINE void fp_getexp(fptype *a, fptype *b)
{
    int expon;
    frexpl(*b, &expon);
    *a = (long double) (expon - 1);
    fp_round(a);
}
STATIC_INLINE void fp_getman(fptype *a, fptype *b)
{
    int expon;
    *a = frexpl(*b, &expon) * 2.0;
    fp_round(a);
}
STATIC_INLINE void fp_div(fptype *a, fptype *b)
{
    *a /= *b;
    fp_round(a);
}
STATIC_INLINE void fp_div_single(fptype *a, fptype *b)
{
    *a /= *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_div_double(fptype *a, fptype *b)
{
    *a /= *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_mod(fptype *a, fptype *b, uae_u64 *q, uae_s8 *s)
{
    fptype quot;
#ifdef USE_HOST_ROUNDING
    quot = truncl(*a / *b);
#else
    quot = fp_round_to_zero(*a / *b);
#endif
    if (quot < 0.0) {
        *s = 1;
        quot = -quot;
    } else {
        *s = 0;
    }
    *q = (uae_u64)quot;
    *a = fmodl(*a, *b);
    fp_round(a);
}
STATIC_INLINE void fp_add(fptype *a, fptype *b)
{
    *a += *b;
    fp_round(a);
}
STATIC_INLINE void fp_add_single(fptype *a, fptype *b)
{
    *a += *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_add_double(fptype *a, fptype *b)
{
    *a += *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_mul(fptype *a, fptype *b)
{
    *a *= *b;
    fp_round(a);
}
STATIC_INLINE void fp_mul_single(fptype *a, fptype *b)
{
    *a *= *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_mul_double(fptype *a, fptype *b)
{
    *a *= *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_sgldiv(fptype *a, fptype *b)
{
    fptype z;
    float mant;
    int expon;
    z = *a / *b;
    
    mant = (float)(frexpl(z, &expon) * 2.0);
    *a = ldexpl((fptype)mant, expon - 1);
}
STATIC_INLINE void fp_rem(fptype *a, fptype *b, uae_u64 *q, uae_s8 *s)
{
    fptype quot;
#ifdef USE_HOST_ROUNDING
    quot = roundl(*a / *b);
#else
    quot = fp_round_to_nearest(*a / *b);
#endif
    if (quot < 0.0) {
        *s = 1;
        quot = -quot;
    } else {
        *s = 0;
    }
    *q = (uae_u64)quot;
    *a = remainderl(*a, *b);
    fp_round(a);
}
STATIC_INLINE void fp_scale(fptype *a, fptype *b)
{
    *a = ldexpl(*a, (int) *b);
    fp_round(a);
}
STATIC_INLINE void fp_sglmul(fptype *a, fptype *b)
{
    fptype z;
    float mant;
    int expon;
    /* FIXME: truncate mantissa of a and b to single precision */
    z = *a * (*b);

    mant = (float)(frexpl(z, &expon) * 2.0);
    *a = ldexpl((fptype)mant, expon - 1);
}
STATIC_INLINE void fp_sub(fptype *a, fptype *b)
{
    *a -= *b;
    fp_round(a);
}
STATIC_INLINE void fp_sub_single(fptype *a, fptype *b)
{
    *a -= *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_sub_double(fptype *a, fptype *b)
{
    *a -= *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_cmp(fptype *a, fptype *b)
{
    *a = (*a - *b); /* FIXME: comparing is different from subtraction */
}
STATIC_INLINE void fp_tst(fptype *a, fptype *b)
{
    *a = *b; /* FIXME: test b */
}

#else // if !USE_LONG_DOUBLE

STATIC_INLINE void fp_move(fptype *a, fptype *b)
{
    *a = *b;
    fp_round(a);
}
STATIC_INLINE void fp_move_single(fptype *a, fptype *b)
{
    *a = *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_move_double(fptype *a, fptype *b)
{
    *a = *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_int(fptype *a, fptype *b)
{
#ifdef USE_HOST_ROUNDING
    *a = rint(*b);
#else
    switch (regs.fpcr & FPCR_ROUNDING_MODE)
    {
        case FPCR_ROUND_NEAR:
            *a = fp_round_to_nearest(*b);
            break;
        case FPCR_ROUND_ZERO:
            *a = fp_round_to_zero(*b);
            break;
        case FPCR_ROUND_MINF:
            *a = fp_round_to_minus_infinity(*b);
            break;
        case FPCR_ROUND_PINF:
            *a = fp_round_to_plus_infinity(*b);
            break;
        default: /* never reached */
            break;
    }
#endif
    fp_round(a);
}
STATIC_INLINE void fp_sinh(fptype *a, fptype *b)
{
    *a = sinh(*b);
    fp_round(a);
}
STATIC_INLINE void fp_intrz(fptype *a, fptype *b)
{
#ifdef USE_HOST_ROUNDING
    *a = trunc(*b);
#else
    *a = fp_round_to_zero (*b);
#endif
    fp_round(a);
}
STATIC_INLINE void fp_sqrt(fptype *a, fptype *b)
{
    *a = sqrt(*b);
    fp_round(a);
}
STATIC_INLINE void fp_sqrt_single(fptype *a, fptype *b)
{
    *a = sqrt(*b);
    fp_round_single(a);
}
STATIC_INLINE void fp_sqrt_double(fptype *a, fptype *b)
{
    *a = sqrt(*b);
    fp_round_double(a);
}
STATIC_INLINE void fp_lognp1(fptype *a, fptype *b)
{
    *a = log1p(*b);
    fp_round(a);
}
STATIC_INLINE void fp_etoxm1(fptype *a, fptype *b)
{
    *a = expm1(*b);
    fp_round(a);
}
STATIC_INLINE void fp_tanh(fptype *a, fptype *b)
{
    *a = tanh(*b);
    fp_round(a);
}
STATIC_INLINE void fp_atan(fptype *a, fptype *b)
{
    *a = atan(*b);
    fp_round(a);
}
STATIC_INLINE void fp_asin(fptype *a, fptype *b)
{
    *a = asin(*b);
    fp_round(a);
}
STATIC_INLINE void fp_atanh(fptype *a, fptype *b)
{
    *a = atanh(*b);
    fp_round(a);
}
STATIC_INLINE void fp_sin(fptype *a, fptype *b)
{
    *a = sin(*b);
    fp_round(a);
}
STATIC_INLINE void fp_tan(fptype *a, fptype *b)
{
    *a = tan(*b);
    fp_round(a);
}
STATIC_INLINE void fp_etox(fptype *a, fptype *b)
{
    *a = exp(*b);
    fp_round(a);
}
STATIC_INLINE void fp_twotox(fptype *a, fptype *b)
{
    *a = pow(2.0, *b);
    fp_round(a);
}
STATIC_INLINE void fp_tentox(fptype *a, fptype *b)
{
    *a = pow(10.0, *b);
    fp_round(a);
}
STATIC_INLINE void fp_logn(fptype *a, fptype *b)
{
    *a = log(*b);
    fp_round(a);
}
STATIC_INLINE void fp_log10(fptype *a, fptype *b)
{
    *a = log10(*b);
    fp_round(a);
}
STATIC_INLINE void fp_log2(fptype *a, fptype *b)
{
    *a = log2(*b);
    fp_round(a);
}
STATIC_INLINE void fp_abs(fptype *a, fptype *b)
{
    *a = fabs(*b);
    fp_round(a);
}
STATIC_INLINE void fp_abs_single(fptype *a, fptype *b)
{
    *a = fabs(*b);
    fp_round_single(a);
}
STATIC_INLINE void fp_abs_double(fptype *a, fptype *b)
{
    *a = fabs(*b);
    fp_round_double(a);
}
STATIC_INLINE void fp_cosh(fptype *a, fptype *b)
{
    *a = cosh(*b);
    fp_round(a);
}
STATIC_INLINE void fp_neg(fptype *a, fptype *b)
{
    *a = -(*b);
    fp_round(a);
}
STATIC_INLINE void fp_neg_single(fptype *a, fptype *b)
{
    *a = -(*b);
    fp_round_single(a);
}
STATIC_INLINE void fp_neg_double(fptype *a, fptype *b)
{
    *a = -(*b);
    fp_round_double(a);
}
STATIC_INLINE void fp_acos(fptype *a, fptype *b)
{
    *a = acos(*b);
    fp_round(a);
}
STATIC_INLINE void fp_cos(fptype *a, fptype *b)
{
    *a = cos(*b);
    fp_round(a);
}
STATIC_INLINE void fp_getexp(fptype *a, fptype *b)
{
    int expon;
    frexp(*b, &expon);
    *a = (long double) (expon - 1);
    fp_round(a);
}
STATIC_INLINE void fp_getman(fptype *a, fptype *b)
{
    int expon;
    *a = frexp(*b, &expon) * 2.0;
    fp_round(a);
}
STATIC_INLINE void fp_div(fptype *a, fptype *b)
{
    *a /= *b;
    fp_round(a);
}
STATIC_INLINE void fp_div_single(fptype *a, fptype *b)
{
    *a /= *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_div_double(fptype *a, fptype *b)
{
    *a /= *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_mod(fptype *a, fptype *b, uae_u64 *q, uae_s8 *s)
{
    fptype quot;
#ifdef USE_HOST_ROUNDING
    quot = trunc(*a / *b);
#else
    quot = fp_round_to_zero(*a / *b);
#endif
    if (quot < 0.0) {
        *s = 1;
        quot = -quot;
    } else {
        *s = 0;
    }
    *q = (uae_u64)quot;
    *a = fmod(*a, *b);
    fp_round(a);
}
STATIC_INLINE void fp_add(fptype *a, fptype *b)
{
    *a += *b;
    fp_round(a);
}
STATIC_INLINE void fp_add_single(fptype *a, fptype *b)
{
    *a += *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_add_double(fptype *a, fptype *b)
{
    *a += *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_mul(fptype *a, fptype *b)
{
    *a *= *b;
    fp_round(a);
}
STATIC_INLINE void fp_mul_single(fptype *a, fptype *b)
{
    *a *= *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_mul_double(fptype *a, fptype *b)
{
    *a *= *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_sgldiv(fptype *a, fptype *b)
{
    fptype z;
    float mant;
    int expon;
    z = *a / *b;
    
    mant = (float)(frexp(z, &expon) * 2.0);
    *a = ldexp((fptype)mant, expon - 1);
}
STATIC_INLINE void fp_rem(fptype *a, fptype *b, uae_u64 *q, uae_s8 *s)
{
    fptype quot;
#ifdef USE_HOST_ROUNDING
    quot = round(*a / *b);
#else
    quot = fp_round_to_nearest(*a / *b);
#endif
    if (quot < 0.0) {
        *s = 1;
        quot = -quot;
    } else {
        *s = 0;
    }
    *q = (uae_u64)quot;
    *a = remainder(*a, *b);
    fp_round(a);
}
STATIC_INLINE void fp_scale(fptype *a, fptype *b)
{
    *a = ldexp(*a, (int) *b);
    fp_round(a);
}
STATIC_INLINE void fp_sglmul(fptype *a, fptype *b)
{
    fptype z;
    float mant;
    int expon;
    /* FIXME: truncate mantissa of a and b to single precision */
    z = *a * (*b);
    
    mant = (float)(frexp(z, &expon) * 2.0);
    *a = ldexp((fptype)mant, expon - 1);
}
STATIC_INLINE void fp_sub(fptype *a, fptype *b)
{
    *a -= *b;
    fp_round(a);
}
STATIC_INLINE void fp_sub_single(fptype *a, fptype *b)
{
    *a -= *b;
    fp_round_single(a);
}
STATIC_INLINE void fp_sub_double(fptype *a, fptype *b)
{
    *a -= *b;
    fp_round_double(a);
}
STATIC_INLINE void fp_cmp(fptype *a, fptype *b)
{
    *a = (*a - *b); /* FIXME: comparing is different from subtraction */
}
STATIC_INLINE void fp_tst(fptype *a, fptype *b)
{
    *a = *b; /* FIXME: test b */
}

#endif // !USE_LONG_DOUBLE

#endif
