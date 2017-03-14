/**
 * @file audio.cpp
 * @brief Provides audio interface for streamer
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

#include "audio.h"
#include "helper.h"

#include <iostream>

#include <sys/ioctl.h>
#include <arpa/inet.h>

//#undef AUDIO_DEBUG
#define AUDIO_DEBUG
//#undef AUDIO_DEBUG_2
#define AUDIO_DEBUG_2

#ifdef AUDIO_DEBUG
	#define D(s_port, a) \
	do { \
		cerr << __FILE__ << ": " << __FUNCTION__ << ": " << __LINE__ << ": sensor port: " << s_port << " "; \
		a; \
	} while (0)
#else
	#define D(s_port, a)
#endif

#ifdef AUDIO_DEBUG_2
	#define D2(s_port, a) \
	do { \
		cerr << __FILE__ << ": " << __FUNCTION__ << ": " << __LINE__ << ": sensor port: " << s_port << " "; \
		a; \
	} while (0)
#else
	#define D2(s_port, a)
#endif

#define SAMPLE_TIME	              20	                        //< restrict ALSA to have this period, in milliseconds
#define BUFFER_TIME	              1000                          //< approximate buffer duration for ALSA, in milliseconds
#define LEN                       1200                          //< the size of data buffer for RTP packet, in bytes

using namespace std;

Audio::Audio(int port, bool enable, Parameters *pars, int sample_rate, int channels) {
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	_present = false;
	stream_name = "audio";
	params = pars;
	sensor_port = port;

	// normalize audio settings
	if (sample_rate == 0)
		sample_rate = SAMPLE_RATE;
	if (sample_rate > 48000)
		sample_rate = 48000;
	if (sample_rate < 11025)
		sample_rate = 11025;
	_sample_rate = sample_rate;
	if (channels == 0)
		channels = SAMPLE_CHANNELS;
	if (channels < 1)
		channels = 1;
	if (channels > 2)
		channels = 2;
	_channels = channels;
	_volume = 65535;
	_volume *= 90;
	_volume /= 100;

	SSRC = 10;
	// here sbuffer_len in samples, not bytes
	sbuffer_len = _sample_rate * SAMPLE_TIME;
	sbuffer_len /= 1000;
	sbuffer_len -= sbuffer_len % 2;
	D(sensor_port, cerr << "sbuffer_len == " << sbuffer_len << endl);
	_ptype = 97;
	sbuffer = NULL;
	packet_buffer = NULL;

	if (enable) {
		// open ALSA for capturing
		int err;
		sbuffer = new short[sbuffer_len * 2 * _channels];
		packet_buffer = new unsigned char[LEN + 20];
		bool init_ok = false;
		while (true) {
			if ((err = snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0)
				break;
			snd_pcm_hw_params_alloca(&hw_params);
			if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0)
				break;
			if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
				break;
			if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_BE)) < 0)
				break;
			unsigned int t = _sample_rate;
			if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &t, 0)) < 0)
				break;
			unsigned int period_time = SAMPLE_TIME * 1000;
			if ((err = snd_pcm_hw_params_set_period_time_near(capture_handle, hw_params, &period_time, 0)) < 0)
				break;
			D(sensor_port, cerr << "period_time == " << period_time << endl);
			unsigned int buffer_time = BUFFER_TIME * 1000;
			if ((err = snd_pcm_hw_params_set_buffer_time_near(capture_handle, hw_params, &buffer_time, 0)) < 0)
				break;
			D(sensor_port, cerr << "buffer_time == " << buffer_time << endl);
			_sample_rate = t;
			if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, _channels)) < 0)
				break;
			if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0)
				break;

			snd_pcm_sw_params_alloca(&sw_params);
			if ((err = snd_pcm_sw_params_current(capture_handle, sw_params)) < 0)
				break;
			if ((err = snd_pcm_sw_params_set_tstamp_mode(capture_handle, sw_params, SND_PCM_TSTAMP_ENABLE)) < 0)
				break;
			/* SND_PCM_TSTAMP_TYPE_GETTIMEOFDAY for some reason does not produce time stamps equal to system time,
			 * so stick with monotonic time stamps and apply the difference between FPGA time and ALSA time stamps to
			 * the current samples during transmission
			 */
			if ((err = snd_pcm_sw_params_set_tstamp_type(capture_handle, sw_params, SND_PCM_TSTAMP_TYPE_MONOTONIC)) < 0)
				break;
			if ((err = snd_pcm_sw_params(capture_handle, sw_params)) < 0)
				break;
			init_ok = true;
			break;
		}
		if (init_ok) {
			D(sensor_port, cerr << "Audio init: ok; with sample rate: " << _sample_rate	<< "; and channels: " << _channels << endl);
			_present = true;
			_play = false;
			set_volume(_volume);
			// create thread...
			init_pthread((void *) this);
		} else {
			D(sensor_port, cerr << "Audio: init FAIL!" << endl);
			_present = false;
		}
	}
}

Audio::~Audio(void) {
	if (_present) {
		snd_pcm_drop(capture_handle);
		snd_pcm_close(capture_handle);
		snd_config_update_free_global();
	}
	if (sbuffer != NULL)
		delete[] sbuffer;
	if (packet_buffer != NULL)
		delete[] packet_buffer;
}

void Audio::set_capture_volume(int nvolume) {
	if (_present == false)
		return;
	snd_mixer_t *mixer;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;
	// allocated at stack, so can't free it!
	snd_mixer_selem_id_alloca(&sid);

	snd_mixer_open(&mixer, 0);
	snd_mixer_attach(mixer, "default");
	snd_mixer_selem_register(mixer, NULL, NULL);
	snd_mixer_load(mixer);

	for (elem = snd_mixer_first_elem(mixer); elem; elem = snd_mixer_elem_next(elem)) {
		snd_mixer_selem_get_id(elem, sid);
		if (!snd_mixer_selem_is_active(elem))
			continue;
		// set volume at percents for capture elements
		snd_mixer_elem_t *selem = snd_mixer_find_selem(mixer, sid);
		if (selem == NULL) {
			break;
		}
		long volume_min = 0;
		long volume_max = 0;
		if (snd_mixer_selem_get_capture_volume_range(selem, &volume_min, &volume_max) == 0) {
			// set volume only for capture
			if (nvolume > 65535)
				nvolume = 65535;
			if (nvolume < 0)
				nvolume = 0;
			long long vol_new = volume_max;
			vol_new *= nvolume;
			vol_new /= 65535;
			long vol = 0;
			snd_mixer_selem_get_capture_volume(selem, SND_MIXER_SCHN_FRONT_LEFT, &vol);
			snd_mixer_selem_set_capture_volume_all(selem, vol_new);
			D(sensor_port, cerr << "element " << snd_mixer_selem_id_get_name(sid) << " - OLD min vol == "
							<< volume_min << "; max vol == " << volume_max << "; volume == " << vol
							<< endl);
			D(sensor_port, snd_mixer_selem_get_capture_volume(selem, SND_MIXER_SCHN_FRONT_LEFT, &vol));
			D(sensor_port, cerr << "element " << snd_mixer_selem_id_get_name(sid) << " - NEW min vol == "
							<< volume_min << "; max vol == " << volume_max << "; volume == " << vol
							<< endl);
		}
	}
	_volume = nvolume;
	snd_mixer_close(mixer);
}

/**
 * @brief Start audio stream if it was enabled
 * The time delta between FPGA time and time stamps reported by ALSA is calculated before stream is started.
 * This delta is applied to time stamps received from ALSA. Previously, this delta was calculated as time difference
 * between FPGA time and system time reported by \e getsystimeofday(), but as for now ALSA reports monotonic
 * time stamps regardless of the time stamp type set by \e snd_pcm_sw_params_set_tstamp_type(). See git commit
 * 58b954f239b695b7deda7a33657841d2c64476ae (or earlier) for previous implementation.
 * @param   ip   ip/port pair for socket
 * @param   port ip/port pair for socket
 * @param   ttl  ttl value for socket
 * @return  None
 */
void Audio::Start(string ip, long port, int ttl) {
	if (!_present)
		return;
	D(sensor_port, cerr << "Audio ---> Start !!!" << endl);
	timestamp_rtcp = 0;
	f_tv.tv_sec = 0;
	f_tv.tv_usec = 0;
	snd_pcm_prepare(capture_handle);
	snd_pcm_reset(capture_handle);

	// get FPGA/ALSA time delta
	delta_fpga_alsa = get_delta_fpga_alsa();
	D2(sensor_port, cerr << "FPGA/ALSA time delta = " << delta_fpga_alsa << " us" << endl);

	RTP_Stream::Start(ip, port, ttl);
}

void Audio::Stop(void) {
	if (!_present)
		return;
	D(sensor_port, cerr << "Audio ---> Stop !!!" << endl);
	RTP_Stream::Stop();
	f_tv.tv_sec = 0;
	f_tv.tv_usec = 0;
}

long Audio::process(void) {
	long ret = 0;
	snd_pcm_status_t *status;
	snd_pcm_status_alloca(&status);
	snd_timestamp_t ts;
	unsigned long avail;

	for(int i = 0; i < 4; i++) {
		snd_pcm_status(capture_handle, status);
		long slen = snd_pcm_readi(capture_handle, sbuffer, sbuffer_len);
		if(slen > 0) {
			// update statistics for RTCP
			avail = (unsigned long)snd_pcm_status_get_avail(status);
			if(avail > 0) {
				snd_pcm_status_get_tstamp(status, &ts);
				timestamp = timestamp_rtcp;
				timestamp += avail;
				if(timestamp > 0xFFFFFFFF)
					timestamp &= 0xFFFFFFFF;
				f_tv.tv_sec = ts.tv_sec;
				f_tv.tv_usec = ts.tv_usec;
				// correct A time to V time for RTCP sync packets
				f_tv.tv_sec -= 1;
				f_tv.tv_usec += 1000000;
				f_tv.tv_sec -= delta_fpga_alsa / 1000000;
				f_tv.tv_usec -= delta_fpga_alsa % 1000000;
				f_tv.tv_sec += f_tv.tv_usec / 1000000;
				f_tv.tv_usec = f_tv.tv_usec % 1000000;
			}
			ret += slen;
			process_send(slen);
			// check again that buffer is not empty
			snd_pcm_status(capture_handle, status);
			avail = (unsigned long)snd_pcm_status_get_avail(status);
			if(avail >= (unsigned long)sbuffer_len) {
				D2(sensor_port, cerr << "process() again - available " << avail << " samples" << endl);
				continue;
			}
			return ret;
		}
		if(slen < 0) {
			cerr << "audio process(): slen < 0" << endl;
			break;
		}
		if(slen == 0) {
			cerr << "audio process(): slen == 0" << endl;
			break;
		}
	}
	return ret;
}

long Audio::process_send(long sample_len) {
	unsigned short pnum;

	void *m = (void *) sbuffer;

	int i;
	long offset = 0;
	int to_send = sample_len * 2 * _channels;
	int size = to_send;
	long count = 0;

	uint32_t ts;
	for (;;) {
		if (to_send == 0)
			break;
		if (to_send > LEN) {
			i = LEN;
			to_send -= i;
		} else {
			i = to_send;
			to_send = 0;
		}
		ts = htonl(timestamp_rtcp);
		uint32_t ts_delta = i / 2;
		ts_delta /= _channels;
		timestamp_rtcp += ts_delta;
		if (timestamp_rtcp > 0xFFFFFFFF)
			timestamp_rtcp &= 0xFFFFFFFF;
		packet_num++;
		pnum = htons(packet_num);
		count += i;
		// fill RTP header
		packet_buffer[0] = 0x80;
		if (count >= size)
			packet_buffer[1] = _ptype + 0x80;
		else
			packet_buffer[1] = _ptype;
		memcpy((void *) &packet_buffer[2], (void *) &pnum, 2);
		memcpy((void *) &packet_buffer[4], (void *) &ts, 4);
		memcpy((void *) &packet_buffer[8], (void *) &SSRC, 4);
		// fill data
		memcpy((void *) &packet_buffer[12], (void *) ((char *) m + offset), i);
		offset += i;
		// send packet
		rtp_packets++;
		rtp_octets += i;                                        // total amount of payload data transmitted
		rtp_socket->send((void *)packet_buffer, i + 12);
	}

	return sample_len;
}

/**
 * @brief Calculate the difference between FPGA time and time stamps reported by ALSA.
 * This function gets two time stamps from FPGA and one time stamp from ALSA driver between the first two,
 * then calculates mean value of the two FPGA samples and finds the delta between the mean value and
 * ALSA time stamp. This value is applied to each time stamp in sound data packet to compensate the difference.
 * @return   The time delta in microseconds
 */
long long Audio::get_delta_fpga_alsa(void)
{
	snd_pcm_status_t *pcm_status;
	snd_timestamp_t snd_ts;
	struct timeval snd_tv;
	struct timeval tv_1, tv_2;
	struct timeval delta_tv;
	unsigned long delta_us;
	long long ret_val;
	bool fpga_greater = false;

	snd_pcm_status_alloca(&pcm_status);
	tv_1 = params->get_fpga_time();
	snd_pcm_status(capture_handle, pcm_status);
	snd_pcm_status_get_tstamp(pcm_status, &snd_ts);
	tv_2 = params->get_fpga_time();
	snd_tv.tv_sec = snd_ts.tv_sec;
	snd_tv.tv_usec = snd_ts.tv_usec;
	delta_us = time_delta_us(tv_2, tv_1);
	delta_us /= 2;
	tv_1 = time_plus_us(tv_1, delta_us);

	if (tv_1.tv_sec > snd_tv.tv_sec)
		fpga_greater = true;
	else if (tv_1.tv_sec == snd_tv.tv_sec)
		if (tv_1.tv_usec > snd_tv.tv_usec)
			fpga_greater = true;
	struct timeval *tv_g = &snd_tv;                             // greater time value
	struct timeval *tv_s = &tv_1;                               // smaller time value
	if (fpga_greater) {
		tv_g = &tv_1;
		tv_s = &snd_tv;
	}
	delta_tv = time_minus(*tv_g, *tv_s);
	ret_val = (long long)delta_tv.tv_sec * 1000000 + (long long)delta_tv.tv_usec;
	if (fpga_greater) {
		ret_val = -ret_val;
	}

	D2(sensor_port, cerr << "ALSA time stamp = " << snd_ts.tv_sec << ":" << snd_ts.tv_usec << endl);
	D2(sensor_port, cerr << "delta_tv = " << delta_tv.tv_sec << ":" << delta_tv.tv_usec << endl);

	return ret_val;
}
