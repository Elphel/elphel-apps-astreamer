#include "helpers.h"
#include <iostream>
using namespace std;

bool String::split(const string &str, char delimiter, string &left, string &right) {
	left = "";
	right = "";
	int i = str.find(delimiter);
//cerr << "_______" << endl;
//cerr << "delimiter == |" << delimiter << "|, i == " << i << endl;
	if(i > 0 && i < (int)str.length()) {
		const char *c = str.c_str();
//cerr << 
		left.insert(0, c, i);
		right.insert(0, &c[i + 1], str.length() - i - 1);
		return true;
	} else
		left = str;
	return false;
}

list<string> String::split_to_list(string str, char delimiter) {
	list<string> l;
	if((str.c_str())[str.length() - 1] != delimiter)
		str += delimiter;
	const char *c = str.c_str();
	int j = 0;
	for(int i = 0; c[i]; i++) {
		if(c[i] == delimiter) {
			string a;
			a.insert(0, &c[j], i - j);
			j = i + 1;
			l.push_back(a);
		}
	}
	return l;
}

map<string, string> String::split_list_to_map(const list<string> &l, char delimiter) {
	map<string, string> m;
	for(list<string>::const_iterator it = l.begin(); it != l.end(); it++) {
		int i = (*it).find(delimiter);
		if(i > 0 && i < (int)(*it).length()) {
			const char *c = (*it).c_str();
			string n, v;
			n.insert(0, c, i);
			v.insert(0, &c[i + 1], (*it).length() - i - 1);
			m[n] = v;
		}
	}
	return m;
}
