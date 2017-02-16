/**
 * @file FILENAME
 * @brief BRIEF DESCRIPTION
 * @copyright Copyright (C) YEAR Elphel Inc.
 * @author AUTHOR <EMAIL>
 *
 * @par License:
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
