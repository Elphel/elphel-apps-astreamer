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
	int ret_val;

	for (size_t i = 0; i < sz; i++) {
		ret_val = pthread_cancel(threads[i]);
		if (!ret_val)
			cout << "pthread_cancel returned " << ret_val << ", sensor port " << i << endl;
	}
}

/**
 * Print help message on stdout.
 * @param   argv   a list of command-line arguments, used to get the name of application
 * @return  None
 */
void print_help(char *argv[])
{
	const char *msg = "Simple RTSP streamer implementation for Elphel393 series cameras.\n"
			"Usage: %s [-s <port>][-D <sound_card>][-f <fps>][-h], where\n\n"
			"\t-h\t\tprint this help message;\n"
			"\t-s <port>\tstream sound from USB microphone over sensor port <port> channel. By default, audio streaming is not\n"
			"\t\t\tenabled if this option is not specified;\n"
			"\t-D <sound_card>\tuse <sound_card> device for sound stream input. The default device is plughw:0,0;\n"
			"\t-f <fps>\tlimit frames per second for video streaming, works for free running mode only.\n"; // this one is processed in streamer class
	printf(msg, argv[0]);
}

int main(int argc, char *argv[]) {
	int ret_val;
	int audio_port = -1;
	string opt;
	map<string, string> args;
	map<string, string>::iterator args_it;
	pthread_t threads[SENSOR_PORTS];
	Streamer *streamers[SENSOR_PORTS] = {NULL};

	// copy command-line arguments to a map container for further processing in streamer class
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

	cout << "Parsed command line arguments:" << endl;
	for (map<string, string>::iterator it = args.begin(); it != args.end(); it++) {
		cerr << "|" << (*it).first << "| == |" << (*it).second << "|" << endl;
	}

	if ((args_it = args.find("h")) != args.end()) {
		print_help(argv);
		exit(EXIT_SUCCESS);
	} else if ((args_it = args.find("s")) != args.end()) {
		audio_port = strtol(args_it->second.c_str(), NULL, 10);
		// sanity check, invalid conversion produces 0 which is fine
		if (audio_port < 0 || audio_port >= SENSOR_PORTS)
			audio_port = -1;
	}

	for (int i = 0; i < SENSOR_PORTS; i++) {
		bool audio_en;
		pthread_attr_t attr;

		if (i == audio_port)
			audio_en = true;
		else
			audio_en = false;

		cout << "Starting a new streamer thread for sensor port " << i << endl;

		streamers[i] = new Streamer(args, i, audio_en);
		pthread_attr_init(&attr);
		ret_val = pthread_create(&threads[i], &attr, Streamer::pthread_f, (void *) streamers[i]);
		if (ret_val != 0) {
			cerr << "Can not spawn streamer thread for port " << i;
			cerr << ", pthread_create returned " << ret_val << endl;
			clean_up(threads, SENSOR_PORTS);
			exit(EXIT_FAILURE);
		}
		pthread_attr_destroy(&attr);
	}

	for (size_t i = 0; i < SENSOR_PORTS; i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}
