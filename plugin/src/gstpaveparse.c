#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define VERSION "0.10"
#define PACKAGE "pave"
#endif

#include <string.h>

#include "gstpaveparse.h"

GST_DEBUG_CATEGORY_STATIC (paveparse_debug);
#define GST_CAT_DEFAULT (paveparse_debug)

/* Properties */

enum
{
	PROP_0,
	PROP_LATENCY_DROP,
};

#define DEFAULT_LATENCY_DROP FALSE

static void gst_paveparse_set_property (GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec);
static void gst_paveparse_get_property (GObject * object, guint prop_id,
	GValue * value, GParamSpec * pspec);

static gboolean gst_paveparse_sink_activate (GstPad * sinkpad);

static GstFlowReturn gst_paveparse_sink_chain (GstPad * pad, GstBuffer * buf);

static GstStaticPadTemplate sink_template_factory =
	GST_STATIC_PAD_TEMPLATE ("sink",
		GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS ("video/x-pave"));

GST_BOILERPLATE (GstPaveParse, gst_paveparse, GstElement, GST_TYPE_ELEMENT);

// -------------------------------- INIT --------------------------------------

static void
gst_paveparse_dispose (GObject * object)
{
	GstPaveParse *pave = GST_PAVEPARSE (object);

	if (pave->caps)
		gst_caps_unref(pave->caps);
	pave->caps = NULL;

	if (pave->adapter) {
		gst_adapter_clear (pave->adapter);
		g_object_unref (pave->adapter);
		pave->adapter = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_paveparse_base_init (gpointer g_class)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
	GstPadTemplate *src_template;

	gst_element_class_add_pad_template (element_class,
		gst_static_pad_template_get (&sink_template_factory));

	src_template = gst_pad_template_new ("src", GST_PAD_SRC,
		GST_PAD_SOMETIMES, gst_caps_new_simple ("video/x-h264", NULL));
	gst_element_class_add_pad_template (element_class, src_template);
	gst_object_unref (src_template);

	gst_element_class_set_details_simple (element_class, "PaVE parser",
		"Codec/Parser/Video",
		"Parse H.264 video from a PaVE stream.",
		"Dardo D. Kleiner <dardokleiner@gmail.com>");
}

static void
gst_paveparse_class_init (GstPaveParseClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *)klass;
	gstelement_class = (GstElementClass *)klass;

	gobject_class->dispose = gst_paveparse_dispose;
	gobject_class->set_property = gst_paveparse_set_property;
	gobject_class->get_property = gst_paveparse_get_property;

	g_object_class_install_property (gobject_class, PROP_LATENCY_DROP,
		g_param_spec_boolean ("latency-drop", "Latency drop",
			"Drop frames under high latency conditions.",
			DEFAULT_LATENCY_DROP,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_paveparse_init (GstPaveParse * pave, GstPaveParseClass * g_class)
{
	pave->sinkpad =
		gst_pad_new_from_static_template (&sink_template_factory, "sink");

	gst_pad_set_activate_function (pave->sinkpad,
		GST_DEBUG_FUNCPTR (gst_paveparse_sink_activate));
	gst_pad_set_chain_function (pave->sinkpad,
		GST_DEBUG_FUNCPTR (gst_paveparse_sink_chain));

	gst_element_add_pad (GST_ELEMENT_CAST (pave), pave->sinkpad);

	pave->srcpad = NULL;

	pave->caps = NULL;

	pave->adapter = NULL;

	pave->first_push = TRUE;
	pave->header_mode = TRUE;
}

static void
gst_paveparse_destroy_sourcepad (GstPaveParse * paveparse)
{
	if (paveparse->srcpad) {
		gst_element_remove_pad (GST_ELEMENT_CAST (paveparse),
			paveparse->srcpad);
		paveparse->srcpad = NULL;
	}
}

static void
gst_paveparse_create_sourcepad (GstPaveParse * paveparse)
{
	GstElementClass *klass = GST_ELEMENT_GET_CLASS (paveparse);
	GstPadTemplate *src_template;

	gst_paveparse_destroy_sourcepad (paveparse);

	src_template = gst_element_class_get_pad_template (klass, "src");
	paveparse->srcpad = gst_pad_new_from_template (src_template, "src");
	gst_pad_use_fixed_caps (paveparse->srcpad);
}

static void
gst_paveparse_add_src_pad (GstPaveParse * pave, GstBuffer * buf)
{
	gst_paveparse_create_sourcepad (pave);
	gst_pad_set_active (pave->srcpad, TRUE);
	gst_pad_set_caps (pave->srcpad, pave->caps);
	gst_caps_replace (&pave->caps, NULL);

	gst_element_add_pad (GST_ELEMENT_CAST (pave), pave->srcpad);
	gst_element_no_more_pads (GST_ELEMENT_CAST (pave));

	GST_LOG_OBJECT (pave, "%s", __FUNCTION__);
}

// ------------------------------ CLASS HANDLERS ------------------------------

static void
gst_paveparse_set_property (GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec)
{
	GstPaveParse *pave = GST_PAVEPARSE (object);

	switch (prop_id) {
		case PROP_LATENCY_DROP:
			pave->latency_drop = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gst_paveparse_get_property (GObject * object, guint prop_id, GValue * value,
	GParamSpec * pspec)
{
	GstPaveParse *pave = GST_PAVEPARSE (object);

	switch (prop_id) {
		case PROP_LATENCY_DROP:
			g_value_set_boolean (value, pave->latency_drop);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

// ----------------------------- SINK PAD HANDLERS ----------------------------

static gboolean
gst_paveparse_sink_activate (GstPad * sinkpad)
{
	GstPaveParse *pave = GST_PAVEPARSE (GST_PAD_PARENT (sinkpad));

	GST_LOG_OBJECT (pave, "%s", __FUNCTION__);

	if (pave->adapter) {
		gst_adapter_clear (pave->adapter);
		g_object_unref (pave->adapter);
		pave->adapter = NULL;
	}

	pave->adapter = gst_adapter_new ();

	return gst_pad_activate_push (sinkpad, TRUE);
}

static GstFlowReturn
gst_paveparse_sink_chain (GstPad * pad, GstBuffer * buf)
{
	GstFlowReturn ret = GST_FLOW_OK;

	GstPaveParse *pave = GST_PAVEPARSE (GST_PAD_PARENT (pad));

	//GST_LOG_OBJECT (pave, "%s(%d)", __FUNCTION__, GST_BUFFER_SIZE (buf));

	gst_adapter_push (pave->adapter, buf);

	if (pave->header_mode) {
		GstCaps *caps = NULL;
		const size_t header_size = sizeof(parrot_video_encapsulation_t);
		const guint8 *peek;
		guint avail = gst_adapter_available (pave->adapter);

		if (avail < header_size)
			goto out;

		peek = gst_adapter_peek (pave->adapter, 4);
		if (!PAVE_CHECK (peek)) {
			GST_WARNING_OBJECT (pave, "no PaVE header found!");
			ret = GST_FLOW_ERROR;
			goto out;
		}

		if (pave->latency_drop) {
			guint offset = header_size;
			gboolean header_ahead = FALSE;
			do {
				guint flush;
				GST_LOG_OBJECT (pave, "scan from %d for %d", offset,
					avail - offset);
				flush = gst_adapter_masked_scan_uint32 (pave->adapter,
					0xffffffff, PAVE_INT32BE_SIGNATURE, offset, avail - offset);
				if (flush == -1) {
					GST_LOG_OBJECT (pave, "no header ahead!");
					header_ahead = FALSE;
				} else if ((flush + header_size) <= avail) {
					parrot_video_encapsulation_t h;
					GST_LOG_OBJECT (pave, "header ahead @ %d", flush);
					gst_adapter_copy (pave->adapter, (guint8 *)&h, flush,
						header_size);
					offset += header_size + h.payload_size;
					if (h.frame_type != FRAME_TYPE_P_FRAME) {
						GST_LOG_OBJECT (pave, "I-Frame ahead @ %d!", flush);
						gst_adapter_flush (pave->adapter, flush);
						avail -= flush;
					}
					header_ahead = TRUE;
				} else {
					//GST_LOG_OBJECT (pave, "partial header ahead @ %d", flush);
					header_ahead = FALSE;
				}
			} while (header_ahead && (offset < avail));

			if (avail < header_size)
				goto out;
		}

		gst_adapter_copy (pave->adapter, (guint8 *)&pave->header, 0,
			header_size);
		gst_adapter_flush (pave->adapter, header_size);

		g_assert (PAVE_CHECK (&pave->header));
		g_assert (PAVE_CURRENT_VERSION == pave->header.version);
		g_assert (CODEC_MPEG4_AVC == pave->header.video_codec);
		g_assert (header_size == pave->header.header_size);

		//video_stage_tcp_dumpPave (&pave->header);

		caps = gst_caps_new_simple ("video/x-h264", 
			"width", G_TYPE_INT, pave->header.display_width,
			"height", G_TYPE_INT, pave->header.display_height,
			"framerate", GST_TYPE_FRACTION, 25, 1,
			NULL);
		gst_caps_replace (&pave->caps, caps);
		gst_caps_replace (&caps, NULL);

		pave->header_mode = FALSE;
	} else {
		GstBuffer *buffer;

		if (gst_adapter_available (pave->adapter) < pave->header.payload_size)
			goto out;

		buffer = gst_adapter_take_buffer (pave->adapter,
			pave->header.payload_size);

		if (G_UNLIKELY (pave->first_push)) {
			pave->first_push = FALSE;
			gst_paveparse_add_src_pad (pave, buf);
		}

		//GST_LOG_OBJECT (pave, "%s(%d)", __FUNCTION__, pave->header.timestamp);
		//GST_BUFFER_TIMESTAMP (buffer) = pave->header.timestamp * 1000000ULL;
		//GST_BUFFER_OFFSET (buffer) = pave->header.frame_number - 1;
		//GST_BUFFER_OFFSET_END (buffer) = pave->header.frame_number;

		gst_buffer_set_caps (buffer, GST_PAD_CAPS (pave->srcpad));

		gst_pad_push (pave->srcpad, buffer);

		pave->header_mode = TRUE;
	}

out:
	return ret;
}

// ---------------------------------- PLUGIN ----------------------------------

static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG_CATEGORY_INIT (paveparse_debug, "paveparse", 0, "PaVE parser");

	return gst_element_register (plugin, "paveparse", GST_RANK_PRIMARY,
		GST_TYPE_PAVEPARSE);
}

static gboolean plugin_init_pave (GstPlugin * plugin)
{
	plugin_init(plugin);
}

GST_PLUGIN_DEFINE(
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	"paveparse",
	"Parse H.264 video from a PaVE stream.",
	plugin_init,
	VERSION,
	"LGPL",
	"GStreamer",
	"http://gstreamer.net/")
