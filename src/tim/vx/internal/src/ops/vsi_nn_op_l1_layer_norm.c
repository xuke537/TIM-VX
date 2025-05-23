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


#include <string.h>
#include <stdlib.h>

#include "vsi_nn_types.h"
#include "vsi_nn_log.h"
#include "vsi_nn_node.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_ops.h"
#include "vsi_nn_tensor.h"
#include "utils/vsi_nn_util.h"
#include "kernel/vsi_nn_kernel.h"
#include "utils/vsi_nn_constraint_check.h"
#include "vsi_nn_tensor_util_prv.h"

typedef struct _l1_layer_norm_local_data_t {
    int32_t placeholder;
} l1_layer_norm_local_data_t;

/*
 Declare number of input and output.
 */
#define _INPUT_NUM          (4)
#define _OUTPUT_NUM         (1)

static vsi_status op_compute
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_param_t * param = NULL;
    vsi_nn_kernel_node_t    n = NULL;
    float eps = self->nn_param.l1_layer_norm.eps;
    int32_t axis = self->nn_param.l1_layer_norm.axis;

    param = vsi_nn_kernel_param_create();

    vsi_nn_kernel_param_add_float32( param, "eps", eps );
    vsi_nn_kernel_param_add_int32( param, "axis", axis );
    n = vsi_nn_kernel_selector( self->graph, "l1_layer_norm",
                    inputs, _INPUT_NUM, outputs, _OUTPUT_NUM, param );

    if ( n != NULL )
    {
        self->n = (vx_node)n;
        status = VSI_SUCCESS;
    }
    if (param != NULL)
    {
        vsi_nn_kernel_param_release( &param );
    }

    return status;
} /* op_compute() */

static vsi_bool op_check
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_bool ret = vsi_nn_is_stream_process_supported_types(self->graph, inputs, self->input.num);

    if (!ret)
    {
        BEGIN_IO_TYPE_DECL(L1_LAYER_NORM, 4, 1)
            IO_TYPE(D_F32,        D_F32,  D_F32,  D_F32,  D_F32)
            IO_TYPE(D_F16,        D_F32,  D_F32,  D_F32,  D_F16)
            IO_TYPE(D_F16,        D_F32,  D_F32,  D_F32,  D_U8|Q_ASYM)
            IO_TYPE(D_F16,        D_F32,  D_F32,  D_F32,  D_I8|Q_DFP)
            IO_TYPE(D_F16,        D_F32,  D_F32,  D_F32,  D_I8|Q_ASYM)
            IO_TYPE(D_F16,        D_F32,  D_F32,  D_F32,  D_I8|Q_SYM)
            IO_TYPE(D_F16,        D_F32,  D_F32,  D_F32,  D_I16|Q_DFP)
            IO_TYPE(D_F16,        D_F32,  D_F32,  D_F32,  D_I16|Q_ASYM)
            IO_TYPE(D_F16,        D_F32,  D_F32,  D_F32,  D_I16|Q_SYM)
            IO_TYPE(D_BF16,       D_F32,  D_F32,  D_F32,  D_BF16)
            IO_TYPE(D_U8|Q_ASYM,  D_F32,  D_F32,  D_F32,  D_U8|Q_ASYM)
            IO_TYPE(D_U8|Q_ASYM,  D_F32,  D_F32,  D_F32,  D_F16)
            IO_TYPE(D_I16|Q_DFP,  D_F32,  D_F32,  D_F32,  D_I16|Q_DFP)
            IO_TYPE(D_I16|Q_ASYM, D_F32,  D_F32,  D_F32,  D_I16|Q_ASYM)
            IO_TYPE(D_I16|Q_SYM,  D_F32,  D_F32,  D_F32,  D_I16|Q_SYM)
            IO_TYPE(D_I16|Q_DFP,  D_F32,  D_F32,  D_F32,  D_F16)
            IO_TYPE(D_I16|Q_ASYM, D_F32,  D_F32,  D_F32,  D_F16)
            IO_TYPE(D_I16|Q_SYM,  D_F32,  D_F32,  D_F32,  D_F16)
            IO_TYPE(D_I8|Q_DFP,   D_F32,  D_F32,  D_F32,  D_I8|Q_DFP)
            IO_TYPE(D_I8|Q_ASYM,  D_F32,  D_F32,  D_F32,  D_I8|Q_ASYM)
            IO_TYPE(D_I8|Q_SYM,   D_F32,  D_F32,  D_F32,  D_I8|Q_SYM)
            IO_TYPE(D_I8|Q_DFP,   D_F32,  D_F32,  D_F32,  D_F16)
            IO_TYPE(D_I8|Q_ASYM,  D_F32,  D_F32,  D_F32,  D_F16)
            IO_TYPE(D_I8|Q_SYM,   D_F32,  D_F32,  D_F32,  D_F16)
        END_IO_TYPE_DECL(L1_LAYER_NORM)
        if (!VALIDATE_OP_IO_TYPES(L1_LAYER_NORM, self, inputs, self->input.num, outputs, self->output.num))
        {
            char* desc = generate_op_io_types_desc(inputs,
                    self->input.num, outputs, self->output.num);
            VSILOGE("Inputs/Outputs data type not support: %s", desc);
            destroy_op_io_types_desc(desc);
            return FALSE;
        }
    }

    return TRUE;
} /* op_check() */

static vsi_bool op_setup
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    int32_t i = 0;
    VSI_UNREFERENCED(self);

    if (VSI_NN_DIM_AUTO == outputs[0]->attr.dim_num)
    {
        outputs[0]->attr.dim_num = inputs[0]->attr.dim_num;
        for (i = 0; i < (int32_t)inputs[0]->attr.dim_num; i++)
        {
            outputs[0]->attr.size[i] = inputs[0]->attr.size[i];
        }
    }
    return TRUE;
} /* op_setup() */

static vsi_status op_init
    (
    vsi_nn_node_t* self
    )
{
    vsi_status status = VSI_SUCCESS;

    self->nn_param.l1_layer_norm.axis = 0;

    return status;
} /* op_init() */


__BEGIN_DECLS

/* Registrar */
DEF_OP_REG
    (
    /* op_name    */ L1_LAYER_NORM,
    /* init       */ op_init,
    /* compute    */ op_compute,
    /* deinit     */ NULL,
    /* check      */ op_check,
    /* setup      */ op_setup,
    /* optimize   */ NULL,
    /* input_num  */ _INPUT_NUM,
    /* output_num */ _OUTPUT_NUM
    );

__END_DECLS

