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
