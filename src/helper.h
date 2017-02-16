#ifndef __H_HELPER__
#define __H_HELPER__

#include <sys/time.h>

inline long time_delta_us(const struct timeval &tv_1, const struct timeval &tv_2) {
	long tu = tv_1.tv_usec;
	if(tv_1.tv_sec != tv_2.tv_sec)
		tu += (tv_1.tv_sec - tv_2.tv_sec) * 1000000;
	tu -= tv_2.tv_usec;
	return tu;
}

inline struct timeval time_plus(struct timeval tv_1, const struct timeval &tv_2) {
	tv_1.tv_sec += tv_2.tv_sec;
	tv_1.tv_usec += tv_2.tv_usec;
	if(tv_1.tv_usec >= 1000000) {
		tv_1.tv_sec++;
		tv_1.tv_usec -= 1000000;
	}
	return tv_1;
}

inline struct timeval time_minus(struct timeval tv_1, const struct timeval &tv_2) {
	tv_1.tv_sec -= tv_2.tv_sec;
	tv_1.tv_usec += 1000000;
	tv_1.tv_usec -= tv_2.tv_usec;
	if(tv_1.tv_usec >= 1000000) {
		tv_1.tv_usec -= 1000000;
	} else {
		tv_1.tv_sec--;
	}
	return tv_1;
}

#endif
