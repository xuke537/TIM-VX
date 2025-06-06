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
#include "vsi_nn_error.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_tensor_util.h"
#include "utils/vsi_nn_util.h"
#include "kernel/vsi_nn_kernel.h"

__BEGIN_DECLS

/*
 * Define kernel meta.
 */
typedef enum
{
    INTERNAL_KERNEL_LPPOOL,
} _internal_kernel_e;

#define _LPPOOL_KERNEL_SOURCE_NAME      "lppool"

// Add kernel hashtable here
#define LPPOOL_HASH_KEY( IN_DTYPE, OUT_DTYPE ) \
        (( IN_DTYPE << 8 ) | ( OUT_DTYPE ))
#define LPPOOL_KERNELS( IN_DTYPE, OUT_DTYPE ) \
        { LPPOOL_HASH_KEY( IN_DTYPE, OUT_DTYPE ), \
        CVIVANTE_NAMESPACE("cl.lppool_"#IN_DTYPE"to"#OUT_DTYPE), \
        _LPPOOL_KERNEL_SOURCE_NAME }, \

typedef struct
{
    uint32_t key;
    char * function_name;
    const char * source_name;
} _kernel_map_type;

static const _kernel_map_type _lppool_kernel_map[] =
{
    // Register kernel here
    LPPOOL_KERNELS( F32, F32 )
    LPPOOL_KERNELS( F32, U32 )
    LPPOOL_KERNELS( F32, I32 )
    LPPOOL_KERNELS( U32, U32 )
    LPPOOL_KERNELS( U32, F32 )
    LPPOOL_KERNELS( I32, I32 )
    LPPOOL_KERNELS( I32, F32 )
    LPPOOL_KERNELS( BF16, BF16 )
};

/*
 * Kernel params
 */
static vx_param_description_t _lppool_kernel_param_def[] =
{
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
};
#define _LPPOOL_PARAM_NUM  _cnt_of_array( _lppool_kernel_param_def )

/*
 * Kernel initializer
 */
DEF_KERNEL_INITIALIZER(_lppool_initializer)
    (
    vsi_nn_kernel_node_t                node,
    const vsi_nn_kernel_node_param_t  * param,
    size_t                              param_size
    )
{
    gpu_param_t gpu_param = {
        3,
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
        };
    vsi_status status = VSI_FAILURE;
    vx_tensor  output = (vx_tensor)param[1];
    vsi_nn_kernel_tensor_attr_t *output_attr  = NULL;
    vsi_size_array_t            *output_shape = NULL;

    VSI_UNREFERENCED(param_size);

    output_attr = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)output );
    CHECK_PTR_FAIL_GOTO( output_attr, "vsi_nn_kernel_tensor_attr_create fail.", final );

    output_shape = output_attr->shape;

    gpu_param.global_scale[0]  = 1;
    gpu_param.global_scale[1]  = 1;
    gpu_param.global_scale[2]  = 1;
    gpu_param.global_size[0]   = (output_shape->data[0] +  gpu_param.global_scale[0] - 1)
                                        /  gpu_param.global_scale[0];
    gpu_param.global_size[1]   = (output_shape->data[1] +  gpu_param.global_scale[1] - 1)
                                        /  gpu_param.global_scale[1];
    gpu_param.global_size[2]   = (output_shape->data[2] +  gpu_param.global_scale[2] - 1)
                                        /  gpu_param.global_scale[2];
    status = vsi_nn_kernel_gpu_config( node, &gpu_param );

final:
    if (output_attr)
    {
        vsi_nn_kernel_tensor_attr_release(&output_attr);
    }

    return status;
} /* _lppool_initializer() */



/*
 * Query kernel
 */
static vsi_status _query_kernel
    (
    vsi_nn_kernel_t * kernel,
    vsi_nn_tensor_t * const * const inputs,
    vsi_nn_tensor_t * const * const outputs
    )
{
    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_dtype_e in_dtype;
    vsi_nn_kernel_dtype_e out_dtype;
    const _kernel_map_type * kernel_map = _lppool_kernel_map;
    size_t kernel_map_size              = _cnt_of_array( _lppool_kernel_map );
    vx_param_description_t * param_def  = _lppool_kernel_param_def;
    vx_kernel_initialize_f  initializer = _lppool_initializer;

    uint32_t key;
    uint32_t i;

    in_dtype  = vsi_nn_kernel_map_dtype( inputs[0]->attr.dtype.vx_type );
    out_dtype = vsi_nn_kernel_map_dtype( outputs[0]->attr.dtype.vx_type );

#define _PACK_SELECT_KEY( in_dtype, out_dtype ) \
     (( in_dtype ) | (out_dtype << 8 ))
    switch (_PACK_SELECT_KEY(in_dtype, out_dtype))
    {
    case _PACK_SELECT_KEY(F32, F32):
    case _PACK_SELECT_KEY(F16, F16):
    case _PACK_SELECT_KEY(F32, F16):
    case _PACK_SELECT_KEY(F16, F32):
         key = LPPOOL_HASH_KEY( F32, F32);
         break;
    case _PACK_SELECT_KEY(F32, U8):
    case _PACK_SELECT_KEY(F16, U8):
         key = LPPOOL_HASH_KEY( F32, U32);
         break;
    case _PACK_SELECT_KEY(F32, I8):
    case _PACK_SELECT_KEY(F32, I16):
    case _PACK_SELECT_KEY(F16, I8):
    case _PACK_SELECT_KEY(F16, I16):
         key = LPPOOL_HASH_KEY( F32, I32);
         break;
    case _PACK_SELECT_KEY(U8, U8):
         key = LPPOOL_HASH_KEY( U32, U32);
         break;
    case _PACK_SELECT_KEY(U8, F16):
    case _PACK_SELECT_KEY(U8, F32):
         key = LPPOOL_HASH_KEY( U32, F32);
         break;
    case _PACK_SELECT_KEY(I8, I8):
    case _PACK_SELECT_KEY(I8, I16):
    case _PACK_SELECT_KEY(I16, I8):
    case _PACK_SELECT_KEY(I16, I16):
         key = LPPOOL_HASH_KEY( I32, I32);
         break;
    case _PACK_SELECT_KEY(I8, F16):
    case _PACK_SELECT_KEY(I8, F32):
    case _PACK_SELECT_KEY(I16, F16):
    case _PACK_SELECT_KEY(I16, F32):
         key = LPPOOL_HASH_KEY( I32, F32);
         break;
    default:
         key = LPPOOL_HASH_KEY( in_dtype, out_dtype);
         break;
    }
#undef _PACK_SELECT_KEY

    for ( i = 0; i < (uint32_t)kernel_map_size; i ++ )
    {
        if ( kernel_map[i].key == key )
        {
            break;
        }
    }
    if ( i < (uint32_t)kernel_map_size )
    {
        snprintf( kernel->info.name, VX_MAX_KERNEL_NAME, "%s",  kernel_map[i].function_name );
        kernel->info.parameters  = param_def;
        kernel->info.numParams   = _cnt_of_array( _lppool_kernel_param_def );
        kernel->info.initialize  = initializer;
        // Register code source
        vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_CODE, 2,
                "eltwise_ops_helper",
                kernel_map[i].source_name );
        // Register binary source
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
    vsi_nn_kernel_node_param_t node_params[_LPPOOL_PARAM_NUM];
    vsi_nn_kernel_node_t node = NULL;
    int32_t ksize_x  = vsi_nn_kernel_param_get_int32(params, "ksize_x");
    int32_t ksize_y  = vsi_nn_kernel_param_get_int32(params, "ksize_y");
    int32_t stride_x = vsi_nn_kernel_param_get_int32(params, "stride_x");
    int32_t stride_y = vsi_nn_kernel_param_get_int32(params, "stride_y");
    int32_t pad_left = vsi_nn_kernel_param_get_int32(params, "pad_left");
    int32_t pad_top  = vsi_nn_kernel_param_get_int32(params, "pad_top");
    int32_t p        = vsi_nn_kernel_param_get_int32(params, "p");
    int32_t width    = (int32_t)inputs[0]->attr.size[0];
    int32_t height   = (int32_t)inputs[0]->attr.size[1];
    float   outputScale  = vsi_nn_get_tensor_scale(outputs[0]);
    float   outputTail   = (float)vsi_nn_get_tensor_zero_point(outputs[0]);
    float   inputScale   = vsi_nn_get_tensor_scale(inputs[0]);
    float   inputTail    = (float)vsi_nn_get_tensor_zero_point(inputs[0]);

    if ( !vsi_nn_kernel_gpu_check_shape( inputs[0]->attr.size,
                inputs[0]->attr.dim_num )
     || !vsi_nn_kernel_gpu_check_shape( outputs[0]->attr.size,
                outputs[0]->attr.dim_num ))
    {
        return NULL;
    }

    outputScale = 1.0f / outputScale;
    inputTail   = -(inputTail * inputScale);

    status = _query_kernel( kernel, inputs, outputs );
    if ( VSI_SUCCESS == status)
    {
        node = vsi_nn_kernel_create_node( graph, kernel );
        if ( node )
        {
            /* Set inputs and outputs */
            uint32_t index = 2;
            vsi_nn_kernel_node_pack_io( node_params, _LPPOOL_PARAM_NUM,
                    inputs, input_num, outputs, output_num );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &ksize_x );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &ksize_y );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &stride_x );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &stride_y );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &pad_left );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &pad_top );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &p );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &width );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &height );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &inputScale );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &inputTail );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &outputScale );
            node_params[index++] = vsi_nn_kernel_scalar_create( graph, F32, &outputTail );
            /* Pass parameters to node. */
            status  = vsi_nn_kernel_node_pass_param( node, node_params, _LPPOOL_PARAM_NUM );
            vsi_nn_kernel_scalar_release( &node_params[2] );
            vsi_nn_kernel_scalar_release( &node_params[3] );
            vsi_nn_kernel_scalar_release( &node_params[4] );
            vsi_nn_kernel_scalar_release( &node_params[5] );
            vsi_nn_kernel_scalar_release( &node_params[6] );
            vsi_nn_kernel_scalar_release( &node_params[7] );
            vsi_nn_kernel_scalar_release( &node_params[8] );
            vsi_nn_kernel_scalar_release( &node_params[9] );
            vsi_nn_kernel_scalar_release( &node_params[10] );
            vsi_nn_kernel_scalar_release( &node_params[11] );
            vsi_nn_kernel_scalar_release( &node_params[12] );
            vsi_nn_kernel_scalar_release( &node_params[13] );
            vsi_nn_kernel_scalar_release( &node_params[14] );
        }
    }
    return node;
} /* _setup() */

__END_DECLS

REGISTER_BACKEND_CL( lppool, _setup )

