/**
 * @file parameters.cpp
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

#include "parameters.h"
//#include "streamer.h"

using namespace std;

//#undef PARAMETERS_DEBUG
#define PARAMETERS_DEBUG

#ifdef PARAMETERS_DEBUG
	#define D(a) a
#else
	#define D(a)
#endif

//Parameters *Parameters::_parameters = NULL;
static const char *ctl_file_names[] = {
		DEV393_PATH(DEV393_FRAMEPARS0), DEV393_PATH(DEV393_FRAMEPARS1),
		DEV393_PATH(DEV393_FRAMEPARS2), DEV393_PATH(DEV393_FRAMEPARS3)
};

Parameters::Parameters(int port) {
	string err_msg;

	if ((port >= 0) && (port < SENSOR_PORTS)) {
		sensor_port = port;
	} else {
		string port_str = static_cast<ostringstream &>((ostringstream() << dec << port)).str();
		err_msg = "port number specified is invalid: " + port_str;
		throw invalid_argument(err_msg);
	}

	fd_fparmsall = open(ctl_file_names[sensor_port], O_RDWR);
	if (fd_fparmsall < 0) {
		err_msg = "can not open " + *ctl_file_names[sensor_port];
		throw std::runtime_error(err_msg);
	}
	frameParsAll = (struct framepars_all_t *) mmap(0,
			sizeof(struct framepars_all_t), PROT_READ | PROT_WRITE, MAP_SHARED,
			fd_fparmsall, 0);
	if ((int) frameParsAll == -1) {
		frameParsAll = NULL;
		err_msg = "Error in mmap " + *ctl_file_names[sensor_port];
		throw std::invalid_argument(err_msg);
	}
	framePars = frameParsAll->framePars;
	globalPars = frameParsAll->globalPars;
}

Parameters::~Parameters(void) {
	if (frameParsAll != NULL) {
		munmap(frameParsAll, sizeof(struct framepars_all_t));
		frameParsAll = NULL;
	}
	if (fd_fparmsall > 0)
		close(fd_fparmsall);
}

/**
 * @brief Read either G_* parameter (these are 'regular' values defined by number) or P_* parameter
 *         (it can be read for up to 6 frames ahead, but current interface only allows to read last/current value)
 * @param GPNumber parameter number (as defined in c313a.h), G_* parameters have numbers above FRAMEPAR_GLOBALS, P_* - below)
 * @return parameter value
 */
unsigned long Parameters::getGPValue(unsigned long GPNumber) {
	return (GPNumber >= FRAMEPAR_GLOBALS) ? GLOBALPARS_SNGL(GPNumber) :
			framePars[GLOBALPARS_SNGL(G_THIS_FRAME) & PARS_FRAMES_MASK].pars[GPNumber];
}

/**
 * @brief Set value of the specified global (G_*) parameter
 * @param GNumber - parameter number (as defined in c313a.h)
 * @param value  - value to set
 */
void Parameters::setGValue(unsigned long GNumber, unsigned long value) {
	GLOBALPARS_SNGL(GNumber) = value;
}

unsigned long Parameters::getFrameValue(unsigned long FPNumber) {
	return framePars[GLOBALPARS_SNGL(G_THIS_FRAME) & PARS_FRAMES_MASK].pars[FPNumber];
}

bool Parameters::daemon_enabled(void) {
	return((getFrameValue(P_DAEMON_EN) & (1 << DAEMON_BIT_STREAMER)) != 0);
}

void Parameters::setPValue(unsigned long *val_array, int count) {
	this->write(val_array, sizeof(unsigned long) * count);
}

/**
 * @brief Get current FPGA time
 * @return   Time value in \e timeval structure
 */
struct timeval Parameters::get_fpga_time(void)
{
	struct timeval tv;
	unsigned long write_data[] = {FRAMEPARS_GETFPGATIME, 0};

	write(write_data, sizeof(unsigned long) * 2);
	tv.tv_sec = getGPValue(G_SECONDS);
	tv.tv_usec = getGPValue(G_MICROSECONDS);

	return tv;
}
