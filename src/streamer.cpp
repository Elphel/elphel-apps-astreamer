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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "streamer.h"
#include "helpers.h"
#include "socket.h"

#include <iostream>

using namespace std;

#undef RTSP_DEBUG
//#define RTSP_DEBUG
#undef RTSP_DEBUG_2
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

Streamer *Streamer::_streamer = NULL;

Streamer::Streamer(const map<string, string> &_args) {
	if(_streamer != NULL)
		throw("can't make an another instance of streamer");
	else
		_streamer = this;
	session = new Session();
	args = _args;
	audio = NULL;
	session->process_audio = false;
	session->audio.sample_rate = 0;
	session->audio.channels = 0;
	session->rtp_out.ip_custom = false;
	session->rtp_out.ip_cached = 0;
	session->video.fps_scale = 1;
	audio_init();
	video = new Video();
	if(opt_present("f")) {
		float fps = 0;
		fps = atof(args["f"].c_str());
		if(fps < 0.1)
			fps = 0;
D(		cout << "use fps: " << fps << endl;)
		video->fps(fps);
	}
	rtsp_server = NULL;
	connected_count = 0;
}

void Streamer::audio_init(void) {
	if(audio != NULL) {
D(		cerr << "delete audio" << endl;)
		delete audio;
	}
D(	cout << "audio_enabled == " << session->process_audio << endl;)
	audio = new Audio(session->process_audio, session->audio.sample_rate, session->audio.channels);
	if(audio->present() && session->process_audio) {
		session->process_audio = true;
		session->audio.type = audio->ptype();
		session->audio.sample_rate = audio->sample_rate();
		session->audio.channels = audio->channels();
	} else {
		session->process_audio = false;
		session->audio.type = -1;
		session->audio.sample_rate = 0;
		session->audio.channels = 0;
	}
}

Streamer::~Streamer(void) {
	delete video;
	delete audio;
}

int Streamer::f_handler(void *ptr, RTSP_Server *rtsp_server, RTSP_Server::event event) {
	Streamer *__this = (Streamer *)ptr;
	return __this->handler(rtsp_server, event);
}

int Streamer::update_settings(bool apply) {
D(		cerr << "update_settings" << endl;)

	// check settings, normalize its, return 1 if was changed
	// update settings at application if apply = 1 and parameters change isn't on-fly safe, update parameters always
#define _CAN_BE_CHANGED	11
	unsigned long changes_array[2 * (_CAN_BE_CHANGED + 1)];
	int changes_array_i = 2;
	bool params_update = false;

	// update application settings
//	if(connected_count == 0) {

	// don't change "on the fly" if someone already connected - like mcast clients
	Parameters *params = Parameters::instance();
	// multicast parameters
	// - multicast ip
	// new values
	bool result = false;
	//----------------
	// frame skip, or FPS scale
	int frames_skip = params->getGPValue(P_STROP_FRAMES_SKIP);
	if(frames_skip < 0 || frames_skip > 0xFFFF) {
		if(frames_skip < 0)
			frames_skip = 0;
		if(frames_skip < 0xFFFF)
			frames_skip = 0xFFFF;
		changes_array[changes_array_i + 0] = P_STROP_FRAMES_SKIP;
		changes_array[changes_array_i + 1] = (unsigned long)frames_skip;
		changes_array_i += 2;
		params_update = true;
	}
	frames_skip += 1; // convert to fps_scale format;
	if(frames_skip != session->video.fps_scale) {
		if(apply)
			session->video.fps_scale = frames_skip;
//cerr << "session->video.fps_scale = " << session->video.fps_scale << endl;
		result = true;
	}	
	//----------------
	// transport parameters
	bool transport_was_changed = false;
	bool param_multicast = params->getGPValue(P_STROP_MCAST_EN);
	if(param_multicast || session->rtp_out.multicast) {
		// multicast/unicast
		if(param_multicast != session->rtp_out.multicast) {
			if(apply)
				session->rtp_out.multicast = param_multicast;
			transport_was_changed = true;
		}
		// IP
		unsigned long ip = params->getGPValue(P_STROP_MCAST_IP);
		bool ip_was_changed = false;
		// switch custom/default IP
		if((ip == 0) && session->rtp_out.ip_custom)
			ip_was_changed = true;
		if((ip != 0) && !session->rtp_out.ip_custom)
			ip_was_changed = true;
		// change of custom IP
		if((ip != 0) && session->rtp_out.ip_custom)
			if(ip != session->rtp_out.ip_cached)
				ip_was_changed = true;
		if(ip_was_changed) {
			if(ip != 0) {
				struct in_addr a;
				uint32_t a_min = ntohl(inet_addr("224.0.0.0"));
				uint32_t a_max = ntohl(inet_addr("239.255.255.255"));
				if(a_min > a_max) {
					uint32_t a = a_min;
					a_min = a_max;
					a_max = a;
				}
				if(ip < a_min)
					ip = a_min;
				if(ip > a_max)
					ip = a_max;
				a.s_addr = htonl(ip);
D(				cerr << "multicast ip asked: " << inet_ntoa(a) << endl;)
				if(apply) {
					session->rtp_out.ip_cached = ip;
					session->rtp_out.ip_custom = true;
					session->rtp_out.ip = inet_ntoa(a);
					changes_array[changes_array_i + 0] = P_STROP_MCAST_IP;
					changes_array[changes_array_i + 1] = ip;
					changes_array_i += 2;
				}
			} else {
				struct in_addr a = Socket::mcast_from_local();
D(				cerr << "multicast ip generated: " << inet_ntoa(a) << endl;)
				if(apply) {
					session->rtp_out.ip_custom = false;
					session->rtp_out.ip = inet_ntoa(a);
				}
			}
			transport_was_changed = true;
		}
D(		if(apply))
D(			cerr << "actual multicast IP: " << session->rtp_out.ip << endl;)
		// port
		int port = params->getGPValue(P_STROP_MCAST_PORT);
		if(port != session->rtp_out.port_video) {
			if(port < 1024)
				port = 1024;
			if(port > 65532)
				port = 65532;
			if(apply) {
				session->rtp_out.port_video = port;
				session->rtp_out.port_audio = port + 2;
				changes_array[changes_array_i + 0] = P_STROP_MCAST_PORT;
				changes_array[changes_array_i + 1] = session->rtp_out.port_video;
				changes_array_i += 2;
			}
			transport_was_changed = true;
		}
		// ttl
		int ttl = params->getGPValue(P_STROP_MCAST_TTL);
		if(ttl != atoi(session->rtp_out.ttl.c_str())) {
			if(ttl < 1)
				ttl = 1;
			if(ttl > 15)
				ttl = 15;
			if(apply) {
				char buf[8];
				sprintf(buf, "%d", ttl);
				session->rtp_out.ttl = buf;
				changes_array[changes_array_i + 0] = P_STROP_MCAST_TTL;
				changes_array[changes_array_i + 1] = ttl;
				changes_array_i += 2;
			}
			transport_was_changed = true;
		}
	}
	if(transport_was_changed)
		params_update = true;

	//-----------------
	// audio parameters
	bool audio_was_changed = false;
	bool audio_proc = true;
	bool f_audio_rate = false;
	bool f_audio_channels = false;
	// - enabled/disabled
	if(params->getGPValue(P_STROP_AUDIO_EN) == 0)
		audio_proc = false;
	int audio_rate = params->getGPValue(P_STROP_AUDIO_RATE);
	int audio_channels = params->getGPValue(P_STROP_AUDIO_CHANNEL);

	if(audio_proc != session->process_audio)
		audio_was_changed = true;
	if(audio_rate != session->audio.sample_rate)
		f_audio_rate = true;
	if(audio_channels != session->audio.channels)
		f_audio_channels = true;
	if((audio_proc || session->process_audio) && (f_audio_rate || f_audio_channels))
		audio_was_changed = true;
	if(apply) {
		bool audio_restarted = false;
		if(audio_was_changed) {
			session->process_audio = audio_proc;
			session->audio.sample_rate = audio_rate;
			session->audio.channels = audio_channels;
D2(			cerr << "Audio was changed. Should restart it" << endl;)
			audio_init();
			audio_restarted = true;
			// if audio enable was asked, check what soundcard really is connected
			if(audio_proc) {
				if(!audio->present()) {
					session->process_audio = false;
					changes_array[changes_array_i + 0] = P_STROP_AUDIO_EN;
					changes_array[changes_array_i + 1] = 0;
					changes_array_i += 2;
				}
			}
			if(f_audio_rate) {
				changes_array[changes_array_i + 0] = P_STROP_AUDIO_RATE;
				changes_array[changes_array_i + 1] = session->audio.sample_rate;
				changes_array_i += 2;
			}
			if(f_audio_channels) {
				changes_array[changes_array_i + 0] = P_STROP_AUDIO_CHANNEL;
				changes_array[changes_array_i + 1] = session->audio.channels;
				changes_array_i += 2;
			}
		}
		// was started before new client - must reinit audio
		if(!audio_restarted && session->process_audio)
			audio_init();
	}
	result = result || audio_was_changed || transport_was_changed;

	// apply volume if audio is enabled, and volume was changed
	
	if(session->process_audio) {
		if(audio->present()) {
			// check volume
			long volume = audio->volume();
			int audio_volume = params->getGPValue(P_AUDIO_CAPTURE_VOLUME);
			// and apply it
			if(audio_volume != volume) {
				audio->set_volume(audio_volume);
				changes_array[changes_array_i + 0] = P_AUDIO_CAPTURE_VOLUME;
				changes_array[changes_array_i + 1] = audio->volume();
				changes_array_i += 2;
				params_update = true;
			}
		}
	}

	// update array of changes
	// set frame to update
	if(apply || params_update) {
		changes_array[0] = FRAMEPARS_SETFRAME;
		changes_array[1] = params->getGPValue(G_THIS_FRAME) + 1;
		params->setPValue(changes_array, changes_array_i);
	}
	//------------------------------
	// update current image settings
//	if(apply) {
		// here - create new function from where update all settings
		struct video_desc_t video_desc = video->get_current_desc();
		if(video_desc.valid) {
			session->video.width = video_desc.width;
			session->video.height = video_desc.height;
			session->video.fps = video_desc.fps;
			session->video.fps /= session->video.fps_scale;
		}
		session->video.type = video->ptype();
//	}

	if(result)
		return 1;
	return 0;
}

int Streamer::handler(RTSP_Server *rtsp_server, RTSP_Server::event event) {
	static bool _play = false;
D(	cerr << "event: running= " << running << " ";)
	switch(event) {
	case RTSP_Server::DESCRIBE:  /// Update frame size, fps before starting new stream (generating SDP file)
		update_settings(true);
		break;
	case RTSP_Server::PARAMS_WAS_CHANGED:  /// Update frame size, fps before starting new stream (generating SDP file)
		return (update_settings(false) || !(Parameters::instance()->daemon_enabled()));
	case RTSP_Server::PLAY:
D(		cerr << "==PLAY==";)
		if(connected_count == 0) {
			int ttl = -1;
			if(session->rtp_out.multicast)
				ttl = atoi(session->rtp_out.ttl.c_str());
			video->Start(session->rtp_out.ip, session->rtp_out.port_video, session->video.fps_scale, ttl);
			if(audio != NULL)
				audio->Start(session->rtp_out.ip, session->rtp_out.port_audio, ttl);
		}
		connected_count++;
		_play = true;
		running = true;
		break;
	case RTSP_Server::PAUSE:
D(		cerr << "PAUSE";)
		connected_count--;
		if(connected_count <= 0) {
			video->Stop();
			if(audio != NULL)
				audio->Stop();
			connected_count = 0;
			_play = false;
			running = false;
		}
		break;
	case RTSP_Server::TEARDOWN:
D(		cerr << "TEARDOWN";)
		if(!running) {
D(			cerr << " was not running";)
			break;
		}
		connected_count--;
		if(connected_count <= 0) {
			video->Stop();
			if(audio != NULL)
				audio->Stop();
			connected_count = 0;
			_play = false;
			running = false;
		}
		break;
	case RTSP_Server::RESET:
D(		cerr << "RESET";)
		if(!running) {
D(			cerr << " was not running";)
			break;
		}
		video->Stop();
		if(audio != NULL)
			audio->Stop();
		connected_count = 0;
		_play = false;
		running = false;
		break;
/*
	case RTSP_Server::IS_DAEMON_ENABLED:
D(		cerr << "IS_DAEMON_ENABLED video->isDaemonEnabled(-1)=" << video->isDaemonEnabled(-1) << endl;)
		return video->isDaemonEnabled(-1);
		break;
*/
	default:
D(		cerr << "unknown == " << event;)
		break;
	}
D(	cerr << endl;)
	return 0;
}

void Streamer::Main(void) {
D(		cerr << "start Main" << endl;)
	string def_mcast = "232.1.1.1";
	int def_port = 20020;
	string def_ttl = "2";

	session->rtp_out.ip_cached = 0;
	session->rtp_out.ip_custom = true;
	session->rtp_out.ip = "0";
	session->rtp_out.port_video = def_port;
	session->rtp_out.port_audio = def_port + 2;
	session->rtp_out.ttl = def_ttl;
	rtsp_server = NULL;
	while(true) {
		/// Check if the streamer is enabled, restart loop after waiting
		if(!video->waitDaemonEnabled(-1)) {
			sched_yield();
			continue; /// may use particular bit instead of the "default" -1
		}
		update_settings(true);
		/// Got here if is and was enabled (may use more actions instead of just "continue"
		// start RTSP server
D2(		cerr << "start server" << endl;)
		if(rtsp_server == NULL)
			rtsp_server = new RTSP_Server(Streamer::f_handler, (void *)this, session);
		rtsp_server->main();
D2(		cerr << "server was stopped" << endl;)
D2(		cerr << "stop video" << endl;)
		video->Stop();
D2(		cerr << "stop audio" << endl;)
		if(audio != NULL) {
			audio->Stop();
			// free audio resource - other app can use soundcard
D2(			cerr << "delete audio" << endl;)
			delete audio;
			audio = NULL;
		}
	}
}
