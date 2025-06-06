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
#include <stdlib.h>
#include <math.h>
#include "vsi_nn_types.h"
#include "vsi_nn_platform.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_node.h"
#include "vsi_nn_log.h"
#include "vsi_nn_test.h"
#include "vsi_nn_error.h"
#include "vsi_nn_tensor_util.h"
#include "utils/vsi_nn_util.h"
#include "utils/vsi_nn_dtype_util.h"
#include "kernel/vsi_nn_kernel.h"
#include "libnnext/vsi_nn_vxkernel.h"
#include "kernel/vsi_nn_kernel_gpu_shape_optimize.h"

#define _CPU_ARG_NUM            (1)
#define _CPU_INPUT_NUM          (1)
#define _CPU_OUTPUT_NUM         (1)
#define _CPU_IO_NUM             (_CPU_INPUT_NUM + _CPU_OUTPUT_NUM)
#define _CPU_PARAM_NUM          (_CPU_ARG_NUM + _CPU_IO_NUM)
#define _KERNEL_NAME            ("com.vivantecorp.extension.Softmax2VXC")
#define _KERNEL_NAME_U8         ("com.vivantecorp.extension.Softmax2VXC_u8")

#define SCALAR_INPUT_AXIS          (2)

__BEGIN_DECLS

static vx_param_description_t kernel_param_def[] =
{
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED}
};
#define _EVIS_PARAM_NUM          _cnt_of_array(kernel_param_def)

DEF_KERNEL_INITIALIZER(_softmax_initializer)
    (
    vsi_nn_kernel_node_t node,
    const vsi_nn_kernel_node_param_t* param,
    size_t param_size
    )
{
    vsi_status status = VSI_FAILURE;
    int sf_size = 0;
    vsi_nn_kernel_tensor_attr_t* attr[2] = {NULL, NULL};
    float srcZP = 0.0f;
    float srcScale = 1.0f;
    float dstZP = 0.0f;
    float dstScale = 1.0f;
    // Alignment with a power of two value.
    gpu_param_t gpu_param = {
        2,          // workdim
        {0, 0, 0},  // global_offset: control the start location be processed in the image
        {0, 0, 0},  // global_scale: how many pixels could be processed by a single thread
        {0, 0, 0},  // local_size: local group size in thread
        {0, 0, 0}}; // global_size: image size in thread

    VSI_UNREFERENCED(param_size);

    attr[0] = vsi_nn_kernel_tensor_attr_create((vsi_nn_kernel_tensor_t)param[0]);
    attr[1] = vsi_nn_kernel_tensor_attr_create((vsi_nn_kernel_tensor_t)param[1]);
    if ((!attr[0]) || (!attr[1]))
    {
        VSILOGE("Query failure! at line");
        return status;
    }

    sf_size  =  (int)attr[0]->shape->data[0];
    srcScale = attr[0]->scale;
    srcZP = (float)attr[0]->zero_point;
    dstScale = 1.0f / attr[1]->scale;
    dstZP = (float)attr[1]->zero_point;

    gpu_param.global_offset[0] = 0;
    gpu_param.global_offset[1] = 0;
    gpu_param.global_scale[0]  = 1;
    gpu_param.global_scale[1]  = 1;
    gpu_param.local_size[0]    = 1;
    gpu_param.local_size[1]    = 1;
    gpu_param.global_size[0]   =
        gpu_align_p2((attr[0]->shape->data[1] + gpu_param.global_scale[0] - 1) / gpu_param.global_scale[0],
                gpu_param.local_size[0]);
    gpu_param.global_size[1]   =
        gpu_align_p2((1 + gpu_param.global_scale[1] - 1) / gpu_param.global_scale[1],
                gpu_param.local_size[1]);
    {
        gpu_dp_inst_t Uni4x4_Fp16ToFp32 = {{
            0x01010101, // TCfg
            0x00000000, // ASelt
            0x00010000, 0x00030002, // ABin
            0x02020202, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000400, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000000, 0x00000001, 0x00000000,
            0x00000001, 0x00000000, 0x00000001, 0x00000000 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniExtract8Bin_2x8 = {{
            0x11111111, // TCfg
            0x11110000, // ASelt
            0x06040200, 0x06040200, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002400, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001,
            0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};

        status = vsi_nn_kernel_gpu_add_param( node,
                "Uni4x4_Fp16ToFp32", &Uni4x4_Fp16ToFp32 );
        status |= vsi_nn_kernel_gpu_add_param( node,
                "uniExtract8Bin_2x8", &uniExtract8Bin_2x8 );
        status |= vsi_nn_kernel_gpu_add_param(node,
                "sf_size", &sf_size);
        status |= vsi_nn_kernel_gpu_add_param(node, "srcScale", &srcScale);
        status |= vsi_nn_kernel_gpu_add_param(node, "srcZP", &srcZP);
        status |= vsi_nn_kernel_gpu_add_param(node, "dstScale", &dstScale);
        status |= vsi_nn_kernel_gpu_add_param(node, "dstZP", &dstZP);
    }

    status |= vsi_nn_kernel_gpu_config( node, &gpu_param );

    if(status != VSI_SUCCESS)
    {
        VSILOGE("Initializer  failure!");
    }
    if (attr[0])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[0] );
        attr[0] = NULL;
    }
    if (attr[1])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[1] );
        attr[1] = NULL;
    }

    return status;
}

static const vx_kernel_description_t _kernel_info1 =
{
    KERNEL_ID_PLACEHOLDER,
    _KERNEL_NAME,
    NULL,
    kernel_param_def,
    _cnt_of_array( kernel_param_def ),
    vsi_nn_KernelValidator,
    NULL,
    NULL,
    _softmax_initializer,
    vsi_nn_KernelDeinitializer
};

static const vx_kernel_description_t _kernel_info2 =
{
    KERNEL_ID_PLACEHOLDER,
    _KERNEL_NAME_U8,
    NULL,
    kernel_param_def,
    _cnt_of_array( kernel_param_def ),
    vsi_nn_KernelValidator,
    NULL,
    NULL,
    _softmax_initializer,
    vsi_nn_KernelDeinitializer
};

static vsi_status _query_kernel
    (
    vsi_nn_tensor_t* const* const inputs,
    vsi_nn_tensor_t* const* const outputs,
    vsi_nn_kernel_t* kernel
    )
{
    vsi_nn_kernel_dtype_e in_dtype;
    vsi_nn_kernel_dtype_e out_dtype;

    in_dtype = vsi_nn_kernel_map_dtype(inputs[0]->attr.dtype.vx_type);
    out_dtype = vsi_nn_kernel_map_dtype(outputs[0]->attr.dtype.vx_type);

    if (in_dtype == U8 && out_dtype == U8)
    {
        memmove( &kernel->info, &_kernel_info2, sizeof(vx_kernel_description_t) );
    }
    else
    {
        memmove( &kernel->info, &_kernel_info1, sizeof(vx_kernel_description_t) );
    }

    vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_CODE, 2,
            "vsi_nn_kernel_header",
            "custom_softmax" );
    vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_EXECUTABLE, 1,
            "custom_softmax" );
    return VSI_SUCCESS;
}

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
    vsi_status status = VSI_SUCCESS;
    vsi_nn_kernel_node_param_t backend_params[_CPU_PARAM_NUM] = {NULL};
    vsi_nn_kernel_node_t node = NULL;
    int32_t axis = 0;
    vsi_nn_tensor_t* reshape_tensors[2] = {NULL};
    vsi_size_t shapes[2][VSI_NN_MAX_DIM_NUM] = {{0}};
    uint32_t rank_in = 0;
    int32_t new_axis = 0;
    uint32_t i = 0;
    vsi_bool ret = vx_false_e;

    VSI_UNREFERENCED(input_num);
    VSI_UNREFERENCED(output_num);

    axis = vsi_nn_kernel_param_get_int32(params, "axis");

    ret = vsi_nn_kernel_optimize_softmax_shape(inputs[0]->attr.size,
                                           inputs[0]->attr.dim_num,
                                           axis,
                                           shapes[0],
                                           &rank_in,
                                           &new_axis);

    if (ret)
    {
        reshape_tensors[0] = vsi_nn_reshape_tensor(graph, inputs[0], shapes[0], rank_in);
        reshape_tensors[1] = vsi_nn_reshape_tensor(graph, outputs[0], shapes[0], rank_in);
    }
    else
    {
        return NULL;
    }

    if (!vsi_nn_kernel_gpu_check_shape(reshape_tensors[0]->attr.size,
                                       reshape_tensors[0]->attr.dim_num) ||
        new_axis > 2)
    {
        return NULL;
    }

    status = _query_kernel( inputs, outputs, kernel );
    if( VSI_SUCCESS == status)
    {
        node = vsi_nn_kernel_create_node( graph, kernel );
        if( node )
        {
            /* Set inputs and outputs */
            vsi_nn_kernel_node_pack_io( backend_params, _CPU_PARAM_NUM,
                    reshape_tensors, _CPU_INPUT_NUM, &reshape_tensors[1], _CPU_OUTPUT_NUM );
            backend_params[SCALAR_INPUT_AXIS] = vsi_nn_kernel_scalar_create(
                    graph, I32, &new_axis );

            /* Pass parameters to node. */
            status = vsi_nn_kernel_node_pass_param( node, backend_params, _CPU_PARAM_NUM );
            vsi_nn_kernel_scalar_release( &backend_params[SCALAR_INPUT_AXIS] );
        }
        else
        {
            status = VSI_FAILURE;
        }
    }

    for (i = 0; i < 2; i++)
    {
        vsi_safe_release_tensor(reshape_tensors[i]);
    }
    return node;
} /* _setup() */

__END_DECLS

REGISTER_BACKEND_EVIS( custom_softmax, _setup )
