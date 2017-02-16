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

#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/poll.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>

using namespace std;

#include "socket.h"
#include "helpers.h"

#undef RTSP_DEBUG
#undef RTSP_DEBUG_2

#ifdef RTSP_DEBUG
	#define D(a) a
#else
	#define D(a)
#endif

#ifdef RTSP_DEBUG_2
	#define D2(a) a
#else
	#define D2(a)
#endif

#define BUF_SIZE	2048

TCP_Client::TCP_Client(bool enable_exception) {
	ex = enable_exception;
	fd = -1;
}

TCP_Client::~TCP_Client() {
	disconnect();
}

void TCP_Client::connect(string ip, string port) {
	if((fd = ::socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		cerr << "fail socket()" << endl;
		throw(false);
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(::atol(port.c_str()));
	addr.sin_addr.s_addr = ::inet_addr(ip.c_str());

	if(::connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1) {
		cerr << "fail connect() to " << ip << ":" << port << endl;
		throw(false);
	}
	// TODO: we must use size as MTU for interface... and also - checking for not single frames...
	buf = (char *)malloc(1500);
}

void TCP_Client::disconnect(void) {
	if(fd > 0) {
		::close(fd);
		free(buf);
		buf = NULL;
	}
	fd = -1;
}

void TCP_Client::send(const string *data, int opt) {
	if(::send(fd, data->c_str(), data->length(), opt) < 0) {
		cerr << "fail to send(const string *)" << endl;
		throw(false);
	}
}

void TCP_Client::send(const string data, int opt) {
	int er;
	if(::send(fd, data.c_str(), data.length(), opt) < 0) {
		er = errno;
		cerr << "fail to send(const string): " << strerror(er) << endl;
		throw(false);
	}
}

void TCP_Client::send(void *data, int len, int opt) {
	if(::send(fd, data, len, opt) < 0) {
		cerr << "fail to send(void *, len)" << endl;
		throw(false);
	}
}

void TCP_Client::recv(string &r, int opt) {
	int l, er;
	if((l = ::recv(fd, buf, BUF_SIZE - 1, opt)) > 0) {
		buf[l] = '\0';
		r = buf;
	} else {
		er = errno;
		cerr << "fail to recv(string &): " << strerror(er) << endl;
		throw (false);
	}
}

/*
 * Socket class...
 */

unsigned long Socket::ip_to_number(string ip) {
	string left;
	string right;
	unsigned long rez = 0;
	while(String::split(ip, '.', left, right)) {
		rez += atol(left.c_str());
		rez <<= 8;
		ip = right;
	}
	return rez;
}

bool Socket::_is_multicast(string ip) {
	bool rez = true;
	unsigned long a_min = ip_to_number("224.0.0.0");
	unsigned long a_max = ip_to_number("239.255.255.255");
	if(a_min > a_max) {
		unsigned long a = a_min;
		a_min = a_max;
		a_max = a;
	}
	unsigned long a_ip = ip_to_number(ip.c_str());
	if(a_ip > a_max)
		rez = false;
	if(a_ip < a_min)
		rez = false;
	return rez;
}

struct in_addr Socket::mcast_from_local(void) {
	int _fd;
	int reuse = 1;
	_fd = socket(AF_INET, SOCK_DGRAM, 0);
	setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse));
	struct ifreq ifr;
	struct sockaddr_in *sin;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");
	struct ifconf ifc;
	ifc.ifc_len = 1;
	ifc.ifc_req = &ifr;
	ioctl(_fd, SIOCGIFADDR, &ifr);
	close(_fd);
	sin = (struct sockaddr_in *)(ifr.ifr_addr.sa_data - 2);
	sin->sin_addr.s_addr &= (0xFFFFFF0F);
	sin->sin_addr.s_addr |= (0x000000E0);
	return sin->sin_addr;
}

/// use ttl parameter only with UDP sockets
Socket::Socket(string _ip, int _port, stype _type, int ttl) {
	ip = _ip;
	port = _port;
	is_multicast = false;

D(cerr << "new socket..." << endl;)
	_state = STATE_EMPTY;

	int t = 0;
	type = _type;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = PF_INET;
	addr.sin_port = htons(port);

	switch(type) {
	case TYPE_TCP: {
		fd = socket(PF_INET, SOCK_STREAM, 0);
		int flag = 1;
		setsockopt(fd, IPPROTO_IP, SO_REUSEADDR, &flag, sizeof(flag));
		if(ip != "")
			addr.sin_addr.s_addr = inet_addr(ip.c_str());
		else
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if(ttl>0)	
			setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
		t = bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr));
D(		int er = errno;)
D(		cerr << "TCP ::bind() == " << t; if(t != 0) cerr << "; errno == " << strerror(er); cerr << endl;)
		break;
	}
	case TYPE_UDP: {
		fd = socket(PF_INET, SOCK_DGRAM, 0);
		struct sockaddr_in saddr;
		memset(&saddr, 0, sizeof(struct sockaddr_in));
		saddr.sin_family = PF_INET;
		saddr.sin_port = htons(0);
		saddr.sin_addr.s_addr = htonl(INADDR_ANY);
		::bind(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
		addr.sin_addr.s_addr = inet_addr(ip.c_str());
		t = ::connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr));
D(		cerr << "::connect() == " << t << endl;)
		if((is_multicast = _is_multicast(ip)) == true) {
			struct ip_mreqn multiaddr;
			multiaddr.imr_multiaddr.s_addr = inet_addr(ip.c_str());
			multiaddr.imr_address.s_addr = htonl(INADDR_ANY);
			multiaddr.imr_ifindex = 0;
			setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multiaddr, sizeof(struct sockaddr_in));
		}
		// check TTL
		if(ttl > 0) {
//cerr << "try to set TTL to value == " << ttl << endl;
			setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
		}
		break;
	}
	default:
		break;
	}
}

Socket::Socket(int _fd, string _ip, int _port, stype _t) {
	_state = STATE_EMPTY;

	fd =_fd;
	ip = _ip;
	port = _port;
	type = _t;
D(	cerr << "socket: ip == " << ip << "; port == " << port << endl;)
}

Socket::~Socket() {
	close(fd);
}

int Socket::poll(list<Socket *> &s, int timeout) {
	struct pollfd *pfd;
	ssize_t s_size = s.size();

D2(cerr << "Socket::poll()..." << endl;)
	pfd = (struct pollfd *)malloc(sizeof(struct pollfd) * s_size);
	memset(pfd, 0, sizeof(struct pollfd) * s_size);
	int i = 0;
D2(cerr << "socket.fd == ";)
	for(list<Socket *>::iterator it = s.begin(); it != s.end(); it++, i++) {
		pfd[i].fd = (*it)->fd;
D2(cerr << pfd[i].fd << "; ";)
//		pfd[i].events = 0xFFFF;
		pfd[i].events = POLLIN;
		pfd[i].revents = 0x00;
	}
D2(cerr << endl;)
	int p = ::poll(pfd, s_size, timeout);
	i = 0;
	for(list<Socket *>::iterator it = s.begin(); it != s.end(); it++, i++) {
		(*it)->_state = STATE_EMPTY;
D2(cerr << "revents == " << pfd[i].revents << "; POLLIN == " << POLLIN << endl;)
		if(pfd[i].revents & POLLIN) {
			(*it)->_state = STATE_IN;
D2(cerr << "STATE_IN; fd == " << (*it)->fd << "; revents == " << pfd[i].revents << endl;)
		} 
		if(pfd[i].revents & POLLHUP) {
//		if(pfd[i].revents & POLLHUP || pfd[i].revents & POLLERR) {
			(*it)->_state = STATE_DISCONNECT;
//D2(cerr << "STATE_DISCONNECT; fd == " << (*it)->fd << "; revents == " << pfd[i].revents << endl;)
cerr << "STATE_DISCONNECT; fd == " << (*it)->fd << "; revents == " << pfd[i].revents << endl;
		} 
	}
	free((void *)pfd);
	return p;
}

void Socket::listen(int in) {
	long sock_flags;

	sock_flags = fcntl(fd, F_GETFL);
	sock_flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, sock_flags);
	int l = ::listen(fd, in);
D(cerr << "listen() == " << l << endl;)
	l++;
}

Socket *Socket::accept(void) {
	int _fd;
	struct sockaddr_in addr;
	socklen_t addr_len = 0;
	long sock_flags;
	Socket *s;

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr_len = sizeof(struct sockaddr_in);
	_fd = ::accept(fd, (struct sockaddr *)&addr, &addr_len);
	if(_fd < 0)
		return NULL;
	sock_flags = fcntl(_fd, F_GETFL);
	sock_flags |= O_NONBLOCK;
	fcntl(_fd, F_SETFL, sock_flags);
D(cerr << "accept == " << _fd << "; addr_len == " << addr_len << endl;)
	s = new Socket(_fd, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), type);
	return s;
}

void Socket::set_so_keepalive(int val) {
	int keepalive = val;
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

bool Socket::recv(string &rez) {
	rez = "???";
	char buf[1501];
	int l = 0;
	buf[0] = '\0';

	if((l = ::recv(fd, buf, 1500, MSG_NOSIGNAL | MSG_DONTWAIT)) > 0) {
		buf[l] = '\0';
		rez = buf;
D(cerr << "read - ok!"<< endl;)
		return true;
	} else {
		if(l == 0) {
			rez = "";
			return true;
		}
	}
	rez = "";
	return false;
}

bool Socket::send(void *data, int len) {
	return (::send(fd, data, len, MSG_NOSIGNAL | MSG_DONTWAIT) > 0);
}

bool Socket::send(const string *data) {
	if(::send(fd, data->c_str(), data->length(), 0) > 0)
		return true;
	return false;
}

bool Socket::send(const string data) {
	if(::send(fd, data.c_str(), data.length(), MSG_NOSIGNAL | MSG_DONTWAIT) > 0)
		return true;
	return false;
}

uint16_t in_chksum(const uint16_t *addr, register uint32_t len) {
	int32_t nleft = len;
	const uint16_t *w = addr;
	uint16_t answer;
	int32_t sum = 0;

	while(nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}
	if(nleft == 1)
		sum += htons(*(uint8_t  *)w << 8);
	sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
	sum += (sum >> 16);                     /* add carry */
	answer = ~sum;                          /* truncate to 16 bits */
	return answer;
}

bool Socket::send2v(void **v_ptr, int *v_len) {
	struct iovec iov[2];
	int iovcnt = sizeof(iov) / sizeof(struct iovec);
	iov[0].iov_base = v_ptr[0];
	iov[0].iov_len = v_len[0];
	iov[1].iov_base = v_ptr[1];
	iov[1].iov_len = v_len[1];
	if(::writev(fd, iov, iovcnt))
		return true;
	return false;
}

bool Socket::send3v(void **v_ptr, int *v_len) {
	struct iovec iov[3];
	int iovcnt = sizeof(iov) / sizeof(struct iovec);
	iov[0].iov_base = v_ptr[0];
	iov[0].iov_len = v_len[0];
	iov[1].iov_base = v_ptr[1];
	iov[1].iov_len = v_len[1];
	iov[2].iov_base = v_ptr[2];
	iov[2].iov_len = v_len[2];
	if(::writev(fd, iov, iovcnt))
		return true;
	return false;
}
