#include "precompiled.h"
#include <cinder/app/AppBasic.h>
#include "util.h"

int mainFuncImpl(cinder::app::AppBasic* app) {
	try {
		createConsole();
		cinder::app::AppBasic::prepareLaunch();														
		cinder::app::Renderer *ren = new RendererGl;													
		cinder::app::AppBasic::executeLaunch( app, ren, "SApp" );										
		cinder::app::AppBasic::cleanupLaunch();														
	}catch(ci::gl::GlslProgCompileExc const& e) {
		cout << "caught: " << endl << e.what() << endl;
		system("pause");
	}
	return 0;																					
}