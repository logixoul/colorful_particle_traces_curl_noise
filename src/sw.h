#pragma once
#include "precompiled.h"
#include "util.h"
#include <map>
#include <string>

struct sw {
	struct Entry {
		int index;
		string desc;
		float time;
	};
	static void start() { Stopwatch::Start(); }
	static void printElapsed(string desc = "") {
		cout << desc << " took " << Stopwatch::GetElapsedMilliseconds() << "ms" << endl;
	}
	static void timeit(string desc, std::function<void()> func) {
		start();
		func();
		if(times.find(desc) == times.end())
		{
			Entry entry;
			entry.index = times.size();
			entry.desc = desc;
			entry.time = 0.0f;
			times[desc] = entry;
		}
		times[desc].time += Stopwatch::GetElapsedMilliseconds();
	}
	static void beginFrame() {
		times.clear();
	}
	static void endFrame() {
		cout << "=== TIMINGS ===" << endl;
		vector<Entry> ordered(times.size());
		foreach(auto& pair, times) {
			//cout << pair.first << " took " << pair.second.time << "ms" << endl;
			Entry entry = pair.second;
			ordered[entry.index] = entry;
		}
		foreach(auto& entry, ordered) {
			cout << entry.desc << " took " << entry.time << "ms" << endl;
		}
	}
	static std::map<string, Entry> times;
};
