#ifndef _HELPERS__H_
#define _HELPERS__H_

#include <string>
#include <list>
#include <map>

using namespace std;

class String {
public:
	static bool split(const string &str, char delimiter, string &left, string &right);
	static list<string> split_to_list(string str, char delimiter);
	static map<string, string> split_list_to_map(const list<string> &l, char delimiter);
};

#endif // _HELPERS__H_
