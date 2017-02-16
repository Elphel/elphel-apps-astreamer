#ifndef __H_STREAMER__
#define __H_STREAMER__

#include <string>
#include <map>

#include "video.h"
#include "audio.h"
#include "rtsp.h"

using namespace std;

class Streamer {
public:
	Streamer(const map<string, string> &args);
	~Streamer();
	void Main(void);
	bool opt_present(string name) {
		if(args.find(name) != args.end())
			return true;
		return false;
	}
	static Streamer *instance(void) {
		return _streamer;
	}
protected:
	static Streamer *_streamer;
	static int f_handler(void *ptr, RTSP_Server *rtsp_server, RTSP_Server::event event);
	int handler(RTSP_Server *rtsp_server, RTSP_Server::event event);

	int update_settings(bool apply = false);

	map<string, string> args;
	RTSP_Server *rtsp_server;
	Session *session;

	Audio *audio;
	Video *video;
	bool running;
	int connected_count;
	void audio_init(void);
};

#endif // __H_STREAMER__
