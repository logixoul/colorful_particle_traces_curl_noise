#ifndef MAINFUNC_IMPL_H
#define MAINFUNC_IMPL_H

namespace cinder {
	namespace app {
		class AppBasic;
	}
}

// usage:
// int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
// 		return mainFuncImpl(new SApp());
// }
int mainFuncImpl(cinder::app::AppBasic* app);

#endif