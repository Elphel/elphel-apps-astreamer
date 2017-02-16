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
