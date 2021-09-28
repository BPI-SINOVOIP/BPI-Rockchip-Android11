/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/

/*****************************************************************************/
/*                                                                           */
/*  File name        : ia_basic_ops32.h                                      */
/*                                                                           */
/*  Description      : this file has 32bit basic operations                  */
/*                                                                           */
/*  List of functions: 1.  min32                                             */
/*                     2.  max32                                             */
/*                     3.  mult16x16in32                                     */
/*                     4.  mult16x16in32_shl                                 */
/*                     5.  mult16x16in32_shl_sat                             */
/*                     6.  shl32                                             */
/*                     7.  shl32_sat                                         */
/*                     8.  shl32_dir                                         */
/*                     9.  shl32_dir_sat                                     */
/*                     10. shr32                                             */
/*                     11. shr32_dir                                         */
/*                     12. shr32_dir_sat                                     */
/*                     13. add32                                             */
/*                     14. sub32                                             */
/*                     15. add32_sat                                         */
/*                     16. sub32_sat                                         */
/*                     17. norm32                                            */
/*                     18. bin_expo32                                        */
/*                     19. abs32                                             */
/*                     20. abs32_sat                                         */
/*                     21. negate32                                          */
/*                     22. negate32_sat                                      */
/*                     23. div32                                             */
/*                     24. mac16x16in32                                      */
/*                     25. mac16x16in32_shl                                  */
/*                     26. mac16x16in32_shl_sat                              */
/*                     27. msu16x16in32                                      */
/*                     28. msu16x16in32_shl                                  */
/*                     29. msu16x16in32_shl_sat                              */
/*                     30. add32_shr                                         */
/*                     31. sub32_shr                                         */
/*                                                                           */
/*  Issues / problems: none                                                  */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* File includes                                                             */
/*  ia_type_def.h                                                            */
/*  ia_constants.h                                                           */
/*****************************************************************************/

#ifndef __IA_BASIC_OPS32_H__
#define __IA_BASIC_OPS32_H__

/*****************************************************************************/
/*                                                                           */
/*  Function name : min32                                                    */
/*                                                                           */
/*  Description   : returns the minima of 2 32 bit variables                 */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : minimum of 2 inputs                                      */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 min_val - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 min32(WORD32 a, WORD32 b)
{
    WORD32 min_val;

    min_val = (a < b) ? a : b;

    return min_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : max32                                                    */
/*                                                                           */
/*  Description   : returns the maxima of 2 32 bit variables                 */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : maximum of 2 inputs                                      */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 max_val - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 max32(WORD32 a, WORD32 b)
{
    WORD32 max_val;

    max_val = (a > b) ? a : b;

    return max_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : shl32                                                    */
/*                                                                           */
/*  Description   : shifts a 32-bit value left by specificed bits            */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD b                                         */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : shift a by b                                             */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 out_val - 32 bit signed value                     */
/*                                                                           */
/*  assumptions   : 0 <= b <= 31                                             */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/
static PLATFORM_INLINE WORD32 shl32(WORD32 a, WORD b)
{
    WORD32 out_val;

    if(b > 31)
        out_val = 0;
    else
        out_val = (WORD32)a << b;

    return out_val;
}
/*****************************************************************************/
/*                                                                           */
/*  Function name : shr32                                                    */
/*                                                                           */
/*  Description   : shifts a 32-bit value right by specificed bits           */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD b                                         */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : shift right a by b                                       */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 out_val - 32 bit signed value                     */
/*                                                                           */
/*  assumptions   : 0 <= b <= 31                                             */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 shr32(WORD32 a, WORD b)
{
    WORD32 out_val;

    if(b > 31)
    {
        if(a < 0)
            out_val = -1;
        else
            out_val = 0;
    }
    else
    {
        out_val = (WORD32)a >> b;
    }

    return out_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : shl32_sat                                                */
/*                                                                           */
/*  Description   : shifts a 32-bit value left by specificed bits and        */
/*                  saturates it to 32 bits                                  */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD b                                         */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : shift a by 1 b times if crosses 32_bits saturate         */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 out_val - 32 bit signed value                     */
/*                                                                           */
/*  assumptions   : 0 <= b <= 31                                             */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 shl32_sat(WORD32 a, WORD b)
{
    WORD32 out_val = a;
    /*clip the max shift value to avoid unnecessary looping*/
    if(b > (WORD)((sizeof(WORD32) * 8)))
        b = (sizeof(WORD32) * 8);
    for(; b > 0; b--)
    {
        if(a > (WORD32)0X3fffffffL)
        {
            out_val = MAX_32;
            break;
        }
        else if(a < (WORD32)0xc0000000L)
        {
            out_val = MIN_32;
            break;
        }

        a = shl32(a, 1);
        out_val = a;
    }
    return (out_val);
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : shl32_dir                                                */
/*                                                                           */
/*  Description   : shifts a 32-bit value left by specificed bits, shifts    */
/*                  it right if specified no. of bits is negative            */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD b                                         */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : if b -ve shift right else shift left                     */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 out_val - 32 bit signed value                     */
/*                                                                           */
/*  assumptions   : -31 <= b <= 31                                           */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 shl32_dir(WORD32 a, WORD b)
{
    WORD32 out_val;

    if(b < 0)
    {
        out_val = shr32(a, -b);
    }
    else
    {
        out_val = shl32(a, b);
    }

    return out_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : shl32_dir_sat                                            */
/*                                                                           */
/*  Description   : shifts a 32-bit value left by specificed bits with sat,  */
/*                  shifts it right if specified no. of bits is negative     */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD b                                         */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : if b -ve shift right else shift left with sat            */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 out_val - 32 bit signed value                     */
/*                                                                           */
/*  assumptions   : -31 <= b <= 31                                           */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 shl32_dir_sat(WORD32 a, WORD b)
{
    WORD32 out_val;

    if(b < 0)
    {
        out_val = shr32(a, -b);
    }
    else
    {
        out_val = shl32_sat(a, b);
    }

    return out_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : shr32_dir                                                */
/*                                                                           */
/*  Description   : shifts a 32-bit value right by specificed bits, shifts   */
/*                  it left if specified no. of bits is negative             */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD b                                         */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : if b +ve shift right else shift left                     */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 out_val - 32 bit signed value                     */
/*                                                                           */
/*  assumptions   : -31 <= b <= 31                                           */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 shr32_dir(WORD32 a, WORD b)
{
    WORD32 out_val;

    if(b < 0)
    {
        out_val = shl32(a, -b);
    }
    else
    {
        out_val = shr32(a, b);
    }

    return out_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : shr32_dir_sat                                            */
/*                                                                           */
/*  Description   : shifts a 32-bit value right by specificed bits, shifts   */
/*                  it left with sat if specified no. of bits is negative    */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD b                                         */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : if b +ve shift right else shift left with sat            */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 out_val - 32 bit signed value                     */
/*                                                                           */
/*  assumptions   : -31 <= b <= 31                                           */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 shr32_dir_sat(WORD32 a, WORD b)
{
    WORD32 out_val;

    if(b < 0)
    {
        out_val = shl32_sat(a, -b);
    }
    else
    {
        out_val = shr32(a, b);
    }

    return out_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : mult16x16in32                                            */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and returns their 32-bit   */
/*                  result                                                   */
/*                                                                           */
/*  Inputs        : WORD16 a, WORD16 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : multiply 2 inputs                                        */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 product - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mult16x16in32(WORD16 a, WORD16 b)
{
    WORD32 product;

    product = (WORD32)a * (WORD32)b;

    return product;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : mult16x16in32_shl                                        */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and returns their 32-bit   */
/*                  result after removing 1 redundant sign bit. no sat       */
/*                                                                           */
/*  Inputs        : WORD16 a, WORD16 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : multiply 2 inputs, shift left by 1                       */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 product - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mult16x16in32_shl(WORD16 a, WORD16 b)
{
    WORD32 product;

    product = shl32(mult16x16in32(a, b), 1);

    return product;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : mult16x16in32_shl_sat                                    */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and returns their 32-bit   */
/*                  result after removing 1 redundant sign bit with sat      */
/*                                                                           */
/*  Inputs        : WORD16 a, WORD16 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : if inputs mi_ns return MAX32 else                        */
/*                  multiply 2 inputs shift left by 1                        */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 product - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mult16x16in32_shl_sat(WORD16 a, WORD16 b)
{
    WORD32 product;
    product = (WORD32)a * (WORD32)b;
    if(product != (WORD32)0x40000000L)
    {
        product = shl32(product, 1);
    }
    else
    {
        product = MAX_32;
    }
    return product;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : add32                                                    */
/*                                                                           */
/*  Description   : adds 2 32 bit variables without sat                      */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : add 2 inputs                                             */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 sum - 32 bit signed value                         */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 add32(WORD32 a, WORD32 b)
{
    WORD32 sum;

    sum = (WORD32)a + (WORD32)b;

    return sum;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : sub32                                                    */
/*                                                                           */
/*  Description   : subs 2 32 bit variables without sat                      */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : sub 2 inputs                                             */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 diff - 32 bit signed value                        */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 sub32(WORD32 a, WORD32 b)
{
    WORD32 diff;

    diff = (WORD32)a - (WORD32)b;

    return diff;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : add32_sat                                                */
/*                                                                           */
/*  Description   : adds 2 32 bit variables with sat                         */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : add 2 inputs if overflow saturate                        */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 sum - 32 bit signed value                         */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 add32_sat(WORD32 a, WORD32 b)
{
    WORD32 sum;

    sum = add32(a, b);

    if((((WORD32)a ^ (WORD32)b) & (WORD32)MIN_32) == 0)
    {
        if(((WORD32)sum ^ (WORD32)a) & (WORD32)MIN_32)
        {
            sum = (a < 0) ? MIN_32 : MAX_32;
        }
    }

    return sum;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : sub32_sat                                                */
/*                                                                           */
/*  Description   : subs 2 32 bit variables with sat                         */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : sub 2 inputs, if overflow saturate                       */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 diff - 32 bit signed value                        */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 sub32_sat(WORD32 a, WORD32 b)
{
    WORD32 diff;

    diff = sub32(a, b);

    if((((WORD32)a ^ (WORD32)b) & (WORD32)MIN_32) != 0)
    {
        if(((WORD32)diff ^ (WORD32)a) & (WORD32)MIN_32)
        {
            diff = (a < 0L) ? MIN_32 : MAX_32;
        }
    }

    return (diff);
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : norm32                                                   */
/*                                                                           */
/*  Description   : returns number of redundant sign bits in a               */
/*                  32-bit value. for a value of 0, returns 0                */
/*                                                                           */
/*  Inputs        : WORD32 a                                                 */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : abs input, left shift till reaches 0x40000000            */
/*                  return no. of left shifts                                */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD norm_val - 0 <= norm_val < 32                       */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD norm32(WORD32 a)
{
    WORD norm_val;

    if(a == 0)
    {
        norm_val = 31;
    }
    else
    {
        if(a == (WORD32)0xffffffffL)
        {
            norm_val = 31;
        }
        else
        {
            if(a < 0)
            {
                a = ~a;
            }
            for(norm_val = 0; a < (WORD32)0x40000000L; norm_val++)
            {
                a <<= 1;
            }
        }
    }

    return norm_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : bin_expo32                                               */
/*                                                                           */
/*  Description   : returns the position of the most significant bit.        */
/*                  for negative numbers, it ignores leading zeros to        */
/*                  determine the position of most significant bit.          */
/*                  note: for a value of zero returns 31                     */
/*                                                                           */
/*  Inputs        : WORD32 a                                                 */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : substract 31 from norm_val                               */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD bin_expo_val - 0 <= val < 32                        */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD bin_expo32(WORD32 a)
{
    WORD bin_expo_val;

    bin_expo_val = 31 - norm32(a);

    return bin_expo_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : abs32                                                    */
/*                                                                           */
/*  Description   : returns the value of 32-bit number without sat.          */
/*                                                                           */
/*  Inputs        : WORD32 a                                                 */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : if -ve then negate                                       */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 abs_val - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 abs32(WORD32 a)
{
    WORD32 abs_val;

    abs_val = a;

    if(a < 0)
    {
        abs_val = -a;
    }

    return abs_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : abs32_sat                                                */
/*                                                                           */
/*  Description   : returns the value of 32-bit number with sat.             */
/*                                                                           */
/*  Inputs        : WORD32 a                                                 */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : if -ve then negate, abs(-32768) is 32767                 */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 abs_val - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 abs32_sat(WORD32 a)
{
    WORD32 abs_val;

    abs_val = a;

    if(a == (WORD32)MIN_32)
    {
        abs_val = MAX_32;
    }
    else if(a < 0)
    {
        abs_val = -a;
    }

    return abs_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : negate32                                                 */
/*                                                                           */
/*  Description   : returns the negated value of 32-bit number without sat.  */
/*                                                                           */
/*  Inputs        : WORD32 a                                                 */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : negate input                                             */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 neg_val - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 negate32(WORD32 a)
{
    WORD32 neg_val;

    neg_val = -a;

    return neg_val;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : negate32                                                 */
/*                                                                           */
/*  Description   : returns the negated value of 32-bit number with sat.     */
/*                                                                           */
/*  Inputs        : WORD32 a                                                 */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : negate input, if -32768 then 32767                       */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 neg_val - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 negate32_sat(WORD32 a)
{
    WORD32 neg_val;

    neg_val = -a;
    if(a == (WORD32)MIN_32)
    {
        neg_val = MAX_32;
    }

    return neg_val;
}
/*****************************************************************************/
/*                                                                           */
/*  Function name : subc_32                                                 */
/*                                                                           */
/*  Description   : implemnets the subc operation c64x                  .     */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : implemnets the subc operation c64x                       */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 neg_val - 32 bit signed value                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       Mudit Mehrotra        created                     */
/*                                                                           */
/*****************************************************************************/
static PLATFORM_INLINE UWORD32 subc_32(UWORD32 nr, UWORD32 dr)
{
    UWORD32 result;
    if(nr >= dr)
    {
        result = (((nr - dr) << 1) + 1);
    }
    else
    {
        result = (UWORD32)nr << 1;
    }
    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : div32                                                    */
/*                                                                           */
/*  Description   : divides 2 32 bit variables and returns the quotient      */
/*                  the q-format of the result is modified                   */
/*                  ( a/b  to Q30 precision)                                 */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b, WORD16 *q_format                     */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : non-restoration algo(shift & substract)                  */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 quotient - 32 bit signed value                    */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/
static PLATFORM_INLINE WORD32 div32(WORD32 a, WORD32 b, WORD *q_format)
{
    WORD32 quotient;
    UWORD32 mantissa_nr, mantissa_dr;
    WORD sign = 0;

    LOOPINDEX i;
    WORD q_nr, q_dr;

    mantissa_nr = a;
    mantissa_dr = b;
    quotient = 0;

    if((a < 0) && (0 != b))
    {
        a = -a;
        sign = (WORD)(sign ^ -1);
    }

    if(b < 0)
    {
        b = -b;
        sign = (WORD)(sign ^ -1);
    }

    if(0 == b)
    {
        *q_format = 0;
        return (a);
    }

    quotient = 0;

    q_nr = norm32(a);
    mantissa_nr = (UWORD32)a << (q_nr);
    q_dr = norm32(b);
    mantissa_dr = (UWORD32)b << (q_dr);
    *q_format = (WORD)(30 + q_nr - q_dr);

    for(i = 0; i < 31; i++)
    {
        /* quotient = quotient << 1; */
        WORD32 bit;

        /*if(mantissa_nr  >=  mantissa_dr)
        {

            mantissa_nr = (((mantissa_nr - mantissa_dr) << 1) + 1);
        }
        else
        {
            mantissa_nr = (UWORD32)mantissa_nr << 1;
        }
        */
        mantissa_nr = subc_32(mantissa_nr, mantissa_dr);
        bit = (mantissa_nr & 0x00000001);
        mantissa_nr = mantissa_nr & 0xfffffffe;
        quotient = (quotient << 1) + bit;
    }

    if(sign < 0)
    {
        quotient = -quotient;
    }

    return quotient;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : mac16x16in32                                             */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and accumulates their      */
/*                  result in a 32 bit variable without sat                  */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b, WORD16 c                             */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : multiply & add                                           */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 acc - 32 bit signed value                         */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac16x16in32(WORD32 a, WORD16 b, WORD16 c)
{
    WORD32 acc;

    acc = mult16x16in32(b, c);

    acc = add32(a, acc);

    return acc;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : mac16x16in32_shl                                         */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and accumulates their      */
/*                  result in a 32 bit variable without sat                  */
/*                  after removing a redundant sign bit in the product       */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD16 b, WORD16 c                             */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : multiply, shift left by 1 & add                          */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 acc - 32 bit signed value                         */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac16x16in32_shl(WORD32 a, WORD16 b, WORD16 c)
{
    WORD32 acc;

    acc = mult16x16in32_shl(b, c);

    acc = add32(a, acc);

    return acc;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : mac16x16in32_shlsat                                      */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and accumulates their      */
/*                  result in a 32 bit variable with sat                     */
/*                  after removing a redundant sign bit in the product       */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD16 b, WORD16 c                             */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : multiply, shift left by 1 & add with sat                 */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 acc - 32 bit signed value                         */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac16x16in32_shl_sat(WORD32 a, WORD16 b, WORD16 c)
{
    WORD32 acc;

    acc = mult16x16in32_shl_sat(b, c);

    acc = add32_sat(a, acc);

    return acc;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : msu16x16in32                                             */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and substracts their       */
/*                  result from a 32 bit variable without sat                */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD32 b, WORD16 c                             */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : multiply & sub                                           */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 acc - 32 bit signed value                         */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu16x16in32(WORD32 a, WORD16 b, WORD16 c)
{
    WORD32 acc;

    acc = mult16x16in32(b, c);

    acc = sub32(a, acc);

    return acc;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : msu16x16in32_shl                                         */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and substracts their       */
/*                  result from a 32 bit variable without sat                */
/*                  after removing a redundant sign bit in the product       */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD16 b, WORD16 c                             */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : multiply, shift left by 1 & sub                          */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 acc - 32 bit signed value                         */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu16x16in32_shl(WORD32 a, WORD16 b, WORD16 c)
{
    WORD32 acc;

    acc = mult16x16in32_shl(b, c);

    acc = sub32(a, acc);

    return acc;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : msu16x16in32_shlsat                                      */
/*                                                                           */
/*  Description   : multiplies two 16 bit numbers and substracts their       */
/*                  result from a 32 bit variable with sat                   */
/*                  after removing a redundant sign bit in the product       */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD16 b, WORD16 c                             */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : multiply, shift left by 1 & sub with sat                 */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 acc - 32 bit signed value                         */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu16x16in32_shl_sat(WORD32 a, WORD16 b, WORD16 c)
{
    WORD32 acc;

    acc = mult16x16in32_shl_sat(b, c);

    acc = sub32_sat(a, acc);

    return acc;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : add32_shr                                                */
/*                                                                           */
/*  Description   : adding two 32 bit numbers and taking care of overflow    */
/*                  by downshifting both numbers before addition             */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD16 b, WORD16 c                             */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : shift right inputs by 1 & add                            */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 sum 32 bit signed value                           */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 add32_shr(WORD32 a, WORD32 b)
{
    WORD32 sum;

    a = shr32(a, 1);
    b = shr32(b, 1);

    sum = add32(a, b);

    return sum;
}

/*****************************************************************************/
/*                                                                           */
/*  Function name : sub32_shr                                                */
/*                                                                           */
/*  Description   : substracting two 32 bit numbers and taking care of       */
/*                  overflow by downshifting both numbers before addition    */
/*                                                                           */
/*  Inputs        : WORD32 a, WORD16 b, WORD16 c                             */
/*                                                                           */
/*  Globals       : none                                                     */
/*                                                                           */
/*  Processing    : shift right inputs by 1 & sub                            */
/*                                                                           */
/*  Outputs       : none                                                     */
/*                                                                           */
/*  Returns       : WORD32 diff - 32 bit signed value                        */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        20 11 2003       aadithya kamath       created                     */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 sub32_shr(WORD32 a, WORD32 b)
{
    WORD32 diff;

    a = shr32(a, 1);
    b = shr32(b, 1);

    diff = sub32(a, b);

    return diff;
}
#endif /* __IA_BASIC_OPS32_H__ */
