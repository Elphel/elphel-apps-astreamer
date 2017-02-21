/**
 * @file parameters.h
 * @brief Provides interface to global camera parameters
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

#ifndef _PARS__H_
#define _PARS__H_

#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <elphel/c313a.h>

using namespace std;

class Parameters {
public:
//	inline static Parameters *instance(void) {
//		if(_parameters == NULL)
//			_parameters = new Parameters();
//		return _parameters;
//	}
	Parameters(int port);
	~Parameters(void);

	/// interface to global camera parameters 
	unsigned long getGPValue(unsigned long GPNumber);
	void setGValue(unsigned long GNumber, unsigned long value);
	unsigned long getFrameValue(unsigned long FPNumber);
	int write(unsigned long *data, int length) { return ::write(fd_fparmsall, (void *)data, length); }
	off_t lseek(off_t offset, int whence) { return ::lseek(fd_fparmsall, offset, whence); }
	bool daemon_enabled(void);
	void setPValue(unsigned long *val_array, int count);
protected:
//	static Parameters *_parameters;

	struct framepars_all_t *frameParsAll;
	struct framepars_t *framePars;
	unsigned long *globalPars;
	int fd_fparmsall;
	int sensor_port;
};

#endif // _VIDEO__H_
