#ifndef _SESSION__H_
#define _SESSION__H_

#include <string>

using namespace std;

#define VIDEO_MJPEG "MJPG"

struct transport_t {
	string ip;
	bool ip_custom;	// true, if user set IP by himself, otherwise false
	unsigned long ip_cached;	// cashed IP to check parameter
//	string port;
	int port_video;
	int port_audio;
	bool process_audio;
	bool multicast;
	string ttl;
};

struct video_t {
	int type;	// == "", if not present
	double fps;		// == 0, if unspecified
	int width;
	int height;
	int fps_scale; // 0,1 - no scale, 2 - scale 2x etc...
};

struct audio_t {
	int type;
	int sample_rate;
	int channels;
	int volume;	// for inner use only - from 0 to 65535 - current audio capture volume
};

class Session {
public:
	string id;
	struct transport_t rtp_in;
	struct transport_t rtp_out;
	struct video_t video;
	struct audio_t audio;
	bool process_audio;
};

#endif // _SESSION__H_
