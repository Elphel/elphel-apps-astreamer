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
