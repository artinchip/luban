/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors:  <qi.xu@artinchip.com>
 */

#ifndef __GST_VE_DEC_H__
#define __GST_VE_DEC_H__

#include <gst/video/gstvideodecoder.h>
#include "mpp_decoder.h"
#include "gstmppallocator.h"

G_BEGIN_DECLS

#define GST_TYPE_VE_DEC \
	(gst_ve_dec_get_type())
#define GST_VE_DEC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VE_DEC,GstVeDec))
#define GST_VE_DEC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VE_DEC,GstVeDecClass))
#define GST_IS_VE_DEC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VE_DEC))
#define GST_IS_VE_DEC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VE_DEC))

typedef struct _GstVeDec		GstVeDec;
typedef struct _GstVeDecClass		GstVeDecClass;

struct VeFrame {
	long long pts;
	int system_frame_number; // system_frame_number in struct GstVideoCodecFrame
};

struct _GstVeDec {
	GstVideoDecoder decoder;

	enum mpp_codec_type dec_type;
	struct mpp_decoder *mpp_dec;

	GstMppAllocator *allocator;

	GstVideoCodecState *input_state;

	GstVideoInfo info;

	int width;
	int height;

	GHashTable *pts2frame;

	// system_frame_number in struct GstVideoCodecFrame
	GList *system_frame_number_in_ve;
	int task_ret;

	int stop_flag;

	// decode Task
	GstTask* dec_task;
	GRecMutex lock;
	int dec_task_ret;
};

struct _GstVeDecClass {
	GstVideoDecoderClass decoder_class;
};

GType gst_ve_dec_get_type(void);

G_END_DECLS

#endif /* __GST_VPU_DEC_H__ */
