#pragma once
#include "precompiled.h"
#include <strstream>

class my_console {
public:
	static void beginFrame();
	static void clr();
	static void endFrame();
	template<class T>
	static void print(T value) {
		myStream << value;
	}
private:
	static stringstream myStream;
};

class QDebug {
public:
	QDebug() { }
	~QDebug() {
		my_console::print('\n');
	}
};
template<class T>
QDebug& operator<<(QDebug& prev, T value) {
	my_console::print(value);
	return prev;
}

extern QDebug qDebug();