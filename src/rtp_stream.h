/**
 * @file rtp_stream.h
 * @brief Base class for RTP streams
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

#ifndef __H_RTP_STREAM__
#define __H_RTP_STREAM__

#include <sys/time.h>
#include <cstring>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

#include "socket.h"
#include "parameters.h"

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
	Parameters *params;
	int sensor_port;
};

#endif //__H_RTP_STREAM__
