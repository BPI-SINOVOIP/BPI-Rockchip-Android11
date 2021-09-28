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
/*  file name        : ia_basic_ops40.h                                      */
/*                                                                           */
/*  description      : this file has all basic operations, which have        */
/*                     40 bit intermediate operations                        */
/*                                                                           */
/*  list of functions: 1. norm40                                             */
/*                     2. add32_shr40                                        */
/*                     3. sub32_shr40                                        */
/*                     4. mult32x16in32_shl                                  */
/*                     5. mult32x16in32                                      */
/*                     6. mult32x16in32_shl_sat                              */
/*                     7. mult32_shl                                         */
/*                     8. mult32                                             */
/*                     9. mult32_shl_sat                                     */
/*                     10.mac32x16in32                                       */
/*                     11.mac32x16in32_shl                                   */
/*                     12.mac32x16in32_shl_sat                               */
/*                     13.mac32                                              */
/*                     14.mac32_shl                                          */
/*                     15.mac32_shl_sat                                      */
/*                     16.msu32x16in32                                       */
/*                     17.msu32x16in32_shl                                   */
/*                     18.msu32x16in32_shl_sat                               */
/*                     19.msu32                                              */
/*                     20.msu32_shl                                          */
/*                     21.msu32_shl_sat                                      */
/*                     22.mac3216_arr40                                      */
/*                     23.mac32_arr40                                        */
/*                     24.mac16_arr40                                        */
/*                     25.add32_arr40                                        */
/*                                                                           */
/*  issues / problems: none                                                  */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

#ifndef __IA_BASIC_OPS40_H__
#define __IA_BASIC_OPS40_H__

/*****************************************************************************/
/* file includes                                                             */
/*  ia_type_def.h                                                            */
/*  ia_basic_ops32.h                                                         */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  function name : norm40                                                   */
/*                                                                           */
/*  description   : normalize input to 32 bits, return denormalizing info    */
/*                  static function                                          */
/*                                                                           */
/*  inputs        : WORD40  *in                                              */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : if input above 32_bits then only the upper 8 bits        */
/*                  normalized to fit in 32_bits else normal 32_bit norming  */
/*                                                                           */
/*  outputs       : normalized 32 bit value                                  */
/*                                                                           */
/*  returns       : WORD16 exponent                                          */
/*                                                                           */
/*  assumptions   : if supplied input is 0 the result returned is 31         */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD16 norm40(WORD40 *in)
{
    WORD16 expo;
    WORD32 tempo;
    WORD40 cmp_val = (WORD40)-2147483648.0;

    if(0 == (*in))
        return 31;

    if(((*in) <= 0x7fffffff) && ((WORD40)(*in) >= cmp_val))
    {
        tempo = (WORD32)(*in);
        expo = norm32(tempo);
        *in = tempo << expo;

        return (expo);
    }

    tempo = (WORD32)((*in) >> 31);
    expo = 31 - (norm32(tempo));
    *in = (*in) >> expo;

    return (-expo);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : add32_shr40                                              */
/*                                                                           */
/*  description   : adds two numbers and right shifts once                   */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : add and right shift                                      */
/*                                                                           */
/*  returns       : WORD32 sum                                               */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 add32_shr40(WORD32 a, WORD32 b)
{
    WORD40 sum;

    sum = (WORD40)a + (WORD40)b;
    sum = sum >> 1;

    return ((WORD32)sum);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : sub32_shr40                                              */
/*                                                                           */
/*  description   : subtracts and right shifts once                          */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : substract and right shift                                */
/*                                                                           */
/*  returns       : WORD32 sum                                               */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 sub32_shr40(WORD32 a, WORD32 b)
{
    WORD40 sum;

    sum = (WORD40)a - (WORD40)b;
    sum = sum >> 1;

    return ((WORD32)sum);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mult32x16in32_shl                                        */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 return bits 46 to 15         */
/*                  doesnt take care of saturation                           */
/*                                                                           */
/*  inputs        : WORD32 a, WORD16 b                                       */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply and right shift by 15                           */
/*                                                                           */
/*  returns       : WORD32 result                                            */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mult32x16in32_shl(WORD32 a, WORD16 b)
{
    WORD32 result;
    LWORD64 temp_result;

    temp_result = (LWORD64)a * (LWORD64)b;

    result = (WORD32)((temp_result + 16384) >> 15);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mult32x16in32                                            */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 return bits 47 to 16         */
/*                                                                           */
/*  inputs        : WORD32 a, WORD16 b                                       */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply and right shift by 16                           */
/*                                                                           */
/*  returns       : WORD32 result                                            */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mult32x16in32(WORD32 a, WORD16 b)
{
    WORD32 result;
    LWORD64 temp_result;

    temp_result = (LWORD64)a * (LWORD64)b;

    result = (WORD32)((temp_result + 16384) >> 16);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mult32x16in32_shl_sat                                    */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 return bits 46 to 15         */
/*                  take care of saturation (MIN32 x MIN16 = MAX32)          */
/*                                                                           */
/*  inputs        : WORD32 a, WORD16 b                                       */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : if input mi_ns return MAX32 else                         */
/*                  multiply and right shift by 15                           */
/*                                                                           */
/*  returns       : WORD32 result                                            */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mult32x16in32_shl_sat(WORD32 a, WORD16 b)
{
    WORD32 result;

    if(a == (WORD32)0x80000000 && b == (WORD16)0x8000)
    {
        result = (WORD32)0x7fffffff;
    }
    else
    {
        result = mult32x16in32_shl(a, b);
    }

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mult32_shl                                               */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 return bits 62 to 31         */
/*                  doesnt take care of saturation                           */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply and right shift by 31                           */
/*                                                                           */
/*  returns       : WORD32 result                                            */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mult32_shl(WORD32 a, WORD32 b)
{
    WORD32 result;
    LWORD64 temp_result;

    temp_result = (LWORD64)a * (LWORD64)b;
    result = (WORD32)(temp_result >> 31);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mult32                                                   */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 return bits 63 to 32         */
/*                                                                           */
/*  inputs        : WORD32 a, WORD16 b                                       */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply and right shift by 32                           */
/*                                                                           */
/*  returns       : WORD32 result                                            */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mult32(WORD32 a, WORD32 b)
{
    WORD32 result;
    LWORD64 temp_result;

    temp_result = (LWORD64)a * (LWORD64)b;
    result = (WORD32)(temp_result >> 32);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mult32_shl_sat                                           */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 return bits 62 to 31         */
/*                  take care of saturation (MIN32 x MIN32 = MAX32)          */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b                                       */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : if input mi_ns return MAX32 else                         */
/*                  multiply and right shift by 31                           */
/*                                                                           */
/*  returns       : WORD32 result                                            */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

#define MPYHIRC(x, y)                                                                              \
    (((int)((short)(x >> 16) * (unsigned short)(y & 0x0000FFFF) + 0x4000) >> 15) +                 \
     ((int)((short)(x >> 16) * (short)((y) >> 16)) << 1))

#define MPYLUHS(x, y) ((int)((unsigned short)(x & 0x0000FFFF) * (short)(y >> 16)))

static PLATFORM_INLINE WORD32 mult32_shl_sat(WORD32 a, WORD32 b)
{
    WORD32 high;

    high = (MPYHIRC(a, b) + (MPYLUHS(a, b) >> 15));

    return high;
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac32x16in32                                             */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 add bits 47 to 16 to acc     */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD16 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply, right shift by 16 & add to acc                 */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac32x16in32(WORD32 a, WORD32 b, WORD16 c)
{
    WORD32 result;

    result = a + mult32x16in32(b, c);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac32x16in32_shl                                         */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 add bits 46 to 15 to acc     */
/*                  doesnt take care of saturation                           */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD16 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply, right shift by 15 & add to acc                 */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac32x16in32_shl(WORD32 a, WORD32 b, WORD16 c)
{
    WORD32 result;

    result = a + mult32x16in32_shl(b, c);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac32x16in32_shl_sat                                     */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 add bits 46 to 15 to acc     */
/*                  takes care of saturation in multiply and addition        */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD16 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : if input mi_ns add MAX32 else multiply,                  */
/*                  right shift by 15 & add to acc with saturation           */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac32x16in32_shl_sat(WORD32 a, WORD32 b, WORD16 c)
{
    return (add32_sat(a, mult32x16in32_shl_sat(b, c)));
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac32                                                    */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 add bits 63 to 32 to acc     */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD32 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply, right shift by 32 & add to acc                 */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac32(WORD32 a, WORD32 b, WORD32 c)
{
    WORD32 result;

    result = a + mult32(b, c);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac32_shl                                                 */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 add bits 62 to 31 to acc     */
/*                  doesnt take care of saturation                           */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD32 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply, right shift by 31 & add to acc                 */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac32_shl(WORD32 a, WORD32 b, WORD32 c)
{
    WORD32 result;

    result = a + mult32_shl(b, c);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac32_shl_sat                                              */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 add bits 62 to 31 to acc     */
/*                  takes care of saturation in multiply and addition        */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD32 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : if input mi_ns add MAX32 else multiply,                   */
/*                  right shift by 31 & add to acc with saturation           */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac32_shl_sat(WORD32 a, WORD32 b, WORD32 c)
{
    return (add32_sat(a, mult32_shl_sat(b, c)));
}

/*****************************************************************************/
/*                                                                           */
/*  function name : msu32x16in32                                             */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 sub bits 47 to 16 from acc   */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD16 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply, right shift by 16 & sub from acc               */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu32x16in32(WORD32 a, WORD32 b, WORD16 c)
{
    WORD32 result;

    result = a - mult32x16in32(b, c);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : msu32x16in32_shl                                          */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 sub bits 46 to 15 from acc   */
/*                  doesnt take care of saturation                           */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD16 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply, right shift by 15 & sub from acc               */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu32x16in32_shl(WORD32 a, WORD32 b, WORD16 c)
{
    WORD32 result;

    result = a - mult32x16in32_shl(b, c);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : msu32x16in32_shl_sat                                     */
/*                                                                           */
/*  description   : multiply WORD32 with WORD16 sub bits 46 to 15 from acc   */
/*                  takes care of saturation in multiply and addition        */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD16 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : if input mi_ns sub MAX32 else multiply,                  */
/*                  right shift by 15 & sub from acc with saturation         */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu32x16in32_shl_sat(WORD32 a, WORD32 b, WORD16 c)
{
    return (sub32_sat(a, mult32x16in32_shl_sat(b, c)));
}

/*****************************************************************************/
/*                                                                           */
/*  function name : msu32                                                    */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 sub bits 63 to 32 from acc   */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD32 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply, right shift by 32 & sub from acc               */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu32(WORD32 a, WORD32 b, WORD32 c)
{
    WORD32 result;

    result = a - mult32(b, c);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : msu32_shl                                                */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 sub bits 62 to 31 from acc   */
/*                  doesnt take care of saturation                           */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD32 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply, right shift by 31 & sub from acc               */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu32_shl(WORD32 a, WORD32 b, WORD32 c)
{
    WORD32 result;

    result = a - mult32_shl(b, c);

    return (result);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : msu32_shl_sat                                            */
/*                                                                           */
/*  description   : multiply WORD32 with WORD32 sub bits 62 to 31 from acc   */
/*                  takes care of saturation in multiply and addition        */
/*                                                                           */
/*  inputs        : WORD32 a, WORD32 b, WORD32 c                             */
/*                                                                           */
/*  outputs       : none                                                     */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : if input mi_ns sub MAX32 else multiply,                  */
/*                  right shift by 31 & sub from acc with saturation         */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 msu32_shl_sat(WORD32 a, WORD32 b, WORD32 c)
{
    return (sub32_sat(a, mult32_shl_sat(b, c)));
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac3216_arr40                                            */
/*                                                                           */
/*  description   : returns normalized 32 bit accumulated result and         */
/*                  denormalizing info                                       */
/*                                                                           */
/*  inputs        : WORD32 x[], WORD16 y[], LOOPINDEX length                 */
/*                                                                           */
/*  outputs       : WORD16 *q_val                                            */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply and accumalate in WORD40 finally normalize      */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  assumptions   : length < 256 for strict definition of WORD40             */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac3216_arr40(WORD32 *x, WORD16 *y, LOOPINDEX length, WORD16 *q_val)
{
    LOOPINDEX i;
    WORD40 sum = 0;

    for(i = 0; i < length; i++)
    {
        sum += (WORD40)(mult32x16in32(x[i], y[i]));
    }

    *q_val = norm40(&sum);

    return (WORD32)sum;
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac32_arr40                                              */
/*                                                                           */
/*  description   : returns normalized 32 bit accumulated result and         */
/*                  denormalizing info                                       */
/*                                                                           */
/*  inputs        : WORD32 x[], WORD32 y[], LOOPINDEX length                 */
/*                                                                           */
/*  outputs       : WORD16 *q_val                                            */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply and accumalate in WORD40 finally normalize      */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  assumptions   : length < 256 for strict definition of WORD40             */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac32_arr40(WORD32 *x, WORD32 *y, LOOPINDEX length, WORD16 *q_val)
{
    LOOPINDEX i;
    WORD40 sum = 0;

    for(i = 0; i < length; i++)
    {
        sum += (WORD40)(mult32(x[i], y[i]));
    }

    *q_val = norm40(&sum);

    return ((WORD32)sum);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : mac16_arr40                                              */
/*                                                                           */
/*  description   : returns normalized 32 bit accumulated result and         */
/*                  denormalizing info                                       */
/*                                                                           */
/*  inputs        : WORD16 x[], WORD16 y[], LOOPINDEX length                 */
/*                                                                           */
/*  outputs       : WORD16 *q_val                                            */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : multiply and accumalate in WORD40 finally normalize      */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  assumptions   : length < 256 for strict definition of WORD40             */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 mac16_arr40(WORD16 *x, WORD16 *y, LOOPINDEX length, WORD16 *q_val)
{
    LOOPINDEX i;
    WORD40 sum = 0;

    for(i = 0; i < length; i++)
    {
        sum += (WORD40)((WORD32)x[i] * (WORD32)y[i]);
    }

    *q_val = norm40(&sum);

    return ((WORD32)sum);
}

/*****************************************************************************/
/*                                                                           */
/*  function name : add32_arr40                                              */
/*                                                                           */
/*  description   : returns normalized 32 bit accumulated result and         */
/*                  denormalizing info                                       */
/*                                                                           */
/*  inputs        : WORD32 x[], LOOPINDEX length                             */
/*                                                                           */
/*  outputs       : WORD16 *q_val                                            */
/*                                                                           */
/*  globals       : none                                                     */
/*                                                                           */
/*  processing    : accumalate in WORD40 finally normalize                   */
/*                                                                           */
/*  returns       : WORD32 accumulated result                                */
/*                                                                           */
/*  assumptions   : length < 256 for strict definition of WORD40             */
/*                                                                           */
/*  issues        : none                                                     */
/*                                                                           */
/*  revision history :                                                       */
/*                                                                           */
/*        DD MM YYYY       author                changes                     */
/*        06 12 2002       ashok M/chetan K      created                     */
/*        21 11 2003       raghavendra K R       modified(bug fixes)         */
/*        15 11 2004       tejaswi/vishal        modified(bug fixes/cleanup) */
/*                                                                           */
/*****************************************************************************/

static PLATFORM_INLINE WORD32 add32_arr40(WORD32 *in_arr, LOOPINDEX length, WORD16 *q_val)
{
    LOOPINDEX i;
    WORD40 sum = 0;

    for(i = 0; i < length; i++)
    {
        sum += (WORD40)in_arr[i];
    }

    *q_val = norm40(&sum);

    return ((WORD32)sum);
}

#endif /* __IA_BASIC_OPS40_H__ */
