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

#include <softfloat.h>

float_ctrl fp_ctrl;

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


/* Functions for setting host/library modes and getting status */
STATIC_INLINE void init_fp_mode(int fpu_model)
{
    float_init(&fp_ctrl);
    
    if (fpu_model == 68040) {
        set_special_flags(cmp_signed_nan, &fp_ctrl);
    } else if (fpu_model == 68060) {
        set_special_flags(infinity_clear_intbit, &fp_ctrl);
    } else {
        set_special_flags(addsub_swap_inf, &fp_ctrl);
    }
}
STATIC_INLINE void set_fp_mode(uae_u32 mode_control)
{
    set_float_detect_tininess(float_tininess_before_rounding, &fp_ctrl);
    
    switch(mode_control & FPCR_ROUNDING_PRECISION) {
        case FPCR_PRECISION_SINGLE: // single
            set_float_rounding_precision(32, &fp_ctrl);
            break;
        case FPCR_PRECISION_DOUBLE: // double
            set_float_rounding_precision(64, &fp_ctrl);
            break;
        case FPCR_PRECISION_EXTENDED: // extended
            set_float_rounding_precision(80, &fp_ctrl);
            break;
        default: // double
            set_float_rounding_precision(64, &fp_ctrl);
            break;
    }

    switch(mode_control & FPCR_ROUNDING_MODE) {
        case FPCR_ROUND_NEAR: // to neareset
            set_float_rounding_mode(float_round_nearest_even, &fp_ctrl);
            break;
        case FPCR_ROUND_ZERO: // to zero
            set_float_rounding_mode(float_round_to_zero, &fp_ctrl);
            break;
        case FPCR_ROUND_MINF: // to minus
            set_float_rounding_mode(float_round_down, &fp_ctrl);
            break;
        case FPCR_ROUND_PINF: // to plus
            set_float_rounding_mode(float_round_up, &fp_ctrl);
            break;
    }
}
STATIC_INLINE void get_fp_status(uae_u32 *status)
{
    int8 flags = get_float_exception_flags(&fp_ctrl);
    
    if (flags & float_flag_signaling)
        *status |= 0x4000;
    if (flags & float_flag_invalid)
        *status |= 0x2000;
    if (flags & float_flag_overflow)
        *status |= 0x1000;
    if (flags & float_flag_underflow)
        *status |= 0x0800;
    if (flags & float_flag_divbyzero)
        *status |= 0x0400;
    if (flags & float_flag_inexact)
        *status |= 0x0200;
    if (flags & float_flag_decimal)
        *status |= 0x0100;
}
STATIC_INLINE void clear_fp_status(void)
{
    set_float_exception_flags(0, &fp_ctrl);
}

/* Helper functions */
STATIC_INLINE const char *fp_print(fptype *fx)
{
    static char fs[32];
    bool n, u, d;
    fptype x;
    int32 len;
    int8 save_exception_flags;
    
    n = floatx80_is_negative(*fx);
    u = floatx80_is_unnormal(*fx);
    d = floatx80_is_denormal(*fx);

    if (floatx80_is_infinity(*fx)) {
        sprintf(fs, "%c%s", n?'-':'+', "inf");
    } else if (floatx80_is_signaling_nan(*fx)) {
        sprintf(fs, "%c%s", n?'-':'+', "snan");
    } else if (floatx80_is_nan(*fx)) {
        sprintf(fs, "%c%s", n?'-':'+', "nan");
    } else {
        len = 17;
        save_exception_flags = get_float_exception_flags(&fp_ctrl);
        set_float_exception_flags(0, &fp_ctrl);
        x = floatx80_to_floatdecimal(*fx, &len, &fp_ctrl);
        
        sprintf(fs, "%c%01lld.%016llde%c%04d%s%s", n?'-':'+',
                x.low/LIT64(10000000000000000), x.low%LIT64(10000000000000000),
                (x.high&0x4000)?'-':'+', x.high&0x3FFF, d?"D":u?"U":"",
                (get_float_exception_flags(&fp_ctrl)&float_flag_inexact)?"~":"");

        set_float_exception_flags(save_exception_flags, &fp_ctrl);
    }
    
    return fs;
}

/* Functions for detecting float type */
STATIC_INLINE bool fp_is_snan(fptype *fp)
{
    return floatx80_is_signaling_nan(*fp) != 0;
}
STATIC_INLINE void fp_unset_snan(fptype *fp)
{
    fp->low |= LIT64(0x4000000000000000);
}
STATIC_INLINE bool fp_is_nan (fptype *fp)
{
    return floatx80_is_nan(*fp) != 0;
}
STATIC_INLINE bool fp_is_infinity (fptype *fp)
{
    return floatx80_is_infinity(*fp) != 0;
}
STATIC_INLINE bool fp_is_zero(fptype *fp)
{
    return floatx80_is_zero(*fp) != 0;
}
STATIC_INLINE bool fp_is_neg(fptype *fp)
{
    return floatx80_is_negative(*fp) != 0;
}
STATIC_INLINE bool fp_is_denormal(fptype *fp)
{
    return floatx80_is_denormal(*fp) != 0;
}
STATIC_INLINE bool fp_is_unnormal(fptype *fp)
{
    return floatx80_is_unnormal(*fp) != 0;
}

/* Function for normalizing unnormals */
STATIC_INLINE void fp_normalize(fptype *fp)
{
    *fp = floatx80_normalize(*fp);
}

/* Functions for converting between float formats */
STATIC_INLINE void to_single(fptype *fp, uae_u32 wrd1)
{
    float32 f = wrd1;
    *fp = float32_to_floatx80_allowunnormal(f);
}
STATIC_INLINE uae_u32 from_single(fptype *fp)
{
    float32 f = floatx80_to_float32(*fp, &fp_ctrl);
    return f;
}

STATIC_INLINE void to_double(fptype *fp, uae_u32 wrd1, uae_u32 wrd2)
{
    float64 f = ((float64)wrd1 << 32) | wrd2;
    *fp = float64_to_floatx80_allowunnormal(f);
}
STATIC_INLINE void from_double(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2)
{
    float64 f = floatx80_to_float64(*fp, &fp_ctrl);
    *wrd1 = f >> 32;
    *wrd2 = (uae_u32)f;
}

STATIC_INLINE void to_exten(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    fp->high = (uae_u16)(wrd1 >> 16);
    fp->low = ((uae_u64)wrd2 << 32) | wrd3;
}
STATIC_INLINE void from_exten(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
{
    floatx80 f = floatx80_to_floatx80(*fp, &fp_ctrl);
    *wrd1 = (uae_u32)(f.high << 16);
    *wrd2 = f.low >> 32;
    *wrd3 = (uae_u32)f.low;
}
STATIC_INLINE void to_exten_fmovem(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    fp->high = (uae_u16)(wrd1 >> 16);
    fp->low = ((uae_u64)wrd2 << 32) | wrd3;
}
STATIC_INLINE void from_exten_fmovem(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
{
    *wrd1 = (uae_u32)(fp->high << 16);
    *wrd2 = fp->low >> 32;
    *wrd3 = (uae_u32)fp->low;
}

STATIC_INLINE uae_s64 to_int(fptype *src, int size)
{
    switch (size) {
        case 0: return floatx80_to_int8(*src, &fp_ctrl);
        case 1: return floatx80_to_int16(*src, &fp_ctrl);
        case 2: return floatx80_to_int32(*src, &fp_ctrl);
        default: return 0;
    }
}
STATIC_INLINE fptype from_int(uae_s32 src)
{
    return int32_to_floatx80(src);
}

STATIC_INLINE void to_pack(fptype *fp, uae_u32 *wrd)
{
    floatx80 f;
    int i;
    uae_s32 exp;
    uae_s64 mant;
    uae_u32 pack_exp, pack_int, pack_se, pack_sm;
    uae_u64 pack_frac;
    
    if (((wrd[0] >> 16) & 0x7fff) == 0x7fff) {
        // infinity has extended exponent and all 0 packed fraction
        // nans are copies bit by bit
        to_exten(fp, wrd[0], wrd[1], wrd[2]);
        return;
    }
    if (!(wrd[0] & 0xf) && !wrd[1] && !wrd[2]) {
        // exponent is not cared about, if mantissa is zero
        wrd[0] &= 0x80000000;
        to_exten(fp, wrd[0], wrd[1], wrd[2]);
        return;
    }
    
    pack_exp = (wrd[0] >> 16) & 0xFFF;              // packed exponent
    pack_int = wrd[0] & 0xF;                        // packed integer part
    pack_frac = ((uae_u64)wrd[1] << 32) | wrd[2];   // packed fraction
    pack_se = (wrd[0] >> 30) & 1;                   // sign of packed exponent
    pack_sm = (wrd[0] >> 31) & 1;                   // sign of packed significand
    
    exp = 0;
    
    for (i = 0; i < 3; i++) {
        exp *= 10;
        exp += (pack_exp >> (8 - i * 4)) & 0xF;
    }
    
    if (pack_se) {
        exp = -exp;
    }
    
    exp -= 16;
    
    if (exp < 0) {
        exp = -exp;
        pack_se = 1;
    }
    
    mant = pack_int;
    
    for (i = 0; i < 16; i++) {
        mant *= 10;
        mant += (pack_frac >> (60 - i * 4)) & 0xF;
    }

    f.high = exp & 0x3FFF;
    f.high |= pack_se ? 0x4000 : 0;
    f.high |= pack_sm ? 0x8000 : 0;
    f.low = mant;
    
    *fp = floatdecimal_to_floatx80(f, &fp_ctrl);
}
STATIC_INLINE void from_pack(fptype *fp, uae_u32 *wrd, uae_s32 kfactor)
{
    floatx80 f = floatx80_to_floatdecimal(*fp, &kfactor, &fp_ctrl);
    
    uae_u32 pack_exp, pack_exp4, pack_int, pack_se, pack_sm;
    uae_u64 pack_frac;
    
    uae_u32 exponent;
    uae_u64 significand;

    uae_s32 len;
    uae_u64 digit;
    
    if ((f.high & 0x7FFF) == 0x7FFF) {
        wrd[0] = (uae_u32)(f.high << 16);
        wrd[1] = f.low >> 32;
        wrd[2] = (uae_u32)f.low;
    } else {
        exponent = f.high & 0x3FFF;
        significand = f.low;
        
        pack_int = 0;
        pack_frac = 0;
        len = kfactor; // SoftFloat saved len to kfactor variable
        while (len > 0) {
            len--;
            digit = significand % 10;
            significand /= 10;
            if (len == 0) {
                pack_int = digit;
            } else {
                pack_frac |= digit << (64 - len * 4);
            }
        }
        
        pack_exp = 0;
        pack_exp4 = 0;
        len = 4;
        while (len > 0) {
            len--;
            digit = exponent % 10;
            exponent /= 10;
            if (len == 0) {
                pack_exp4 = digit;
            } else {
                pack_exp |= digit << (12 - len * 4);
            }
        }
        
        pack_se = f.high & 0x4000;
        pack_sm = f.high & 0x8000;
        
        wrd[0] = pack_exp << 16;
        wrd[0] |= pack_exp4 << 12;
        wrd[0] |= pack_int;
        wrd[0] |= pack_se ? 0x40000000 : 0;
        wrd[0] |= pack_sm ? 0x80000000 : 0;
        
        wrd[1] = pack_frac >> 32;
        wrd[2] = pack_frac & 0xffffffff;
    }
}

/* Functions for returning exception state data */
STATIC_INLINE fptype fp_get_internal_overflow(void)
{
    return getFloatInternalOverflow(&fp_ctrl);
}
STATIC_INLINE fptype fp_get_internal_underflow(void)
{
    return getFloatInternalUnderflow(&fp_ctrl);
}
STATIC_INLINE fptype fp_get_internal_round_all(void)
{
    return getFloatInternalRoundedAll(&fp_ctrl);
}
STATIC_INLINE fptype fp_get_internal_round(void)
{
    return getFloatInternalRoundedSome(&fp_ctrl);
}
STATIC_INLINE fptype fp_get_internal_round_exten(void)
{
    return getFloatInternalFloatx80(&fp_ctrl);
}
STATIC_INLINE fptype fp_get_internal(void)
{
    return getFloatInternalUnrounded(&fp_ctrl);
}
STATIC_INLINE uae_u32 fp_get_internal_grs(void)
{
    return (uae_u32)getFloatInternalGRS(&fp_ctrl);
}

/* Function for denormalizing */
STATIC_INLINE void fp_denormalize(fptype *fp, int esign)
{
    *fp = floatx80_denormalize(*fp, esign);
}

/* Functions for rounding */

// round to float with extended precision exponent
STATIC_INLINE void fp_round32(fptype *fp)
{
    *fp = floatx80_round32(*fp, &fp_ctrl);
}

// round to double with extended precision exponent
STATIC_INLINE void fp_round64(fptype *fp)
{
    *fp = floatx80_round64(*fp, &fp_ctrl);
}

// round to float
STATIC_INLINE void fp_round_single(fptype *fp)
{
    *fp = floatx80_round_to_float32(*fp, &fp_ctrl);
}

// round to double
STATIC_INLINE void fp_round_double(fptype *fp)
{
    *fp = floatx80_round_to_float64(*fp, &fp_ctrl);
}

// round to selected precision
STATIC_INLINE void fp_round(fptype *a)
{
    switch(get_float_rounding_precision(&fp_ctrl)) {
        case 32:
            *a = floatx80_round_to_float32(*a, &fp_ctrl);
            break;
        case 64:
            *a = floatx80_round_to_float64(*a, &fp_ctrl);
            break;
        default:
            break;
    }
}

/* Arithmetic functions */
STATIC_INLINE void fp_move(fptype *a, fptype *b)
{
    *a = floatx80_move(*b, &fp_ctrl);
}
STATIC_INLINE void fp_int(fptype *a, fptype *b)
{
    *a = floatx80_round_to_int(*b, &fp_ctrl);
}
STATIC_INLINE void fp_intrz(fptype *a, fptype *b)
{
    *a = floatx80_round_to_int_toward_zero(*b, &fp_ctrl);
}
STATIC_INLINE void fp_sqrt(fptype *a, fptype *b)
{
    *a = floatx80_sqrt(*b, &fp_ctrl);
}
STATIC_INLINE void fp_abs(fptype *a, fptype *b)
{
    *a = floatx80_abs(*b, &fp_ctrl);
}
STATIC_INLINE void fp_neg(fptype *a, fptype *b)
{
    *a = floatx80_neg(*b, &fp_ctrl);
}
STATIC_INLINE void fp_getexp(fptype *a, fptype *b)
{
    *a = floatx80_getexp(*b, &fp_ctrl);
}
STATIC_INLINE void fp_getman(fptype *a, fptype *b)
{
    *a = floatx80_getman(*b, &fp_ctrl);
}
STATIC_INLINE void fp_div(fptype *a, fptype *b)
{
    *a = floatx80_div(*a, *b, &fp_ctrl);
}
STATIC_INLINE void fp_mod(fptype *a, fptype *b, uae_u64 *q, uae_s8 *s)
{
    *a = floatx80_mod(*a, *b, q, s, &fp_ctrl);
}
STATIC_INLINE void fp_add(fptype *a, fptype *b)
{
    *a = floatx80_add(*a, *b, &fp_ctrl);
}
STATIC_INLINE void fp_mul(fptype *a, fptype *b)
{
    *a = floatx80_mul(*a, *b, &fp_ctrl);
}
STATIC_INLINE void fp_sgldiv(fptype *a, fptype *b)
{
    *a = floatx80_sgldiv(*a, *b, &fp_ctrl);
}
STATIC_INLINE void fp_rem(fptype *a, fptype *b, uae_u64 *q, uae_s8 *s)
{
    *a = floatx80_rem(*a, *b, q, s, &fp_ctrl);
}
STATIC_INLINE void fp_scale(fptype *a, fptype *b)
{
    *a = floatx80_scale(*a, *b, &fp_ctrl);
}
STATIC_INLINE void fp_sglmul(fptype *a, fptype *b)
{
    *a = floatx80_sglmul(*a, *b, &fp_ctrl);
}
STATIC_INLINE void fp_sub(fptype *a, fptype *b)
{
    *a = floatx80_sub(*a, *b, &fp_ctrl);
}
STATIC_INLINE void fp_cmp(fptype *a, fptype *b)
{
    *a = floatx80_cmp(*a, *b, &fp_ctrl);
}
STATIC_INLINE void fp_tst(fptype *a, fptype *b)
{
    *a = floatx80_tst(*b, &fp_ctrl);
}

STATIC_INLINE void fp_sinh(fptype *a, fptype *b)
{
    *a = floatx80_sinh(*b, &fp_ctrl);
}
STATIC_INLINE void fp_lognp1(fptype *a, fptype *b)
{
    *a = floatx80_lognp1(*b, &fp_ctrl);
}
STATIC_INLINE void fp_etoxm1(fptype *a, fptype *b)
{
    *a = floatx80_etoxm1(*b, &fp_ctrl);
}
STATIC_INLINE void fp_tanh(fptype *a, fptype *b)
{
    *a = floatx80_tanh(*b, &fp_ctrl);
}
STATIC_INLINE void fp_atan(fptype *a, fptype *b)
{
    *a = floatx80_atan(*b, &fp_ctrl);
}
STATIC_INLINE void fp_asin(fptype *a, fptype *b)
{
    *a = floatx80_asin(*b, &fp_ctrl);
}
STATIC_INLINE void fp_atanh(fptype *a, fptype *b)
{
    *a = floatx80_atanh(*b, &fp_ctrl);
}
STATIC_INLINE void fp_sin(fptype *a, fptype *b)
{
    *a = floatx80_sin(*b, &fp_ctrl);
}
STATIC_INLINE void fp_tan(fptype *a, fptype *b)
{
    *a = floatx80_tan(*b, &fp_ctrl);
}
STATIC_INLINE void fp_etox(fptype *a, fptype *b)
{
    *a = floatx80_etox(*b, &fp_ctrl);
}
STATIC_INLINE void fp_twotox(fptype *a, fptype *b)
{
    *a = floatx80_twotox(*b, &fp_ctrl);
}
STATIC_INLINE void fp_tentox(fptype *a, fptype *b)
{
    *a = floatx80_tentox(*b, &fp_ctrl);
}
STATIC_INLINE void fp_logn(fptype *a, fptype *b)
{
    *a = floatx80_logn(*b, &fp_ctrl);
}
STATIC_INLINE void fp_log10(fptype *a, fptype *b)
{
    *a = floatx80_log10(*b, &fp_ctrl);
}
STATIC_INLINE void fp_log2(fptype *a, fptype *b)
{
    *a = floatx80_log2(*b, &fp_ctrl);
}
STATIC_INLINE void fp_cosh(fptype *a, fptype *b)
{
    *a = floatx80_cosh(*b, &fp_ctrl);
}
STATIC_INLINE void fp_acos(fptype *a, fptype *b)
{
    *a = floatx80_acos(*b, &fp_ctrl);
}
STATIC_INLINE void fp_cos(fptype *a, fptype *b)
{
    *a = floatx80_cos(*b, &fp_ctrl);
}

/* Functions with fixed precision */
STATIC_INLINE void fp_move_single(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(32, &fp_ctrl);
    *a = floatx80_move(*b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_abs_single(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(32, &fp_ctrl);
    *a = floatx80_abs(*b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_neg_single(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(32, &fp_ctrl);
    *a = floatx80_neg(*b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_add_single(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(32, &fp_ctrl);
    *a = floatx80_add(*a, *b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_sub_single(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(32, &fp_ctrl);
    *a = floatx80_sub(*a, *b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_mul_single(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(32, &fp_ctrl);
    *a = floatx80_mul(*a, *b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_div_single(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(32, &fp_ctrl);
    *a = floatx80_div(*a, *b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_sqrt_single(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(32, &fp_ctrl);
    *a = floatx80_sqrt(*b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_move_double(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(64, &fp_ctrl);
    *a = floatx80_move(*b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_abs_double(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(64, &fp_ctrl);
    *a = floatx80_abs(*b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_neg_double(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(64, &fp_ctrl);
    *a = floatx80_neg(*b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_add_double(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(64, &fp_ctrl);
    *a = floatx80_add(*a, *b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_sub_double(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(64, &fp_ctrl);
    *a = floatx80_sub(*a, *b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_mul_double(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(64, &fp_ctrl);
    *a = floatx80_mul(*a, *b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_div_double(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(64, &fp_ctrl);
    *a = floatx80_div(*a, *b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}
STATIC_INLINE void fp_sqrt_double(fptype *a, fptype *b)
{
    int8 oldprec = get_float_rounding_precision(&fp_ctrl);
    set_float_rounding_precision(64, &fp_ctrl);
    *a = floatx80_sqrt(*b, &fp_ctrl);
    set_float_rounding_precision(oldprec, &fp_ctrl);
}



#if 0 /* Old fallback functions disabled */
static const long double twoto32 = 4294967296.0;

STATIC_INLINE void to_native(long double *fp, fptype fpx)
{
    int expon;
    long double frac;
    
    expon = fpx.high & 0x7fff;
    
    if (floatx80_is_zero(fpx)) {
        *fp = floatx80_is_negative(fpx) ? -0.0 : +0.0;
        return;
    }
    if (floatx80_is_nan(fpx)) {
        *fp = sqrtl(-1);
        return;
    }
    if (floatx80_is_infinity(fpx)) {
        *fp = floatx80_is_negative(fpx) ? logl(0.0) : (1.0/0.0);
        return;
    }
    
    frac = (long double)fpx.low / (long double)(twoto32 * 2147483648.0);
    if (floatx80_is_negative(fpx))
        frac = -frac;
    *fp = ldexpl (frac, expon - 16383);
}
STATIC_INLINE void from_native(long double fp, fptype *fpx)
{
    int expon;
    long double frac;
    
    if (signbit(fp))
        fpx->high = 0x8000;
    else
        fpx->high = 0x0000;
    
    if (isnan(fp)) {
        fpx->high |= 0x7fff;
        fpx->low = LIT64(0xffffffffffffffff);
        return;
    }
    if (isinf(fp)) {
        fpx->high |= 0x7fff;
        fpx->low = LIT64(0x0000000000000000);
        return;
    }
    if (fp == 0.0) {
        fpx->low = LIT64(0x0000000000000000);
        return;
    }
    if (fp < 0.0)
        fp = -fp;
    
    frac = frexpl (fp, &expon);
    frac += 0.5 / (twoto32 * twoto32);
    if (frac >= 1.0) {
        frac /= 2.0;
        expon++;
    }
    fpx->high |= (expon + 16383 - 1) & 0x7fff;
    fpx->low = (bits64)(frac * (long double)(twoto32 * twoto32));
    
    while (!(fpx->low & LIT64( 0x8000000000000000))) {
        if (fpx->high == 0) {
            break;
        }
        fpx->low <<= 1;
        fpx->high--;
    }
}

STATIC_INLINE void fp_sinhf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_sinh_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = sinhl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_lognp1f(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_lognp1_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = log1pl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_etoxm1f(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_etoxm1_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = expm1l(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_tanhf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_tanh_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = tanhl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_atanf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_atan_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = atanl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_asinf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_asin_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = asinl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_atanhf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_atanh_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = atanhl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_sinf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_sin_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = sinl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_tanf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_tan_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = tanl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_etoxf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_etox_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = expl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_twotoxf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_twotox_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = powl(2.0, fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_tentoxf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_tentox_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = powl(10.0, fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_lognf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_logn_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = logl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_log10f(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_log10_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = log10l(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_log2f(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_log2_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = log2l(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_coshf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_cosh_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = coshl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_acosf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_acos_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = acosl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_cosf(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_cos_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = cosl(fp);
    from_native(fp, a);
    fp_round(a);
}
#endif

#endif
