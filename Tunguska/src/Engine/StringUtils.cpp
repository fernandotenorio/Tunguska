#include "Engine/StringUtils.h"
#include <cstddef>
#include <algorithm>
#include <cctype>

int fileIndex(std::string file){
	if (file == "a") return 0;
	if (file == "b") return 1;
	if (file == "c") return 2;
	if (file == "d") return 3;
	if (file == "e") return 4;
	if (file == "f") return 5;
	if (file == "g") return 6;
	if (file == "h") return 7;
	return -1;
}

bool startsWith(std::string str, std::string prefix){
    return str.substr(0, prefix.size()) == prefix;
}

std::string trim(const std::string &s)
{
   auto wsfront = std::find_if_not(s.begin(),s.end(),[](int c){return std::isspace(c);});
   auto wsback = std::find_if_not(s.rbegin(),s.rend(),[](int c){return std::isspace(c);}).base();
   return (wsback<=wsfront ? std::string() : std::string(wsfront,wsback));
}

bool stringContains(std::string s1, std::string s2){
	if (s1.find(s2) != std::string::npos) 
		return true;
	return false;
}

std::vector<std::string> splitString(std::string s, std::string delim){
	std::vector<std::string> tokens;
	std::size_t start = 0;
	std::size_t end = s.find(delim);
	
    while (end != std::string::npos){
        tokens.push_back(s.substr(start, end - start));
        start = end + delim.length();
        end = s.find(delim, start);
    }

    tokens.push_back(s.substr(start, end));
	return tokens;
}

std::string toLower(std::string s){
	std::string lower;
	lower.resize(s.size());
	std::transform(s.begin(), s.end(), lower.begin(), ::tolower);
	return lower;
}

std::string toUpper(std::string s){
	std::string lower;
	lower.resize(s.size());
	std::transform(s.begin(), s.end(), lower.begin(), ::toupper);
	return lower;
}

int parseIntChar(char c){
	if (c == '0')return 0;
	if (c == '1')return 1;
	if (c == '2')return 2;
	if (c == '3')return 3;
	if (c == '4')return 4;
	if (c == '5')return 5;
	if (c == '6')return 6;
	if (c == '7')return 7;
	if (c == '8')return 8;
	if (c == '9')return 9;
	return -1;
}

int parseIntString(std::string s){
	for (int i = 0; i < 10; i++){
		if (intToString(i) == s)
			return i;
	}
	return -1;
}

std::string intToString(int i){
	if (i == 0) return "0";
	if (i == 1) return "1";
	if (i == 2) return "2";
	if (i == 3) return "3";
	if (i == 4) return "4";
	if (i == 5) return "5";
	if (i == 6) return "6";
	if (i == 7) return "7";
	if (i == 8) return "8";
	if (i == 9) return "9";
	return "";
}
