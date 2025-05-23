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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vsi_nn_types.h"
#include "vsi_nn_tensor.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_log.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_tensor_util.h"
#include "vsi_nn_error.h"
#include "utils/vsi_nn_util.h"
#include "kernel/vsi_nn_kernel.h"
#include "kernel/vsi_nn_kernel_eltwise.h"

__BEGIN_DECLS

#define VX_KERNEL_NAME_PRE_PROCESS_YUV444_SCALE_U8TOF16    CVIVANTE_NAMESPACE("evis.pre_process_yuv444_scale_U8toF16")
#define VX_KERNEL_NAME_PRE_PROCESS_YUV444_SCALE_U8TOI16    CVIVANTE_NAMESPACE("evis.pre_process_yuv444_scale_U8toI16")
#define VX_KERNEL_NAME_PRE_PROCESS_YUV444_SCALE_U8TOU8     CVIVANTE_NAMESPACE("evis.pre_process_yuv444_scale_U8toU8")
#define VX_KERNEL_NAME_PRE_PROCESS_YUV444_SCALE_U8TOI8     CVIVANTE_NAMESPACE("evis.pre_process_yuv444_scale_U8toI8")
#define VX_KERNEL_NAME_PRE_PROCESS_YUV444_COPY_U8TOU8      CVIVANTE_NAMESPACE("evis.pre_process_yuv444_copy_U8toU8")
#define VX_KERNEL_NAME_PRE_PROCESS_YUV444_COPY_U8TOF16     CVIVANTE_NAMESPACE("evis.pre_process_yuv444_copy_U8toF16")

#define KERNEL_SOURCE_1    "pre_process_yuv444_scale",
#define KERNEL_SOURCE_3    "pre_process_yuv444_scale_fp16",
#define KERNEL_SOURCE_4    "pre_process_yuv444_copy_u8",

typedef enum
{
    COPY = 0,
    SCALE,
    TRANS,
    COPY_TRANS
} vsi_nn_kernel_convert_type_e;

#define HASH_PRE_PROCESS_YUV444_KEY(_input0_type, _output_type, _convert_type, _image_2d) \
    ((_input0_type << 24) | (_output_type << 16) | (_convert_type << 8) | (_image_2d))

#define TENSOR_PRE_PROCESS_YUV444_KERNELS(IN0_TYPE, OUT_TYPE, CONVERT_TYPE, SOURCE) \
    { HASH_PRE_PROCESS_YUV444_KEY(IN0_TYPE, OUT_TYPE, CONVERT_TYPE, 0), \
        VX_KERNEL_NAME_PRE_PROCESS_YUV444_##CONVERT_TYPE##_##IN0_TYPE##TO##OUT_TYPE, \
        SOURCE },

static const struct {
        uint32_t key;
        char* function_name;
        const char* source_name;
    } pre_process_yuv444_map[] =
{
    TENSOR_PRE_PROCESS_YUV444_KERNELS(U8, F16, SCALE,        KERNEL_SOURCE_3)
    TENSOR_PRE_PROCESS_YUV444_KERNELS(U8, I16, SCALE,        KERNEL_SOURCE_1)
    TENSOR_PRE_PROCESS_YUV444_KERNELS(U8, U8,  SCALE,        KERNEL_SOURCE_1)
    TENSOR_PRE_PROCESS_YUV444_KERNELS(U8, I8,  SCALE,        KERNEL_SOURCE_1)
    TENSOR_PRE_PROCESS_YUV444_KERNELS(U8, U8,  COPY,         KERNEL_SOURCE_4)
    TENSOR_PRE_PROCESS_YUV444_KERNELS(U8, F16, COPY,         KERNEL_SOURCE_4)
};

static vx_param_description_t vxPreProcessYuv444Kernel_param_def[] =
{
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
};
#define _EVIS_PRE_PROCESS_YUV444_PARAM_NUM          _cnt_of_array(vxPreProcessYuv444Kernel_param_def)

DEF_KERNEL_INITIALIZER(_pre_process_yuv444_copy_initializer)
    (
    vsi_nn_kernel_node_t node,
    const vsi_nn_kernel_node_param_t * param,
    size_t param_size
    )
{
    vsi_status status = VSI_FAILURE;
    gpu_param_t shaderParam = {
        3,          // workdim
        {0, 0, 0},  // globalWorkOffset: control the start location be processed in the image
        {0, 0, 0},  // globalWorkScale: how many pixels could be processed by a single thread
        {0, 0, 0},  // localWorkSize: local group size in thread
        {0, 0, 0}}; // globalWorkSize: image size in thread

    int32_t     dstZP      = 0;
    float       dstScale   = 1;
    int32_t     reorder    = 0;
    int32_t     order1     = 2;
    uint32_t    width      = 0;
    uint32_t    height     = 0;

    vsi_nn_kernel_tensor_attr_t * attr[1] = { NULL };
    vsi_size_array_t * out_shape = NULL;

    VSI_UNREFERENCED(param_size);

    attr[0] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[3] );
    CHECK_PTR_FAIL_GOTO( attr[0], "Create tensor attr buffer fail.", OnError );

    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[12], &reorder);
    CHECK_STATUS_FAIL_GOTO(status, OnError );

    out_shape  = attr[0]->shape;
    width      = (uint32_t)(out_shape->data[0]);
    height     = (uint32_t)(out_shape->data[1]);

    if (reorder != 0)
    {
        reorder = 2;
        order1 = 0;
    }

    dstScale = 1.0f / attr[0]->scale;
    dstZP = attr[0]->zero_point;

    shaderParam.global_scale[0]  = 16;
    shaderParam.global_scale[1]  = 1;
    shaderParam.global_scale[2]  = 1;
    shaderParam.global_size[0]   = gpu_align_p2((width + shaderParam.global_scale[0] - 1)
        / shaderParam.global_scale[0], 4);
    shaderParam.global_size[1]   = gpu_align_p2((height + shaderParam.global_scale[1] - 1)
        / shaderParam.global_scale[1], 2);
    shaderParam.global_size[2]   = 1;

    status = vsi_nn_kernel_gpu_config( node, &shaderParam );
    CHECK_STATUS_FAIL_GOTO(status, OnError);

    {
        gpu_dp_inst_t uniCalculateTmpR1st_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00110000, 0x00330022, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x0199012a, 0x00000000, 0x0199012a, 0x00000000,
                0x0199012a, 0x00000000, 0x0199012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpR2nd_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00550044, 0x00770066, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x0199012a, 0x00000000, 0x0199012a, 0x00000000,
                0x0199012a, 0x00000000, 0x0199012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpR3rd_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00990088, 0x00bb00aa, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x0199012a, 0x00000000, 0x0199012a, 0x00000000,
                0x0199012a, 0x00000000, 0x0199012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpR4th_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00dd00cc, 0x00ff00ee, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x0199012a, 0x00000000, 0x0199012a, 0x00000000,
                0x0199012a, 0x00000000, 0x0199012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateR1st_4x4 = {{
                0x0f0f0f0f, // TCfg
                0x04040404, // ASelt
                0x00010000, 0x00030002, // ABin
                0x00000000, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00002608, // AccumType, ConstantType, and PostShift
                0x00000000, 0x00000000, 0x00000000, 0x00000000,
                0x00000000, 0x00000000, 0x00000000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateTmpG1st_4x4 = {{
                0x09090909, // TCfg
                0x04040404, // ASelt
                0x00110000, 0x00330022, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000,
                0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpG2nd_4x4 = {{
                0x09090909, // TCfg
                0x04040404, // ASelt
                0x00550044, 0x00770066, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000,
                0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpG3rd_4x4 = {{
                0x09090909, // TCfg
                0x04040404, // ASelt
                0x00990088, 0x00bb00aa, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000,
                0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpG4th_4x4 = {{
                0x09090909, // TCfg
                0x04040404, // ASelt
                0x00dd00cc, 0x00ff00ee, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000,
                0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateTmpGbyU_2x8 = {{
                0x66666666, // TCfg
                0x44444444, // ASelt
                0x03020100, 0x07060504, // ABin
                0xaaaaaaaa, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x00010064, 0x00010064, 0x00010064, 0x00010064,
                0x00010064, 0x00010064, 0x00010064, 0x00010064 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateTmpGbyU2_2x8 = {{
                0x66666666, // TCfg
                0x44444444, // ASelt
                0x0b0a0908, 0x0f0e0d0c, // ABin
                0xaaaaaaaa, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x00010064, 0x00010064, 0x00010064, 0x00010064,
                0x00010064, 0x00010064, 0x00010064, 0x00010064 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateG1st_4x4 = {{
                0x07070707, // TCfg
                0x04040404, // ASelt
                0x00110000, 0x00330022, // ABin
                0x08080808, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00002608, // AccumType, ConstantType, and PostShift
                0x00010000, 0x00000000, 0x00010000, 0x00000000,
                0x00010000, 0x00000000, 0x00010000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateG2nd_4x4 = {{
                0x07070707, // TCfg
                0x04040404, // ASelt
                0x00510040, 0x00730062, // ABin
                0x08080808, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00002608, // AccumType, ConstantType, and PostShift
                0x00010000, 0x00000000, 0x00010000, 0x00000000,
                0x00010000, 0x00000000, 0x00010000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateTmpB1st_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00110000, 0x00330022, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x0204012a, 0x00000000, 0x0204012a, 0x00000000,
                0x0204012a, 0x00000000, 0x0204012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpB2nd_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00550044, 0x00770066, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x0204012a, 0x00000000, 0x0204012a, 0x00000000,
                0x0204012a, 0x00000000, 0x0204012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpB3rd_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00990088, 0x00bb00aa, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x0204012a, 0x00000000, 0x0204012a, 0x00000000,
                0x0204012a, 0x00000000, 0x0204012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpB4th_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00dd00cc, 0x00ff00ee, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000600, // AccumType, ConstantType, and PostShift
                0x0204012a, 0x00000000, 0x0204012a, 0x00000000,
                0x0204012a, 0x00000000, 0x0204012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniQuantU8toU8LoB_2x8 = {{
                0x99999999, // TCfg
                0x44444444, // ASelt
                0x03020100, 0x07060504, // ABin
                0x99999999, // BSelt
                0x06060606, 0x06060606, // BBin
                0x00000100, // AccumType, ConstantType, and PostShift
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000,
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniQuantU8toU8HiB_2x8 = {{
                0x99999999, // TCfg
                0x44444444, // ASelt
                0x0b0a0908, 0x0f0e0d0c, // ABin
                0x99999999, // BSelt
                0x06060606, 0x06060606, // BBin
                0x00000100, // AccumType, ConstantType, and PostShift
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000,
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniQuantU8toU8LoG_2x8 = {{
                0x99999999, // TCfg
                0x44444444, // ASelt
                0x23222120, 0x27262524, // ABin
                0x99999999, // BSelt
                0x06060606, 0x06060606, // BBin
                0x00000100, // AccumType, ConstantType, and PostShift
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000,
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniQuantU8toU8HiG_2x8 = {{
                0x99999999, // TCfg
                0x44444444, // ASelt
                0x2b2a2928, 0x2f2e2d2c, // ABin
                0x99999999, // BSelt
                0x06060606, 0x06060606, // BBin
                0x00000100, // AccumType, ConstantType, and PostShift
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000,
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniQuantU8toU8LoR_2x8 = {{
                0x99999999, // TCfg
                0x44444444, // ASelt
                0x43424140, 0x47464544, // ABin
                0x99999999, // BSelt
                0x06060606, 0x06060606, // BBin
                0x00000100, // AccumType, ConstantType, and PostShift
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000,
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniQuantU8toU8HiR_2x8 = {{
                0x99999999, // TCfg
                0x44444444, // ASelt
                0x4b4a4948, 0x4f4e4d4c, // ABin
                0x99999999, // BSelt
                0x06060606, 0x06060606, // BBin
                0x00000100, // AccumType, ConstantType, and PostShift
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000,
                0x3c000000, 0x3c000000, 0x3c000000, 0x3c000000 // Constant
        }, GPU_DP_TYPE_16 };
        switch( attr[0]->dtype )
        {
        case U8:
        case F16:
            {
                // R
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpR1st_4x4", &uniCalculateTmpR1st_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpR2nd_4x4", &uniCalculateTmpR2nd_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpR3rd_4x4", &uniCalculateTmpR3rd_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpR4th_4x4", &uniCalculateTmpR4th_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateR1st_4x4", &uniCalculateR1st_4x4);

                //G
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpG1st_4x4", &uniCalculateTmpG1st_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpG2nd_4x4", &uniCalculateTmpG2nd_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpG3rd_4x4", &uniCalculateTmpG3rd_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpG4th_4x4", &uniCalculateTmpG4th_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpGbyU_2x8", &uniCalculateTmpGbyU_2x8);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpGbyU2_2x8", &uniCalculateTmpGbyU2_2x8);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateG1st_4x4", &uniCalculateG1st_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateG2nd_4x4", &uniCalculateG2nd_4x4);

                //B
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpB1st_4x4", &uniCalculateTmpB1st_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpB2nd_4x4", &uniCalculateTmpB2nd_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpB3rd_4x4", &uniCalculateTmpB3rd_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpB4th_4x4", &uniCalculateTmpB4th_4x4);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateB1st_4x4", &uniCalculateR1st_4x4);

                status |= vsi_nn_kernel_gpu_add_param(node, "uniQuantU8toU8LoB_2x8", &uniQuantU8toU8LoB_2x8);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniQuantU8toU8HiB_2x8", &uniQuantU8toU8HiB_2x8);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniQuantU8toU8LoG_2x8", &uniQuantU8toU8LoG_2x8);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniQuantU8toU8HiG_2x8", &uniQuantU8toU8HiG_2x8);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniQuantU8toU8LoR_2x8", &uniQuantU8toU8LoR_2x8);
                status |= vsi_nn_kernel_gpu_add_param(node, "uniQuantU8toU8HiR_2x8", &uniQuantU8toU8HiR_2x8);

                status |= vsi_nn_kernel_gpu_add_param(node, "zp", &dstZP);
                status |= vsi_nn_kernel_gpu_add_param(node, "outputScale", &dstScale);
                status |= vsi_nn_kernel_gpu_add_param(node, "rOrder", &reorder);
                status |= vsi_nn_kernel_gpu_add_param(node, "bOrder", &order1);
                CHECK_STATUS_FAIL_GOTO(status, OnError );
            }
            break;
        default:
            break;
        }
    }

OnError:
    if (attr[0])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[0] );
        attr[0] = NULL;
    }
    return status;
} /* _pre_process_yuv444_copy_initializer() */

DEF_KERNEL_INITIALIZER(_pre_process_yuv444_initializer)
    (
    vsi_nn_kernel_node_t node,
    const vsi_nn_kernel_node_param_t * param,
    size_t param_size
    )
{
    vsi_status status = VSI_FAILURE;
    gpu_param_t shaderParam = {
        3,          // workdim
        {0, 0, 0},  // globalWorkOffset: control the start location be processed in the image
        {0, 0, 0},  // globalWorkScale: how many pixels could be processed by a single thread
        {0, 0, 0},  // localWorkSize: local group size in thread
        {0, 0, 0}}; // globalWorkSize: image size in thread

    int32_t     dstZP      = 0;
    float       dstScale   = 1;
    int32_t     reorder    = 0;
    int32_t     order1     = 2;
    uint32_t    width      = 0;
    uint32_t    height     = 0;

    vsi_nn_kernel_tensor_attr_t * attr[1] = { NULL };
    vsi_size_array_t * out_shape = NULL;

    VSI_UNREFERENCED(param_size);

    attr[0] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[3] );
    CHECK_PTR_FAIL_GOTO( attr[0], "Create tensor attr buffer fail.", OnError );

    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[12], &reorder);
    CHECK_STATUS_FAIL_GOTO(status, OnError );

    out_shape  = attr[0]->shape;
    dstZP      = attr[0]->zero_point;
    dstScale   = 1.0f / attr[0]->scale;
    width      = (uint32_t)(out_shape->data[0]);
    height     = (uint32_t)(out_shape->data[1]);

    if (reorder != 0)
    {
        reorder = 2;
        order1 = 0;
    }

    shaderParam.global_scale[0]  = 4;
    shaderParam.global_scale[1]  = 1;
    shaderParam.global_scale[2]  = 1;
    shaderParam.global_size[0]   = gpu_align_p2((width + shaderParam.global_scale[0] - 1)
        / shaderParam.global_scale[0], 4);
    shaderParam.global_size[1]   = gpu_align_p2((height + shaderParam.global_scale[1] - 1)
        / shaderParam.global_scale[1], 2);
    shaderParam.global_size[2]   = 1;

    status = vsi_nn_kernel_gpu_config( node, &shaderParam );
    CHECK_STATUS_FAIL_GOTO(status, OnError);

    {
        gpu_dp_inst_t uniConvertInt32toUint8_2x8 = {{
            0x33333333, // TCfg
            0x11110000, // ASelt
            0x03020100, 0x03020100, // ABin
            0x00000000, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002400, // AccumType, ConstantType, and PostShift
            0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateTmpRWise_4x4 = {{
            0x05050505, // TCfg
            0x04040404, // ASelt
            0x00110000, 0x00330022, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x0199012a, 0x00000000, 0x0199012a, 0x00000000, 0x0199012a, 0x00000000, 0x0199012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpRWise2nd_4x4 = {{
            0x05050505, // TCfg
            0x04040404, // ASelt
            0x00550044, 0x00770066, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x0199012a, 0x00000000, 0x0199012a, 0x00000000, 0x0199012a, 0x00000000, 0x0199012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpRWise3rd_4x4 = {{
            0x05050505, // TCfg
            0x04040404, // ASelt
            0x00990088, 0x00bb00aa, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x0199012a, 0x00000000, 0x0199012a, 0x00000000, 0x0199012a, 0x00000000, 0x0199012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpRWise4th_4x4 = {{
            0x05050505, // TCfg
            0x04040404, // ASelt
            0x00dd00cc, 0x00ff00ee, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x0199012a, 0x00000000, 0x0199012a, 0x00000000, 0x0199012a, 0x00000000, 0x0199012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateR1st_4x4 = {{
            0x0f0f0f0f, // TCfg
            0x04040404, // ASelt
            0x00010000, 0x00030002, // ABin
            0x00000000, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002608, // AccumType, ConstantType, and PostShift
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateTmpGWise_4x4 = {{
            0x09090909, // TCfg
            0x04040404, // ASelt
            0x00110000, 0x00330022, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpGWise2nd_4x4 = {{
            0x09090909, // TCfg
            0x04040404, // ASelt
            0x00550044, 0x00770066, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpGWise3rd_4x4 = {{
            0x09090909, // TCfg
            0x04040404, // ASelt
            0x00990088, 0x00bb00aa, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpGWise4th_4x4 = {{
            0x09090909, // TCfg
            0x04040404, // ASelt
            0x00dd00cc, 0x00ff00ee, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000, 0x00d0012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateTmpGbyU_2x8 = {{
            0x66666666, // TCfg
            0x44444444, // ASelt
            0x03020100, 0x07060504, // ABin
            0xaaaaaaaa, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00010064, 0x00010064, 0x00010064, 0x00010064, 0x00010064, 0x00010064, 0x00010064, 0x00010064 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpGbyU2nd_2x8 = {{
            0x66666666, // TCfg
            0x44444444, // ASelt
            0x0b0a0908, 0x0f0e0d0c, // ABin
            0xaaaaaaaa, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00010064, 0x00010064, 0x00010064, 0x00010064, 0x00010064, 0x00010064, 0x00010064, 0x00010064 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateGWise_4x4 = {{
            0x07070707, // TCfg
            0x04040404, // ASelt
            0x00110000, 0x00330022, // ABin
            0x08080808, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002608, // AccumType, ConstantType, and PostShift
            0x00010000, 0x00000000, 0x00010000, 0x00000000, 0x00010000, 0x00000000, 0x00010000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateGWise2nd_4x4 = {{
            0x07070707, // TCfg
            0x04040404, // ASelt
            0x00510040, 0x00730062, // ABin
            0x08080808, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002608, // AccumType, ConstantType, and PostShift
            0x00010000, 0x00000000, 0x00010000, 0x00000000, 0x00010000, 0x00000000, 0x00010000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniCalculateTmpBWise_4x4 = {{
            0x05050505, // TCfg
            0x04040404, // ASelt
            0x00110000, 0x00330022, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x0204012a, 0x00000000, 0x0204012a, 0x00000000, 0x0204012a, 0x00000000, 0x0204012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpBWise2nd_4x4 = {{
            0x05050505, // TCfg
            0x04040404, // ASelt
            0x00550044, 0x00770066, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x0204012a, 0x00000000, 0x0204012a, 0x00000000, 0x0204012a, 0x00000000, 0x0204012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpBWise3rd_4x4 = {{
            0x05050505, // TCfg
            0x04040404, // ASelt
            0x00990088, 0x00bb00aa, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x0204012a, 0x00000000, 0x0204012a, 0x00000000, 0x0204012a, 0x00000000, 0x0204012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniCalculateTmpBWise4th_4x4 = {{
            0x05050505, // TCfg
            0x04040404, // ASelt
            0x00dd00cc, 0x00ff00ee, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x0204012a, 0x00000000, 0x0204012a, 0x00000000, 0x0204012a, 0x00000000, 0x0204012a, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniDescaleU8_4x4 = {{
            0x0f0f0f0f, // TCfg
            0x04040404, // ASelt
            0x00010000, 0x00030002, // ABin
            0x00000000, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002614, // AccumType, ConstantType, and PostShift
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniBilinearTmp1st_4x4 = {{
            0x09090909, // TCfg
            0x00000000, // ASelt
            0x00450001, 0x00cd0089, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00010001, 0x00000000, 0x00010001, 0x00000000, 0x00010001, 0x00000000, 0x00010001, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniBilinearTmp2nd_4x4 = {{
            0x01010101, // TCfg
            0x00000000, // ASelt
            0x00040000, 0x000c0008, // ABin
            0x02020202, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000400, 0x00000000, 0x00000400, 0x00000000, 0x00000400, 0x00000000, 0x00000400, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniBilinearTmp3rd_4x4 = {{
            0x69696969, // TCfg
            0x00000000, // ASelt
            0x45670123, 0xcdef89ab, // ABin
            0xaaaaaaaa, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00010001, 0x00010001, 0x00010001, 0x00010001, 0x00010001, 0x00010001, 0x00010001, 0x00010001 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniBilinearTmp4th_4x4 = {{
            0x09090909, // TCfg
            0x00000000, // ASelt
            0x00460002, 0x00ce008a, // ABin
            0x0a0a0a0a, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x04000400, 0x00000000, 0x04000400, 0x00000000, 0x04000400, 0x00000000, 0x04000400, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniConvertHalftoFp16_2x8 = {{
            0x11111111, // TCfg
            0x11110000, // ASelt
            0x06040200, 0x06040200, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16 };

        status = vsi_nn_kernel_gpu_add_param(node, "uniCalculateR1st_4x4", &uniCalculateR1st_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpGbyU_2x8", &uniCalculateTmpGbyU_2x8);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpGbyU2nd_2x8", &uniCalculateTmpGbyU2nd_2x8);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateB1st_4x4", &uniCalculateR1st_4x4);

        status |= vsi_nn_kernel_gpu_add_param(node, "uniDescaleU8_4x4", &uniDescaleU8_4x4);

        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpRWise_4x4", &uniCalculateTmpRWise_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpRWise2nd_4x4", &uniCalculateTmpRWise2nd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpRWise3rd_4x4", &uniCalculateTmpRWise3rd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpRWise4th_4x4", &uniCalculateTmpRWise4th_4x4);

        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpGWise_4x4", &uniCalculateTmpGWise_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpGWise2nd_4x4", &uniCalculateTmpGWise2nd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpGWise3rd_4x4", &uniCalculateTmpGWise3rd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpGWise4th_4x4", &uniCalculateTmpGWise4th_4x4);

        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpBWise_4x4", &uniCalculateTmpBWise_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpBWise2nd_4x4", &uniCalculateTmpBWise2nd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpBWise3rd_4x4", &uniCalculateTmpBWise3rd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateTmpBWise4th_4x4", &uniCalculateTmpBWise4th_4x4);

        status |= vsi_nn_kernel_gpu_add_param(node, "uniBilinearTmp1st_4x4", &uniBilinearTmp1st_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniBilinearTmp2nd_4x4", &uniBilinearTmp2nd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniBilinearTmp3rd_4x4", &uniBilinearTmp3rd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniBilinearTmp4th_4x4", &uniBilinearTmp4th_4x4);

        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateGWise_4x4", &uniCalculateGWise_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "uniCalculateGWise2nd_4x4", &uniCalculateGWise2nd_4x4);
        status |= vsi_nn_kernel_gpu_add_param(node, "rOrder", &reorder);

        status |= vsi_nn_kernel_gpu_add_param(node, "bOrder", &order1);
        CHECK_STATUS_FAIL_GOTO(status, OnError );

        switch( attr[0]->dtype )
        {
        case U8:
        case I8:
        case I16:
            {
                status = vsi_nn_kernel_gpu_add_param(node, "uniConvertInt32toUint8_2x8", &uniConvertInt32toUint8_2x8);
                status |= vsi_nn_kernel_gpu_add_param(node, "outputScale", &dstScale);
                status |= vsi_nn_kernel_gpu_add_param(node, "zp", &dstZP);
                CHECK_STATUS_FAIL_GOTO(status, OnError );
            }
            break;
        case F16:
            {
                status = vsi_nn_kernel_gpu_add_param(node, "uniConvertHalftoFp16_2x8", &uniConvertHalftoFp16_2x8);
                CHECK_STATUS_FAIL_GOTO(status, OnError );
            }
            break;
        default:
            break;
        }
    }

OnError:
    if (attr[0])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[0] );
        attr[0] = NULL;
    }
    return status;
} /* _pre_process_yuv444_initializer() */

static vsi_status _query_kernel
    (
    vsi_nn_tensor_t* const* const inputs,
    vsi_nn_tensor_t* const* const outputs,
    vsi_nn_kernel_t* kernel,
    const vsi_nn_kernel_param_t * params
    )
{
    vsi_nn_kernel_dtype_e input0_dtype = U8;
    vsi_nn_kernel_dtype_e output_dtype = U8;
    vsi_nn_kernel_convert_type_e convert_type = SCALE;
    vsi_status status = VSI_FAILURE;
    uint32_t key = 0;
    size_t i = 0;
    vsi_bool enable_copy  = vsi_nn_kernel_param_get_int32( params, "enable_copy" );

    input0_dtype = vsi_nn_kernel_map_dtype( inputs[0]->attr.dtype.vx_type );
    output_dtype = vsi_nn_kernel_map_dtype( outputs[0]->attr.dtype.vx_type );

    if (enable_copy && (output_dtype == U8 || output_dtype == F16))
    {
        convert_type = COPY;
    }
    else
    {
        convert_type = SCALE;
    }

    key = HASH_PRE_PROCESS_YUV444_KEY( input0_dtype, output_dtype, convert_type, 0 );

    for ( i = 0; i < _cnt_of_array(pre_process_yuv444_map); i ++ )
    {
        if ( pre_process_yuv444_map[i].key == key )
        {
            break;
        }
    }
    if ( i < _cnt_of_array(pre_process_yuv444_map) )
    {
        snprintf( kernel->info.name, VX_MAX_KERNEL_NAME, "%s",  pre_process_yuv444_map[i].function_name );
        kernel->info.parameters = vxPreProcessYuv444Kernel_param_def;
        kernel->info.numParams = _cnt_of_array( vxPreProcessYuv444Kernel_param_def );

        if (enable_copy && (output_dtype == U8 || output_dtype == F16))
        {
            kernel->info.initialize = _pre_process_yuv444_copy_initializer;
        }
        else
        {
            kernel->info.initialize = _pre_process_yuv444_initializer;
        }
        vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_CODE, 2,
                "vsi_nn_kernel_header",
                pre_process_yuv444_map[i].source_name );
        vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_EXECUTABLE, 1,
                pre_process_yuv444_map[i].source_name );
        status = VSI_SUCCESS;
    }
    return status;
} /* _query_kernel() */

static vsi_nn_kernel_node_t _setup
    (
    vsi_nn_graph_t              * graph,
    vsi_nn_tensor_t            ** inputs,
    size_t                        input_num,
    vsi_nn_tensor_t            ** outputs,
    size_t                        output_num,
    const vsi_nn_kernel_param_t * params,
    vsi_nn_kernel_t             * kernel
    )
{
    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_node_param_t tmp_params[_EVIS_PRE_PROCESS_YUV444_PARAM_NUM] = { NULL };
    vsi_nn_kernel_node_t node = NULL;
    vsi_nn_tensor_t* reshape_tensors[1] = {NULL};
    int32_t trans = 0;

    VSI_UNREFERENCED(input_num);
    VSI_UNREFERENCED(output_num);

    if ( !vsi_nn_kernel_gpu_check_shape( outputs[0]->attr.size,
                outputs[0]->attr.dim_num ) )
    {
        return NULL;
    }

    status = _query_kernel( inputs, outputs, kernel, params );
    if ( VSI_SUCCESS == status)
    {
        node = vsi_nn_kernel_create_node( graph, kernel );
        if ( node )
        {
            uint32_t index = 4;
            int32_t scale_x  = vsi_nn_kernel_param_get_int32( params, "scale_x" );
            int32_t scale_y  = vsi_nn_kernel_param_get_int32( params, "scale_y" );
            int32_t left     = vsi_nn_kernel_param_get_int32( params, "left" );
            int32_t top      = vsi_nn_kernel_param_get_int32( params, "top" );
            float r_mean     = vsi_nn_kernel_param_get_float32( params, "r_mean" );
            float g_mean     = vsi_nn_kernel_param_get_float32( params, "g_mean" );
            float b_mean     = vsi_nn_kernel_param_get_float32( params, "b_mean" );
            float r_scale    = vsi_nn_kernel_param_get_float32( params, "r_scale" );
            float g_scale    = vsi_nn_kernel_param_get_float32( params, "g_scale" );
            float b_scale    = vsi_nn_kernel_param_get_float32( params, "b_scale" );
            int32_t reverse  = vsi_nn_kernel_param_get_int32( params, "reverse" );

            /* Pass parameters to node. */
            vsi_nn_kernel_node_pack_io( tmp_params, _EVIS_PRE_PROCESS_YUV444_PARAM_NUM,
                inputs, 3, outputs, 1 );

            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &scale_x );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &scale_y );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &left );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &top );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &r_mean );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &g_mean );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &b_mean );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &r_scale );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &reverse );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &trans );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &g_scale );
            tmp_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &b_scale );
            status = vsi_nn_kernel_node_pass_param( node, tmp_params, _EVIS_PRE_PROCESS_YUV444_PARAM_NUM );
            CHECK_STATUS(status);
            vsi_nn_kernel_scalar_release( &tmp_params[4] );
            vsi_nn_kernel_scalar_release( &tmp_params[5] );
            vsi_nn_kernel_scalar_release( &tmp_params[6] );
            vsi_nn_kernel_scalar_release( &tmp_params[7] );
            vsi_nn_kernel_scalar_release( &tmp_params[8] );
            vsi_nn_kernel_scalar_release( &tmp_params[9] );
            vsi_nn_kernel_scalar_release( &tmp_params[10] );
            vsi_nn_kernel_scalar_release( &tmp_params[11] );
            vsi_nn_kernel_scalar_release( &tmp_params[12] );
            vsi_nn_kernel_scalar_release( &tmp_params[13] );
            vsi_nn_kernel_scalar_release( &tmp_params[14] );
            vsi_nn_kernel_scalar_release( &tmp_params[15] );
        }
    }
    if(reshape_tensors[0])
    {
        vsi_nn_ReleaseTensor(&reshape_tensors[0]);
    }
    return node;
} /* _setup() */

__END_DECLS

REGISTER_BACKEND_EVIS( pre_process_yuv444, _setup )

