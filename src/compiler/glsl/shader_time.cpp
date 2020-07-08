/*
 * Copyright © 2019 Igalia S.L.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "main/mtypes.h"
#include "main/shader_time.h"
#include "compiler/glsl_types.h"

/**
 * Creates an SSBO block for MESA_SHADER_TIME and returns a pointer to it.
 *
 * This block is a uint64_t array with length = MESA_SHADER_STAGES.
 * Each element will be used to store the number of cycles the shader program
 * takes to execute.
 */
struct gl_uniform_block *
_mesa_create_shader_time_block(void *ctx, GLuint binding)
{
   struct gl_uniform_block *block =
      rzalloc_array(ctx, struct gl_uniform_block, 1);
   struct gl_uniform_buffer_variable *extra_uniforms_info =
      rzalloc_array(ctx, struct gl_uniform_buffer_variable, 1);

   block->Name = ralloc_strdup(ctx, SHADER_TIME_IFACE_NAME);
   block->NumUniforms = 1;
   block->Uniforms = extra_uniforms_info;
   // TODO: generate random interface and variable name
   block->Uniforms[0].Name =
      ralloc_strdup(ctx, SHADER_TIME_IFACE_NAME "." SHADER_TIME_VAR_NAME);
   block->Uniforms[0].IndexName =
      ralloc_strdup(ctx, SHADER_TIME_IFACE_NAME "." SHADER_TIME_VAR_NAME);

   static const glsl_struct_field field[] = {
      glsl_struct_field(glsl_type::get_array_instance(
                           glsl_type::uint64_t_type, MESA_SHADER_STAGES, 0),
                        SHADER_TIME_VAR_NAME)
   };
   block->Uniforms[0].Type =
      glsl_type::get_interface_instance(field, ARRAY_SIZE(field),
                                        GLSL_INTERFACE_PACKING_STD430,
                                        false, SHADER_TIME_IFACE_NAME);
   block->Uniforms[0].Offset = 0;
   block->Uniforms[0].RowMajor = 0;

   block->Binding = binding;
   block->UniformBufferSize =
      MESA_SHADER_STAGES * glsl_type::uint64_t_type->std430_size(0);
   block->stageref = 0;
   block->linearized_array_index = 0;
   block->_Packing = GLSL_INTERFACE_PACKING_STD430;
   block->_RowMajor = 0;

   return block;
}

