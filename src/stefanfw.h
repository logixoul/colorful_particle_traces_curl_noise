#pragma once
#include "precompiled.h"
#include "cfg1.h"
#include "qdebug.h"
#include "sw.h"

extern float mouseX, mouseY;
extern bool keys[256];
extern bool keys2[256];
extern bool mouseDown_[3];
extern int wsx, wsy; // define and initialize those in main.cpp

// stefan's framework
namespace stefanfw {
	void beginFrame();
	void endFrame();
	struct EventHandler {
		bool keyDown(KeyEvent e);
		bool keyUp(KeyEvent e);
		bool mouseDown(MouseEvent e);
		bool mouseUp(MouseEvent e);
		void subscribeToEvents(ci::app::App& app);
	} extern eventHandler;
};