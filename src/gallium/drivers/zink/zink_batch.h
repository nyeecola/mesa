/*
 * Copyright 2018 Collabora Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ZINK_BATCH_H
#define ZINK_BATCH_H

#include <vulkan/vulkan.h>

#include "util/u_dynarray.h"

struct zink_context;
struct zink_fence;
struct zink_framebuffer;
struct zink_render_pass;
struct zink_resource;

struct zink_batch {
   VkCommandBuffer cmdbuf;
   struct zink_fence *fence;

   struct zink_render_pass *rp;
   struct zink_framebuffer *fb;

   struct set *resources;

   struct util_dynarray zombie_samplers;
};

void
zink_start_cmdbuf(struct zink_context *ctx, struct zink_batch *batch);

void
zink_end_cmdbuf(struct zink_context *ctx, struct zink_batch *batch);

void
zink_batch_reference_resoure(struct zink_batch *cmdbuf,
                             struct zink_resource *res);

#endif