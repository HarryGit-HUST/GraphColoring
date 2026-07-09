////////////////////////////////
/// usage : 1.	utilities for NP-hard problem solvers.
/// 
/// note  : 1.	this file is a template file.
///			2.	this file will be replaced by standard implementation after submission.
////////////////////////////////

#ifndef CN_HUST_SZX_NPBENCHMARK_UTIL_H
#define CN_HUST_SZX_NPBENCHMARK_UTIL_H


#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#if _WIN32 // for `peakMemoryUsage`.
#include <Windows.h>
#include <Psapi.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#endif // _WIN32


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

struct JobFighter { // lock-free thread pool for homogeneous tasks.
	using IsJobTaken = std::function<bool()>;
	using Job = std::function<void(IsJobTaken isJobTaken)>;

	static constexpr int DefaultThreadStartGapInSec = 1;

	static void fightForJob(int workerNum, Job userJob, int threadStartGapInSec = DefaultThreadStartGapInSec) {
		std::vector<std::thread> workers; workers.reserve(workerNum);
		std::atomic<int> anj = 0; // next free job for all worker.
		for (int w = 0; w < workerNum; ++w) {
			workers.emplace_back([&]() {
				int j = 0; // job that this worker is looking.
				int nj = -1; // next free job reserved for this worker.
				userJob([&]() {
					if (j > nj) { nj = anj++; } // if the reserved job is done, reserve next free job.
					return (j++ != nj); // return whether the looking job is not the reserved job.
				});
			});
			std::this_thread::sleep_for(std::chrono::seconds(threadStartGapInSec));
		}
		for (int w = 0; w < workerNum; ++w) { workers[w].join(); }
	}
};

struct MemoryUsage {
	size_t physicalMemory;
	size_t virtualMemory;

	static size_t bytePeakMemory() {
		size_t mu = 0;

		#if _WIN32
		PROCESS_MEMORY_COUNTERS pmc;
		HANDLE hProcess = GetCurrentProcess();
		if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
			mu = pmc.PeakPagefileUsage;
		}
		CloseHandle(hProcess);
		#else
		#if (__MACH__) || defined(__APPLE__) || defined(__FreeBSD__)
		constexpr size_t Unit = 1; // B.
		#else // linux.
		constexpr size_t Unit = 1024; // KB.
		#endif
		struct rusage rusage;
		getrusage(RUSAGE_SELF, &rusage);
		mu = rusage.ru_maxrss * Unit;
		#endif // _WIN32

		return mu;
	}
};

}


#endif // CN_HUST_SZX_NPBENCHMARK_UTIL_H
