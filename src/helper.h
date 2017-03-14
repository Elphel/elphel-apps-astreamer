/**
 * @file helper.h
 * @brief Various time helper functions to work with \e timeval structures
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

/**
 * @brief Add microseconds to a value represented by \e timeval structure
 * @param   tv   time to which microseconds should be added
 * @param   us   the value of microseconds
 * @return  The sum of two values
 */
inline struct timeval time_plus_us(struct timeval &tv, unsigned long us)
{
	tv.tv_usec += us;
	tv.tv_sec += tv.tv_usec / 1000000;
	tv.tv_usec = tv.tv_usec % 1000000;

	return tv;
}

#endif /* __H_HELPER__ */
