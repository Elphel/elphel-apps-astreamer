/**
 * @file main.cpp
 * @brief Spawn single instance of streamer for each sensor port.
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

#include <iostream>
#include <string>
#include <map>

#include "streamer.h"

using namespace std;

#include <unistd.h>
#include <linux/sysctl.h>
#include <elphel/c313a.h>
#include <pthread.h>

/**
 * Unconditionally cancel all threads.
 * @param   threads   an array of thread pointers
 * @return  None
 */
void clean_up(pthread_t *threads, size_t sz) {
	for (size_t i = 0; i < sz; i++)
		pthread_cancel(threads[i]);
}

int main(int argc, char *argv[]) {
	string opt;
	map<string, string> args;
	pthread_t threads[SENSOR_PORTS];
	Streamer *streamers[SENSOR_PORTS] = {NULL};

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] != '\0') {
			if (opt != "")
				args[opt] = "";
			opt = &argv[i][1];
			continue;
		} else {
			if (opt != "") {
				args[opt] = argv[i];
				opt = "";
			}
		}
	}
	if (opt != "")
		args[opt] = "";
	for (map<string, string>::iterator it = args.begin(); it != args.end(); it++) {
		cerr << "|" << (*it).first << "| == |" << (*it).second << "|" << endl;
	}

	for (int i = 0; i < SENSOR_PORTS; i++) {
		pthread_attr_t attr;
		streamers[i] = new Streamer(args, i);

		pthread_attr_init(&attr);
		if (!pthread_create(&threads[i], &attr, Streamer::pthread_f, (void *) streamers[i])) {
			cerr << "Can not spawn streamer thread for port " << i << endl;
			clean_up(threads, SENSOR_PORTS);
			exit(EXIT_FAILURE);
		}
		pthread_attr_destroy(&attr);
	}
	for (size_t i = 0; i < SENSOR_PORTS; i++)
		pthread_join(threads[i], NULL);

	return 0;
}
