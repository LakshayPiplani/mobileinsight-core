#ifndef __DM_COLLECTOR_C_HDLC_H__
#define __DM_COLLECTOR_C_HDLC_H__

#include <string>

std::string encode_hdlc_frame (const char *payld, int length);
void feed_binary (const char *b, int length);
void reset_binary ();
bool get_next_frame (std::string& output_frame, bool& crc_correct);
void check_frame_format (std::string& output_frame);

// Off by default. When on, feed_binary()/get_next_frame() print the buffer
// state (MI(PARTIAL)/MI(FRAME)/MI(LEFTOVER)) to stderr on every call — verbose,
// re-dumps the whole pending buffer each time, meant for debugging framing
// issues only.
void set_hdlc_verbose (bool on);

#endif  // __DM_COLLECTOR_C_HDLC_H__
