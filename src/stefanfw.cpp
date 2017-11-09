#include "precompiled.h"
#include "stefanfw.h"

float mouseX, mouseY;
bool keys[256];
bool keys2[256];
bool mouseDown_[3];

namespace stefanfw {

	EventHandler eventHandler;

	void beginFrame() {
		ci::app::AppBase* app = ci::app::App::get();

		sw::beginFrame();

		wsx = app->getWindowWidth();
		wsy = app->getWindowHeight();

		auto relMousePos = app->getMousePos() - app->getWindowPos();
		::mouseX = relMousePos.x / (float)wsx;
		::mouseY = relMousePos.y / (float)wsy;
	}

	void endFrame() {
		sw::endFrame();
		cfg1::print();
		std::cout << std::flush;
	}

	// todo make this take ref
	bool EventHandler::keyDown(KeyEvent e) {
		keys[e.getChar()] = true;
		if(e.isControlDown()&&e.getCode()!=KeyEvent::KEY_LCTRL)
		{
			keys2[e.getChar()] = !keys2[e.getChar()];
		}
		return true;
	}

	bool EventHandler::keyUp(KeyEvent e) {
		keys[e.getChar()] = false;
		return true;
	}

	bool EventHandler::mouseDown(MouseEvent e)
	{
		mouseDown_[e.isLeft() ? 0 : e.isMiddle() ? 1 : 2] = true;
		return true;
	}
	bool EventHandler::mouseUp(MouseEvent e)
	{
		mouseDown_[e.isLeft() ? 0 : e.isMiddle() ? 1 : 2] = false;
		return true;
	}

	void EventHandler::subscribeToEvents(ci::app::App& app) {
		auto window = app.getWindow();
		window->getSignalKeyDown().connect([&](KeyEvent& e) { keyDown(e); });
		window->getSignalKeyUp().connect([&](KeyEvent& e) { keyUp(e); });
		window->getSignalMouseDown().connect([&](MouseEvent& e) { mouseDown(e); });
		window->getSignalMouseUp().connect([&](MouseEvent& e) { mouseUp(e); });
	}

} // namespace stefanfw