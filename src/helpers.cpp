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
