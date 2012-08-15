#include <gst/gst.h>
#include <iostream>
#include <Windows.h>
#include <iostream>
#include "pave/gstpaveparse.h"

using namespace std;

GMainLoop *loop;


static gboolean
my_bus_callback (GstBus     *bus,
		 GstMessage *message,
		 gpointer    data)
{
	g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_ERROR: {
		GError *err;
		gchar *debug;

		gst_message_parse_error (message, &err, &debug);
		g_print ("Error: %s\n", err->message);
		g_error_free (err);
		g_free (debug);

		//g_main_loop_quit (loop);
		break;
	}
	case GST_MESSAGE_EOS:
		/* end-of-stream */
		g_main_loop_quit (loop);
		break;
	default:
		/* unhandled message */
		break;
	}

	/* we want to be notified again the next time there is a message
	* on the bus, so returning TRUE (FALSE means we want to stop watching
	* for messages on the bus and our callback should not be called again)
	*/
	return TRUE;
}

int main (int   argc, char *argv[])
{
	GstElement *source, *parser, *decode, *view, *pipeline;
	GstBus *bus;

	/* init GStreamer */
	gst_init (&argc, &argv);

	/* register pave plugin */
	_GstPaveParse pave_plugin;
	if (!gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR,
		"paveparse", "Parses PaVE (Parrot Video Encapsulation) streams into h.264.",
		plugin_init_pave,
		"1.0", "LGPL", "ARDrone", "ARDrone",
		"https://projects.ardrone.org/boards/1/topics/show/4282")) {
		cout << "ERROR: PaVE element factory plugin has not been correctly loaded" << endl;
		return 1;
	}

	/* create elements */
	pipeline = gst_pipeline_new ("main_pipeline");
	source = gst_element_factory_make ("filesrc", "source");
	parser = gst_element_factory_make ("paveparse", "parser");
	decode = gst_element_factory_make ("ffdec_h264", "decoder");
	view = gst_element_factory_make ("sdlvideosink", "view");

	// build graph
	gst_bin_add_many(GST_BIN(pipeline), source, parser, decode, view, NULL);
	if (!gst_element_link_many(source, parser, decode, view, NULL)) {
		std::cout << "Couldn't link elements" << std::endl;
		return 0;
	}
	
	
	// play file
	//g_object_set(player, "uri", "file:///E:/Media/HanRiver/Video recordings/MVI_2538.MOV", NULL);
	//g_object_set(player, "uri", "http://www.arielnet.com/topics2/AVI_files/tenis0001.avi", NULL);
	//g_object_set(source, "location", "http://192.168.1.1:5555", NULL);
	g_object_set(source, "location", "E:\\Media\\ARDrone\\record.h264", NULL);
	gst_element_set_state (GST_ELEMENT(pipeline), GST_STATE_PLAYING);

	// create bus
	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_watch(bus, my_bus_callback, NULL);

	// run loop
	//loop = g_main_loop_new (NULL, FALSE);
	//g_main_loop_run(loop);

	
	while(true) {
		Sleep(10);
		GstMessage message;
		gpointer data;
		//gst_bus_async_signal_func(bus, &message, data);
	}

	// clean up
	gst_object_unref(bus);
	gst_object_unref(pipeline);
	g_main_loop_unref(loop);

	return 0;
}
