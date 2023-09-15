#include <gst/gst.h>

typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *depay;
  GstElement *decode;
  GstElement *sink;
} CustomData;


int main(int argc, char *argv[]) {
  CustomData data;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  gboolean terminate = FALSE;

  // Init GStreamer
  gst_init (&argc, &argv);

  /* Create the elements */
  data.source = gst_element_factory_make("udpsrc", "source");
  data.depay = gst_element_factory_make("rtph264depay", "pay");
  data.decode = gst_element_factory_make("avdec_h264", "enc");
  data.sink = gst_element_factory_make("autovideosink", "sink");

  /* Create the empty pipeline */
  data.pipeline = gst_pipeline_new("test-pipeline");

  // Set udp source
  g_object_set(G_OBJECT(data.source), "port", 8080, NULL);

  if (!data.pipeline || !data.source || !data.depay || !data.decode || !data.sink) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  // Build the pipeline
  gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.depay, data.decode, data.sink, NULL);
  if (!gst_element_link_many(data.source, data.depay, data.decode, data.sink, NULL)) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  // Start the pipeline
  ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  // Listen to the bus
  bus = gst_element_get_bus(data.pipeline);
  do {
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Parse message */
    if (msg != NULL) {
      GError *err;
      gchar *debug_info;

      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          gst_message_parse_error (msg, &err, &debug_info);
          g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
          g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
          g_clear_error(&err);
          g_free(debug_info);
          terminate = TRUE;
          break;
        case GST_MESSAGE_EOS:
          g_print("End-Of-Stream reached.\n");
          terminate = TRUE;
          break;
        case GST_MESSAGE_STATE_CHANGED:
          /* We are only interested in state-changed messages from the pipeline */
          if (GST_MESSAGE_SRC(msg) == GST_OBJECT (data.pipeline)) {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
            g_print("Pipeline state changed from %s to %s:\n",
                gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
          }
          break;
        default:
          /* We should not reach here */
          g_printerr("Unexpected message received.\n");
          break;
      }
      gst_message_unref(msg);
    }
  } while(!terminate);

  /* Free resources */
  gst_object_unref(bus);
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}

