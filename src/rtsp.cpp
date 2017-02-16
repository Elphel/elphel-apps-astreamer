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

#include "rtsp.h"
#include "helpers.h"
#include "parameters.h"

#include <string.h>
#include <vector>

using namespace std;

#undef RTSP_DEBUG
#undef RTSP_DEBUG_2
//#define RTSP_DEBUG
//#define RTSP_DEBUG_2

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

#define RTSP_SIGN	"rtsp://"
#define RTSP_PORT_1	554
#define RTSP_PORT_2	8554
//#define RTSP_PORT_3	7070

// TODO
_Request::_Request(const string &request) {
	string s;
	int prev = 0;
	int i = 0;
	int len = request.length();
	const char *c = request.c_str();
	bool first = true;
	string name, value;

	while(i < len) {
		s = "";
		for(; i < len && c[i] != '\n' && c[i] != '\r' && c[i] != '\0'; i++);
		if(c[i] == '\n' || c[i] == '\r') {
			if(i == prev) {
				i++;
				prev = i;
				continue;
			}
			s.insert(0, request, prev, i - prev);
//			cerr << "prev == " << prev << "; i == " << i << ";--> " << "\trequest string: \"" << s << "\"" << endl;
			if(first) {
				int x = s.find(" ");
				method.insert(0, s, 0, x);
				int y = s.find(" ", x + 1);
				uri.insert(0, s, x + 1, y - x - 1);
//				cerr  << "method == |" << method << "|; uri == |" << uri << "|" << endl;
				first = false;
			} else {
				name = "";
				value = "";
				int x = s.find(": ");
				name.insert(0, s, 0, x);
				value.insert(0, s, x + 2, s.length() - x - 1);
				fields[name] = value;
//				cerr << "pair: name == |" << name << "|; value == |" << value << "|" << endl;
			}
		}
		if(c[i] == '\0')
			break;
		i++;
		prev = i;
	}
}

void _Responce::add_field(const string &name, const string &value) {
	fields[name] = value;
}

void _Responce::add_include(const string &_include) {
	include = _include;
}

string _Responce::serialize() {
	string rez;
	switch(_status) {
	case STATUS_OK:
		rez = "RTSP/1.0 200 OK\r\n";
		break;
	case STATUS_BUSY:
		rez = "RTSP/1.0 455 Server is busy\r\n";
		break;
	case STATUS_EMPTY:
		rez = "";
		return rez;
	}
	for(map<string, string>::iterator it = fields.begin(); it != fields.end(); it++) {
		rez += (*it).first + ": " + (*it).second + "\r\n";
	}
	rez += "\r\n";
	if(include.length() != 0) {
		rez += include;
	}
	return rez;
}

RTSP_Server::RTSP_Server(int (*h)(void *, RTSP_Server *, RTSP_Server::event), void *handler_data, Session *_session) {
	socket_main_1 = NULL;
	socket_main_2 = NULL;
//	socket_main_3 = NULL;
	handler_f = h;
	this->handler_data = handler_data;
	session = _session;
//	_busy = NULL;
}

void RTSP_Server::main(void) {
	list<Socket *> s;
	// create listen socket...

	// once opened socket to listen can be closed and reopen again by Socket's implementation:
	// opened port assigned to process id and is keeped all time while process is alive
	// so, keep this socket
	if(socket_main_1 == NULL) {
		socket_main_1 = new Socket("", RTSP_PORT_1);
		socket_main_1->listen(2);
	}
	if(socket_main_2 == NULL) {
		socket_main_2 = new Socket("", RTSP_PORT_2);
		socket_main_2->listen(2);
	}
/*
	if(socket_main_3 == NULL) {
		socket_main_3 = new Socket("", RTSP_PORT_3);
		socket_main_3->listen(2);
	}
*/
/*
	if(socket_main == NULL) {
//		socket_main = new Socket("", 554);
//cerr << "create socket to listen port" << endl;
		socket_main = new Socket("", RTSP_PORT);
//		Socket *socket_1 = new Socket("", 554);
//		Socket *socket_2 = new Socket("", 8554);
//		Socket *socket_3 = new Socket("", 7070);
		socket_main->listen(2);
	} else {
//cerr << "main socket already exist" << endl;
	}
*/
	s.push_back(socket_main_1);
	s.push_back(socket_main_2);
//	s.push_back(socket_main_3);

D(static int count = 0;)
	bool to_poll = true;
//	Parameters *params = Parameters::instance();
	while(true) {
		if(to_poll) {
			int poll_rez = Socket::poll(s, 500);
D( {if(count < 5) {
	cerr << "poll..." << endl;
	count++;
	}
});
			// TODO here:
			// if client connected - check for changes of all possible parameters
			// if client not connected - check only about enable/disable bit to prevent overhead
			if(handler(PARAMS_WAS_CHANGED)) {
				// stop all sessions, and restart streamer
				handler(RESET);
				break;
			}
			if(poll_rez == 0) {
				/// poll was finished with timeout, not socket event
				to_poll = true;
				continue;
			}
		}
		to_poll = true;
		for(list<Socket *>::iterator it = s.begin(); it != s.end(); it++) {
			Socket::state state = (*it)->state_refresh();
			if(state == Socket::STATE_IN || state == Socket::STATE_DISCONNECT) {
D2(cerr << endl << "something happen on the socket!" << endl;)
				if(*it == socket_main_1 || *it == socket_main_2) {// || *it == socket_main_3) {
//					Socket *in = socket_main->accept();
					Socket *in = (*it)->accept();
					if(in) {
						in->set_so_keepalive(1);
D2(cerr << "processed - s.push_back(in)" << endl;)
D2(fprintf(stderr, "added : 0x%08X\n", in);)
						s.push_back(in);
					}
				} else {
					// check for remove closed socket !
D2(cerr << "was with non-main socket" << endl;)
					if(!process(*it)) {
D2(cerr << "process failed - remove it!" << endl;)
D2(fprintf(stderr, "delete: 0x%08X\n", *it);)
						delete *it;
						s.remove(*it);
						// check about counters etc...
						to_poll = false;
						break;
					}
				}
			}
		}
	}
	// stop all - by TEARDOWN command
	for(list<Socket *>::iterator it = s.begin(); it != s.end(); it++) {
		// keep the 'main' i.e. server socket
		if(*it != socket_main_1 && *it != socket_main_2) {// && *it != socket_main_3) {
D2(fprintf(stderr, "delete: 0x%08X\n", *it);)
			delete *it;
		}
	}
	// reset sockets IDs and state
//	_busy = NULL;
	active_sockets.erase(active_sockets.begin(), active_sockets.end());
}

bool RTSP_Server::process_teardown(Socket *s) {
//			if(!session->rtp_out.multicast)
//				_busy = s;
	for(list<Socket *>::iterator it = active_sockets.begin(); it != active_sockets.end(); it++)
		if(*it == s) {
//			if(*it != socket_main)
//				delete *it;
			active_sockets.erase(it);
			return true;
		}
	return false;
}

bool RTSP_Server::process_play(Socket *s, bool add) {
	// discard unicast client if client already exist
	if(!session->rtp_out.multicast)
		if(active_sockets.size() != 0)
			return false;
	if(add)
		active_sockets.push_back(s);
	return true;
}

bool RTSP_Server::process(Socket *s) {
	string req;
	_Request *request;
	_Responce *responce = NULL;
	bool r = true;
	_Responce::status status = _Responce::STATUS_EMPTY;

D(cerr << "RTSP_Server::process()" << endl;)
	bool to_teardown = false;
	// recv failed - socket error - should close socket and check teardown
	if(!s->recv(req))
		to_teardown = true;
	// empty request mean connection closed (behavior of Socket)
	if(req == "")
		to_teardown = true;
	// decrement active clients, and stop stream if no clients
	if(to_teardown) {
		if(process_teardown(s))
			handler(TEARDOWN);
		return false;
	}
D(cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << " part_REQUEST: " << endl << "\"" << req << "\"" << endl;)
	// check for partial request...
	part_of_request += req;
	req = part_of_request;
	const char *c = req.c_str();
	string end;
	int e = req.length();
	e -= 4;
	if(e <= 0)
		e = 0;
	for(int i = req.length() - 1; i >= e; i--) {
		end += c[i];
	}
	if(end.find("\n\n") != 0 && end.find("\n\r\n\r") != 0) {
		return true;
	} else {
		part_of_request = "";
	}
	// process...
	request = new _Request(req);

	Parameters *params = Parameters::instance();
	responce = new _Responce();
//	responce->add_field("CSeq", (*request->get_fields().find("CSeq")).second);
	responce->add_field("CSeq", (request->get_fields())["CSeq"]);
	if(request->get_method() == "OPTIONS") {
		if(params->daemon_enabled()) {
			status = _Responce::STATUS_OK;
			responce->add_field("Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE");
			// check transport...
			if(!session->rtp_out.multicast)
				session->rtp_out.ip = s->source_ip();
		} else {
			status = _Responce::STATUS_BUSY;
		}
	}
	if(request->get_method() == "DESCRIBE") {
		if(params->daemon_enabled() && process_play(s, false)) {
			handler(DESCRIBE); /// Streamer should update session data (width, height, fps)
			status = _Responce::STATUS_OK;
			char buf[8];
			string sdp = make_sdp(request->get_uri());
			responce->add_field("Content-Type", "application/sdp");
			sprintf(buf, "%d", (int)sdp.length());
			responce->add_field("Content-Length", buf);
			responce->add_include(sdp);
		} else {
			status = _Responce::STATUS_BUSY;
		}
	}
	if(request->get_method() == "SETUP") {
		if(params->daemon_enabled() && process_play(s, false)) {
			status = _Responce::STATUS_OK;
			// TODO: random session number
			responce->add_field("Session", "47112344");
//			responce->add_field("Transport", make_transport((*request->get_fields().find("Transport")).second));
			responce->add_field("Transport", make_transport(request->get_fields()["Transport"]));
		} else {
			status = _Responce::STATUS_BUSY;
		}
	}
	if(request->get_method() == "TEARDOWN") {
		status = _Responce::STATUS_OK;
		process_teardown(s);
		handler(TEARDOWN);
	}
	if(request->get_method() == "PLAY") {
		if(params->daemon_enabled() && (process_play(s))) {
			status = _Responce::STATUS_OK;
			handler(PLAY);
		} else {
			status = _Responce::STATUS_BUSY;
		}
	}
	if(request->get_method() == "PAUSE") {
		status = _Responce::STATUS_OK;
		handler(PAUSE);
	}
	delete request;
	responce->set_status(status);
D(	cerr << "\tRESPONSE: " << endl << "\"" << responce->serialize() << "\"" << endl;)
	r = s->send(responce->serialize());
	delete responce;
	return r;
}

string RTSP_Server::make_sdp(string uri) {
	char buf[256];

D(cerr << "make SDP" << endl;)
	// make video description
	// m = - port + PAYLOAD
	string rez;
///*
	rez += "m=video ";
	sprintf(buf, "%d", session->rtp_out.port_video);
	if(session->process_audio == false) {
		if(session->rtp_out.multicast) {
			rez += buf;
		} else {
			rez += "0";
		}
	} else {
		rez += buf;
	}
	rez += " RTP/AVP ";
	sprintf(buf, "%d", session->video.type);
	rez += buf;
	rez += "\r\n";	// port
	if(session->rtp_out.multicast) {
		rez += "a=type:multicast\r\n";
		rez += "c=IN IP4 " + session->rtp_out.ip + "/" + session->rtp_out.ttl + "\r\n";	// IP + TTL
	} else {
		rez += "a=type:unicast\r\n";
		rez += "c=IN IP4 0.0.0.0\r\n";	// IP + TTL
	}
	sprintf(buf, "%.4f", session->video.fps);
//	rez += "a=framerate:";
//	rez += buf;
	rez += "\r\na=x-framerate:";
	rez += buf;
//cerr << "x-framerate == " << buf << endl;
	if(session->video.width > 0) {
		sprintf(buf, "%d", session->video.width);
//		rez += "\r\na=width:";
//		rez += buf;
		rez += "\r\na=x-width:";
		rez += buf;
	}
	if(session->video.height > 0) {
		sprintf(buf, "%d", session->video.height);
//		rez += "\r\na=height:";
//		rez += buf;
		rez += "\r\na=x-height:";
		rez += buf;
	}
	if(session->video.width > 0 && session->video.height > 0) {
		sprintf(buf, "%d,%d", session->video.width, session->video.height);
		rez += "\r\na=x-dimensions:";
		rez += buf;
	}
	rez += "\r\n";
//*/
	// make audio description, if present
	if(session->process_audio) {
//		sprintf(buf, "m=audio %d RTP/AVP %d\r\n", atoi(session->rtp_out.port.c_str()) + 2, session->audio.type);
		sprintf(buf, "m=audio %d RTP/AVP %d\r\n", session->rtp_out.port_audio, session->audio.type);
		rez += buf;
		sprintf(buf, "a=rtpmap:%d L16/%d/%d\r\n", session->audio.type, session->audio.sample_rate, session->audio.channels);
//cerr << "SDP: " << buf << endl;
		rez += buf;
	}
/*
	if(session->rtp_out.multicast) {
		rez += "a=type:multicast\r\n";
		rez += "c=IN IP4 " + session->rtp_out.ip + "/" + session->rtp_out.ttl + "\r\n";	// IP + TTL
	} else {
		rez += "a=type:unicast\r\n";
		rez += "c=IN IP4 0.0.0.0\r\n";	// IP + TTL
	}
*/
	rez += "a=control:" + uri + "\r\n";
D(cerr << "make SDP - ok!" << endl;)
	return rez;
}

string RTSP_Server::make_transport(string req) {
	if(req == "")
		return "";
	map<string, string> m = String::split_list_to_map(String::split_to_list(req, ';'), '=');
	string client_port = m["client_port"];
	int port_req = session->rtp_out.port_video;
//D(cerr << "client_port == " << client_port << endl;)
	if(client_port != "") {
		string first, second;
		String::split(client_port, '-', first, second);
		port_req = atol(first.c_str());
//D(cerr << "client_port == |" << client_port << "|; first == |" << first << "|; second == |" << "|" << endl;)
//		session->rtp_out.port = first;
	}

	if(!session->rtp_out.multicast) {
		if(!session->process_audio) {
			session->rtp_out.port_video = port_req;
		}
	}

	string rez = "RTP/AVP;";

	char buf[128];
	sprintf(buf, "%d", port_req);

	if(session->rtp_out.multicast)
		rez += "multicast;destination=" + session->rtp_out.ip + ";port=" + buf;
    else
		rez += "unicast;destination=" + session->rtp_out.ip + ";server_port=" + buf;

    if(session->process_audio) {
		sprintf(buf, "-%d", port_req + 1);
//		rez += "-";
//		long p = atol(session->rtp_out.port.c_str());
//		p++;
//		sprintf(buf, "%d", p);
		rez += buf;
	}
	if(session->rtp_out.multicast)
		rez += ";ttl=" + session->rtp_out.ttl;
//	rez += "destination=" + transport.ip + ";port=" + transport.port;
//	if(transport.multicast)
//		rez += ";ttl=" + transport.ttl;
	return rez;

}

RTSP_Server::~RTSP_Server() {
//	cerr << "destroy RTSP_Server object" << endl;
//	delete socket_main;
//	if (request)  delete request;
//	if (responce) delete responce;
}

