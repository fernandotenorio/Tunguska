#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <vector>
#include <string>

bool startsWith(std::string str, std::string prefix);
std::vector<std::string> splitString(std::string s, std::string delim);
std::string trim(const std::string &s);
bool stringContains(std::string s1, std::string s2);
int parseIntString(std::string s);
int parseIntChar(char c);
std::string intToString(int i);
std::string toLower(std::string s);
std::string toUpper(std::string s);
int fileIndex(std::string file);

#endif