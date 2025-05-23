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
#include "vsi_nn_error.h"
#include "vsi_nn_tensor_util.h"
#include "utils/vsi_nn_util.h"
#include "kernel/vsi_nn_kernel.h"
#include "kernel/vsi_nn_kernel_gpu_shape_optimize.h"
#if !(VX_LOGSOFTMAX_VX_SUPPORT)
__BEGIN_DECLS


/*
 * Define kernel meta.
 */
#define HASH_LOG_SOFTMAX_KEY(_axis, _input_type, _output_type, _image_2d, exceed_limit) \
    ((_axis << 24) | (_input_type << 16) | (_output_type << 8) | (_image_2d << 4) | exceed_limit)

 #define VSI_NN_GEN_LOG_SOFTMAX_KERNEL_SOURCE_NAME(_axis) \
    "log_softmax_axis"#_axis

 #define VSI_NN_GEN_LOG_SOFTMAX_EXCEED_KERNEL_SOURCE_NAME(_axis) \
    "log_softmax_exceed_axis"#_axis

#define HASH_LOG_SOFTMAX_SH_KERNEL_NAME(AXIS, SRC0_TYPE, DST_TYPE) \
    CVIVANTE_NAMESPACE("cl.log_softmax_axis"#AXIS"_"#SRC0_TYPE"to"#DST_TYPE)

#define TENSOR_LOG_SOFTMAX_KERNELS(AXIS, SRC0_TYPE, OUT_TYPE) \
    {   HASH_LOG_SOFTMAX_KEY(AXIS, SRC0_TYPE, OUT_TYPE, 0, 0), \
        HASH_LOG_SOFTMAX_SH_KERNEL_NAME(AXIS, SRC0_TYPE, OUT_TYPE), \
        VSI_NN_GEN_LOG_SOFTMAX_KERNEL_SOURCE_NAME(AXIS) },

#define TENSOR_LOG_SOFTMAX_FLOAT(AXIS, SRC0_TYPE, OUT_TYPE) \
    {   HASH_LOG_SOFTMAX_KEY(AXIS, SRC0_TYPE, OUT_TYPE, 0, 0), \
        HASH_LOG_SOFTMAX_SH_KERNEL_NAME(AXIS, F32, F32), \
        VSI_NN_GEN_LOG_SOFTMAX_KERNEL_SOURCE_NAME(AXIS) },

#define TENSOR_LOG_SOFTMAX_BFLOAT(AXIS, SRC0_TYPE, OUT_TYPE) \
    {   HASH_LOG_SOFTMAX_KEY(AXIS, SRC0_TYPE, OUT_TYPE, 0, 0), \
        HASH_LOG_SOFTMAX_SH_KERNEL_NAME(AXIS, SRC0_TYPE, OUT_TYPE), \
        VSI_NN_GEN_LOG_SOFTMAX_KERNEL_SOURCE_NAME(AXIS) },

#define HASH_LOG_SOFTMAX_SH_KERNEL_2D_NAME(AXIS, SRC0_TYPE, DST_TYPE) \
    CVIVANTE_NAMESPACE("cl.log_softmax_axis"#AXIS"_"#SRC0_TYPE"to"#DST_TYPE"_2D")

#define TENSOR_LOG_SOFTMAX_KERNELS_2D(AXIS, SRC0_TYPE, OUT_TYPE) \
    {   HASH_LOG_SOFTMAX_KEY(AXIS, SRC0_TYPE, OUT_TYPE, 1, 0), \
        HASH_LOG_SOFTMAX_SH_KERNEL_2D_NAME(AXIS, SRC0_TYPE, OUT_TYPE), \
        VSI_NN_GEN_LOG_SOFTMAX_KERNEL_SOURCE_NAME(AXIS) },

#define TENSOR_LOG_SOFTMAX_FLOAT_2D(AXIS, SRC0_TYPE, OUT_TYPE) \
    {   HASH_LOG_SOFTMAX_KEY(AXIS, SRC0_TYPE, OUT_TYPE, 1, 0), \
        HASH_LOG_SOFTMAX_SH_KERNEL_2D_NAME(AXIS, F32, F32), \
        VSI_NN_GEN_LOG_SOFTMAX_KERNEL_SOURCE_NAME(AXIS) },

#define TENSOR_LOG_SOFTMAX_BFLOAT_2D(AXIS, SRC0_TYPE, OUT_TYPE) \
    {   HASH_LOG_SOFTMAX_KEY(AXIS, SRC0_TYPE, OUT_TYPE, 1, 0), \
        HASH_LOG_SOFTMAX_SH_KERNEL_2D_NAME(AXIS, SRC0_TYPE, OUT_TYPE), \
        VSI_NN_GEN_LOG_SOFTMAX_KERNEL_SOURCE_NAME(AXIS) },

#define HASH_LOG_SOFTMAX_EXCEED_SH_KERNEL_NAME(AXIS, SRC0_TYPE, DST_TYPE) \
    CVIVANTE_NAMESPACE("cl.log_softmax_exceed_axis"#AXIS"_"#SRC0_TYPE"to"#DST_TYPE)

#define TENSOR_LOG_SOFTMAX_EXCEED_KERNELS(AXIS, SRC0_TYPE, OUT_TYPE) \
    {   HASH_LOG_SOFTMAX_KEY(AXIS, SRC0_TYPE, OUT_TYPE, 0, 1), \
        HASH_LOG_SOFTMAX_EXCEED_SH_KERNEL_NAME(AXIS, SRC0_TYPE, OUT_TYPE), \
        VSI_NN_GEN_LOG_SOFTMAX_EXCEED_KERNEL_SOURCE_NAME(AXIS) },

static const struct {
        uint32_t key;
        char* function_name;
        const char* source_name;
    } kernel_map[] =
{
    TENSOR_LOG_SOFTMAX_FLOAT(0, F32, F32)
    TENSOR_LOG_SOFTMAX_FLOAT(1, F32, F32)
    TENSOR_LOG_SOFTMAX_FLOAT(2, F32, F32)
    TENSOR_LOG_SOFTMAX_BFLOAT(0, BF16, BF16)
    TENSOR_LOG_SOFTMAX_BFLOAT(1, BF16, BF16)
    TENSOR_LOG_SOFTMAX_BFLOAT(2, BF16, BF16)

    TENSOR_LOG_SOFTMAX_FLOAT_2D(0, F32, F32)
    TENSOR_LOG_SOFTMAX_FLOAT_2D(1, F32, F32)
    TENSOR_LOG_SOFTMAX_BFLOAT_2D(0, BF16, BF16)
    TENSOR_LOG_SOFTMAX_BFLOAT_2D(1, BF16, BF16)

    TENSOR_LOG_SOFTMAX_KERNELS(0, U8, U8)
    TENSOR_LOG_SOFTMAX_KERNELS(1, U8, U8)
    TENSOR_LOG_SOFTMAX_KERNELS(2, U8, U8)

    TENSOR_LOG_SOFTMAX_KERNELS_2D(0, U8, U8)
    TENSOR_LOG_SOFTMAX_KERNELS_2D(1, U8, U8)

    TENSOR_LOG_SOFTMAX_EXCEED_KERNELS(0, U8, U8)
    TENSOR_LOG_SOFTMAX_EXCEED_KERNELS(1, U8, U8)

    TENSOR_LOG_SOFTMAX_EXCEED_KERNELS(0, F32, F32)
    TENSOR_LOG_SOFTMAX_EXCEED_KERNELS(1, F32, F32)

    TENSOR_LOG_SOFTMAX_EXCEED_KERNELS(0, BF16, BF16)
    TENSOR_LOG_SOFTMAX_EXCEED_KERNELS(1, BF16, BF16)

};

/*
 * Kernel params
 */
static vx_param_description_t kernel_param_def[] =
{
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
};

#define _CL_PARAM_NUM          _cnt_of_array(kernel_param_def)
#define SCALAR_INPUT_AXIS            (2)
#define SCALAR_INPUT_BETA            (3)
#define SCALAR_INPUT_SCALE           (4)
#define SCALAR_OUTPUT_SCALE          (5)
#define SCALAR_OUTPUT_ZP             (6)

/*
 * Kernel initializer
 */
DEF_KERNEL_INITIALIZER(_log_softmax_initializer)
    (
    vsi_nn_kernel_node_t node,
    const vsi_nn_kernel_node_param_t * param,
    size_t param_size
    )
{
    gpu_param_t gpu_param = {
        3,         // workdim
        {0, 0, 0}, // globalWorkOffset: control the start location be processed in the image
        {0, 0, 0}, // globalWorkScale: how many pixels could be processed by a single thread
        {0, 0, 0}, // localWorkSize: local group size in thread
        {0, 0, 0}  // globalWorkSize: image size in thread
        };

    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_tensor_attr_t * attr[2] = { NULL };
    vsi_size_array_t * out_shape = NULL;
    int32_t axis = 0;

    VSI_UNREFERENCED(param_size);

    attr[0] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[0] );
    CHECK_PTR_FAIL_GOTO( attr[0], "Create tensor attr buffer fail.", final );
    attr[1] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[1] );
    CHECK_PTR_FAIL_GOTO( attr[1], "Create tensor attr buffer fail.", final );

    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[2], &axis);
    CHECK_STATUS_FAIL_GOTO(status, final );

    out_shape  = attr[1]->shape;

    gpu_param.global_scale[0] = 1;
    gpu_param.global_scale[1] = 1;
    gpu_param.global_scale[2] = 1;
    gpu_param.global_size[0] = axis == 0 ? 1 : out_shape->data[0];
    gpu_param.global_size[1] = axis == 1 ? 1 : out_shape->data[1];
    gpu_param.global_size[2] = out_shape->size > 2 ? (axis == 2 ? 1 : out_shape->data[2]) : 1;

    status = vsi_nn_kernel_gpu_config( node, &gpu_param );

final:
    if (attr[0])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[0] );
    }

    if (attr[1])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[1] );
    }

    return status;
} /* _log_softmax_initializer() */

DEF_KERNEL_INITIALIZER(_log_softmax_exceed_initializer)
    (
    vsi_nn_kernel_node_t node,
    const vsi_nn_kernel_node_param_t * param,
    size_t param_size
    )
{
    gpu_param_t gpu_param = {
        2,         // workdim
        {0, 0, 0}, // globalWorkOffset: control the start location be processed in the image
        {0, 0, 0}, // globalWorkScale: how many pixels could be processed by a single thread
        {0, 0, 0}, // localWorkSize: local group size in thread
        {0, 0, 0}  // globalWorkSize: image size in thread
        };

    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_tensor_attr_t * attr[2] = { NULL };
    vsi_size_array_t * out_shape = NULL;
    int32_t axis = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t depth = 0;

    VSI_UNREFERENCED(param_size);

    attr[0] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[0] );
    CHECK_PTR_FAIL_GOTO( attr[0], "Create tensor attr buffer fail.", final );
    attr[1] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[1] );
    CHECK_PTR_FAIL_GOTO( attr[1], "Create tensor attr buffer fail.", final );

    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[2], &axis);
    CHECK_STATUS_FAIL_GOTO(status, final );

    out_shape  = attr[1]->shape;

    width = (int32_t)(out_shape->data[0]);
    height = (int32_t)(out_shape->data[1]);
    depth = attr[1]->shape->size > 2 ? (int32_t)(out_shape->data[2]) : 1;
    gpu_param.global_scale[0] = 1;
    gpu_param.global_scale[1] = 1;
    if (axis == 0)
    {
        gpu_param.global_size[0] = 1;
        gpu_param.global_size[1] = depth;
    }
    else
    {
        gpu_param.global_size[0] = width;
        gpu_param.global_size[1] = 1;
    }

    status = vsi_nn_kernel_gpu_config( node, &gpu_param );
    if (axis == 0)
    {
        status |= vsi_nn_kernel_gpu_add_param( node, "width", &width );
    }
    else
    {
        status |= vsi_nn_kernel_gpu_add_param( node, "depth", &depth );
    }
    status |= vsi_nn_kernel_gpu_add_param( node, "height", &height );

final:
    if (attr[0])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[0] );
    }

    if (attr[1])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[1] );
    }

    return status;
}

static vsi_status _query_kernel
    (
    vsi_nn_tensor_t* const* const inputs,
    vsi_nn_tensor_t* const* const outputs,
    int32_t axis,
    vsi_bool image_2d,
    vsi_bool exceed_limit,
    vsi_nn_kernel_t* kernel
    )
{
    vsi_nn_kernel_dtype_e input_dtype;
    vsi_nn_kernel_dtype_e output_dtype;
    vsi_status status = VSI_FAILURE;
    uint32_t key;
    size_t i;

    input_dtype = vsi_nn_kernel_map_dtype( inputs[0]->attr.dtype.vx_type );
    output_dtype = vsi_nn_kernel_map_dtype( outputs[0]->attr.dtype.vx_type );

    if (input_dtype == F16)
    {
        input_dtype = F32;
    }
    if (output_dtype == F16)
    {
        output_dtype = F32;
    }
    if (exceed_limit) image_2d = vx_false_e;
    key = HASH_LOG_SOFTMAX_KEY( axis, input_dtype, output_dtype, image_2d, exceed_limit );

    for( i = 0; i < _cnt_of_array(kernel_map); i ++ )
    {
        if( kernel_map[i].key == key )
        {
            break;
        }
    }
    if( i < _cnt_of_array(kernel_map) )
    {
        snprintf( kernel->info.name, VX_MAX_KERNEL_NAME, "%s",  kernel_map[i].function_name );
        kernel->info.parameters = kernel_param_def;
        kernel->info.numParams = _cnt_of_array( kernel_param_def );
        if (exceed_limit)
        {
            kernel->info.initialize = _log_softmax_exceed_initializer;
        }
        else
        {
            kernel->info.initialize = _log_softmax_initializer;
        }
        vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_CODE, 1,
                kernel_map[i].source_name );
        vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_EXECUTABLE, 1,
                kernel_map[i].source_name );
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
    vsi_nn_kernel_node_param_t node_params[_CL_PARAM_NUM] = {NULL};
    vsi_bool image_2d = FALSE;
    vsi_nn_kernel_node_t node = NULL;
    vsi_nn_tensor_t* reshape_tensors[2] = { NULL };
    vsi_size_t shapes[2][VSI_NN_MAX_DIM_NUM] = { { 0 } };
    uint32_t rank_in = 0;
    int32_t axis = 0;
    int32_t new_axis = 0;
    vsi_bool ret = vx_false_e;
    vsi_bool exceed_limit = vx_false_e;
    uint32_t i   = 0;
    float beta = 0;
    float inputScale = vsi_nn_get_tensor_scale(inputs[0]);
    float outputScale = 1.0f / vsi_nn_get_tensor_scale(outputs[0]);
    float outputZP = (float)vsi_nn_get_tensor_zero_point(outputs[0]) + 0.5f;
    float scaleValue = (vx_float32)(log10(exp(1.0f)) / log10(2.0f));

    VSI_UNREFERENCED(input_num);
    VSI_UNREFERENCED(output_num);

    axis = vsi_nn_kernel_param_get_int32(params, "axis");
    beta = vsi_nn_kernel_param_get_float32(params, "beta");

    scaleValue = scaleValue * beta * inputScale;
    beta = beta * inputScale;

    if (inputs[0]->attr.size[axis] >= GPU_TENSOR_MAX_WIDTH)
    {
        exceed_limit = vx_true_e;
    }

    ret = vsi_nn_kernel_optimize_softmax_shape(
            inputs[0]->attr.size, inputs[0]->attr.dim_num, axis,
            shapes[0], &rank_in, &new_axis);

    if (ret)
    {
        reshape_tensors[0] = vsi_nn_reshape_tensor( graph,
                inputs[0], shapes[0], rank_in );
        reshape_tensors[1] = vsi_nn_reshape_tensor( graph,
                outputs[0], shapes[0], rank_in );
    }
    else
    {
        return NULL;
    }

    if( !vsi_nn_kernel_gpu_check_shape( reshape_tensors[0]->attr.size,
                reshape_tensors[0]->attr.dim_num )
     || new_axis > 2 || (new_axis == 2 && exceed_limit))
    {
        return NULL;
    }

    image_2d = ((reshape_tensors[0]->attr.dim_num == 2 || reshape_tensors[0]->attr.size[2] == 1)
        && new_axis != 2);
    status = _query_kernel( inputs, outputs, new_axis, image_2d, exceed_limit, kernel );
    if( VSI_SUCCESS == status)
    {
        node = vsi_nn_kernel_create_node( graph, kernel );

        if( node )
        {
            vsi_nn_kernel_node_pack_io( node_params, _CL_PARAM_NUM,
                    reshape_tensors, 1, &reshape_tensors[1], 1 );

            node_params[SCALAR_INPUT_AXIS] = vsi_nn_kernel_scalar_create(
                    graph, I32, &new_axis );
            node_params[SCALAR_INPUT_BETA] = vsi_nn_kernel_scalar_create(
                    graph, F32, &beta );
            node_params[SCALAR_INPUT_SCALE] = vsi_nn_kernel_scalar_create(
                    graph, F32, &scaleValue );
            node_params[SCALAR_OUTPUT_SCALE] = vsi_nn_kernel_scalar_create(
                    graph, F32, &outputScale );
            node_params[SCALAR_OUTPUT_ZP] = vsi_nn_kernel_scalar_create(
                    graph, F32, &outputZP );

            /* Pass parameters to node. */
            status  = vsi_nn_kernel_node_pass_param( node, node_params, _CL_PARAM_NUM );
            VSI_ASSERT( status == VSI_SUCCESS );

            vsi_nn_kernel_scalar_release( &node_params[SCALAR_INPUT_AXIS] );
            vsi_nn_kernel_scalar_release( &node_params[SCALAR_INPUT_BETA] );
            vsi_nn_kernel_scalar_release( &node_params[SCALAR_INPUT_SCALE] );
            vsi_nn_kernel_scalar_release( &node_params[SCALAR_OUTPUT_SCALE] );
            vsi_nn_kernel_scalar_release( &node_params[SCALAR_OUTPUT_ZP] );
        }
    }

    for (i = 0; i < 2; i++)
    {
        vsi_safe_release_tensor(reshape_tensors[i]);
    }

    return node;
} /* _setup() */

__END_DECLS

REGISTER_BACKEND_CL( log_softmax, _setup )
#endif