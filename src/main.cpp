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
