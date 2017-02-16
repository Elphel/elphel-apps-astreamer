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

#include "parameters.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <asm/elphel/c313a.h>

#include <iostream>
#include "streamer.h"
using namespace std;

#undef PARAMETERS_DEBUG
//#define PARAMETERS_DEBUG

#ifdef PARAMETERS_DEBUG
	#define D(a) a
#else
	#define D(a)
#endif


Parameters *Parameters::_parameters = NULL;

Parameters::Parameters(void) {
	fd_fparmsall = open("/dev/frameparsall", O_RDWR);
	if(fd_fparmsall < 0)
		throw("can't open /dev/frameparsall");
	//! now try to mmap
	frameParsAll = (struct framepars_all_t *) mmap(0, sizeof (struct framepars_all_t) , PROT_READ | PROT_WRITE , MAP_SHARED, fd_fparmsall, 0);
	if((int)frameParsAll == -1)
		throw("Error in mmap /dev/frameparsall");
	framePars = frameParsAll->framePars;
	globalPars = frameParsAll->globalPars;
}

Parameters::~Parameters(void) {
	if(fd_fparmsall > 0)
		close(fd_fparmsall);
}

/**
 * @brief Read either G_* parameter (these are 'regular' values defined by number) or P_* parameter
 *         (it can be read for up to 6 frames ahead, but current interface only allows to read last/current value)
 * @param GPNumber parameter number (as defined in c313a.h), G_* parameters have numbers above FRAMEPAR_GLOBALS, P_* - below)
 * @return parameter value
 */
unsigned long Parameters::getGPValue(unsigned long GPNumber) {
	return (GPNumber >= FRAMEPAR_GLOBALS) ? GLOBALPARS(GPNumber) : framePars[GLOBALPARS(G_THIS_FRAME) & PARS_FRAMES_MASK].pars[GPNumber];
}

/**
 * @brief Set value of the specified global (G_*) parameter
 * @param GNumber - parameter number (as defined in c313a.h)
 * @param value  - value to set
 */
void Parameters::setGValue(unsigned long GNumber, unsigned long value) {
	GLOBALPARS(GNumber) = value;
}

unsigned long Parameters::getFrameValue(unsigned long FPNumber) {
	return framePars[GLOBALPARS(G_THIS_FRAME) & PARS_FRAMES_MASK].pars[FPNumber];
}

bool Parameters::daemon_enabled(void) {
	return((getFrameValue(P_DAEMON_EN) & (1 << DAEMON_BIT_STREAMER)) != 0);
}

void Parameters::setPValue(unsigned long *val_array, int count) {
/*
	long target_frame = params->getGPValue(G_THIS_FRAME) + FRAMES_AHEAD_FPS;
	write_data[0] = FRAMEPARS_SETFRAME;
	write_data[1] = target_frame; /// wait then for that frame to be available on the output plus 2 frames for fps to be stable
	write_data[2] = P_FP1000SLIM;
	write_data[3] = (unsigned long)fps * 1000;
	write_data[4] = P_FPSFLAGS;
	write_data[5] = 3;
*/
//	long rslt = write(fd_fparmsall, write_data, sizeof(write_data));
	this->write(val_array, sizeof(unsigned long) * count);
}

//------------------------------------------------------------------------------
