/**
MIT License

Copyright (c) 2025 Can Vural

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
#include <math.h>

#include "gstgifskienc.h"

#define GST_CAT_DEFAULT gst_gifskienc_debug_category
GST_DEBUG_CATEGORY_STATIC(gst_gifskienc_debug_category);

enum {
    PROP_0,
    PROP_QUALITY,
    PROP_FILE_LOCATION,
};

#define DEFAULT_QUALITY 90

/* prototypes */
static void gst_gifskienc_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_gifskienc_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static gboolean gst_gifskienc_start(GstVideoEncoder *encoder);
static gboolean gst_gifskienc_stop(GstVideoEncoder *encoder);
static gboolean gst_gifskienc_set_format(GstVideoEncoder *encoder, GstVideoCodecState *state);
static GstFlowReturn gst_gifskienc_handle_frame(GstVideoEncoder *encoder, GstVideoCodecFrame *frame);
static GstFlowReturn gst_gifskienc_finish(GstVideoEncoder *encoder);
static gboolean gst_gifskienc_propose_allocation(GstVideoEncoder *encoder, GstQuery *query);

#define VIDEO_SINK_CAPS \
GST_VIDEO_CAPS_MAKE("{ RGB, RGBA }")

static GstStaticPadTemplate gifskienc_sink_factory =
        GST_STATIC_PAD_TEMPLATE("sink",
                                GST_PAD_SINK,
                                GST_PAD_ALWAYS,
                                GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGB, RGBA}"))
        );

static GstStaticPadTemplate gifskienc_src_factory =
        GST_STATIC_PAD_TEMPLATE("src",
                                GST_PAD_SRC,
                                GST_PAD_ALWAYS,
                                GST_STATIC_CAPS ("image/gif, "
                                    "framerate = (fraction) [0, 100]")
        );

/* class initialization */

#define gst_gifskienc_parent_class parent_class
G_DEFINE_TYPE(GstGifskienc, gst_gifskienc, GST_TYPE_VIDEO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE(gifskienc, "gifskienc", GST_RANK_PRIMARY, GST_TYPE_GIFSKIENC);

static void gst_gifskienc_class_init(GstGifskiencClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = (GstElementClass *) klass;
    GstVideoEncoderClass *video_encoder_class = GST_VIDEO_ENCODER_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    gst_element_class_add_static_pad_template(element_class, &gifskienc_sink_factory);
    gst_element_class_add_static_pad_template(element_class, &gifskienc_src_factory);

    gst_element_class_set_static_metadata(element_class,
                                          "Gifski GIF encoder", "Codec/Encoder/Video", "Encode videos in GIF format",
                                          "Can Vural <can@vural.dev>");

    gobject_class->set_property = gst_gifskienc_set_property;
    gobject_class->get_property = gst_gifskienc_get_property;
    video_encoder_class->start = GST_DEBUG_FUNCPTR(gst_gifskienc_start);
    video_encoder_class->stop = GST_DEBUG_FUNCPTR(gst_gifskienc_stop);
    video_encoder_class->set_format = GST_DEBUG_FUNCPTR(gst_gifskienc_set_format);
    video_encoder_class->handle_frame = GST_DEBUG_FUNCPTR(gst_gifskienc_handle_frame);
    video_encoder_class->finish = GST_DEBUG_FUNCPTR(gst_gifskienc_finish);
    video_encoder_class->propose_allocation = GST_DEBUG_FUNCPTR(gst_gifskienc_propose_allocation);

    g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_uint ("quality", "Quality",
          "Quality setting for GIF encoding (0-100)",
          0, 100, DEFAULT_QUALITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_FILE_LOCATION,
      g_param_spec_string("location", "File Location",
          "Full path to the file to write the GIF to",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    GST_DEBUG_CATEGORY_INIT(gst_gifskienc_debug_category, "gifskienc", 0,
                            "Gifski encoding element");
}

static void gst_gifskienc_init(GstGifskienc *self) {
    GST_PAD_SET_ACCEPT_TEMPLATE(GST_VIDEO_ENCODER_SINK_PAD(self));

    self->quality = DEFAULT_QUALITY;
    self->gifski = NULL;
    self->input_state = NULL;
    self->frame_count = 0;
    self->input_format = GST_VIDEO_FORMAT_UNKNOWN;
}

void gst_gifskienc_set_property(GObject *object, const guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGifskienc *enc = GST_GIFSKIENC(object);

    if (property_id == PROP_QUALITY) {
        enc->quality = g_value_get_uint(value);
    } else if (property_id == PROP_FILE_LOCATION) {
        enc->file_location = g_value_dup_string(value);
    } else
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

void gst_gifskienc_get_property(GObject *object, const guint property_id,
                                GValue *value, GParamSpec *pspec) {
    const GstGifskienc *enc = GST_GIFSKIENC(object);

    if (property_id == PROP_QUALITY) {
        g_value_set_uint(value, enc->quality);
    } else if (property_id == PROP_FILE_LOCATION) {
        g_value_set_string(value, enc->file_location);
    } else {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static gboolean gst_gifskienc_start(GstVideoEncoder *encoder) {
    GstGifskienc *enc = GST_GIFSKIENC(encoder);

    enc->gifski = gifski_new(&(GifskiSettings){
        .quality = enc->quality,
    });

    if (!enc->gifski) {
        GST_ERROR_OBJECT (enc, "Failed to create gifski instance");
        return FALSE;
    }

    gifski_set_file_output(enc->gifski, enc->file_location);

    enc->frame_count = 0;
    enc->output_buffer = NULL;
    enc->current_frame = NULL;

    return TRUE;
}

static gboolean gst_gifskienc_stop(GstVideoEncoder *encoder) {
    GstGifskienc *enc = GST_GIFSKIENC(encoder);

    if (enc->gifski) {
        enc->gifski = NULL;
    }

    if (enc->input_state) {
        gst_video_codec_state_unref (enc->input_state);
        enc->input_state = NULL;
    }

    if(enc->output_buffer != NULL) {
        gst_buffer_unref(enc->output_buffer);
    }

    enc->output_buffer = NULL;

    return TRUE;
}

static gboolean gst_gifskienc_set_format(GstVideoEncoder *encoder, GstVideoCodecState *state) {
    GstGifskienc *enc = GST_GIFSKIENC(encoder);
    const GstVideoInfo *info = &state->info;

    const GstVideoFormat format = GST_VIDEO_INFO_FORMAT (info);

    if (format != GST_VIDEO_FORMAT_RGB && format != GST_VIDEO_FORMAT_RGBA) {
        GST_ERROR_OBJECT (enc, "Invalid color format");

        return FALSE;
    }

    enc->input_format = format;

    if (enc->input_state) {
        gst_video_codec_state_unref(enc->input_state);
    }

    enc->input_state = gst_video_codec_state_ref(state);

    GstVideoCodecState *output_state = gst_video_encoder_set_output_state(
        GST_VIDEO_ENCODER(enc),
        gst_caps_new_empty_simple("image/gif"),
        enc->input_state
    );

    gst_video_codec_state_unref(output_state);
    return TRUE;
}

static GstFlowReturn gst_gifskienc_handle_frame(GstVideoEncoder *encoder, GstVideoCodecFrame *frame) {
    GstGifskienc *enc = GST_GIFSKIENC(encoder);

    enc->current_frame = frame;

    GstVideoFrame vframe;
    if (!gst_video_frame_map (&vframe, &enc->input_state->info, frame->input_buffer, GST_MAP_READ)) {
        return GST_FLOW_ERROR;
    }

    const GstVideoFormat format = GST_VIDEO_FRAME_FORMAT(&vframe);
    const guint8 *pixels = GST_VIDEO_FRAME_PLANE_DATA (&vframe, 0);
    const guint width = GST_VIDEO_FRAME_WIDTH(&vframe);
    const guint height = GST_VIDEO_FRAME_HEIGHT(&vframe);
    const guint stride = GST_VIDEO_FRAME_PLANE_STRIDE (&vframe, 0);
    gint fps = GST_VIDEO_INFO_FPS_N(&vframe.info);

    if (fps <= 0) {
        fps = 30;
    }

    guint8 *raw_buffer = malloc(width*height*4);

    const gint frame_bytes_per_pixel = format == GST_VIDEO_FORMAT_RGB ? 3 : 4;

    for (guint h = 0; h < height; ++h) {
        const gint bytes_per_pixel = 4;
        const guint8 *src_row = pixels + h * stride;
        guint8 *dst_row = raw_buffer + h * width * bytes_per_pixel;

        for (guint w = 0; w < width; ++w) {
            const guint8 *src_pixel = src_row + w * frame_bytes_per_pixel;
            guint8 *dst_pixel = dst_row + w * bytes_per_pixel;

            dst_pixel[0] = src_pixel[0]; // R
            dst_pixel[1] = src_pixel[1]; // G
            dst_pixel[2] = src_pixel[2]; // B
            dst_pixel[3] = format == GST_VIDEO_FORMAT_RGB ? 255 : src_pixel[3]; // A
        }
    }

    GST_DEBUG("Frame Count %d Frame pts: %lu Calculated PTS: %g Info FPS N:%d",
        enc->frame_count,
        frame->pts,
        frame->pts / 1000000000.0,
        fps
    );

    GST_DEBUG("Frame dimensions: %dx%d", width, height);

    const gint res = gifski_add_frame_rgba(enc->gifski, enc->frame_count,
                                           width, height, raw_buffer,
                                           frame->pts / 1000000000.0
    );
    g_free(raw_buffer);

    enc->frame_count++;

    gst_video_frame_unmap (&vframe);
    if (res != GIFSKI_OK) {
        GST_ERROR_OBJECT (enc, "Failed to add frame to gifski: %d", res);
        return GST_FLOW_ERROR;
    }

    GST_VIDEO_CODEC_FRAME_FLAG_SET(frame, GST_VIDEO_CODEC_FRAME_FLAG_SYNC_POINT);

    return gst_video_encoder_finish_frame (encoder, frame);
}

static GstFlowReturn gst_gifskienc_finish(GstVideoEncoder *encoder) {
    GstGifskienc *enc = GST_GIFSKIENC(encoder);

    GST_DEBUG("Finishing encoding");

    if(enc->gifski != NULL) {
        const int res = gifski_finish(enc->gifski);

        if (res != GIFSKI_OK) {
            GST_ERROR_OBJECT (enc, "Failed to finish GIF encoding: %d", res);
            return GST_FLOW_ERROR;
        }
    }

    // Get the source pad of the encoder
    GstPad *srcpad = gst_element_get_static_pad(GST_ELEMENT(enc), "src");
    if (!srcpad) {
        GST_ERROR_OBJECT (enc, "Failed to get src pad");
        return GST_FLOW_ERROR;
    }

    // Create an EOS event
    GstEvent *event = gst_event_new_eos();

    // Push the EOS event downstream
    gst_pad_push_event (srcpad, event);
    gst_object_unref (srcpad);
    GST_DEBUG("Finished encoding");

    return GST_FLOW_OK;
}

static gboolean gst_gifskienc_propose_allocation(GstVideoEncoder *encoder, GstQuery *query) {
    gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

    return GST_VIDEO_ENCODER_CLASS(parent_class)->propose_allocation(encoder, query);
}

static gboolean plugin_init(GstPlugin *plugin) {
    gboolean ret = FALSE;

    ret |= GST_ELEMENT_REGISTER(gifskienc, plugin);

    return ret;
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "gifskienc"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gifskienc"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://vural.dev/"
#endif

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  gifskienc,
                  "Gifski GIF encoder",
                  plugin_init, VERSION, "MIT", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
