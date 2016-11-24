/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * copy video filter
 */

#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "libswscale/swscale.h"
#include "avfilter.h"
#include "internal.h"
#include "video.h"
#include "tf.h"

static int query_formats(AVFilterContext *ctx)
{
    AVFilterFormats *formats = NULL;
    int fmt;

    printf("query format!!\n");
    tf_init("/vagrant/graph.pb");
    for (fmt = 0; av_pix_fmt_desc_get(fmt); fmt++)
    {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(fmt);
        int ret;
        if (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)
            continue;
        if ((ret = ff_add_format(&formats, fmt)) < 0)
            return ret;
    }

    return ff_set_common_formats(ctx, formats);
}

static int filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterLink *outlink = inlink->dst->outputs[0];
    AVFrame *out = ff_get_video_buffer (outlink, in->width, in->height);
    struct SwsContext *sws_ctx = NULL;
    sws_ctx = sws_getContext(
            inlink->w,
            inlink->h,
            AV_PIX_FMT_YUV420P,
            outlink->w,
            outlink->h,
            AV_PIX_FMT_RGB24,
            SWS_BILINEAR, NULL, NULL, NULL
            );
    uint8_t *prgb24 = calloc(3 * inlink->w * inlink->h, sizeof(uint8_t));
    uint8_t *rgb24[1] = { prgb24 };
    int rgb24_stride[1] = { 3 * inlink->w };
    sws_scale(sws_ctx, in->data, in->linesize, 0, inlink->h, rgb24, rgb24_stride);
    if (!out) {
        av_frame_free(&in);
        return AVERROR(ENOMEM);
    }
    av_frame_copy_props(out, in);

    tf_transfer(rgb24[0], inlink->w, inlink->h);

    sws_ctx = sws_getContext(
            inlink->w,
            inlink->h,
            AV_PIX_FMT_RGB24,
            outlink->w,
            outlink->h,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, NULL, NULL, NULL
            );
    sws_scale(sws_ctx, rgb24, rgb24_stride, 0, inlink->h, out->data, out->linesize);

    free(prgb24);
    av_frame_free(&in);
    return ff_filter_frame(outlink, out);
}

static const AVFilterPad avfilter_vf_copy_inputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
        .filter_frame = filter_frame,
    },
    {NULL}
};

static const AVFilterPad avfilter_vf_copy_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    {NULL}
};

AVFilter ff_vf_tf = {
    .name = "tf",
    .description = NULL_IF_CONFIG_SMALL ("Style transfer network"),
    .inputs = avfilter_vf_copy_inputs,
    .outputs = avfilter_vf_copy_outputs,
    .query_formats = query_formats,
};
