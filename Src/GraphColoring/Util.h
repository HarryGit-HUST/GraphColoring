////////////////////////////////
/// usage : 1.	utilities for graph coloring solver.
/// 
/// note  : 1.	this file is a template file.
///			2.	this file will be replaced by standard implementation after submission.
////////////////////////////////

#ifndef CN_HUST_SZX_NPBENCHMARK_UTIL_H
#define CN_HUST_SZX_NPBENCHMARK_UTIL_H


#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>


namespace szx {

struct File {
    static std::string extractFileName(const std::string& path) {
        size_t i = path.size();
        while (--i >= 0) {
            if ((path[i] == '\\') || (path[i] == '/')) { return path.substr(i + 1); }
        }
        return path;
    }

	static std::string readAllByte(const std::string& path) {
		std::string s;

		std::ifstream ifs(path, std::ios::ate | std::ios::binary);
		if (!ifs.is_open()) { return s; }

		size_t fsize = static_cast<size_t>(ifs.tellg());
		ifs.seekg(0);
		s.reserve(fsize + 1);
		s.resize(fsize);
		ifs.read(&s[0], fsize);

		return s;
	}
};

struct Timer {
    // duration in milliseconds.
    static long long msDuration(const std::chrono::steady_clock::time_point& begin, const std::chrono::steady_clock::time_point& end) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    }
};

}


#endif // CN_HUST_SZX_NPBENCHMARK_UTIL_H
