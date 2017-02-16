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

#include <iostream>
#include <string>
#include <map>

#include "streamer.h"

using namespace std;

#include <unistd.h>
#include <linux/sysctl.h>

int main(int argc, char *argv[]) {
	string opt;
	map<string, string> args;
	for(int i = 1; i < argc; i++) {
		if(argv[i][0] == '-' && argv[i][1] != '\0') {
			if(opt != "")
				args[opt] = "";
			opt = &argv[i][1];
			continue;
		} else {
			if(opt != "") {
				args[opt] = argv[i];
				opt = "";
			}
		}
	}
	if(opt != "")
		args[opt] = "";
	for(map<string, string>::iterator it = args.begin(); it != args.end(); it++) {
		cerr << "|" << (*it).first << "| == |" << (*it).second << "|" << endl;
	}

	Streamer *streamer = new Streamer(args);
	streamer->Main();
	return 0;
}
