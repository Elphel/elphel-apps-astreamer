/**
 * @file video.cpp
 * @brief Provides video interface for streamer
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

#include "video.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>
#include <elphel/x393_devices.h>

#include "streamer.h"

using namespace std;

//#undef VIDEO_DEBUG
//#undef VIDEO_DEBUG_2	// for timestamp monitoring
//#undef VIDEO_DEBUG_3	// for FPS monitoring
#define VIDEO_DEBUG
#define VIDEO_DEBUG_2	// for timestamp monitoring
#define VIDEO_DEBUG_3	// for FPS monitoring

#ifdef VIDEO_DEBUG
	#define D(a) a
#else
	#define D(a)
#endif

#ifdef VIDEO_DEBUG_2
	#define D2(a) a
#else
	#define D2(a)
#endif

#ifdef VIDEO_DEBUG_3
	#define D3(a) a
#else
	#define D3(a)
#endif

//Video *video = NULL;

#define QTABLES_INCLUDE

//int fd_circbuf = 0;
//int fd_jpeghead = 0; /// to get quantization tables
//int fd_fparmsall = 0;
int lastDaemonBit = DAEMON_BIT_STREAMER;

//struct framepars_all_t   *frameParsAll;
//struct framepars_t       *framePars;
//unsigned long            *globalPars; /// parameters that are not frame-related, their changes do not initiate any actions

static const char *circbuf_file_names[] = {
		DEV393_PATH(DEV393_CIRCBUF0), DEV393_PATH(DEV393_CIRCBUF1),
		DEV393_PATH(DEV393_CIRCBUF2), DEV393_PATH(DEV393_CIRCBUF3)
};
static const char *jhead_file_names[] = {
		DEV393_PATH(DEV393_JPEGHEAD0), DEV393_PATH(DEV393_JPEGHEAD1),
		DEV393_PATH(DEV393_JPEGHEAD2), DEV393_PATH(DEV393_JPEGHEAD3)
};

Video::Video(int port, Parameters *pars) {
	string err_msg;

	D( cerr << "Video::Video() on port " << port << endl;)
	D( cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << endl;)
	params = pars;
	sensor_port = port;
	stream_name = "video";
//	params = Parameters::instance();
	waitDaemonEnabled(-1); /// <0 - use default
	fd_circbuf = open(circbuf_file_names[sensor_port], O_RDONLY);
	if (fd_circbuf < 0) {
		err_msg = "can't open " + static_cast<ostringstream &>(ostringstream() << dec << sensor_port).str();
		throw runtime_error(err_msg);
	}

	buffer_length = lseek(fd_circbuf, 0, SEEK_END);
	/// mmap for all the lifetime of the program, not per stream. AF
	buffer_ptr = (unsigned long *) mmap(0, buffer_length, PROT_READ, MAP_SHARED, fd_circbuf, 0);
	if ((int) buffer_ptr == -1) {
		err_msg = "can't mmap " + *circbuf_file_names[sensor_port];
		throw runtime_error(err_msg);
	}
	cout << "<-- 1" << endl;
//	buffer_ptr_s = (unsigned long *) mmap(buffer_ptr + (buffer_length >> 2), buffer_length,
//			PROT_READ, MAP_FIXED | MAP_SHARED, fd_circbuf, 0);   /// preventing buffer rollovers
	buffer_ptr_s = (unsigned long *) mmap(buffer_ptr + (buffer_length >> 2), 100 * 4096,
			PROT_READ, MAP_FIXED | MAP_SHARED, fd_circbuf, 0);   /// preventing buffer rollovers
	cout << "<-- 2" << endl;
	if ((int) buffer_ptr_s == -1) {
		err_msg = "can't create second mmap for " + *circbuf_file_names[sensor_port];
		throw runtime_error(err_msg);
	}
	cout << "<-- 3" << endl;

	/// Skip several frames if it is just booted
	/// May get stuck here if compressor is off, it should be enabled externally
	D( cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << " frame=" << params->getGPValue(G_THIS_FRAME) << " buffer_length=" << buffer_length << endl;)
	while (params->getGPValue(G_THIS_FRAME) < 10) {
		lseek(fd_circbuf, LSEEK_CIRC_TOWP, SEEK_END); /// get to the end of buffer
		lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END); /// wait frame got ready there
	}
	/// One more wait always to make sure compressor is actually running
	lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);
	lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);
	D(cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << " frame=" << params->getGPValue(G_THIS_FRAME) << " buffer_length=" << buffer_length <<endl;)
	fd_jpeghead = open(jhead_file_names[sensor_port], O_RDWR);
	if (fd_jpeghead < 0) {
		err_msg = "can't open " + *jhead_file_names[sensor_port];
		throw runtime_error(err_msg);
	}
	qtables_include = true;

	SSRC = 12;
	_ptype = 26;
	rtp_socket = NULL;
	rtcp_socket = NULL;
	_play = false;
	prev_jpeg_wp = 0;
	f_quality = -1;

	// create thread...
	init_pthread((void *) this);
	D( cerr << __FILE__<< ":" << __FUNCTION__ << ":" << __LINE__ << endl;)
}

Video::~Video(void) {
	cerr << "Video::~Video() on port " << sensor_port << endl;
	if (buffer_ptr != NULL) {
		munmap(buffer_ptr, buffer_length);
		buffer_ptr = NULL;
	}
	if (buffer_ptr_s != NULL) {
		munmap(buffer_ptr_s, buffer_length);
		buffer_ptr_s = NULL;
	}
	if (fd_circbuf > 0)
		close(fd_circbuf);
	if (fd_jpeghead > 0)
		close(fd_jpeghead);
}

/// Compressor should be turned on outside of the streamer
#define TURN_COMPRESSOR_ON 0
void Video::Start(string ip, long port, int _fps_scale, int ttl) {
D(	cerr  << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << "_play=" << _play << endl;)
	if(_play) {
		cerr << "ERROR-->> wrong usage: Video()->Start() when already play!!!" << endl;
		return;
	}
//return;
	// statistic
	v_t_sec = 0;
	v_t_usec = 0;
	v_frames = 0;
	// create udp socket
	struct video_desc_t video_desc = get_current_desc(false);
	f_width = video_desc.width;
	f_height = video_desc.height;
	used_width = f_width;
	used_height = f_height;
//	f_width = width();
//	f_height = height();
	fps_scale = _fps_scale;
	if(fps_scale < 1)
		fps_scale = 1;
	fps_scale_c = 0;
	/// start compressor...NOTE: Maybe it should not?
#if TURN_COMPRESSOR_ON
	unsigned long write_data[4];
	write_data[0] = FRAMEPARS_SETFRAME;
	write_data[1] = params->getGPValue(G_THIS_FRAME) + 1;
	write_data[2] = P_COMPRESSOR_RUN;
	write_data[3] = COMPRESSOR_RUN_CONT;
	write(fd_fparmsall, write_data, sizeof(write_data));
#endif
	RTP_Stream::Start(ip, port, ttl);
}

void Video::Stop(void) {
	if(!_play)
		return;
//return;
	RTP_Stream::Stop();
		_play = false;
	// destroy udp socket
	prev_jpeg_wp = 0;
}


/**
 * @brief check if this application is enabled (by appropriate bit in P_DAEMON_EN), if not - 
 * and wait until enabled (return false when enabled)
 * @param daemonBit - bit number to accept control in P_DAEMON_EN parameter
 * @return (after possible waiting) true if there was no waiting, false if there was waiting
 */
bool Video::waitDaemonEnabled(int daemonBit) { // <0 - use default
	if ((daemonBit >= 0) && (daemonBit < 32))
		lastDaemonBit = daemonBit;
	unsigned long this_frame = params->getGPValue(G_THIS_FRAME);
/// No semaphors, so it is possible to miss event and wait until the streamer will be re-enabled before sending message,
/// but it seems not so terrible
	D(cerr << " lseek(fd_circbuf" << fd_circbuf << ", LSEEK_DAEMON_CIRCBUF+lastDaemonBit, SEEK_END)... " << endl;)
	lseek(fd_circbuf, LSEEK_DAEMON_CIRCBUF + lastDaemonBit, SEEK_END); /// 
	D(cerr << "...done" << endl;)

	if (this_frame == params->getGPValue(G_THIS_FRAME))
		return true;
	return false;
}

/**
 * @brief check if this application is enabled (by appropriate bit in P_DAEMON_EN)
 * @param daemonBit - bit number to accept control in P_DAEMON_EN parameter
 * @return (after possible waiting) true if there was no waiting, false if there was waiting
 */
bool Video::isDaemonEnabled(int daemonBit) { // <0 - use default
	if((daemonBit >= 0) && (daemonBit < 32))
		lastDaemonBit = daemonBit;
//	return((framePars[GLOBALPARS(G_THIS_FRAME) & PARS_FRAMES_MASK].pars[P_DAEMON_EN] & (1 << lastDaemonBit)) != 0);
	return((params->getFrameValue(P_DAEMON_EN) & (1 << lastDaemonBit)) != 0);
}


/**
 * @brief Return (byte) pointer to valid frame 'before' current(if current is invalid - use latest,
 * wait if none are ready. Restore (or modify if had to wait) file pointer.
 * fill provided frame_pars with the metadata (including the time stamp)
 * @param frame_pars - pointer to a interframe parameters structure
 * @param before - how many frames before current pointer is needed
 * @return pointer (offset in circbuf) to the frame start
 */
long Video::getFramePars(struct interframe_params_t *frame_pars, long before, long ptr_before) {
	long cur_pointer, p;

	long this_pointer = 0;
	if(ptr_before > 0) {
		/// if we need some before frame, we should set pointer to saved one (saved with before == 0)
		this_pointer = lseek(fd_circbuf, ptr_before, SEEK_SET); /// restore the file pointer
	}
	if(ptr_before < 0) {
		/// otherwise, set pointer to the actual frame
		this_pointer = lseek(fd_circbuf, LSEEK_CIRC_TOWP, SEEK_END);  /// byte index in circbuf of the frame start
	}
	if(ptr_before == 0)
		this_pointer = lseek(fd_circbuf, 0, SEEK_CUR); /// save orifinal file pointer
	char *char_buffer_ptr = (char *)buffer_ptr;
	if(lseek(fd_circbuf, LSEEK_CIRC_VALID, SEEK_END) < 0) { /// Invalid frame - reset to the latest acquired
		this_pointer = lseek(fd_circbuf, LSEEK_CIRC_LAST, SEEK_END);    /// Last acquired frame (may be not yet available if none are)
	}
	cur_pointer = this_pointer;
	if(before == 0)
		lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);
	while(before && (((p = lseek(fd_circbuf, LSEEK_CIRC_PREV, SEEK_END))) >= 0)) { /// try to get earlier valid frame
		cur_pointer = p;
		before--;
	}

	/// if 'before' is still >0 - not enough frames acquired, wait for more 
	while(before > 0) {
		lseek(fd_circbuf, this_pointer, SEEK_SET);
		lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);
		this_pointer = lseek(fd_circbuf, LSEEK_CIRC_NEXT, SEEK_END);
		before--;
	}
	long metadata_start = cur_pointer - 32;
	if(metadata_start < 0)
		metadata_start += buffer_length;
	/// copy the interframe data (timestamps are not yet there)
D(cerr  << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << " before=" << before << " metadata_start=" << metadata_start << endl;)
	memcpy(frame_pars, &char_buffer_ptr[metadata_start], 32);
	long jpeg_len = frame_pars->frame_length; //! frame_pars->frame_length is now the length of bitstream
	if(frame_pars->signffff != 0xffff) {
		cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << "  Wrong signature in getFramePars() (broken frame), frame_pars->signffff="<< frame_pars->signffff << endl;
                int i;
                long * dd =(long *) frame_pars;
                cerr << hex << (metadata_start/4) << ": ";
//                for (i=0;i<8;i++) {
                for (i=0;i<8;i++) {
                  cerr << hex << dd[i] << "  ";
                }
                cerr << dec << endl;
		return -1;
	} else {
//            cerr << hex << (metadata_start/4) << dec << endl; ///************* debug
        }
	///   find location of the timestamp and copy it to the frame_pars structure
	///==================================
	long timestamp_start = (cur_pointer) + ((jpeg_len + CCAM_MMAP_META + 3) & (~0x1f)) + 32 - CCAM_MMAP_META_SEC; //! magic shift - should index first byte of the time stamp
	if(timestamp_start >= buffer_length)
		timestamp_start -= buffer_length;
	memcpy(&(frame_pars->timestamp_sec), &char_buffer_ptr[timestamp_start], 8);
	if(ptr_before == 0)
		lseek(fd_circbuf, this_pointer, SEEK_SET); /// restore the file pointer
//D(cerr  << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << " this_pointer=" << this_pointer << " cur_pointer=" << cur_pointer << endl;)
	return cur_pointer;
}


/// In the next function I assume that the frame pointed by current file pointer in circbuf is ready. Otherwise
/// (if it points at next frame to be acquired) we need to increase argument of getValidFrame(long before) by 1
/// get all parameters together
#define FRAMEPARS_BEFORE 0 /// Change to 1 if frames are not yet ready when these functions are called
struct video_desc_t Video::get_current_desc(bool with_fps) {
	struct interframe_params_t frame_pars, prev_pars;
	struct video_desc_t video_desc;
	video_desc.valid = false;
	long ptr = -1;
	if((ptr = getFramePars(&frame_pars, FRAMEPARS_BEFORE, -1)) < 0) {
		return video_desc;
	} else {
		if(with_fps) {
			if(getFramePars(&prev_pars,  FRAMEPARS_BEFORE + 1, ptr) < 0)
				return video_desc;
			double fps = (frame_pars.timestamp_sec - prev_pars.timestamp_sec);
			fps *= 1000000.0;
			fps += frame_pars.timestamp_usec;
			fps -= prev_pars.timestamp_usec;
D3(			float _f = fps;)
			fps = 1000000.0 / fps;
			video_desc.fps = (used_fps = fps);
D(			cerr  << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << " fps=" << fps << endl;)
//			cerr  << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__ << " fps=" << video_desc.fps << endl;
D3(if(_f == 0))
D3(cerr << "delta == " << _f << endl << endl;)
		}
	}
	video_desc.valid = true;
//	video_desc.width = (used_width = frame_pars.width);
//	video_desc.height = (used_height = frame_pars.height);
	video_desc.width = frame_pars.width;
	video_desc.height = frame_pars.height;
	video_desc.quality = frame_pars.quality2;
	return video_desc;
}

void Video::fps(float fps) {
	if(fps < 0.01)
		return;
	/// currently limiting FPS only works with free running TODO: Add external trigger frequency support.
	unsigned long write_data[6];
	long target_frame = params->getGPValue(G_THIS_FRAME) + FRAMES_AHEAD_FPS;
	write_data[0] = FRAMEPARS_SETFRAME;
	write_data[1] = target_frame; /// wait then for that frame to be available on the output plus 2 frames for fps to be stable
	write_data[2] = P_FP1000SLIM;
	write_data[3] = (unsigned long)fps * 1000;
	write_data[4] = P_FPSFLAGS;
	write_data[5] = 3;
//	long rslt = write(fd_fparmsall, write_data, sizeof(write_data));
	int rslt = params->write(write_data, sizeof(write_data));
	if(rslt == sizeof(write_data)) { /// written OK
//		lseek(fd_fparmsall, LSEEK_FRAME_WAIT_ABS + target_frame + FRAMES_SKIP_FPS, SEEK_END); /// skip frames 
		params->lseek(LSEEK_FRAME_WAIT_ABS + target_frame + FRAMES_SKIP_FPS, SEEK_END); /// skip frames 
	}
}

#define USE_REAL_OLD_TIMESTAMP 0
long Video::capture(void) {
	long frame_len;
	struct interframe_params_t frame_pars;
//	long len;

	int quality;
	unsigned long latestAvailableFrame_ptr;
	unsigned long frameStartByteIndex;
	int before;
	///Make sure the streamer is not disabled through the bit in P_DAEMON_EN
//	if((framePars[pars->getGPValue(G_THIS_FRAME) & PARS_FRAMES_MASK].pars[P_DAEMON_EN] & (1 << lastDaemonBit)) == 0) {
	if((params->getFrameValue(P_DAEMON_EN) & (1 << lastDaemonBit)) == 0) {
		return -DAEMON_DISABLED;  /// return exception (will stop the stream)
	}
	frameStartByteIndex = lseek(fd_circbuf, LSEEK_CIRC_TOWP, SEEK_END);  /// byte index in circbuf of the frame start
	latestAvailableFrame_ptr = frameStartByteIndex;
	lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);

	frame_ptr = (char *)((unsigned long)buffer_ptr + latestAvailableFrame_ptr);
//fprintf(stderr, "frame_ptr == %08X; ", frame_ptr);
	if(latestAvailableFrame_ptr < 32)
		latestAvailableFrame_ptr += buffer_length;
	latestAvailableFrame_ptr >>= 2;
	frame_len = buffer_ptr[latestAvailableFrame_ptr - 1];
//	read timestamp
	char *ts_ptr = (char *)((unsigned long)frame_ptr + (long)(((frame_len + CCAM_MMAP_META + 3) & (~0x1f)) + 32 - CCAM_MMAP_META_SEC));
	unsigned long t[2];
	memcpy(&t, (void *)ts_ptr, 8);
	f_tv.tv_sec = t[0];
	f_tv.tv_usec = t[1];
	// read Q value
	char *meta = (char *)frame_ptr;
	meta -= 32;
	if(meta < (char *)buffer_ptr)
		meta += buffer_length;
	struct interframe_params_t *fp = (struct interframe_params_t *)meta;
	/// See if the frame parameters are the same as used when starting the stream,
	/// Otherwise check  for up to G_SKIP_DIFF_FRAME older frames and return them instead,
	/// If that number is exceeded - return exception
	/// Each time the latest acquired frame is considered, so we do not need to save frmae poointer additionally
	if((fp->width != used_width) || (fp->height != used_height)) {
		for(before = 1; before <= (int)params->getGPValue(G_SKIP_DIFF_FRAME); before++) {
			if(((frameStartByteIndex = getFramePars(&frame_pars, before))) && (frame_pars.width == used_width) && (frame_pars.height == used_height)) {
				/// substitute older frame instead of the latest one. Leave wrong timestamp?
				/// copying code above (may need some cleanup). Maybe - just move earlier so there will be no code duplication?
				latestAvailableFrame_ptr = frameStartByteIndex;
				frame_ptr = (char *)((unsigned long)buffer_ptr + latestAvailableFrame_ptr);
				if(latestAvailableFrame_ptr < 32)
					latestAvailableFrame_ptr += buffer_length;
				latestAvailableFrame_ptr >>= 2;
				frame_len = buffer_ptr[latestAvailableFrame_ptr - 1];
#if USE_REAL_OLD_TIMESTAMP
/// read timestamp
				ts_ptr = (char *)((unsigned long)frame_ptr + (long)(((frame_len + CCAM_MMAP_META + 3) & (~0x1f)) + 32 - CCAM_MMAP_META_SEC));
				memcpy(&t, (void *)ts_ptr, 8);
				f_tv.tv_sec = t[0];
				f_tv.tv_usec = t[1];
#endif
//cerr << "skip frame: before == " << before << endl;
//cerr << "used_width == " << used_width << "; current_width == " << frame_pars.width << endl;
				/// update interframe data pointer
//				char *meta = (char *)frame_ptr;
				meta = (char *)frame_ptr;
				meta -= 32;
				if(meta < (char *)buffer_ptr)
					meta += buffer_length;
				fp = (struct interframe_params_t *)meta;
				break;
			}
		}
		if(before > (int) params->getGPValue(G_SKIP_DIFF_FRAME)) {
D(cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__<< " Killing stream because of frame size change " << endl;)
			return -SIZE_CHANGE; /// It seems that frame size is changed for good, need to restart the stream
		}
D(cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__<< " Waiting for the original frame size to be restored , using " << before << " frames ago" << endl;)
	}
	///long Video::getFramePars(struct interframe_params_t * frame_pars, long before) {
	///getGPValue(unsigned long GPNumber)
	quality = fp->quality2;
	if(qtables_include && quality != f_quality) {
D(cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__<< " Updating quality tables, new quality is " << quality << endl;)
			lseek(fd_jpeghead, frameStartByteIndex | 2, SEEK_END); /// '||2' indicates that we need just quantization tables, not full JPEG header
			read(fd_jpeghead, (void *)&qtable[0], 128);
	}
	f_quality = quality;
/*
	// check statistic
static bool first = true;
if(first) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	first = false;
	fprintf(stderr, "VIDEO first with time: %d:%06d at: %d:%06d\n", f_tv.tv_sec, f_tv.tv_usec, tv.tv_sec, tv.tv_usec);
}
*/
	return frame_len;	
}

long Video::process(void) {
//D(cerr << "< ";)
	int _plen = 1400;
	int to_send = _plen;
	int _qtables_len = 128 + 4;
	long frame_len = capture();
	if(frame_len == 0) {
//D(cerr << "[";)
//		return false;
		return 0; /// now never here
	} else {
		if(frame_len < 0) {
D(cerr << __FILE__<< ":"<< __FUNCTION__ << ":" <<__LINE__<< "capture returned negative" << frame_len << endl;)
//			return false;
			return frame_len; /// attention (restart) is needed
		}
	}
	// check FPS decimation
	bool to_skip = true;
	if(fps_scale_c == 0)
		to_skip = false;
	fps_scale_c++;
	if(fps_scale_c >= fps_scale)
		fps_scale_c = 0;
//cerr << "fps_scale == " << fps_scale << "; fps_scale_c == " << fps_scale_c << "; to_skip == " << to_skip << endl;
	if(to_skip)
		return 1;

	int to_send_len = frame_len;
	unsigned char h[20 + 4];
	int packet_len = 0;
	unsigned char *data = (unsigned char *)frame_ptr;

	uint64_t t = f_tv.tv_sec;
	t *= 90000;
	t &= 0x00FFFFFFFF;
	timestamp = t;
	double f = f_tv.tv_usec;
	f /= 1000000.0;
	f *= 90000.0;
	timestamp += (uint32_t)f;
	uint32_t ts;
	ts = timestamp;
	ts = htonl(ts);

	long offset = 0;
	void *v_ptr[4];
	int v_len[4] = {0, 0, 0, 0};
	bool first = true;
	while(to_send_len && _play) {
		unsigned long pnum = htons(packet_num);
		bool last = false;
		to_send = _plen;
		if(qtables_include && first)
			to_send = _plen - _qtables_len;
		if(to_send_len <= to_send) {
			packet_len = to_send_len;
			to_send_len = 0;
			last = true;
		} else {
			packet_len = to_send;
			to_send_len -= to_send;
		}
		// make RTP packet
		h[0] = 0x80;
		if(!last)
			h[1] = _ptype;
		else
			h[1] = 0x80 + _ptype;
		memcpy((void *)&h[2], (void *)&pnum, 2);
		memcpy((void *)&h[4], (void *)&ts, 4);
		memcpy((void *)&h[8], (void *)&SSRC, 4);
		// make MJPEG header
		unsigned long off = htonl(offset);
		memcpy((void *)&h[12], (void *)&off, 4);
		h[12] = 0x00;
		h[16] = 0x01;
		unsigned int q = f_quality;
		if(qtables_include)
			q += 128;
		h[17] = (unsigned char)(q & 0xFF);
		if(f_width <= 2040)
			h[18] = (f_width / 8) & 0xFF;
		else
			h[18] = 0;
		if(f_height <= 2040)
			h[19] = (f_height / 8) & 0xFF;
		else
			h[19] = 0;
		h[20] = 0;
		h[21] = 0;
		unsigned short l = htons(128);
		memcpy((void *)&h[22], (void *)&l, 2);
		// update RTCP statistic
		rtp_packets++;
		rtp_octets += packet_len + 8; // data + MJPEG header
		// send vector
		if(first) {
			v_ptr[0] = h;
			if(qtables_include) {
				v_len[0] = 24;
				v_ptr[1] = qtable;
				v_len[1] = 128;
				v_ptr[2] = data;
				v_len[2] = packet_len;
				rtp_socket->send3v(&v_ptr[0], &v_len[0]);
			} else {
				v_len[0] = 20;
				v_ptr[1] = data;
				v_len[1] = packet_len;
				rtp_socket->send2v(&v_ptr[0], &v_len[0]);
			}
			first = false;
		} else {
			v_ptr[0] = h;
			v_len[0] = 20;
			v_ptr[1] = data;
			v_len[1] = packet_len;
			rtp_socket->send2v(&v_ptr[0], &v_len[0]);
		}
		// --==--
		packet_num++;
		data += packet_len;
		offset += packet_len;
	}
//D(cerr << "]";)
//	return true;
	return 1;
}
