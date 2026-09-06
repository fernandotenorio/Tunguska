#pragma once

namespace Tune {
#ifdef TUNGUSKA_SPSA
#define PARAM(name, def, lo, hi, scale, group, description) inline int name = def;
#else
#define PARAM(name, def, lo, hi, scale, group, description) inline constexpr int name = def;
#endif
#include "parameters.def"
#undef PARAM
}

#ifdef TUNGUSKA_SPSA
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
namespace Tune {
struct Entry { const char* name; int* value; int initial; int minimum; int maximum; };
inline Entry entries[] = {
#define PARAM(name, def, lo, hi, scale, group, description) {"SPSA_" #name, &name, def, lo, hi},
#include "parameters.def"
#undef PARAM
};
inline std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline void printOptions() {
    for (const auto& e : entries)
        std::cout << "option name " << e.name << " type spin default " << e.initial
                  << " min " << e.minimum << " max " << e.maximum << std::endl;
}
inline Entry* find(const std::string& name) {
    for (auto& e : entries) if (lower(e.name) == name) return &e;
    return nullptr;
}
}
#endif
