#include "precompiled.h"
#include "my_console.h"
#include "stuff.h"

stringstream my_console::myStream;

void my_console::beginFrame() {
	myStream = stringstream();
}

void my_console::clr() {
	clearconsole();
	gotoxy(0, 0);
}

void my_console::endFrame() {
	cout << myStream.str();
	cout.flush();
}

QDebug qDebug() {
	return QDebug();
}
