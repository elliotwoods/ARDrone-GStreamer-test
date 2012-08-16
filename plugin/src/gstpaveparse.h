#ifndef __GST_PAVEPARSE_H__
#define __GST_PAVEPARSE_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "pave.h"

static gboolean plugin_init_pave (GstPlugin * plugin);

G_BEGIN_DECLS

#define GST_TYPE_PAVEPARSE              (gst_paveparse_get_type())
#define GST_PAVEPARSE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PAVEPARSE, GstPaveParse))
#define GST_PAVEPARSE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PAVEPARSE, GstPaveParseClass))
#define GST_IS_PAVEPARSE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PAVEPARSE))
#define GST_IS_PAVEPARSE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_PAVEPARSE))

typedef struct _GstPaveParse GstPaveParse;
typedef struct _GstPaveParseClass GstPaveParseClass;

struct _GstPaveParse
{
	GstElement element;

	GstPad *sinkpad, *srcpad;

	GstCaps *caps;

	GstAdapter *adapter;

	parrot_video_encapsulation_t header;

	gboolean first_push;
	gboolean header_mode;

	gboolean latency_drop;
};

struct _GstPaveParseClass
{
	GstElementClass parent_class;
};

GType gst_paveparse_get_type(void);

G_END_DECLS

#endif /* __GST_PAVEPARSE_H__ */
