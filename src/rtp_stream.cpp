/**
 * @file rtp_stream.cpp
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

#include <arpa/inet.h>
#include <iostream>

#include "rtp_stream.h"
#include "helper.h"

using namespace std;

#define CNAME "elphel393"

//#undef RTP_DEBUG
#define RTP_DEBUG

#ifdef RTP_DEBUG
	#define D(s_port, a) \
	do { \
		cerr << __FILE__ << ": " << __FUNCTION__ << ": " << __LINE__ << ": sensor port: " << s_port << " "; \
		a; \
	} while (0)
#else
	#define D(a)
#endif

RTP_Stream::RTP_Stream(void) {
	_play = false;
	pthread_mutex_init(&pthm_flow, NULL);
	stream_name = "unknown";
	rtp_socket = NULL;
	rtcp_socket = NULL;
	sem_init(&sem_play, 0, 0);
	pth_id = -1;
	packet_num = 0;
}

RTP_Stream::~RTP_Stream() {
//cerr << "RTP_Stream::~RTP_Stream() for stream " << stream_name << endl;
	if (pth_id >= 0)
		pthread_cancel(pth);
	if (rtp_socket != NULL)
		delete rtp_socket;
	if (rtcp_socket != NULL)
		delete rtcp_socket;
	sem_destroy(&sem_play);
	pthread_mutex_destroy(&pthm_flow);
}

void RTP_Stream::init_pthread(void *__this) {
	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	pth_id = pthread_create(&pth, &tattr, RTP_Stream::pthread_f, (void *)__this);
}

void *RTP_Stream::pthread_f(void *_this) {
	RTP_Stream *__this = (RTP_Stream *)_this;
	return __this->thread();
}

void RTP_Stream::Start(string ip, int port, int ttl) {
	D(sensor_port, cerr << " new " << stream_name << " UDP socket at port: " << port << endl);
	pthread_mutex_lock(&pthm_flow);
	if (!_play) {
		rtp_socket = new Socket(ip, port, Socket::TYPE_UDP, ttl);
		rtcp_socket = new Socket(ip, port + 1, Socket::TYPE_UDP, ttl);
		rtp_packets = 0;
		rtp_octets = 0;
		rtcp_tv.tv_sec = 0;
		rtcp_tv.tv_usec = 0;
//		rtcp_delay = 2500000;	// in usec
//		rtcp_delay = 1250000;	// in usec
		rtcp_delay = 2000000;	// in usec
		_play = true;
		/// unlock semaphore - 'play' event
		sem_post(&sem_play);
	}
	pthread_mutex_unlock(&pthm_flow);
}

void RTP_Stream::Stop(void) {
	D(sensor_port, cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ <<endl);
//cerr << "RTP_Stream::Stop() for stream " << stream_name << " - begin" << endl;
	pthread_mutex_lock(&pthm_flow);
	if (_play) {
//cerr << "RTP_Stream::Stop() for stream " << stream_name << " - in progress" << endl;
		/// reset semaphore
		sem_init(&sem_play, 0, 0);
		_play = false;
//		delete rtcp_socket;
		if (rtp_socket != NULL) {
			delete rtp_socket;
			rtp_socket = NULL;
		}
		if (rtcp_socket != NULL) {
			delete rtcp_socket;
			rtcp_socket = NULL;
		}
	}
	pthread_mutex_unlock(&pthm_flow);
//cerr << "RTP_Stream::Stop() for stream " << stream_name << " - end" << endl;
}

/**
 * @brief Thread that invokes the video (and audio too) frame acquisition/ transmission
 *        In the current implementation video process() is blocking, but may be made
 *        non-blocking again later (through poll()). In addition to bool result
 *        (false if no frames are available, there are now long process(), with
 *        additional <0 result for the video frame change/stream shut down detected
 *        in Video class.
 * @return never
 */
void *RTP_Stream::thread(void) {
	D(sensor_port, cerr << "RTP_Stream::thread(void)" << endl);
	for (;;) {
		pthread_mutex_lock(&pthm_flow);
		if (_play) {
			long f = process();
			if (f > 0)
				rtcp();
			// process() and rtcp() use sockets
			pthread_mutex_unlock(&pthm_flow);
			if (f < 0) {
				D(sensor_port, cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__<< "process exception detected: " << f << endl);
//				cerr << "Stop() from thread for stream " << stream_name << endl;
				Stop();
			}
		} else { /// wait for 'play' event semaphore
			pthread_mutex_unlock(&pthm_flow);
			sem_wait(&sem_play);
		}
	}
	return NULL;
}

void RTP_Stream::rtcp(void) {
	// check time for next one RTCP...
	if (f_tv.tv_sec == 0 && f_tv.tv_usec == 0)
		return;
	long td = time_delta_us(f_tv, rtcp_tv);
	if (td < 0) {
		rtcp_tv = f_tv;
		return;
	}
	if (td < rtcp_delay)
		return;
	rtcp_tv = f_tv;
	rtcp_send_sdes();
	rtcp_send_sr();
}

void RTP_Stream::rtcp_send_sr(void) {
	char packet[8 + 20]; // by RTP RFC 3550, for SR RTCP packet needed 8 + 20 bytes
	int packet_len = sizeof(packet);
	uint16_t us;
	uint32_t ul;

	// RTCP header
	packet[0] = 0x81;
	packet[1] = 200;	// SR
	us = htons(((packet_len) / 4) - 1);
	memcpy((void *) &packet[2], (void *) &us, 2);
	memcpy((void *) &packet[4], (void *) &SSRC, 4);
	// NTP timestamp is a fixed point 32.32 format time
	ul = htonl(f_tv.tv_sec);
	memcpy((void *) &packet[8], (void *) &ul, 4);
	double d = f_tv.tv_usec;
	d /= 1000000.0;
	d *= 65536.0;
	d *= 4096.0;
	uint32_t f = (uint32_t) d;
	if (f > 0x0FFFFFFF)
		f = 0x0FFFFFFF;
	f <<= 4;
	ul = htonl(f);
	memcpy((void *) &packet[12], (void *) &ul, 4);
	ul = htonl(timestamp);
	memcpy((void *) &packet[16], (void *) &ul, 4);
	ul = htonl(rtp_packets);
	memcpy((void *) &packet[20], (void *) &ul, 4);
	ul = htonl(rtp_octets);
	memcpy((void *) &packet[24], (void *) &ul, 4);
	rtcp_socket->send(packet, packet_len);
}

void RTP_Stream::rtcp_send_sdes(void) {
	char packet[8 + 4 + 128]; // by RTP RFC 3550, for SDES RTCP packet needed 8 + 4 + ... bytes, 
	// so get additional 128 bytes for 126 CNAME field
	int packet_len = 0;
	int padding = 0;

	uint16_t us;
	const char *cname = CNAME;
	int cname_len = strlen(cname);
	bzero((void *) packet, 140); //8+4+128

	// RTCP header
	packet[0] = 0x81;
	packet[1] = 202;
	memcpy((void *) &packet[4], (void *) &SSRC, 4);
	packet_len += 8;

	// SDES fields
	packet[8] = 0x01;
	memcpy((void *) &packet[10], (void *) cname, cname_len);
	packet_len += 2; // + cname_len;
	// calculate common length SDES
	padding = (cname_len + 2) % 4;
	if (padding)
		cname_len += (4 - padding);
	packet[9] = cname_len;
	// each chunk  MUST be terminated by one or more null octets(RFC3350)
	packet_len += (cname_len + 4);

	us = htons((packet_len / 4) - 1);
	memcpy((void *) &packet[2], (void *) &us, 2);

	rtcp_socket->send(packet, packet_len);
}
