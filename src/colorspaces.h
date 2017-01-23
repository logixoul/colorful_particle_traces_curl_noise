#pragma once
#include "precompiled.h"

struct HslF
{
	float h, s, l;
	HslF(float h, float s, float l);
	HslF(ci::Vec3f const& rgb);
};

Vec3f FromHSL(HslF const& hsl);