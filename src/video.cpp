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
#include <iomanip>

#include "streamer.h"

using namespace std;

//#undef VIDEO_DEBUG
//#undef VIDEO_DEBUG_2	                                        // for timestamp monitoring
#undef VIDEO_DEBUG_3	                                        // for FPS monitoring
#define VIDEO_DEBUG
#define VIDEO_DEBUG_2	                                        // for timestamp monitoring
//#define VIDEO_DEBUG_3	                                        // for FPS monitoring

#undef VIDEO_DEBUG	                                        // for FPS monitoring
#undef VIDEO_DEBUG_2	                                        // for FPS monitoring

#ifdef VIDEO_DEBUG
	#define D(s_port, a) \
	do { \
		cerr << __FILE__ << ": " << __FUNCTION__ << ": " << __LINE__ << ": sensor port: " << s_port << " "; \
		a; \
	} while (0)
#else
	#define D(s_port, a)
#endif

#ifdef VIDEO_DEBUG_2
	#define D2(s_port, a) \
	do { \
		cerr << __FILE__ << ": " << __FUNCTION__ << ": " << __LINE__ << ": sensor port: " << s_port << " "; \
		a; \
	} while (0)
#else
	#define D2(s_port, a)
#endif

#ifdef VIDEO_DEBUG_3
	#define D3(s_port, a) \
	do { \
		cerr << __FILE__ << ": " << __FUNCTION__ << ": " << __LINE__ << ": sensor port: " << s_port << " "; \
		a; \
	} while (0)
#else
	#define D3(s_port, a)
#endif

/** The length of interframe parameters in bytes */
#define METADATA_LEN              32
/** Convert byte offset to double word offset */
#define BYTE2DW(x)                ((x) >> 2)
/** Convert double word offset to byte offset */
#define DW2BYTE(x)                ((x) << 2)

static const char *circbuf_file_names[] = {
		DEV393_PATH(DEV393_CIRCBUF0), DEV393_PATH(DEV393_CIRCBUF1),
		DEV393_PATH(DEV393_CIRCBUF2), DEV393_PATH(DEV393_CIRCBUF3)
};
static const char *jhead_file_names[] = {
		DEV393_PATH(DEV393_JPEGHEAD0), DEV393_PATH(DEV393_JPEGHEAD1),
		DEV393_PATH(DEV393_JPEGHEAD2), DEV393_PATH(DEV393_JPEGHEAD3)
};

/**
 * @brief Start one instance of video interface for circbuf: open and mmap circbuf,
 * start RTP stream in new thread.
 * @param   port   sensor port number this instance should work with
 * @param   pars   pointer to parameters instance for the current sensor port
 * @return  None
 */
Video::Video(int port, Parameters *pars) {
	string err_msg;
	params = pars;
	sensor_port = port;
	stream_name = "video";
	lastDaemonBit = DAEMON_BIT_STREAMER;

	D(sensor_port, cerr << "Video::Video() on sensor port " << port << endl);
	fd_circbuf = open(circbuf_file_names[sensor_port], O_RDONLY);
	if (fd_circbuf < 0) {
		err_msg = "can't open " + static_cast<ostringstream &>(ostringstream() << dec << sensor_port).str();
		throw runtime_error(err_msg);
	}

	buffer_length = lseek(fd_circbuf, 0, SEEK_END);
	waitDaemonEnabled(-1);                                      // <0 - use default

	// mmap for all the lifetime of the program, not per stream. AF
	buffer_ptr = (unsigned long *) mmap(0, buffer_length, PROT_READ, MAP_SHARED, fd_circbuf, 0);
	if ((int) buffer_ptr == -1) {
		err_msg = "can't mmap " + *circbuf_file_names[sensor_port];
		throw runtime_error(err_msg);
	}
	buffer_ptr_end = (unsigned char *)(buffer_ptr + BYTE2DW(buffer_length));

	// Skip several frames if it is just booted
	// May get stuck here if compressor is off, it should be enabled externally
	D(sensor_port, cerr << " frame=" << params->getGPValue(G_THIS_FRAME) << " buffer_length=" << buffer_length << endl);
	while (params->getGPValue(G_THIS_FRAME) < 10) {
		lseek(fd_circbuf, LSEEK_CIRC_TOWP, SEEK_END);           // get to the end of buffer
		lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);           // wait frame got ready there
	}
	// One more wait always to make sure compressor is actually running
	lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);
	lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);
	D(sensor_port, cerr << " frame=" << params->getGPValue(G_THIS_FRAME) << " buffer_length=" << buffer_length <<endl);
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
	D(sensor_port, cerr << "finish constructor" << endl);
}

/**
 * @brief Close and unmap circbuf files
 * @param   None
 * @return  None
 */
Video::~Video(void) {
	cerr << "Video::~Video() on port " << sensor_port << endl;
	if (buffer_ptr != NULL) {
		munmap(buffer_ptr, buffer_length);
		buffer_ptr = NULL;
	}
	if (fd_circbuf > 0)
		close(fd_circbuf);
	if (fd_jpeghead > 0)
		close(fd_jpeghead);
}

void Video::Start(string ip, long port, int _fps_scale, int ttl) {
	D(sensor_port, cerr << "_play=" << _play << endl);
	if (_play) {
		cerr << "ERROR-->> wrong usage: Video()->Start() when already play!!!" << endl;
		return;
	}
	// statistic
	v_t_sec = 0;
	v_t_usec = 0;
	v_frames = 0;
	// create UDP socket
	struct video_desc_t video_desc = get_current_desc(false);
	f_width = video_desc.width;
	f_height = video_desc.height;
	used_width = f_width;
	used_height = f_height;
	fps_scale = _fps_scale;
	if (fps_scale < 1)
		fps_scale = 1;
	fps_scale_c = 0;
	RTP_Stream::Start(ip, port, ttl);
}

void Video::Stop(void) {
	if (!_play)
		return;
	RTP_Stream::Stop();
	_play = false;
	// destroy UDP socket
	prev_jpeg_wp = 0;
}


/**
 * @brief Check if this application is enabled (by appropriate bit in P_DAEMON_EN), if not -
 * and wait until enabled (return false when enabled)
 * @param   daemonBit   bit number to accept control in P_DAEMON_EN parameter
 * @return (after possible waiting) true if there was no waiting, false if there was waiting
 */
bool Video::waitDaemonEnabled(int daemonBit) {                  // <0 - use default
	if ((daemonBit >= 0) && (daemonBit < 32))
		lastDaemonBit = daemonBit;
	unsigned long this_frame = params->getGPValue(G_THIS_FRAME);
	// No semaphores, so it is possible to miss event and wait until the streamer will be re-enabled before sending message,
	// but it seems not so terrible
	D(sensor_port, cerr << " lseek(fd_circbuf" << sensor_port << ", LSEEK_DAEMON_CIRCBUF+lastDaemonBit, SEEK_END)... " << endl);
	lseek(fd_circbuf, LSEEK_DAEMON_CIRCBUF + lastDaemonBit, SEEK_END);
	D(sensor_port, cerr << "...done" << endl);

	if (this_frame == params->getGPValue(G_THIS_FRAME))
		return true;
	return false;
}

/**
 * @brief Check if this application is enabled (by appropriate bit in P_DAEMON_EN)
 * @param   daemonBit   bit number to accept control in P_DAEMON_EN parameter
 * @return (after possible waiting) true if there was no waiting, false if there was waiting
 */
bool Video::isDaemonEnabled(int daemonBit) { // <0 - use default
	if ((daemonBit >= 0) && (daemonBit < 32))
		lastDaemonBit = daemonBit;
	return ((params->getFrameValue(P_DAEMON_EN) & (1 << lastDaemonBit)) != 0);
}


/**
 * @brief Return (byte) pointer to valid frame 'before' current(if current is invalid - use latest,
 * wait if none are ready. Restore (or modify if had to wait) file pointer.
 * fill provided frame_pars with the metadata (including the time stamp)
 * @param   frame_pars   pointer to a interframe parameters structure
 * @param   before   how many frames before current pointer is needed
 * @return  pointer (offset in circbuf) to the frame start
 */
long Video::getFramePars(struct interframe_params_t *frame_pars, long before, long ptr_before) {
	long cur_pointer, p;

	long this_pointer = 0;
	if (ptr_before > 0) {
		// if we need some before frame, we should set pointer to saved one (saved with before == 0)
		this_pointer = lseek(fd_circbuf, ptr_before, SEEK_SET); // restore the file pointer
	}
	if (ptr_before < 0) {
		// otherwise, set pointer to the actual frame
		this_pointer = lseek(fd_circbuf, LSEEK_CIRC_TOWP, SEEK_END); // byte index in circbuf of the frame start
	}
	if (ptr_before == 0)
		this_pointer = lseek(fd_circbuf, 0, SEEK_CUR);          // save original file pointer
	char *char_buffer_ptr = (char *) buffer_ptr;
	if (lseek(fd_circbuf, LSEEK_CIRC_VALID, SEEK_END) < 0) {    // Invalid frame - reset to the latest acquired
		this_pointer = lseek(fd_circbuf, LSEEK_CIRC_LAST, SEEK_END); // Last acquired frame (may be not yet available if none are)
	}
	cur_pointer = this_pointer;
	if (before == 0)
		lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);
	while (before && (((p = lseek(fd_circbuf, LSEEK_CIRC_PREV, SEEK_END))) >= 0)) { // try to get earlier valid frame
		cur_pointer = p;
		before--;
	}

	// if 'before' is still >0 - not enough frames acquired, wait for more
	while (before > 0) {
		lseek(fd_circbuf, this_pointer, SEEK_SET);
		lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);
		this_pointer = lseek(fd_circbuf, LSEEK_CIRC_NEXT, SEEK_END);
		before--;
	}

	// copy the interframe data (time stamps are not yet there)
	long metadata_start = cur_pointer - METADATA_LEN;
	if (metadata_start >= 0) {
		D(sensor_port, cerr << " before=" << before << " metadata_start=" << metadata_start << endl);
		memcpy(frame_pars, &char_buffer_ptr[metadata_start], METADATA_LEN);
	} else {
		// matadata rolls over the end of the buffer and we need to copy both chunks
		size_t meta_len_first = METADATA_LEN - cur_pointer;
		metadata_start += buffer_length;
		memcpy(frame_pars, &char_buffer_ptr[metadata_start], meta_len_first);
		D(sensor_port, cerr << "metadata rolls over: metadata_start = " << metadata_start << "first chunk len = " << meta_len_first);

		size_t meta_len_second = METADATA_LEN - meta_len_first;
		char *dest = (char *)frame_pars;
		memcpy(&dest[meta_len_first], char_buffer_ptr, meta_len_second);
		D(sensor_port, cerr << ", second chunk len = " << meta_len_second << endl);
	}

	long jpeg_len = frame_pars->frame_length;                   // frame_pars->frame_length is now the length of bitstream
	if (frame_pars->signffff != 0xffff) {
		cerr << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__
				<< "  Wrong signature in getFramePars() (broken frame), frame_pars->signffff="
				<< frame_pars->signffff << endl;
		int i;
		long * dd = (long *) frame_pars;
		cerr << hex << (metadata_start / 4) << ": ";
		for (i = 0; i < 8; i++) {
			cerr << hex << dd[i] << "  ";
		}
		cerr << dec << endl;
		return -1;
	}
	// find location of the time stamp and copy it to the frame_pars structure
	long timestamp_start = (cur_pointer) + ((jpeg_len + CCAM_MMAP_META + 3) & (~0x1f)) + 32- CCAM_MMAP_META_SEC; //! magic shift - should index first byte of the time stamp
	if (timestamp_start >= buffer_length)
		timestamp_start -= buffer_length;
	memcpy(&(frame_pars->timestamp_sec), &char_buffer_ptr[timestamp_start], 8);
	if (ptr_before == 0)
		lseek(fd_circbuf, this_pointer, SEEK_SET);              // restore the file pointer

	return cur_pointer;
}


/**
 * @brief Return description of the current video frame, i.e. current video parameters
 * In the next function I assume that the frame pointed by current file pointer in circbuf is ready. Otherwise
 * (if it points at next frame to be acquired) we need to increase argument of getValidFrame(long before) by 1
 * get all parameters together
 */
#define FRAMEPARS_BEFORE 0                                      // Change to 1 if frames are not yet ready when these functions are called
struct video_desc_t Video::get_current_desc(bool with_fps) {
	struct interframe_params_t frame_pars, prev_pars;
	struct video_desc_t video_desc;
	video_desc.valid = false;
	long ptr = -1;
	if ((ptr = getFramePars(&frame_pars, FRAMEPARS_BEFORE, -1)) < 0) {
		return video_desc;
	} else {
		if (with_fps) {
			if (getFramePars(&prev_pars, FRAMEPARS_BEFORE + 1, ptr) < 0)
				return video_desc;
			double fps = (frame_pars.timestamp_sec - prev_pars.timestamp_sec);
			fps *= 1000000.0;
			fps += frame_pars.timestamp_usec;
			fps -= prev_pars.timestamp_usec;
			fps = 1000000.0 / fps;
			video_desc.fps = (used_fps = fps);
			D(sensor_port, cerr << " fps=" << fps << endl);
		}
	}
	video_desc.valid = true;
	video_desc.width = frame_pars.width;
	video_desc.height = frame_pars.height;
	video_desc.quality = frame_pars.quality2;
	return video_desc;
}

void Video::fps(float fps) {
	if (fps < 0.01)
		return;
	// currently limiting FPS only works with free running TODO: Add external trigger frequency support.
	unsigned long write_data[6];
	long target_frame = params->getGPValue(G_THIS_FRAME) + FRAMES_AHEAD_FPS;
	write_data[0] = FRAMEPARS_SETFRAME;
	write_data[1] = target_frame; /// wait then for that frame to be available on the output plus 2 frames for fps to be stable
	write_data[2] = P_FP1000SLIM;
	write_data[3] = (unsigned long) fps * 1000;
	write_data[4] = P_FPSFLAGS;
	write_data[5] = 3;
	int rslt = params->write(write_data, sizeof(write_data));
	if (rslt == sizeof(write_data)) { /// written OK
		params->lseek(LSEEK_FRAME_WAIT_ABS + target_frame + FRAMES_SKIP_FPS, SEEK_END); /// skip frames 
	}
}

/** Get frame length in bytes.
 * @param   offset   byte offset of a frame in cirbuf
 * @return  The length of the frame in bytes
 */
unsigned long Video::get_frame_len(unsigned long offset)
{
	unsigned long len;
	long long len_offset = BYTE2DW(offset) - 1;

	if (len_offset < 0) {
		len_offset = BYTE2DW(buffer_length - offset) - 1;
	}
	len = buffer_ptr[len_offset];

	return len;
}

/** Get interframe parameters for the frame offset given and copy them to the buffer.
 * @param   frame_pars   buffer for interframe parameters
 * @param   offset       starting offset of the frame in circbuf (in bytes)
 * @return  None
 */
void Video::get_frame_pars(void *frame_pars, unsigned long offset)
{
	unsigned long *ptr;
	unsigned long remainder;
	unsigned long pos;

	if (offset >= METADATA_LEN) {
		ptr = &buffer_ptr[BYTE2DW(offset - METADATA_LEN)];
		memcpy(frame_pars, ptr, METADATA_LEN);
		D3(sensor_port, cerr << "Read interframe params, ptr: " << (void *)ptr << endl);
	} else {
		// copy the chunk from the end of the buffer
		remainder = METADATA_LEN - offset;
		pos = buffer_length - offset;
		ptr = &buffer_ptr[BYTE2DW(pos)];
		memcpy(frame_pars, ptr, remainder);
		D3(sensor_port, cerr << "Read interframe params (first chunk), ptr: " << (void *)ptr << endl);

		// copy the chunk from the beginning of the buffer
		char *dest = (char *)frame_pars + remainder;
		memcpy(dest, buffer_ptr, offset);
		D3(sensor_port, cerr << "Read interframe params (second chunk), ptr: " << (void *)buffer_ptr << endl);
	}
}

#define USE_REAL_OLD_TIMESTAMP 0
long Video::capture(void) {
	long frame_len;
	struct interframe_params_t frame_pars;
	struct interframe_params_t curr_frame_params;
	struct interframe_params_t *fp = &curr_frame_params;
	int quality;
	unsigned long latestAvailableFrame_ptr;
	unsigned long frameStartByteIndex;
	int before;

	// make sure the streamer is not disabled through the bit in P_DAEMON_EN
	if ((params->getFrameValue(P_DAEMON_EN) & (1 << lastDaemonBit)) == 0) {
		return -DAEMON_DISABLED;                                // return exception (will stop the stream)
	}
	frameStartByteIndex = lseek(fd_circbuf, LSEEK_CIRC_TOWP, SEEK_END); // byte index in circbuf of the frame start
	latestAvailableFrame_ptr = frameStartByteIndex;
	lseek(fd_circbuf, LSEEK_CIRC_WAIT, SEEK_END);

	frame_ptr = (char *) ((unsigned long) buffer_ptr + latestAvailableFrame_ptr);
	frame_len = get_frame_len(latestAvailableFrame_ptr);
	D3(sensor_port, cerr << "Frame start byte index: " << frameStartByteIndex <<
			", frame pointer: " << (void *)frame_ptr <<
			", frame length: " << frame_len << endl);

	// read time stamp
	unsigned char *ts_ptr = (unsigned char *) ((unsigned long) frame_ptr + (long) (((frame_len + CCAM_MMAP_META + 3) & (~0x1f)) + 32 - CCAM_MMAP_META_SEC));
	if (ts_ptr >= buffer_ptr_end) {
		ts_ptr -= buffer_length;
	}
	unsigned long t[2];
	memcpy(&t, (void *) ts_ptr, 8);
	f_tv.tv_sec = t[0];
	f_tv.tv_usec = t[1];

	// read Q value
	get_frame_pars(fp, latestAvailableFrame_ptr);

	// See if the frame parameters are the same as were used when starting the stream,
	// otherwise check for up to G_SKIP_DIFF_FRAME older frames and return them instead.
	// If that number is exceeded - return exception.
	// Each time the latest acquired frame is considered, so we do not need to save frame pointer additionally
	if ((fp->width != used_width) || (fp->height != used_height)) {
		for (before = 1; before <= (int) params->getGPValue(G_SKIP_DIFF_FRAME); before++) {
			if (((frameStartByteIndex = getFramePars(&frame_pars, before)))
					&& (frame_pars.width == used_width) && (frame_pars.height == used_height)) {
				// substitute older frame instead of the latest one. Leave wrong timestamp?
				// copying code above (may need some cleanup). Maybe - just move earlier so there will be no code duplication?
				latestAvailableFrame_ptr = frameStartByteIndex;
				frame_ptr = (char *) ((unsigned long) buffer_ptr + latestAvailableFrame_ptr);
				frame_len = get_frame_len(latestAvailableFrame_ptr);
				D3(sensor_port, cerr << "Frame length " << frame_len << endl);

#if USE_REAL_OLD_TIMESTAMP
/// read timestamp
				ts_ptr = (char *)((unsigned long)frame_ptr + (long)(((frame_len + CCAM_MMAP_META + 3) & (~0x1f)) + 32 - CCAM_MMAP_META_SEC));
				memcpy(&t, (void *)ts_ptr, 8);
				f_tv.tv_sec = t[0];
				f_tv.tv_usec = t[1];
#endif
				// update interframe data pointer
				get_frame_pars(fp, latestAvailableFrame_ptr);
				D3(sensor_port, cerr << "frame_pars->signffff" << fp->signffff << endl);
				break;
			}
		}
		if (before > (int) params->getGPValue(G_SKIP_DIFF_FRAME)) {
			D(sensor_port, cerr << " Killing stream because of frame size change " << endl);
			return -SIZE_CHANGE; /// It seems that frame size is changed for good, need to restart the stream
		}
		D(sensor_port, cerr << " Waiting for the original frame size to be restored , using " << before << " frames ago" << endl);
	}

	quality = fp->quality2;
	if (qtables_include && quality != f_quality) {
		D(sensor_port, cerr << " Updating quality tables, new quality is " << quality << endl);
		lseek(fd_jpeghead, frameStartByteIndex | 2, SEEK_END);  // '| 2' indicates that we need just quantization tables, not full JPEG header
		read(fd_jpeghead, (void *) &qtable[0], 128);
	}
	f_quality = quality;

	return frame_len;
}

long Video::process(void) {
	int _plen = 1400;
	int to_send = _plen;
	int _qtables_len = 128 + 4;
	long frame_len = capture();
	if (frame_len == 0) {
		return 0;                                               // now never here
	} else {
		if (frame_len < 0) {
			D(sensor_port, cerr << "capture returned negative" << frame_len << endl);
			return frame_len;                                   // attention (restart) is needed
		}
	}
	// check FPS decimation
	bool to_skip = true;
	if (fps_scale_c == 0)
		to_skip = false;
	fps_scale_c++;
	if (fps_scale_c >= fps_scale)
		fps_scale_c = 0;
	if (to_skip)
		return 1;

	int to_send_len = frame_len;
	unsigned char h[20 + 4];
	int packet_len = 0;
	unsigned char *data = (unsigned char *) frame_ptr;

	uint64_t t = f_tv.tv_sec;
	t *= 90000;
	t &= 0x00FFFFFFFF;
	timestamp = t;
	double f = f_tv.tv_usec;
	f /= 1000000.0;
	f *= 90000.0;
	timestamp += (uint32_t) f;
	uint32_t ts;
	ts = timestamp;
	ts = htonl(ts);
	D(sensor_port, cerr << "This frame's time stamp: " << timestamp << endl);

	long offset = 0;
	struct iovec iov[4];
	int vect_num;
	bool first = true;
	while (to_send_len && _play) {
		unsigned long pnum = htons(packet_num);
		bool last = false;
		to_send = _plen;
		if (qtables_include && first)
			to_send = _plen - _qtables_len;
		if (to_send_len <= to_send) {
			packet_len = to_send_len;
			to_send_len = 0;
			last = true;
		} else {
			packet_len = to_send;
			to_send_len -= to_send;
		}
		// make RTP packet
		h[0] = 0x80;
		if (!last)
			h[1] = _ptype;
		else
			h[1] = 0x80 + _ptype;
		memcpy((void *) &h[2], (void *) &pnum, 2);
		memcpy((void *) &h[4], (void *) &ts, 4);
		memcpy((void *) &h[8], (void *) &SSRC, 4);
		// make MJPEG header
		unsigned long off = htonl(offset);
		memcpy((void *) &h[12], (void *) &off, 4);
		h[12] = 0x00;
		h[16] = 0x01;
		unsigned int q = f_quality;
		if (qtables_include)
			q += 128;
		h[17] = (unsigned char) (q & 0xFF);
		if (f_width <= 2040)
			h[18] = (f_width / 8) & 0xFF;
		else
			h[18] = 0;
		if (f_height <= 2040)
			h[19] = (f_height / 8) & 0xFF;
		else
			h[19] = 0;
		h[20] = 0;
		h[21] = 0;
		unsigned short l = htons(128);
		memcpy((void *) &h[22], (void *) &l, 2);
		// update RTCP statistic
		rtp_packets++;
		rtp_octets += packet_len + 8;                           // data + MJPEG header
		// send vector
		vect_num = 0;
		iov[vect_num].iov_base = h;
		if (first) {
			if (qtables_include) {
				iov[vect_num++].iov_len = 24;
				iov[vect_num].iov_base = qtable;
				iov[vect_num++].iov_len = 128;
			} else {
				iov[vect_num++].iov_len = 20;
			}
			first = false;
		} else {
			iov[vect_num++].iov_len = 20;
		}
		if ((data + packet_len) <= buffer_ptr_end) {
			iov[vect_num].iov_base = data;
			iov[vect_num++].iov_len = packet_len;
			data += packet_len;
		} else {
			// current packet rolls over the end of the buffer, split it and set data pointer to the buffer start
			int overshoot = (data + packet_len) - (unsigned char *)(buffer_ptr + BYTE2DW(buffer_length));
			int packet_len_first = packet_len - overshoot;
			iov[vect_num].iov_base = data;
			iov[vect_num++].iov_len = packet_len_first;

			iov[vect_num].iov_base = buffer_ptr;
			iov[vect_num++].iov_len = overshoot;
			D3(sensor_port, cerr << "Current data packet rolls over the buffer, overshoot: " << overshoot <<
					", packet_len_first: " << packet_len_first << endl);
			data = (unsigned char *)buffer_ptr + overshoot;
		}
		rtp_socket->send_vect(iov, vect_num);

		packet_num++;
		offset += packet_len;
	}
	D3(sensor_port, cerr << "Packets sent: " << packet_num << endl);
	return 1;
}
