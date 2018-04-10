/*
 * file_util.h
 *
 *  Created on: 26/05/2011
 *      Author: sachetto
 */

#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_

#include <vector>
#include <string>
#include <iostream>

int getFilesFromDir(std::string dir, std::vector<std::string> &files, std::string filter);
int createDir(const char* dirName);

#endif /* FILE_UTIL_H_ */
