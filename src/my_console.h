#pragma once
//#include "precompiled.h"
#include "stuff.h"
#include <strstream>
class my_console {
public:
	static void beginFrame() {
		//my_cout.str("");
		my_cout = stringstream();
		original_cout_rdbuf = cout.rdbuf(my_cout.rdbuf());
		//cout.rdbuf(my_cout.rdbuf());
	}
	static void clr() {
		clearconsole();
		gotoxy(0, 0);
	}
	static void endFrame() {
		cout.rdbuf(original_cout_rdbuf);
		
		cout << my_cout.str();
		cout.flush();
	}
private:
	static stringstream my_cout;
	static std::streambuf* original_cout_rdbuf;
};