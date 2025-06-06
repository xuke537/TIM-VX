/****************************************************************************
*
*    Copyright (c) 2020 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/
#ifndef _VSI_NN_DTYPE_UTIL_PRV_H
#define _VSI_NN_DTYPE_UTIL_PRV_H

#include "vsi_nn_types.h"
#include "vsi_nn_math.h"
#include "vsi_nn_tensor.h"
#include "vsi_nn_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A helper union for fp32 bit casting.
 */
typedef union {
    float val;
    uint32_t data;
} fp32_bit_cast_t;

static VSI_INLINE_API vsi_bool type_is_integer
    (
    const vsi_nn_type_e type
    )
{
    vsi_bool ret;
    ret = FALSE;
    switch( type )
    {
    case VSI_NN_TYPE_INT4:
    case VSI_NN_TYPE_INT8:
    case VSI_NN_TYPE_INT16:
    case VSI_NN_TYPE_INT32:
    case VSI_NN_TYPE_INT64:
    case VSI_NN_TYPE_UINT4:
    case VSI_NN_TYPE_UINT8:
    case VSI_NN_TYPE_UINT16:
    case VSI_NN_TYPE_UINT32:
    case VSI_NN_TYPE_UINT64:
    case VSI_NN_TYPE_BOOL8:
        ret = TRUE;
        break;
    default:
        break;
    }
    return ret;
} /* type_is_integer() */

static VSI_INLINE_API vsi_bool type_is_signed
    (
    const vsi_nn_type_e type
    )
{
    vsi_bool ret;
    ret = FALSE;
    switch( type )
    {
    case VSI_NN_TYPE_INT4:
    case VSI_NN_TYPE_INT8:
    case VSI_NN_TYPE_INT16:
    case VSI_NN_TYPE_INT32:
    case VSI_NN_TYPE_INT64:
    case VSI_NN_TYPE_FLOAT16:
    case VSI_NN_TYPE_FLOAT32:
    case VSI_NN_TYPE_FLOAT64:
    case VSI_NN_TYPE_BFLOAT16:
    case VSI_NN_TYPE_FLOAT8_E4M3:
    case VSI_NN_TYPE_FLOAT8_E5M2:
        ret = TRUE;
        break;
    default:
        break;
    }
    return ret;
} /* type_is_signed() */

static VSI_INLINE_API uint32_t type_get_bytes
    (
    const vsi_nn_type_e type
    )
{
    switch( type )
    {
    case VSI_NN_TYPE_INT4:
    case VSI_NN_TYPE_UINT4:
        return 0;
    case VSI_NN_TYPE_INT8:
    case VSI_NN_TYPE_UINT8:
    case VSI_NN_TYPE_BOOL8:
    case VSI_NN_TYPE_FLOAT8_E4M3:
    case VSI_NN_TYPE_FLOAT8_E5M2:
        return 1;
    case VSI_NN_TYPE_INT16:
    case VSI_NN_TYPE_UINT16:
    case VSI_NN_TYPE_FLOAT16:
    case VSI_NN_TYPE_BFLOAT16:
        return 2;
    case VSI_NN_TYPE_INT32:
    case VSI_NN_TYPE_UINT32:
    case VSI_NN_TYPE_FLOAT32:
        return 4;
    case VSI_NN_TYPE_INT64:
    case VSI_NN_TYPE_UINT64:
    case VSI_NN_TYPE_FLOAT64:
        return 8;
    default:
        VSILOGE("unsupported type: %d", type);
        return 1;
    }
} /* type_get_bytes() */

static VSI_INLINE_API uint32_t type_get_bits
    (
    const vsi_nn_type_e type
    )
{
    switch( type )
    {
    case VSI_NN_TYPE_INT4:
    case VSI_NN_TYPE_UINT4:
        return 4;
    case VSI_NN_TYPE_INT8:
    case VSI_NN_TYPE_UINT8:
    case VSI_NN_TYPE_BOOL8:
    case VSI_NN_TYPE_FLOAT8_E4M3:
    case VSI_NN_TYPE_FLOAT8_E5M2:
        return 8;
    case VSI_NN_TYPE_INT16:
    case VSI_NN_TYPE_UINT16:
    case VSI_NN_TYPE_FLOAT16:
    case VSI_NN_TYPE_BFLOAT16:
        return 16;
    case VSI_NN_TYPE_INT32:
    case VSI_NN_TYPE_UINT32:
    case VSI_NN_TYPE_FLOAT32:
        return 32;
    case VSI_NN_TYPE_INT64:
    case VSI_NN_TYPE_UINT64:
    case VSI_NN_TYPE_FLOAT64:
        return 64;
    default:
        VSILOGE("unsupported type: %d", type);
        return 1;
    }
} /* type_get_bits() */

static VSI_INLINE_API void type_get_range
    (
    vsi_nn_type_e type,
    double  * max_range,
    double  * min_range
    )
{
    int32_t bits;
    double from, to;
    from = 0.0;
    to = 0.0;
    bits = type_get_bits( type );
    if( type_is_integer( type ) || bits > 0)
    {
        if( type_is_signed( type ) )
        {
            from = (double)(-(1L << (bits - 1)));
            to = (double)((1UL << (bits - 1)) - 1);
        }
        else
        {
            from = 0.0;
            to = (double)((1UL << bits) - 1);
        }
    }
    else
    {
        //  TODO: Add float
    }
    if( NULL != max_range )
    {
        *max_range = to;
    }
    if( NULL != min_range )
    {
        *min_range = from;
    }
} /* type_get_range() */

static VSI_INLINE_API vsi_bool fp32_is_inf
    (
        float val
    )
{
    fp32_bit_cast_t fp32_bit_cast;
    fp32_bit_cast.val = val;
    uint32_t fp32_data = fp32_bit_cast.data;

    if ((fp32_data & (uint32_t)VSI_NN_INT32_MAX) == (uint32_t)VSI_NN_FLOAT32_INF)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static VSI_INLINE_API int32_t fp32_to_affine
    (
    const float  in,
    const float  scale,
    const int32_t    zero_point,
    const vsi_nn_type_e type
    )
{
    int32_t data;
    double max_range;
    double min_range;
    type_get_range( type, &max_range, &min_range );
    data = (int32_t)(vsi_rint( in / scale ) + zero_point );
    data = vsi_nn_max( (int32_t)min_range, vsi_nn_min( (int32_t)max_range , data ) );

    if (fp32_is_inf(in) != 0)
    {
        fp32_bit_cast_t fp32_bit_cast;
        fp32_bit_cast.val = in;
        uint32_t sign = fp32_bit_cast.data >> 31;
        data = sign == 1 ? (int32_t)min_range : (int32_t)max_range;
    }

    return data;
} /* fp32_to_affine() */

static VSI_INLINE_API float affine_to_fp32
    (
    const int32_t    val,
    const float  scale,
    const int32_t    zero_point,
    const vsi_nn_type_e type
    )
{
    float data;
    VSI_UNREFERENCED(type);
    data = ( (float)val - zero_point ) * scale;
    return data;
} /* affine_to_fp32() */

static VSI_INLINE_API int32_t fp32_to_dfp
    (
    const float in,
    const int8_t    fl,
    const vsi_nn_type_e type
    )
{
    int32_t data;
    double max_range;
    double min_range;
    type_get_range( type, &max_range, &min_range );
    if( fl > 0 )
    {
        data = (int32_t)vsi_rint( in * (double)( (int64_t)1 << fl ) );
    }
    else
    {
        data = (int32_t)vsi_rint( in * ( 1.0f / (double)( (int64_t)1 << -fl ) ) );
    }
    data = vsi_nn_min( data, (int32_t)max_range );
    data = vsi_nn_max( data, (int32_t)min_range );

    if (fp32_is_inf(in) != 0)
    {
        fp32_bit_cast_t fp32_bit_cast;
        fp32_bit_cast.val = in;
        uint32_t sign = fp32_bit_cast.data >> 31;
        data = sign == 1 ? (int32_t)min_range : (int32_t) max_range;
    }

    return data;
} /* fp32_to_dfp() */

static VSI_INLINE_API float dfp_to_fp32
    (
    const int32_t val,
    const int8_t  fl,
    const vsi_nn_type_e type
    )
{
    float result;
    VSI_UNREFERENCED(type);
    if( fl > 0 )
    {
        result = (float)val * ( 1.0f / ( (float) ( (int64_t)1 << fl ) ) );
    }
    else
    {
        result = (float)val * ( (float) ( (int64_t)1 << -fl ) );
    }
    return result;
} /* dfp_to_fp32() */

static VSI_INLINE_API vsi_status integer_convert
    (
    const void *    src,
    vsi_nn_type_e   src_type,
    void *          dest,
    vsi_nn_type_e   dest_type
    )
{
    vsi_status status = VSI_SUCCESS;
    if( type_is_integer( src_type ) && type_is_integer( dest_type ) )
    {
        uint8_t    all_zeros[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        uint8_t    all_ones[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
        uint32_t   src_sz = type_get_bytes( src_type );
        uint32_t   dest_sz = type_get_bytes( dest_type );
        uint8_t*   buffer = all_zeros;
        if( src_sz == 0 )
        {
            src_sz = 1;
        }
        if( dest_sz == 0)
        {
            dest_sz = 1;
        }
        if( type_is_signed( src_type ) && (((int8_t *)src)[src_sz - 1] & 0x80) )
        {
            buffer = all_ones;
        }
        memcpy( buffer, src, src_sz );
        memcpy( dest, buffer, dest_sz );
    }
    else
    {
        status = VSI_FAILURE;
    }
    return status;
} /* integer_convert() */

typedef union
{
    uint32_t u;
    float f;
} _fp32_t;

static VSI_INLINE_API float fp16_to_fp32
    (
    int16_t in
    )
{
    const _fp32_t magic = { (254 - 15) << 23 };
    const _fp32_t infnan = { (127 + 16) << 23 };
    _fp32_t o;
    // Non-sign bits
    o.u = ( in & 0x7fff ) << 13;
    o.f *= magic.f;
    if(o.f  >= infnan.f)
    {
        o.u |= 255 << 23;
    }
    //Sign bit
    o.u |= ( in & 0x8000 ) << 16;
    return o.f;
} /* fp16_to_fp32() */

static VSI_INLINE_API float bfp16_to_fp32
    (
    uint16_t in
    )
{
    float out;
    fp32_bit_cast_t fp32_bit_cast;

    fp32_bit_cast.data = (uint32_t)(in << 16);

    out = fp32_bit_cast.val;

    return out;
} /* bfp16_to_fp32() */

static VSI_INLINE_API uint16_t fp32_to_fp16
    (
    float in
    )
{
    fp32_bit_cast_t fp32_bit_cast;
    fp32_bit_cast.val = in;
    uint32_t fp32_data = fp32_bit_cast.data;
    uint32_t t1 = (fp32_data & 0x80000000u) >> 16;  /* sign bit. */
    uint32_t t2 = (fp32_data & 0x7F800000u) >> 13;  /* Exponent bits */
    uint32_t t3 = (fp32_data & 0x007FE000u) >> 13;  /* Mantissa bits, no rounding */
    uint32_t fp16 = 0u;
    if( t2 >= 0x023c00u )
    {
        fp16 = t1 | 0x7BFF;     /* Don't round to infinity. */
    }
    else if( t2 <= 0x01c000u )
    {
        fp16 = t1;
    }
    else
    {
        t2 -= 0x01c000u;
        fp16 = t1 | t2 | t3;
    }
    return (uint16_t) fp16;
} /* fp32_to_fp16() */

static VSI_INLINE_API uint16_t fp32_to_bfp16
    (
    float in
    )
{
    fp32_bit_cast_t fp32_bit_cast;
    fp32_bit_cast.val = in;
    uint32_t fp32_data = fp32_bit_cast.data;
    uint32_t t1 = fp32_data >> 16;

    return (uint16_t) t1;
} /* fp32_to_bfp16() */

static VSI_INLINE_API uint16_t fp32_to_bfp16_rtne
    (
    float in
    )
{
    /*
    Convert a float point to bfloat16, with round-nearest-to-even as rounding method.
    */

    fp32_bit_cast_t fp32_bit_cast;
    fp32_bit_cast.val = in;
    uint32_t fp32_data = fp32_bit_cast.data;
    uint16_t out;

    uint32_t lsb = (fp32_data >> 16) & 1;    /* Least significant bit of resulting bfloat. */
    uint32_t rounding_bias = 0x7fff + lsb;

    if ( VSI_NN_FLOAT32_NAN == in )
    {
        out = 0x7fc0;
    }
    else
    {
        fp32_data += rounding_bias;
        out = (uint16_t) (fp32_data >> 16);
    }

    return out;
} /* fp32_to_bfp16_rtne */

#define FLOAT_BIAS_EXPONENT 127
#define FLOAT_EXPONENT_SIZE 8
#define FLOAT_MANTISSA_SIZE  23
#define FLOAT8_E4M3_BIAS_EXPONENT 7
#define FLOAT8_E4M3_EXPONENT_SIZE 4
#define FLOAT8_E4M3_MANTISSA_SIZE 3
#define FLOAT8_E5M2_BIAS_EXPONENT 15
#define FLOAT8_E5M2_EXPONENT_SIZE 5
#define FLOAT8_E5M2_MANTISSA_SIZE 2

static VSI_INLINE_API uint8_t fp32_to_fp8_e4m3(float in, const float scale) {
    float fp8_f32 = in / scale;
    fp32_bit_cast_t fp32_bit_cast;
    fp32_bit_cast.val = fp8_f32;
    uint32_t in_val = fp32_bit_cast.data;

    uint32_t in_sign = (in_val >> (FLOAT_EXPONENT_SIZE + FLOAT_MANTISSA_SIZE)) & 0x1; /* bit 31 is sign */
    uint32_t in_exp = (in_val >> FLOAT_MANTISSA_SIZE) & 0xFF; /* bit[30: 24] is exp */
    uint32_t in_man = (in_val & 0x7FFFFF);   /* low 23 bits is man */

    uint32_t out_sign = in_sign;
    int32_t out_exp = (in_exp + FLOAT8_E4M3_BIAS_EXPONENT - FLOAT_BIAS_EXPONENT); /* in_exp - fp32bias + SE4M3 bias */
    uint32_t man_rounding = 0, out_man = 0, out_val = 0;

    man_rounding = (in_man + 0x80000) >> 20; /* manrounding is 3 bits */
    if (((man_rounding >> 3) && 0x1) == 1) {
        /* when in_man like 0b11_1, exp += 1, mantissa is 0*/
        out_exp += 1;
    }

    /* Clamp Denorm to zero */
    if (out_exp <= 0) {
        out_exp = 0;
        man_rounding = 0;
        out_sign = 0;
    }

    out_man = man_rounding & 0x7; /* keep low 3 bits of man */
    /* overflow policy */
    if (out_exp >= 16 || (out_exp == 15 && out_man == 7)) {
        out_exp = 15;
        out_man = 6;
#if 0
        if (mode == VX_CONVERT_POLICY_SATURATE) {
            out_exp = 15;
            out_man = 6;
        } else if (mode == VX_CONVERT_POLICY_INF) {
            out_exp = 15;
            out_man = 7;
        } else {
            vxmASSERT(0 && "Error overflow mode!\n");
        }
#endif
    }
    out_val = (out_sign << 7) | (out_exp << 3) | out_man;
    return (uint8_t)(out_val & 0xFF);
} /* fp32_to_fp8_e4m3() */

static VSI_INLINE_API uint8_t fp32_to_fp8_e5m2(float in, const float scale) {
    float fp8_f32 = in / scale;
    fp32_bit_cast_t fp32_bit_cast;
    fp32_bit_cast.val = fp8_f32;
    uint32_t in_val = fp32_bit_cast.data;
    uint32_t in_sign = (in_val >> (FLOAT_EXPONENT_SIZE + FLOAT_MANTISSA_SIZE)) & 0x1; /* bit 31 is sign */
    uint32_t in_exp = (in_val >> FLOAT_MANTISSA_SIZE) & 0xFF; /* bit[30: 24] is exp */
    uint32_t in_man = (in_val & 0x7FFFFF);   /* low 23 bits is man */

    uint32_t out_sign = in_sign;
    int32_t out_exp = (in_exp + FLOAT8_E5M2_BIAS_EXPONENT - FLOAT_BIAS_EXPONENT); /* in_exp - fp32bias + SE5M2 bias */
    uint32_t man_rounding = 0, out_man = 0, out_val = 0;

    man_rounding = (in_man + 0x100000) >> 21; /* manrounding is 2 bits */
    if (((man_rounding >> 2) && 0x1) == 1) {
        /* when in_man like 0b11, exp += 1, mantissa is 0*/
        out_exp += 1;
    }

    /* Clamp Denorm to zero */
    if (out_exp <= 0) {
        out_exp = 0;
        man_rounding = 0;
        out_sign = 0;
    }

    out_man = man_rounding & 0x3; /* keep low 9 bits of man */
    /* overflow policy */
    if (out_exp >= 31) {
        out_exp = 30;
        out_man = 3;
#if 0
        if (mode == VX_CONVERT_POLICY_SATURATE) {
            out_exp = 30;
            out_man = 3;
        } else if (mode == VX_CONVERT_POLICY_INF) {
            out_exp = 31;
            out_man = 0;
        } else {
            vxmASSERT(0 && "Error overflow mode!\n");
        }
#endif
    }
    out_val = (out_sign << 7) | (out_exp << 2) | out_man;
    return (uint8_t)(out_val & 0xFF);
} /* fp32_to_fp8_e5m2() */

static VSI_INLINE_API float fp8_e4m3_to_fp32(uint8_t in, const float scale) {
    float val_fp32;
    uint32_t signOut = 0;
    uint32_t exponentOut = 0;
    uint32_t mantissaOut = 0;
    uint32_t out_u = 0;
    fp32_bit_cast_t fp32_bit_cast;

    {
        uint32_t signIn;
        uint32_t exponentIn;
        uint32_t mantissaIn;
        uint32_t expShiftValue = FLOAT_BIAS_EXPONENT - FLOAT8_E4M3_BIAS_EXPONENT;
        //uint32_t i = 0;
        //uint32_t intMsk = 0x4;

        signIn = (in >> (FLOAT8_E4M3_EXPONENT_SIZE + FLOAT8_E4M3_MANTISSA_SIZE)) & 0x1;
        exponentIn = (in >> FLOAT8_E4M3_MANTISSA_SIZE) & 0xF;
        mantissaIn = in & 0x7;

        signOut = signIn;

        /* clamp subnorm*/
        if (exponentIn == 0) {
            goto final;
        }
        /*
        if (exponentIn == 0 && mantissaIn == 0)
        {
            break;
        }
        else if (exponentIn == 0)
        {
            while (!(mantissaIn & intMsk))
            {
                intMsk >>= 1;
                ++i;
            }
            exponentOut = (exponentIn + expShiftValue - i) & 0xff;
            mantissaIn  = ((mantissaIn ^ intMsk) << (i + 1));
            mantissaOut = (mantissaIn << (FLOAT_MATISSA_SIZE - FLOAT8_E4M3_MANTISSA_SIZE)) & 0x7fffff;
            break;
        }
        */

        if (exponentIn == 0xf && mantissaIn == 0x7) {
            exponentOut = 0xff;
            mantissaOut = 0x400000;
            goto final;
        }

        exponentOut = (exponentIn + expShiftValue) & 0xff;
        mantissaOut = (mantissaIn << (FLOAT_MANTISSA_SIZE - FLOAT8_E4M3_MANTISSA_SIZE)) & 0x7fffff;
    }
final:
    out_u = signOut << 31 | exponentOut << 23 | mantissaOut;
    fp32_bit_cast.data = out_u;
    val_fp32 = fp32_bit_cast.val;

    return val_fp32 * scale;
} /* fp8_e4m3_to_fp32() */

static VSI_INLINE_API float fp8_e5m2_to_fp32(int8_t in, const float scale) {
    float val_fp32;
    uint32_t signOut = 0;
    uint32_t exponentOut = 0;
    uint32_t mantissaOut = 0;
    uint32_t out_u = 0;
    fp32_bit_cast_t fp32_bit_cast;

    {
        uint32_t signIn;
        uint32_t exponentIn;
        uint32_t mantissaIn;
        uint32_t expShiftValue = FLOAT_BIAS_EXPONENT - FLOAT8_E5M2_BIAS_EXPONENT;
        //uint32_t i = 0;
        //uint32_t intMsk = 0x2;

        signIn = (in >> (FLOAT8_E5M2_EXPONENT_SIZE + FLOAT8_E5M2_MANTISSA_SIZE)) & 0x1;
        exponentIn = (in >> FLOAT8_E5M2_MANTISSA_SIZE) & 0x1F;
        mantissaIn = in & 0x3;

        signOut = signIn;

        /* clamp subnorm*/
        if (exponentIn == 0) {
            goto final;
        }
        /*
        if (exponentIn == 0 && mantissaIn == 0)
        {
            break;
        }
        else if (exponentIn == 0)
        {
            while (!(mantissaIn & intMsk))
            {
                intMsk >>= 1;
                ++i;
            }
            exponentOut = (exponentIn + expShiftValue - i) & 0xff;
            mantissaIn = ((mantissaIn ^ intMsk) << (i + 1));
            mantissaOut = (mantissaIn << (FLOAT_MATISSA_SIZE - FLOAT8_E5M2_MANTISSA_SIZE)) & 0x7fffff;
            break;
        }
        */

        if (exponentIn == 0x1f && mantissaIn == 0x3) {
            exponentOut = 0xff;
            mantissaOut = 0x400000;
            goto final;
        }

        exponentOut = (exponentIn + expShiftValue) & 0xff;
        mantissaOut = (mantissaIn << (FLOAT_MANTISSA_SIZE - FLOAT8_E5M2_MANTISSA_SIZE)) & 0x7fffff;
    }
final:
    out_u = signOut << 31 | exponentOut << 23 | mantissaOut;
    fp32_bit_cast.data = out_u;
    val_fp32 = fp32_bit_cast.val;
    return val_fp32 * scale;
} /* fp8_e5m2_to_fp32() */

static VSI_INLINE_API vsi_status dtype_to_float32
    (
    uint8_t *src,
    float   *dst,
    const vsi_nn_dtype_t * src_dtype
    )
{
    switch( src_dtype->vx_type )
    {
    case VSI_NN_TYPE_FLOAT32:
        *dst = *(float *)src;
        break;
    case VSI_NN_TYPE_FLOAT16:
        *dst = fp16_to_fp32( *(int16_t *)src );
        break;
    case VSI_NN_TYPE_BFLOAT16:
        *dst = bfp16_to_fp32( *(uint16_t *)src );
        break;
    case VSI_NN_TYPE_FLOAT8_E4M3:
        *dst = fp8_e4m3_to_fp32(*(int8_t*)src, src_dtype->scale);
        break;
    case VSI_NN_TYPE_FLOAT8_E5M2:
        *dst = fp8_e5m2_to_fp32(*(int8_t *)src, src_dtype->scale);
        break;
    case VSI_NN_TYPE_INT4:
    case VSI_NN_TYPE_UINT4:
    case VSI_NN_TYPE_INT8:
    case VSI_NN_TYPE_BOOL8:
    case VSI_NN_TYPE_UINT8:
    case VSI_NN_TYPE_INT16:
    case VSI_NN_TYPE_UINT16:
    case VSI_NN_TYPE_INT32:
        {
            int32_t src_value = 0;
            integer_convert(src, src_dtype->vx_type, &src_value, VSI_NN_TYPE_INT32 );
            switch( src_dtype->qnt_type )
            {
            case VSI_NN_QNT_TYPE_DFP:
                *dst = dfp_to_fp32( src_value, src_dtype->fl, src_dtype->vx_type );
                break;
            case VSI_NN_QNT_TYPE_AFFINE_SYMMETRIC:
            case VSI_NN_QNT_TYPE_AFFINE_ASYMMETRIC:
                *dst = affine_to_fp32( src_value,
                    src_dtype->scale, src_dtype->zero_point, src_dtype->vx_type );
                break;
            case VSI_NN_QNT_TYPE_NONE:
                *dst = (float)src_value;
                break;
            default:
                break;
            }
        }
        break;
    default:
        return VSI_FAILURE;
    }
    return VSI_SUCCESS;
}

static VSI_INLINE_API vsi_status float32_to_dtype
    (
    float   src,
    uint8_t *dst,
    const vsi_nn_dtype_t * dst_dtype
    )
{
    switch( dst_dtype->vx_type )
    {
    case VSI_NN_TYPE_FLOAT32:
        *(float *)dst = src;
        break;
    case VSI_NN_TYPE_FLOAT16:
        *(int16_t *)dst = fp32_to_fp16( src );
        break;
    case VSI_NN_TYPE_BFLOAT16:
        *(int16_t *)dst = fp32_to_bfp16_rtne( src );
        break;
    case VSI_NN_TYPE_FLOAT8_E4M3:
        *(int8_t *)dst = fp32_to_fp8_e4m3(src, dst_dtype->scale);
        break;
    case VSI_NN_TYPE_FLOAT8_E5M2:
        *(int8_t *)dst = fp32_to_fp8_e5m2(src, dst_dtype->scale);
        break;
    case VSI_NN_TYPE_INT4:
    case VSI_NN_TYPE_UINT4:
    case VSI_NN_TYPE_INT8:
    case VSI_NN_TYPE_BOOL8:
    case VSI_NN_TYPE_UINT8:
    case VSI_NN_TYPE_INT16:
    case VSI_NN_TYPE_UINT16:
    case VSI_NN_TYPE_INT32:
    case VSI_NN_TYPE_UINT32:
        {
            int32_t dst_value = 0;
            switch( dst_dtype->qnt_type )
            {
            case VSI_NN_QNT_TYPE_DFP:
                dst_value = fp32_to_dfp( src, dst_dtype->fl, dst_dtype->vx_type );
                break;
            case VSI_NN_QNT_TYPE_AFFINE_SYMMETRIC:
            case VSI_NN_QNT_TYPE_AFFINE_ASYMMETRIC:
                dst_value = fp32_to_affine( src,
                    dst_dtype->scale, dst_dtype->zero_point, dst_dtype->vx_type );
                break;
            case VSI_NN_QNT_TYPE_NONE:
                dst_value = (int32_t)src;
                break;
            default:
                break;
            }
            integer_convert( &dst_value, VSI_NN_TYPE_INT32, dst, dst_dtype->vx_type );
        }
        break;
    default:
        return VSI_FAILURE;
    }
    return VSI_SUCCESS;
}

#ifdef __cplusplus
}
#endif

vsi_bool vsi_nn_dtype_convert_float_to_quantize_symm8
    (
    const float * buffer, size_t size,
    float scale, int32_t zero_point,
    int8_t * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_symm16
    (
    const float * buffer, size_t size,
    float scale, int32_t zero_point,
    int16_t * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_symm32
    (
    const float * buffer, size_t size,
    float scale, int32_t zero_point,
    int32_t * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_symm64
    (
    const float * buffer, size_t size,
    float scale, int32_t zero_point,
    int64_t * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_asymm8
    (
    const float * buffer, size_t size,
    float scale, int32_t zero_point,
    uint8_t * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_symm8_perchannel
    (
    const float * buffer, size_t size,
    const vsi_size_t * shape, size_t rank,
    const float * scale, size_t scale_size,
    const int32_t * zero_point, size_t zero_point_size,
    int32_t channel_dim,
    int8_t * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_symm8_to_float
    (
    const int8_t * buffer, size_t size,
    float scale, int32_t zero_point,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_symm16_to_float
    (
    const int16_t * buffer, size_t size,
    float scale, int32_t zero_point,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_symm32_to_float
    (
    const int32_t * buffer, size_t size,
    float scale, int32_t zero_point,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_symm64_to_float
    (
    const int64_t * buffer, size_t size,
    float scale, int32_t zero_point,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_asymm8_to_float
    (
    const uint8_t * buffer, size_t size,
    float scale, int32_t zero_point,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_symm8_perchannel_to_float
    (
    const int8_t * buffer, size_t size,
    const vsi_size_t * shape, size_t rank,
    const float * scale, size_t scale_size,
    const int32_t * zero_point, size_t zero_point_size,
    int32_t channel_dim,
    float * out_buffer
    );


#endif
