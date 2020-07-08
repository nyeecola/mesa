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

#include <c99_alloca.h>
#include <assert.h>

#include "shaderapi.h"
#include "bufferobj.h"
#include "shader_time.h"
#include "context.h"
#include "get.h"
#include "compiler/glsl_types.h"

/**
 * Init/free per-context shader times.
 */
void
_mesa_init_shader_times(struct gl_context *ctx)
{
   ctx->ShaderTimes.BufObj = ctx->Driver.NewBufferObject(ctx,
                                                         SHADER_TIME_BUF_ID);

   if (!ctx->ShaderTimes.BufObj) {
      _mesa_problem(ctx, "Failed to create MESA_SHADER_TIME buffer object.");
      return;
   }

   uint64_t data[MESA_SHADER_STAGES] = {0};
   if (!ctx->Driver.BufferData(ctx, GL_SHADER_STORAGE_BUFFER, sizeof(data),
                               data, GL_DYNAMIC_COPY,
                               GL_MAP_WRITE_BIT | GL_MAP_READ_BIT,
                               ctx->ShaderTimes.BufObj)) {
      _mesa_error_no_memory(__func__);
      return;
   }

   ctx->ShaderTimes.LastReportTime = -1.0f;
   ctx->ShaderTimes.NumEntries = 0;
   ctx->ShaderTimes.MaxEntries = SHADER_TIME_INIT_ARR_COUNT;
   ctx->ShaderTimes.Ids = malloc(ctx->ShaderTimes.MaxEntries *
                                 sizeof(*ctx->ShaderTimes.Ids));
   ctx->ShaderTimes.Times = malloc(ctx->ShaderTimes.MaxEntries *
                                   sizeof(*ctx->ShaderTimes.Times));
}

void
_mesa_free_shader_times(struct gl_context *ctx)
{
   if (ctx->ShaderTimes.Ids)
      free(ctx->ShaderTimes.Ids);
   if (ctx->ShaderTimes.Times)
      free(ctx->ShaderTimes.Times);
   if (ctx->ShaderTimes.BufObj)
      ctx->Driver.DeleteBuffer(ctx, ctx->ShaderTimes.BufObj);
}

void
_mesa_prepare_shader_time_buffer(struct gl_context *ctx)
{
   assert(ctx);
   if (!ctx->shader_profiling_enabled || !ctx->_Shader->ActiveProgram)
      return;

   struct gl_buffer_object *bo = ctx->ShaderTimes.BufObj;

   // find the binding point for the ssbo which has block_index == 0
   GLint binding_point = -1;
   GLenum props = GL_BUFFER_BINDING;
   GLint length = 1;
   struct gl_shader_program *shProg = ctx->_Shader->ActiveProgram;
   _mesa_get_program_resourceiv(shProg, GL_SHADER_STORAGE_BLOCK,
                                0, 1, &props, sizeof(props),
                                &length, &binding_point);

   GLint previous_bufobj_id = -1;
   _mesa_GetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,
                     &previous_bufobj_id);
   if (previous_bufobj_id != -1) {
      ctx->ShaderTimes.PreviouslyBoundBufObj = _mesa_lookup_bufferobj(
         ctx, previous_bufobj_id);
   } else {
      ctx->ShaderTimes.PreviouslyBoundBufObj = NULL;
   }

   // this assumes the binding point has been selected so that no conflicts
   // occur, such as using the same binding point that the client uses
   _mesa_bind_buffer_base_shader_storage_buffer(ctx, binding_point, bo);
}

static void
collect_shader_time(struct gl_context *ctx)
{
   struct gl_buffer_object *bo = ctx->ShaderTimes.BufObj;

   // preserve gl user state
   if (ctx->ShaderTimes.PreviouslyBoundBufObj) {
      _mesa_reference_buffer_object(ctx, &ctx->ShaderStorageBuffer,
                                    ctx->ShaderTimes.PreviouslyBoundBufObj);
   }

   GLvoid *p = ctx->Driver.MapBufferRange(ctx, 0, bo->Size,
                                          GL_MAP_READ_BIT | GL_MAP_WRITE_BIT,
                                          bo, MAP_INTERNAL);
   uint64_t data[MESA_SHADER_STAGES];
   memcpy(data, p, sizeof(data));

   GLuint id = ctx->_Shader->ActiveProgram->Name;
   bool new_program = true;
   for (int i = 0; i < ctx->ShaderTimes.NumEntries; i++) {
      if (ctx->ShaderTimes.Ids[i] == id) {
         new_program = false;
         for (int j = 0; j < MESA_SHADER_STAGES; j++) {
            ctx->ShaderTimes.Times[i].Stages[j] += data[j];
         }
         break;
      }
   }
   if (new_program) {
      int index = ctx->ShaderTimes.NumEntries++;
      ctx->ShaderTimes.Ids[index] = id;
      for (int j = 0; j < MESA_SHADER_STAGES; j++) {
         ctx->ShaderTimes.Times[index].Stages[j] = data[j];
      }

      if (ctx->ShaderTimes.NumEntries == ctx->ShaderTimes.MaxEntries) {
         ctx->ShaderTimes.MaxEntries += SHADER_TIME_INIT_ARR_COUNT;
         ctx->ShaderTimes.Ids = realloc(ctx->ShaderTimes.Ids,
                                        ctx->ShaderTimes.MaxEntries *
                                        sizeof(*ctx->ShaderTimes.Ids));
         ctx->ShaderTimes.Times = realloc(ctx->ShaderTimes.Times,
                                          ctx->ShaderTimes.MaxEntries *
                                          sizeof(*ctx->ShaderTimes.Times));
         if (!ctx->ShaderTimes.Ids || !ctx->ShaderTimes.Times)
            _mesa_error_no_memory(__func__);
      }
   }

   memset(p, 0, sizeof(data));

   ctx->Driver.UnmapBuffer(ctx, bo, MAP_INTERNAL);
}

static void
report_shader_time(struct gl_context *ctx)
{
   double totalCycles = 0;
   for (int i = 0; i < ctx->ShaderTimes.NumEntries; i++) {
      for (int j = 0; j < MESA_SHADER_STAGES; j++) {
         totalCycles += ctx->ShaderTimes.Times[i].Stages[j];
      }
   }

   uint64_t totals[MESA_SHADER_STAGES] = {0};

   fprintf(stderr, "-----------------------------------------------------\n");
   fprintf(stderr, "type\t\tID\tcycles\t\t   %% of total\n");
   fprintf(stderr, "-----------------------------------------------------\n");
   for (int i = 0; i < ctx->ShaderTimes.NumEntries; i++) {
      GLuint id = ctx->ShaderTimes.Ids[i];
      uint64_t *times = ctx->ShaderTimes.Times[i].Stages;
      for (int stage = 0; stage < MESA_SHADER_STAGES; stage++) {
         if (times[stage]) {
            fprintf(stderr, "%s\t\t%d\t%-19lu%lf%%\n",
                   _mesa_shader_stage_to_abbrev(stage), id,
                   times[stage], 100*times[stage]/totalCycles);
            totals[stage] += times[stage];
         }
      }
   }
   fprintf(stderr, "-----------------------------------------------------\n");
   for (int stage = 0; stage < MESA_SHADER_STAGES; stage++) {
      fprintf(stderr, "Total %s\t\t%-19lu%lf%%\n",
             _mesa_shader_stage_to_abbrev(stage),
             totals[stage], 100*totals[stage]/totalCycles);
   }
   fprintf(stderr, "-----------------------------------------------------\n");
}

void
_mesa_collect_and_report_shader_time(struct gl_context *ctx)
{
   if (!ctx->shader_profiling_enabled || !ctx->_Shader->ActiveProgram)
      return;

   assert(ctx);

   collect_shader_time(ctx);

   struct timespec tp;
   clock_gettime(CLOCK_MONOTONIC, &tp);
   double curTime = tp.tv_sec + tp.tv_nsec / 1000000000.0;

   if (ctx->ShaderTimes.LastReportTime < 0) {
      ctx->ShaderTimes.LastReportTime = curTime;
   } else if (curTime - ctx->ShaderTimes.LastReportTime > 3.0f) {
      ctx->ShaderTimes.LastReportTime = curTime;
      report_shader_time(ctx);
   }
}

