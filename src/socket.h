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

#ifndef _SOCKETS_H_
#define _SOCKETS_H_

#include <string>
#include <list>
#include <sys/socket.h>
// Elphel, Rocko: socket.h does not include uio.h anymore
#include <sys/uio.h>

using namespace std;

class TCP_Client {
public:
	TCP_Client(bool enable_exception);
	~TCP_Client();
	void connect(string ip, string port);
	void disconnect(void);
	void send(const string *data, int opt = 0);
	void send(const string data, int opt = 0);
	void send(void *data, int len, int opt = 0);
	void recv(string &r, int opt = 0);
protected:
	bool ex;
	int fd;
	char *buf;
};

class Socket {
public:
	enum stype {TYPE_TCP, TYPE_UDP};
	enum state {STATE_EMPTY, STATE_IN, STATE_DISCONNECT};

	Socket(string ip, int port, stype _t = TYPE_TCP, int ttl = 64);
	~Socket();

	bool recv(string &rez);
	bool send(void *data, int len);
	bool send(const string *data);
	bool send(const string data);
	bool send2v(void **v_ptr, int *v_len);
	bool send3v(void **v_ptr, int *v_len);
	bool send_vect(const struct iovec *iov, int num);

	static int poll(list<Socket *> &s, int timeout = -1);
	static int pollout(list<Socket *> &s, int timeout = -1);
	void listen(int in);
	Socket *accept(void);
//	bool connect(void);

	void set_so_keepalive(int val = 1);
	inline state state_refresh(void) {
		state st = _state;
		_state = STATE_EMPTY;
		return st;
	}
	int get_fd(void) {	return fd;};
	int fd;
	string source_ip(void) {
		return ip;
	}
	static struct in_addr mcast_from_local(void);
protected:
	Socket(int _fd, string _ip, int _port, stype _t);

	string ip;
	int port;
	stype type;
//	int fd;
	state _state;

	pthread_mutex_t pthm_sock;

	int ttl;
	unsigned short ip_id;
	bool _is_multicast(string ip);
	unsigned long ip_to_number(string ip);
	bool is_multicast;
/*
	struct sockaddr_ll s_ll;
	struct msghdr msg;
	struct udphdr udp_hdr;
	struct iphdr ip_hdr;
*/
};

#endif
