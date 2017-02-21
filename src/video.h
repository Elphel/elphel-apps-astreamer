/**
 * @file video.h
 * @brief Provides video interface for streamer
 * @copyright Copyright (C) 2017 Elphel Inc.
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

#ifndef _VIDEO__H_
#define _VIDEO__H_

#include <string>
#include "rtp_stream.h"
#include "parameters.h"

using namespace std;

#define FRAMES_AHEAD_FPS 3 /// number of frames ahead of current to write FPS limit
#define FRAMES_SKIP_FPS  3 /// number of frames to wait after target so circbuf will have at least 2 frames with new FPS for calculation

/// structure to store current video description
struct video_desc_t {
	bool valid;
	int width;
	int height;
	int quality;
	float fps;
};

class Video : public RTP_Stream {
public:
	enum vevent {
		VEVENT0,
		DAEMON_DISABLED,
		FPS_CHANGE,
		SIZE_CHANGE
	};

	Video(int port, Parameters *pars);

	virtual ~Video(void);
	/// return description of the current frame - i.e. current video parameters
	struct video_desc_t get_current_desc(bool with_fps = true);
	void fps(float);

	void Start(string ip, long port, int fps_scale, int ttl = -1);
	void Stop(void);
	Parameters *params;

/// Using Video class to interface global camera parameters 
	bool          waitDaemonEnabled(int daemonBit); // <0 - use default
	bool          isDaemonEnabled(int daemonBit); // <0 - use default
protected:
        
	long getFramePars(struct interframe_params_t * frame_pars, long before, long ptr_before = 0); 
	unsigned long prev_jpeg_wp;

	// frame params
	int f_width;
	int f_height;
	int f_quality;
	bool qtables_include;
	unsigned char qtable[128];
//	struct timeval f_tv;
	long buffer_length;
	unsigned long *buffer_ptr;
	unsigned long *buffer_ptr_s;  /// Second copy of the circbuf just after the end of the first to prevent rollovers
	void *frame_ptr;
	int fd_circbuf;
	int fd_jpeghead;

	long capture(void);
//	bool process(void);
	long  process(void);

	// for statistic
	long v_t_sec;
	long v_t_usec;
	int v_frames;
	unsigned long used_width;   ///frame width reported by Video::width(), used as the stream width
	unsigned long used_height;  /// similar to above
	float used_fps;   /// similar to above

	int fps_scale;
	int fps_scale_c; // counter for fps_scale
};

//extern Video *video;

#endif // _VIDEO__H_
