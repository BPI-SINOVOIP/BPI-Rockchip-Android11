/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "AEEStdDef.h"
#include "AEEstd.h"
#include "AEEStdErr.h"
#include "std_dtoa.h"
#include "math.h"

//
//  Useful Macros
//
#define  FAILED(b)               ( (b) != AEE_SUCCESS ? TRUE : FALSE )
#define  CLEANUP_ON_ERROR(b,l)   if( FAILED( b ) ) { goto l; }
#define  FP_POW_10(n)            fp_pow_10(n)

static __inline
uint32 std_dtoa_clz32( uint32 ulVal )
//
// This function returns the number of leading zeroes in a uint32.
// This is a naive implementation that uses binary search. This could be
// replaced by an optimized inline assembly code.
//
{
   if( (int)ulVal <= 0 )
   {
      return ( ulVal == 0 ) ? 32 : 0;
   }
   else
   {
      uint32 uRet = 28;
      uint32 uTmp = 0;
      uTmp = ( ulVal > 0xFFFF ) * 16; ulVal >>= uTmp, uRet -= uTmp;
      uTmp = ( ulVal > 0xFF ) * 8; ulVal >>= uTmp, uRet -= uTmp;
      uTmp = ( ulVal > 0xF ) * 4; ulVal >>= uTmp, uRet -= uTmp;
      return uRet + ( ( 0x55AF >> ( ulVal * 2 ) ) & 3 );
   }
}

static __inline
uint32 std_dtoa_clz64( uint64 ulVal )
//
// This function returns the number of leading zeroes in a uint64.
//
{
    uint32 ulCount = 0;

    if( !( ulVal >> 32 ) )
    {
        ulCount += 32;
    }
    else
    {
        ulVal >>= 32;
    }

    return ulCount + std_dtoa_clz32( (uint32)ulVal );
}

double fp_pow_10( int nPow )
{
   double dRet = 1.0;
   int nI = 0;
   boolean bNegative = FALSE;
   double aTablePos[] = { 0, 1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128,
                          1e256 };
   double aTableNeg[] = { 0, 1e-1, 1e-2, 1e-4, 1e-8, 1e-16, 1e-32, 1e-64, 1e-128,
                          1e-256 };
   double* pTable = aTablePos;
   int nTableSize = STD_ARRAY_SIZE( aTablePos );

   if( 0 == nPow )
   {
      return 1.0;
   }

   if( nPow < 0 )
   {
      bNegative = TRUE;
      nPow = -nPow;
      pTable = aTableNeg;
      nTableSize = STD_ARRAY_SIZE( aTableNeg );
   }

   for( nI = 1; nPow && (nI < nTableSize); nI++ )
   {
      if( nPow & 1 )
      {
         dRet *= pTable[nI];
      }

      nPow >>= 1;
   }

   if( nPow )
   {
      // Overflow. Trying to compute a large power value.
      uint64 ulInf = STD_DTOA_FP_POSITIVE_INF;
      dRet = bNegative ? 0 : UINT64_TO_DOUBLE( ulInf );
   }

   return dRet;
}

double fp_round( double dNumber, int nPrecision )
//
// This functions rounds dNumber to the specified precision nPrecision.
// For example:
//    fp_round(2.34553, 3) = 2.346
//    fp_round(2.34553, 4) = 2.3455
//
{
   double dResult = dNumber;
   double dRoundingFactor = FP_POW_10( -nPrecision ) * 0.5;

   if( dNumber < 0 )
   {
      dResult = dNumber - dRoundingFactor;
   }
   else
   {
      dResult = dNumber + dRoundingFactor;
   }

   return dResult;
}

int fp_log_10( double dNumber )
//
// This function finds the integer part of the log_10( dNumber ).
// The function assumes that dNumber != 0.
//
{
   // Absorb the negative sign
   if( dNumber < 0 )
   {
      dNumber = -dNumber;
   }

   return (int)( floor( log10( dNumber ) ) );
}

int fp_check_special_cases( double dNumber, FloatingPointType* pNumberType )
//
// This function evaluates the input floating-point number dNumber to check for
// following special cases: NaN, +/-Infinity.
// The evaluation is based on the IEEE Standard 754 for Floating Point Numbers
//
{
   int nError = AEE_SUCCESS;
   FloatingPointType NumberType = FP_TYPE_UNKOWN;
   uint64 ullValue = 0;
   uint64 ullSign = 0;
   int64 n64Exponent = 0;
   uint64 ullMantissa = 0;

   ullValue = DOUBLE_TO_UINT64( dNumber );

   // Extract the sign, exponent and mantissa
   ullSign = FP_SIGN( ullValue );
   n64Exponent = FP_EXPONENT_BIASED( ullValue );
   ullMantissa = FP_MANTISSA_DENORM( ullValue );

   //
   // Rules for special cases are listed below:
   // For Infinity, the following needs to be true:
   // 1. Exponent should have all bits set to 1.
   // 2. Mantissa should have all bits set to 0.
   //
   // For NaN, the following needs to be true:
   // 1. Exponent should have all bits set to 1.
   // 2. Mantissa should be non-zero.
   // Note that we do not differentiate between QNaNs and SNaNs.
   //
   if( STD_DTOA_DP_INFINITY_EXPONENT_ID == n64Exponent )
   {
      if( 0 == ullMantissa )
      {
         // Inifinity.
         if( ullSign )
         {
            NumberType = FP_TYPE_NEGATIVE_INF;
         }
         else
         {
            NumberType = FP_TYPE_POSITIVE_INF;
         }
      }
      else
      {
         // NaN
         NumberType = FP_TYPE_NAN;
      }
   }
   else
   {
      // A normal number
      NumberType = FP_TYPE_GENERAL;
   }

   // Set the output value
   *pNumberType = NumberType;

   return nError;
}

int std_dtoa_decimal( double dNumber, int nPrecision,
                      char acIntegerPart[ STD_DTOA_FORMAT_INTEGER_SIZE ],
                      char acFractionPart[ STD_DTOA_FORMAT_FRACTION_SIZE ] )
{
   int nError = AEE_SUCCESS;
   boolean bNegativeNumber = FALSE;
   double dIntegerPart = 0.0;
   double dFractionPart = 0.0;
   double dTempIp = 0.0;
   double dTempFp = 0.0;
   int nMaxIntDigs = STD_DTOA_FORMAT_INTEGER_SIZE;
   uint32 ulI = 0;
   int nIntStartPos = 0;

   // Optimization: Special case an input of 0
   if( 0.0 == dNumber )
   {
      acIntegerPart[0] = '0';
      acIntegerPart[1] = '\0';

      for( ulI = 0; (ulI < STD_DTOA_FORMAT_FRACTION_SIZE - 1) && (nPrecision > 0);
           ulI++, nPrecision-- )
      {
         acFractionPart[ulI] = '0';
      }
      acFractionPart[ ulI ] = '\0';

      goto bail;
   }

   // Absorb the negative sign
   if( dNumber < 0 )
   {
      acIntegerPart[0] = '-';
      nIntStartPos = 1;
      dNumber = -dNumber;
      bNegativeNumber = TRUE;
   }

   // Split the input number into it's integer and fraction parts
   dFractionPart = modf( dNumber, &dIntegerPart );

   // First up, convert the integer part
   if( 0.0 == dIntegerPart )
   {
      acIntegerPart[ nIntStartPos ] = '0';
   }
   else
   {
      double dRoundingConst = FP_POW_10( -STD_DTOA_PRECISION_ROUNDING_VALUE );
      int nIntDigs = 0;
      int nI = 0;

      // Compute the number of digits in the integer part of the number
      nIntDigs = fp_log_10( dIntegerPart ) + 1;

      // For negative numbers, a '-' sign has already been written.
      if( TRUE == bNegativeNumber )
      {
         nIntDigs++;
      }

      // Check for overflow
      if( nIntDigs >= nMaxIntDigs )
      {
         // Overflow!
         // Note that currently, we return a simple AEE_EFAILED for all
         // errors.
         nError = AEE_EFAILED;
         goto bail;
      }

      // Null Terminate the string
      acIntegerPart[ nIntDigs ] = '\0';

      for( nI = nIntDigs - 1; nI >= nIntStartPos; nI-- )
      {
         dIntegerPart = dIntegerPart / 10.0;
         dTempFp = modf( dIntegerPart, &dTempIp );

         // Round it to the a specific precision
         dTempFp = dTempFp + dRoundingConst;

         // Convert the digit to a character
         acIntegerPart[ nI ] = (int)( dTempFp * 10 ) + '0';
         if( !MY_ISDIGIT( acIntegerPart[ nI ] ) )
         {
            // Overflow!
            // Note that currently, we return a simple AEE_EFAILED for all
            // errors.
            nError = AEE_EFAILED;
            goto bail;
         }
         dIntegerPart = dTempIp;
      }
   }

   // Just a double check for integrity sake. This should ideally never happen.
   // Out of bounds scenario. That is, the integer part of the input number is
   // too large.
   if( dIntegerPart !=  0.0 )
   {
      // Note that currently, we return a simple AEE_EFAILED for all
      // errors.
      nError = AEE_EFAILED;
      goto bail;
   }

   // Now, convert the fraction part
   for( ulI = 0; ( nPrecision > 0 ) && ( ulI < STD_DTOA_FORMAT_FRACTION_SIZE - 1 );
        nPrecision--, ulI++ )
   {
      if( 0.0 == dFractionPart )
      {
         acFractionPart[ ulI ] = '0';
      }
      else
      {
         double dRoundingValue = FP_POW_10( -( nPrecision +
                                               STD_DTOA_PRECISION_ROUNDING_VALUE ) );
         acFractionPart[ ulI ] = (int)( ( dFractionPart + dRoundingValue ) * 10.0 ) + '0';
         if( !MY_ISDIGIT( acFractionPart[ ulI ] ) )
         {
            // Overflow!
            // Note that currently, we return a simple AEE_EFAILED for all
            // errors.
            nError = AEE_EFAILED;
            goto bail;
         }

         dFractionPart = ( dFractionPart * 10.0 ) -
                         (int)( ( dFractionPart + FP_POW_10( -nPrecision - 6 ) ) * 10.0 );
      }
   }


bail:

   return nError;
}

int std_dtoa_hex( double dNumber, int nPrecision, char cFormat,
                  char acIntegerPart[ STD_DTOA_FORMAT_INTEGER_SIZE ],
                  char acFractionPart[ STD_DTOA_FORMAT_FRACTION_SIZE ],
                  int* pnExponent )
{
   int nError = AEE_SUCCESS;
   uint64 ullMantissa = 0;
   uint64 ullSign = 0;
   int64 n64Exponent = 0;
   static const char HexDigitsU[] = "0123456789ABCDEF";
   static const char HexDigitsL[] = "0123456789abcde";
   boolean bFirstDigit = TRUE;
   int nI = 0;
   int nF = 0;
   uint64 ullValue = DOUBLE_TO_UINT64( dNumber );
   int nManShift = 0;
   const char *pcDigitArray = ( cFormat == 'A' ) ? HexDigitsU : HexDigitsL;
   boolean bPrecisionSpecified = TRUE;

   // If no precision is specified, then set the precision to be fairly
   // large.
   if( nPrecision < 0 )
   {
      nPrecision = STD_DTOA_FORMAT_FRACTION_SIZE;
      bPrecisionSpecified = FALSE;
   }
   else
   {
      bPrecisionSpecified = TRUE;
   }

   // Extract the sign, exponent and mantissa
   ullSign = FP_SIGN( ullValue );
   n64Exponent = FP_EXPONENT( ullValue );
   ullMantissa = FP_MANTISSA( ullValue );

   // Write out the sign
   if( ullSign )
   {
      acIntegerPart[ nI++ ] = '-';
   }

   // Optimization: Special case an input of 0
   if( 0.0 == dNumber )
   {
      acIntegerPart[0] = '0';
      acIntegerPart[1] = '\0';

      for( nF = 0; (nF < STD_DTOA_FORMAT_FRACTION_SIZE - 1) && (nPrecision > 0);
           nF++, nPrecision-- )
      {
         acFractionPart[nF] = '0';
      }
      acFractionPart[nF] = '\0';

      goto bail;
   }

   // The mantissa is in lower 53 bits (52 bits + an implicit 1).
   // If we are dealing with a denormalized number, then the implicit 1
   // is absent. The above macros would have then set that bit to 0.
   // Shift the mantisaa on to the highest bits.

   if( 0 == ( n64Exponent + STD_DTOA_DP_EXPONENT_BIAS ) )
   {
      // DENORMALIZED NUMBER.
      // A denormalized number is of the form:
      //       0.bbb...bbb x 2^Exponent
      // Shift the mantissa to the higher bits while discarding the leading 0
      ullMantissa <<= 12;

      // Lets update the exponent so as to make sure that the first hex value
      // in the mantissa is non-zero, i.e., at least one of the first 4 bits is
      // non-zero.
      nManShift = std_dtoa_clz64( ullMantissa ) - 3;
      if( nManShift > 0 )
      {
         ullMantissa <<= nManShift;
         n64Exponent -= nManShift;
      }
   }
   else
   {
      // NORMALIZED NUMBER.
      // A normalized number has the following form:
      //       1.bbb...bbb x 2^Exponent
      // Shift the mantissa to the higher bits while retaining the leading 1
      ullMantissa <<= 11;
   }

   // Now, lets get the decimal point out of the picture by shifting the
   // exponent by 1.
   n64Exponent++;

   // Read the mantissa four bits at a time to form the hex output
   for( nI = 0, nF = 0, bFirstDigit = TRUE; ullMantissa != 0;
        ullMantissa <<= 4 )
   {
      uint64 ulHexVal = ullMantissa & 0xF000000000000000uLL;
      ulHexVal >>= 60;
      if( bFirstDigit )
      {
         // Write to the integral part of the number
         acIntegerPart[ nI++ ] = pcDigitArray[ulHexVal];
         bFirstDigit = FALSE;
      }
      else if( nF < nPrecision )
      {
         // Write to the fractional part of the number
         acFractionPart[ nF++ ] = pcDigitArray[ulHexVal];
      }
   }

   // Pad the fraction with trailing zeroes upto the specified precision
   for( ; bPrecisionSpecified && (nF < nPrecision); nF++ )
   {
      acFractionPart[ nF ] = '0';
   }

   // Now the output is of the form;
   //       h.hhh x 2^Exponent
   // where h is a non-zero hexadecimal number.
   // But we were dealing with a binary fraction 0.bbb...bbb x 2^Exponent.
   // Therefore, we need to subtract 4 from the exponent (since the shift
   // was to the base 16 and the exponent is to the base 2).
   n64Exponent -= 4;
   *pnExponent = (int)n64Exponent;

bail:
   return nError;
}
