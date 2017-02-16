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

#ifndef _AUDIO__H_
#define _AUDIO__H_

#include <string>
#define __cplusplus 1
#include <alsa/asoundlib.h>
#include <pthread.h>
#include "rtp_stream.h"

using namespace std;

#define SAMPLE_RATE	44100
#define SAMPLE_CHANNELS 2

class Audio : public RTP_Stream {
public:
	Audio(bool enable, int sample_rate = SAMPLE_RATE, int channels = SAMPLE_CHANNELS);
	virtual ~Audio(void);
	long sample_rate(void) { return _sample_rate; };
	long channels(void) { return _channels; };
	bool present(void) { return _present; };
	long volume(void) { return _volume; };
	void set_volume(long volume) { set_capture_volume(volume); }

	void Start(string ip, long port, int ttl = -1);
	void Stop(void);
protected:
	int fd;
	snd_pcm_t *capture_handle;
	short *sbuffer;
	long sbuffer_len;
	bool _present;
	long _sample_rate;
	long _channels;
	long _volume;

	long capture(void);
	long process(void);
	long process_send(long sample_len);
	void set_capture_volume(int volume);

	uint64_t timestamp_rtcp;
	long delta_fpga_sys;	// A/V clocks delta for RTCP
bool is_first;
bool is_first2;
};

//extern Audio *audio;

#endif // _AUDIO__H_
