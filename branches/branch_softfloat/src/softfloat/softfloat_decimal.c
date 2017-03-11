
/*============================================================================

This C source file is an extension to the SoftFloat IEC/IEEE Floating-point 
Arithmetic Package, Release 2a.

=============================================================================*/

#include "softfloat.h"

/*----------------------------------------------------------------------------
| Methods for converting decimal floats to binary extended precision floats.
*----------------------------------------------------------------------------*/

void mul128by128(int32 *aExp, bits64 *aSig0, bits64 *aSig1, int32 bExp, bits64 bSig0, bits64 bSig1)
{
    int32 zExp;
    bits64 zSig0, zSig1, zSig2, zSig3;
    
    zExp = *aExp;
    zSig0 = *aSig0;
    zSig1 = *aSig1;

    zExp += bExp - 0x3FFE;
    mul128To256(zSig0, zSig1, bSig0, bSig1, &zSig0, &zSig1, &zSig2, &zSig3);
    zSig1 |= (zSig2 | zSig3) != 0;
    if ( 0 < (sbits64) zSig0 ) {
        shortShift128Left( zSig0, zSig1, 1, &zSig0, &zSig1 );
        --zExp;
    }
    *aExp = zExp;
    *aSig0 = zSig0;
    *aSig1 = zSig1;
}

void div128by128(int32 *aExp, bits64 *aSig0, bits64 *aSig1, int32 bExp, bits64 bSig0, bits64 bSig1)
{
    int32 zExp;
    bits64 zSig0, zSig1;
    bits64 rem0, rem1, rem2, rem3, term0, term1, term2, term3;
    
    zExp = *aExp;
    zSig0 = *aSig0;
    zSig1 = *aSig1;
    
    zExp -= bExp - 0x3FFE;
    rem1 = 0;
    if ( le128( bSig0, bSig1, zSig0, zSig1 ) ) {
        shift128Right( zSig0, zSig1, 1, &zSig0, &zSig1 );
        ++zExp;
    }
    zSig0 = estimateDiv128To64( zSig0, zSig1, bSig0 );
    mul128By64To192( bSig0, bSig1, zSig0, &term0, &term1, &term2 );
    sub192( zSig0, zSig1, 0, term0, term1, term2, &rem0, &rem1, &rem2 );
    while ( (sbits64) rem0 < 0 ) {
        --zSig0;
        add192( rem0, rem1, rem2, 0, bSig0, bSig1, &rem0, &rem1, &rem2 );
    }
    zSig1 = estimateDiv128To64( rem1, rem2, bSig0 );
    if ( ( zSig1 & 0x3FFF ) <= 4 ) {
        mul128By64To192( bSig0, bSig1, zSig1, &term1, &term2, &term3 );
        sub192( rem1, rem2, 0, term1, term2, term3, &rem1, &rem2, &rem3 );
        while ( (sbits64) rem1 < 0 ) {
            --zSig1;
            add192( rem1, rem2, rem3, 0, bSig0, bSig1, &rem1, &rem2, &rem3 );
        }
        zSig1 |= ( ( rem1 | rem2 | rem3 ) != 0 );
    }

    *aExp = zExp;
    *aSig0 = zSig0;
    *aSig1 = zSig1;
}

/*----------------------------------------------------------------------------
| Decimal to binary
*----------------------------------------------------------------------------*/

floatx80 floatdecimal_to_floatx80(floatx80 a)
{
    flag decSign, zSign, decExpSign, increment;
    int32 decExp, zExp, mExp, xExp, shiftCount;
    bits64 decSig, zSig0, zSig1, mSig0, mSig1, xSig0, xSig1;
    
    decSign = extractFloatx80Sign(a);
    decExp = extractFloatx80Exp(a);
    decSig = extractFloatx80Frac(a);
    
    if (decExp == 0x7FFF) return a;
    
    if (decExp == 0 && decSig == 0) return a;
    
    decExpSign = (decExp >> 14) & 1;
    decExp &= 0x3FFF;
    
    shiftCount = countLeadingZeros64( decSig );
    zExp = 0x403E - shiftCount;
    zSig0 = decSig << shiftCount;
    zSig1 = 0;
    zSign = decSign;
    
    mExp = 0x4002;
    mSig0 = LIT64(0xA000000000000000);
    xExp = 0x3FFF;
    xSig0 = LIT64(0x8000000000000000);
    
    while (decExp) {
        if (decExp & 1) {
            mul128by128(&xExp, &xSig0, &xSig1, mExp, mSig0, mSig1);
        }
        mul128by128(&mExp, &mSig0, &mSig1, mExp, mSig0, mSig1);
        decExp >>= 1;
    }
    
    if (decExpSign) {
        div128by128(&zExp, &zSig0, &zSig1, xExp, xSig0, xSig1);
    } else {
        mul128by128(&zExp, &zSig0, &zSig1, xExp, xSig0, xSig1);
    }
    
    increment = ( (sbits64) zSig1 < 0 );
    if (float_rounding_mode != float_round_nearest_even) {
        if (float_rounding_mode == float_round_to_zero) {
            increment = 0;
        } else {
            if (zSign) {
                increment = (float_rounding_mode == float_round_down) && zSig1;
            } else {
                increment = (float_rounding_mode == float_round_up) && zSig1;
            }
        }
    }
    if (zSig1) float_raise(float_flag_decimal);
    
    if (increment) {
        ++zSig0;
        if (zSig0 == 0) {
            ++zExp;
            zSig0 = LIT64(0x8000000000000000);
        } else {
            zSig0 &= ~ (((bits64) (zSig1<<1) == 0) & (float_rounding_mode == float_round_nearest_even));
        }
    } else {
        if ( zSig0 == 0 ) zExp = 0;
    }
    return packFloatx80( zSign, zExp, zSig0 );
    
}

