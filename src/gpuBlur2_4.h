#pragma once
#include "precompiled.h"

namespace gpuBlur2_4 {
	gl::Texture run(gl::Texture src, int lvls);
	gl::Texture run_longtail(gl::Texture src, int lvls, float lvlmul);
	float getGaussW();
	float gauss(float f, float width);
	gl::Texture upscale(gl::Texture src, ci::Vec2i toSize);
	gl::Texture upscale(gl::Texture src, float hscale, float vscale);
	gl::Texture singleblur(gl::Texture src, float hscale, float vscale);
}