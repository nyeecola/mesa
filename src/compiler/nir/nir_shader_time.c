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

#include "nir.h"
#include "nir_builder.h"

/*
 * This pass measures the time taken for each shader stage and stores it
 * on the SSBO with block_index 0.
 *
 * It is assumed that this SSBO block_index has been allocated properly
 * beforehand.
 */

static void
nir_shader_time_impl(nir_shader *shader, nir_function_impl *impl)
{
   nir_builder b;
   nir_builder_init(&b, impl);

   // get timestamp at the start of this shader stage
   nir_block *first_block = nir_start_block(impl);
   b.cursor = nir_before_block(first_block);
   nir_intrinsic_instr *start_instr =
      nir_intrinsic_instr_create(shader, nir_intrinsic_shader_clock);
   nir_ssa_dest_init(&start_instr->instr, &start_instr->dest, 2, 32, NULL);
   nir_builder_instr_insert(&b, &start_instr->instr);
   nir_ssa_def *packed_start = nir_pack_64_2x32(&b, &start_instr->dest.ssa);

   // get timestamp at the end of this shader stage
   nir_block *last_block = nir_impl_last_block(impl);
   b.cursor = nir_after_block(last_block);
   nir_intrinsic_instr *end_instr =
      nir_intrinsic_instr_create(shader, nir_intrinsic_shader_clock);
   nir_ssa_dest_init(&end_instr->instr, &end_instr->dest, 2, 32, NULL);
   nir_builder_instr_insert(&b, &end_instr->instr);
   nir_ssa_def *packed_end = nir_pack_64_2x32(&b, &end_instr->dest.ssa);

   // subtract both timestamps
   nir_ssa_def *elapsed = nir_isub(&b, packed_end, packed_start);

   // accumulate result in the array position given by
   // the "shader->info.stage" enum
   nir_intrinsic_instr *store_ssbo =
      nir_intrinsic_instr_create(shader, nir_intrinsic_ssbo_atomic_add);
   store_ssbo->num_components = 1;
   store_ssbo->src[0] = nir_src_for_ssa(nir_imm_int(&b, 0));
   // 8 bytes for each stage (uint64)
   store_ssbo->src[1] =
      nir_src_for_ssa(nir_imm_int(&b, shader->info.stage * 8));
   store_ssbo->src[2] = nir_src_for_ssa(elapsed);
   // TODO: figure out why "32" is the only valid value here,
   //       specially because our ssbo is a uint64
   //
   //       It appears that the intel backend has an assert for 32,
   //       so it will crash if we put 64 there.
   nir_ssa_dest_init(&store_ssbo->instr, &store_ssbo->dest,
                     store_ssbo->num_components, 32, NULL);
   nir_builder_instr_insert(&b, &store_ssbo->instr);
}

void
nir_shader_time(nir_shader *shader)
{
   assert(shader);

   nir_shader_time_impl(shader, nir_shader_get_entrypoint(shader));
}
