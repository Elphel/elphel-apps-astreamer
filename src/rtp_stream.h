#ifndef __H_RTP_STREAM__
#define __H_RTP_STREAM__

#include <sys/time.h>
#include <string>
#include <pthread.h>
#include <semaphore.h>
#include "socket.h"
#include <stdint.h>

using namespace std;

extern int fd_stream;

class RTP_Stream {
public:
	RTP_Stream(void);
	virtual ~RTP_Stream();

	int ptype(void) { return _ptype; };
	virtual void Start(string ip, int port, int ttl = -1);
	virtual void Stop(void);
protected:
	void init_pthread(void *__this);
	pthread_t pth;
	int pth_id;
	static void *pthread_f(void *_this);
	void *thread(void);
	pthread_mutex_t pthm_flow;
//	virtual bool process(void) = 0;
	virtual long process(void) = 0;

	int _ptype;
	bool _play;
	/// semaphore to wait 'play' event
	sem_t sem_play;
	Socket *rtp_socket;
	Socket *rtcp_socket;

	unsigned short packet_num;
	unsigned long SSRC;

	struct timeval f_tv;
	struct timeval rtcp_tv; // time of last SR
	long rtcp_delay;
	unsigned long timestamp;

	unsigned long rtp_packets;
	unsigned long rtp_octets;

	void rtcp(void);
	void rtcp_send_sr(void);
	void rtcp_send_sdes(void);

	string stream_name;
};

#endif //__H_RTP_STREAM__
