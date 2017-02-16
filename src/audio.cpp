#include "audio.h"
#include "helper.h"

#include <iostream>

#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <asm/elphel/c313a.h>

#include "parameters.h"

#undef AUDIO_DEBUG
//#define AUDIO_DEBUG
#undef AUDIO_DEBUG_2
//#define AUDIO_DEBUG_2

#ifdef AUDIO_DEBUG
	#define D(a) a
#else
	#define D(a)
#endif

#ifdef AUDIO_DEBUG_2
	#define D2(a) a
#else
	#define D2(a)
#endif

//#define SAMPLE_TIME	50	// in milliseconds
#define SAMPLE_TIME	20	// in milliseconds
#define BUFFER_TIME	1000 // in milliseconds

using namespace std;

Audio::Audio(bool enable, int sample_rate, int channels) {
//cerr << "Audio::Audio()" << endl;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	_present = false;
	stream_name = "audio";

	// normalize audio settings
	if(sample_rate == 0)	sample_rate = SAMPLE_RATE;
	if(sample_rate > 48000)	sample_rate = 48000;
	if(sample_rate < 11025)	sample_rate = 11025;
	_sample_rate = sample_rate;
	if(channels == 0)		channels = SAMPLE_CHANNELS;
	if(channels < 1)		channels = 1;
	if(channels > 2)		channels = 2;
	_channels = channels;
	_volume = 65535;
	_volume *= 90;
	_volume /= 100;
	
	SSRC = 10;
	// here sbuffer_len in samples, not bytes
	sbuffer_len = _sample_rate * SAMPLE_TIME;
	sbuffer_len /= 1000;
	sbuffer_len -= sbuffer_len % 2;
D(	cerr << "sbuffer_len == " << sbuffer_len << endl;)
	_ptype = 97;
	sbuffer = NULL;

	if(enable) {
	    // open ALSA for capturing
		int err;
		sbuffer = new short[sbuffer_len * 2 * _channels];
		bool init_ok = false;
		while(true) {
			if((err = snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0)
				break;
			snd_pcm_hw_params_alloca(&hw_params);
		   	if((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0)
   				break;
		   	if((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
			   	break;
		   	if((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_BE)) < 0)
   				break;
			unsigned int t = _sample_rate;
			if((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &t, 0)) < 0)
				break;
			unsigned int period_time = SAMPLE_TIME * 1000;
			if((err = snd_pcm_hw_params_set_period_time_near(capture_handle, hw_params, &period_time, 0)) < 0)
				break;
D(			cerr << "period_time == " << period_time << endl;)
			unsigned int buffer_time = BUFFER_TIME * 1000;
			if((err = snd_pcm_hw_params_set_buffer_time_near(capture_handle, hw_params, &buffer_time, 0)) < 0)
				break;
D(			cerr << "buffer_time == " << buffer_time << endl;)
			_sample_rate = t;
			if((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, _channels)) < 0)
				break;
			if((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0)
				break;

			snd_pcm_sw_params_alloca(&sw_params);
			if((err = snd_pcm_sw_params_current(capture_handle, sw_params)) < 0)
				break;
			if((err = snd_pcm_sw_params_set_tstamp_mode(capture_handle, sw_params, SND_PCM_TSTAMP_ENABLE)) < 0)
				break;
			if((err = snd_pcm_sw_params(capture_handle, sw_params)) < 0)
				break;
//			if((err = snd_pcm_prepare(capture_handle)) < 0)
//				break;
			init_ok = true;
			break;
		}
		if(init_ok) {
D(			cerr << endl << "Audio init: ok; with sample rate: " << _sample_rate << "; and channels: " << _channels << endl;)
//			cerr << endl << "Audio init: ok; with sample rate: " << _sample_rate << "; and channels: " << _channels << endl;
			_present = true;
			_play = false;
			set_volume(_volume);
			// create thread...
			init_pthread((void *)this);
		} else {
D(			cerr << "Audio: init FAIL!" << endl;)
//			cerr << "Audio: init FAIL!" << endl;
			_present = false;
		}
	}
}

Audio::~Audio(void) {
//cerr << "Audio::~Audio()" << endl;
	if(_present) {
		snd_pcm_drop(capture_handle);
		snd_pcm_close(capture_handle);
		snd_config_update_free_global();
//cerr << "--------------->> close audio" << endl;
	}
	if(sbuffer != NULL)
		delete[] sbuffer;
}

void Audio::set_capture_volume(int nvolume) {
	if(_present == false)
		return;
//	int err;
	snd_mixer_t *mixer;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;
	// allocated at stack, so can't free it!
	snd_mixer_selem_id_alloca(&sid);

	snd_mixer_open(&mixer, 0);
	snd_mixer_attach(mixer, "default");
	snd_mixer_selem_register(mixer, NULL, NULL);
	snd_mixer_load(mixer);
			
	for(elem = snd_mixer_first_elem(mixer); elem; elem = snd_mixer_elem_next(elem)) {
		snd_mixer_selem_get_id(elem, sid);
		if(!snd_mixer_selem_is_active(elem))
			continue;
		// set volume at percents for capture elements
		snd_mixer_elem_t *selem = snd_mixer_find_selem(mixer, sid);
		if(selem == NULL) {
			break;
		}
		long volume_min = 0;
		long volume_max = 0;
		if(snd_mixer_selem_get_capture_volume_range(selem, &volume_min, &volume_max) == 0) {
			// set volume only for capture
			if(nvolume > 65535)
				nvolume = 65535;
			if(nvolume < 0)
				nvolume = 0;
			long long vol_new = volume_max;
			vol_new *= nvolume;
			vol_new /= 65535;
			long vol = 0;
			snd_mixer_selem_get_capture_volume(selem, SND_MIXER_SCHN_FRONT_LEFT, &vol);
			snd_mixer_selem_set_capture_volume_all(selem, vol_new);
D(			cerr << "element " << snd_mixer_selem_id_get_name(sid) << " - OLD min vol == " << volume_min << "; max vol == " << volume_max << "; volume == " << vol << endl;)
D(			snd_mixer_selem_get_capture_volume(selem, SND_MIXER_SCHN_FRONT_LEFT, &vol);)
D(			cerr << "element " << snd_mixer_selem_id_get_name(sid) << " - NEW min vol == " << volume_min << "; max vol == " << volume_max << "; volume == " << vol << endl;)
		}
	}
	_volume = nvolume;
	snd_mixer_close(mixer);
}

void Audio::Start(string ip, long port, int ttl) {
	if(!_present)
		return;
D(	cerr << "Audio ---> Start !!!" << endl;)
//cerr << "Audio ---> Start !!!" << endl;
//is_first = true;
	timestamp_rtcp = 0;
	f_tv.tv_sec = 0;
	f_tv.tv_usec = 0;
	snd_pcm_prepare(capture_handle);
	snd_pcm_reset(capture_handle);
//cerr << "Audio ---> Start !!! - done" << endl;

	// get FPGA/sys time delta
	Parameters *params = Parameters::instance();
	unsigned long write_data[6];
	write_data[0] = FRAMEPARS_GETFPGATIME;
	write_data[1] = 0;
	params->write(write_data, sizeof(unsigned long) * 2);
	struct timeval tv_1;
	tv_1.tv_sec = params->getGPValue(G_SECONDS);
	tv_1.tv_usec = params->getGPValue(G_MICROSECONDS);

	struct timeval tv_sys;
	gettimeofday(&tv_sys, NULL);

	params->write(write_data, sizeof(unsigned long) * 2);
	struct timeval tv_2;
	tv_2.tv_sec = params->getGPValue(G_SECONDS);
	tv_2.tv_usec = params->getGPValue(G_MICROSECONDS);

	unsigned long delta = tv_2.tv_sec - tv_1.tv_sec;
	delta *= 1000000;
	delta += tv_2.tv_usec;
	delta -= tv_1.tv_usec;
	delta /= 2;
	struct timeval tv_fpga;
	tv_fpga.tv_sec = tv_1.tv_sec;
	tv_fpga.tv_usec = tv_1.tv_usec + delta;
	tv_fpga.tv_sec += tv_fpga.tv_usec / 1000000;
	tv_fpga.tv_usec = tv_fpga.tv_usec % 1000000;

	bool fpga_gt = false;
	if(tv_fpga.tv_sec > tv_sys.tv_sec)
		fpga_gt = true;
	if (tv_fpga.tv_sec == tv_sys.tv_sec)
		if(tv_fpga.tv_usec > tv_sys.tv_usec)
			fpga_gt = true;
	struct timeval *tv_b = &tv_sys;
	struct timeval *tv_s = &tv_fpga;
	if(fpga_gt) {
		tv_b = &tv_fpga;
		tv_s = &tv_sys;
	}
	delta_fpga_sys = 0;
	delta_fpga_sys = tv_b->tv_sec - tv_s->tv_sec;
	delta_fpga_sys *= 1000000;
	delta_fpga_sys += tv_b->tv_usec;
	delta_fpga_sys -= tv_s->tv_usec;
	if(fpga_gt)
		delta_fpga_sys = -delta_fpga_sys;

//fprintf(stderr, "first time == %d:%06d; second time == %d:%06d\n", tv_1.tv_sec, tv_1.tv_usec, tv_2.tv_sec, tv_2.tv_usec);
//fprintf(stderr, "FPGA time == %d:%06d; system time == %d:%06d\n", tv_fpga.tv_sec, tv_fpga.tv_usec, tv_sys.tv_sec, tv_sys.tv_usec);
//fprintf(stderr, "times delta == %06d\n\n", delta_fpga_sys);

	RTP_Stream::Start(ip, port, ttl);
}

void Audio::Stop(void) {
	if(!_present)
		return;
//	cerr << "Audio ---> Stop !!!" << endl;
D(	cerr << "Audio ---> Stop !!!" << endl;)
	RTP_Stream::Stop();
	f_tv.tv_sec = 0;
	f_tv.tv_usec = 0;
}

long Audio::process(void) {
//	static timeval ts_old = {0, 0};
	long ret = 0;
	snd_pcm_status_t *status;
	snd_pcm_status_alloca(&status);
	snd_timestamp_t ts;
	unsigned long avail;
//bool first = true;

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
				f_tv.tv_sec -= delta_fpga_sys / 1000000;
				f_tv.tv_usec -= delta_fpga_sys % 1000000;
				f_tv.tv_sec += f_tv.tv_usec / 1000000;
				f_tv.tv_usec = f_tv.tv_usec % 1000000;
/*
				if(is_first) {
					struct timeval tv;
					gettimeofday(&tv, NULL);
					is_first = false;
					fprintf(stderr, "AUDIO first with time: %d:%06d at: %d:%06d\n", f_tv.tv_sec, f_tv.tv_usec, tv.tv_sec, tv.tv_usec);
					sec = f_tv.tv_sec;
				} else {
					if(sec != f_tv.tv_sec) {
						
						sec = f_tv.tv_sec;
					}
				}
*/
			}
/*
			snd_pcm_status(capture_handle, status);
			snd_pcm_status_get_tstamp(status, &ts);
			avail = (unsigned long)snd_pcm_status_get_avail(status);
			struct timeval tv_real;
			gettimeofday(&tv_real, NULL);
			struct timeval t_delta = {0, 0};
			if(ts_old.tv_sec != 0 && ts_old.tv_usec != 0) {
				long td = ts.tv_sec - ts_old.tv_sec;
				td *= 1000000;
				td += ts.tv_usec;
				td -= ts_old.tv_usec;
				t_delta.tv_sec = td / 1000000;
				t_delta.tv_usec = td % 1000000;
			}
fprintf(stderr, "status timestamp == %d:%06d at %d:%06d; delta == %d:%06d, slen == %d; avail == %d\n", ts.tv_sec, ts.tv_usec, tv_real.tv_sec, tv_real.tv_usec, t_delta.tv_sec, t_delta.tv_usec, slen, avail);
			ts_old = ts;
*/
			// --==--
			ret += slen;
			process_send(slen);
			// check again what buffer is not empty
			snd_pcm_status(capture_handle, status);
			avail = (unsigned long)snd_pcm_status_get_avail(status);
//c++;
			if(avail >= (unsigned long)sbuffer_len) {
//cerr << "process() again - available " << avail << " samples" << endl;
				continue;
			}
//if(c != 0)
//cerr << "process " << c << " times" << endl;
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
	static unsigned short packet_num = 0;
	unsigned short pnum;

	void *m = (void *)sbuffer;

	int i;
	long offset = 0;
#define LEN 1200
	static unsigned char *d = NULL;
	if(d == NULL) {
		d = (unsigned char *)malloc(LEN + 20);
	}
	int to_send = sample_len * 2 * _channels;
	int size = to_send;
	long count = 0;
	
	uint32_t ts;
	for(;;) {
		if(to_send == 0)
			break;
		if(to_send > LEN) {
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
		if(timestamp_rtcp > 0xFFFFFFFF)
			timestamp_rtcp &= 0xFFFFFFFF;
		//
		packet_num++;
		pnum = htons(packet_num);
		count += i;
		// fill RTP header
		d[0] = 0x80;
		if(count >= size)
			d[1] = _ptype + 0x80;
		else
			d[1] = _ptype;
		memcpy((void *)&d[2], (void *)&pnum, 2);
		memcpy((void *)&d[4], (void *)&ts, 4);
		memcpy((void *)&d[8], (void *)&SSRC, 4);
        // fill data
		memcpy((void *)&d[12], (void *)((char *)m + offset), i);
		offset += i;
		// send packet
		rtp_packets++;
		rtp_octets += i; // data + MJPEG header
		rtp_socket->send((void *)d, i + 12);
    }

	return sample_len;
}
