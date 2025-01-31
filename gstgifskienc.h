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

#ifndef _GST_GIFSKIENC_H_
#define _GST_GIFSKIENC_H_

#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
#include <gifski.h>

G_BEGIN_DECLS

#define GST_TYPE_GIFSKIENC   (gst_gifskienc_get_type())
#define GST_GIFSKIENC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GIFSKIENC,GstGifskienc))
#define GST_GIFSKIENC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GIFSKIENC,GstGifskiencClass))
#define GST_IS_GIFSKIENC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GIFSKIENC))
#define GST_IS_GIFSKIENC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GIFSKIENC))

typedef struct _GstGifskienc GstGifskienc;
typedef struct _GstGifskiencClass GstGifskiencClass;

struct _GstGifskienc
{
  GstVideoEncoder element;

  /* Properties */
  guint quality;
  const gchar *file_location;

  /* Internal */
  gifski *gifski;
  GstVideoFormat input_format;
  GstVideoCodecState *input_state;
  gint frame_count;
  GstBuffer *output_buffer;
  GstVideoCodecFrame *current_frame;
};

struct _GstGifskiencClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_gifskienc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE(gifskienc);

G_END_DECLS

#endif
