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

#ifndef SHADER_TIME_H
#define SHADER_TIME_H

#include "main/glheader.h"

/* just IDs/strings that stands out when debugging */
#define SHADER_TIME_BUF_ID 21212
#define SHADER_TIME_INIT_ARR_COUNT 5
#define SHADER_TIME_IFACE_NAME "__shaderTimeIFaceName"
#define SHADER_TIME_VAR_NAME "__shaderTimeVarName"

struct gl_context;

extern void
_mesa_init_shader_times(struct gl_context *ctx);

extern void
_mesa_free_shader_times(struct gl_context *ctx);

extern void
_mesa_prepare_shader_time_buffer(struct gl_context *ctx);

extern void
_mesa_collect_and_report_shader_time(struct gl_context *ctx);

extern struct gl_uniform_block *
_mesa_create_shader_time_block(void *ctx, GLuint binding);

#endif /* SHADER_TIME_H */
