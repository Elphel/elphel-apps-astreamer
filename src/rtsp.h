/**
 * @file rtsp.h
 * @brief RTSP server implementation
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

#ifndef _RTSP_H_
#define _RTSP_H_

//#include "types.h"
#include "session.h"
#include "socket.h"
#include "parameters.h"

#include <map>
#include <list>
#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>

using namespace std;

class _Request {
public:
	_Request(const string &req);
	inline const string &get_method(void) {
		return method;
	}
	inline const string &get_uri(void) {
		return uri;
	}
	inline map<string, string> &get_fields(void) {
		return fields;
	}
protected:
	string method;
	string uri;
	map<string, string> fields;
};

class _Responce {
public:
	enum status {STATUS_EMPTY, STATUS_BUSY, STATUS_OK};
	_Responce():_status(STATUS_EMPTY) {};
	inline void set_status(status st) {
		_status = st;
	}
	void add_field(const string &name, const string &value);
	void add_include(const string &include);
	string serialize(void);
protected:
	status _status;
	map<string, string> fields;
	string include;
};

class RTSP_Server {
public:
	enum event {
		EMPTY,
		DESCRIBE,
		PLAY,
		PAUSE,
		TEARDOWN,
		PARAMS_WAS_CHANGED,
		RESET
//                IS_DAEMON_ENABLED
	};
	// if transport == NULL - wait for set_transport(), when client connected and ask DESCRIBE

	RTSP_Server(int (*h)(void *, RTSP_Server *, RTSP_Server::event), void *handler_data, Parameters *pars, Session *_session = NULL);
	~RTSP_Server();
	
	// deprecated
	void main(void);
protected:
	bool process(Socket *s);
	string make_sdp(string uri);
	string make_transport(string req);
	Session *session;
	// socket to accept requests
//	Socket *socket_main;

	int (*handler_f)(void *, RTSP_Server *, RTSP_Server::event);
	void *handler_data;
	int handler(RTSP_Server::event event) {
		return handler_f(handler_data, this, event);
	}
	// keep Sockets of clients to identify them and block second client in unicast mode
	list<Socket *> active_sockets;
	bool process_teardown(Socket *s);
	bool process_play(Socket *s, bool add = true); // if secons argument is false - just check what server not busy
//	void *_busy; // pointer to Socket of client, what use current unicast stream

	string part_of_request;
	// thread...
//	RTP *rtp;
	Socket *socket_main_1;
	Socket *socket_main_2;
//	Socket *socket_main_3;
	Parameters *params;
};

#endif // _RTSP_H_
