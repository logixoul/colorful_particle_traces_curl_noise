#pragma once

#include "precompiled.h"
#include "util.h"

void beginRTT(gl::Texture fbotex);
void endRTT();

struct Str {
	string s;
	Str& operator<<(string s2) {
		s += s2 + "\n";
		return *this;
	}
	Str& operator<<(Str s2) {
		s += s2.s + "\n";
		return *this;
	}
	operator const char*() {
		return s.c_str();
	}
};

extern std::map<string, float> globaldict;
void globaldict_default(string s, float f);
template<class T>
struct optional {
	T val;
	bool exists;
	optional(T const& t) { val=t; exists=true; }
	optional() { exists=false; }
};
struct ShadeOpts
{
	ShadeOpts(){
		//_ifmt=GL_RGBA16F;
		_scaleX = _scaleY = 1.0f;
	}
	ShadeOpts& ifmt(GLenum val) { _ifmt=val; return *this; }
	ShadeOpts& scale(float val) { _scaleX=val; _scaleY=val; return *this; }
	ShadeOpts& scale(float valX, float valY) { _scaleX=valX; _scaleY=valY; return *this; }
	//ShadeOpts& tex(gl::Texture val) { _texv.push_back(val); }
	optional<GLenum> _ifmt;
	float _scaleX, _scaleY;
	//vector<gl::Texture> _texv;
};
//typedef ShadeOpts Shade;
struct Shade
{
public:
	Shade()
	{
		_scaleX = _scaleY = 1.0f;
	}
	Shade& src(string val) { _src = val; return *this; }
	Shade& expr(string val) {
		_src = "void shade() {";
		_src += "_out = " + val + ";";
		_src += "}";
		return *this;
	}
	Shade& tex(gl::Texture val) { _texv.push_back(val); return *this; }
	Shade& ifmt(GLenum val) { _ifmt=val; return *this; }
	Shade& scale(float val) { _scaleX=val; _scaleY=val; return *this; }
	Shade& scale(float valX, float valY) { _scaleX=valX; _scaleY=valY; return *this; }
	Shade& operator()(gl::Texture val) { tex(val); return *this; }

	gl::Texture run();

	string _src;
	vector<gl::Texture> _texv;
	optional<GLenum> _ifmt;
	float _scaleX, _scaleY;
};
gl::Texture shade(vector<gl::Texture> texv, const char* fshader_constChar, ShadeOpts const& opts=ShadeOpts());
inline gl::Texture shade(vector<gl::Texture> texv, const char* fshader_constChar, float resScale)
{
	return shade(texv, fshader_constChar, ShadeOpts().scale(resScale));
}
namespace shade_h_detail {
	namespace {
		gl::Texture::Format get_gtexfmt() { gl::Texture::Format f; f.setInternalFormat(GL_RGB32F); return f; };
		gl::Texture::Format gtexfmt = get_gtexfmt();
	}
}
#if 0
inline gl::Texture gtex(Array2D<float> a)
{
	gl::Texture tex(a.w, a.h, shade_h_detail::gtexfmt);
	tex.bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, a.w, a.h, GL_LUMINANCE, GL_FLOAT, a.data);
	return tex;
}
/*gl::Texture gtex(Array2D<Vec2f> a)
{
	gl::Texture tex(a.w, a.h, shade_h_detail::gtexfmt);
	tex.bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, a.w, a.h, GL_RG, GL_FLOAT, a.data);
	return tex;
}*/
inline gl::Texture gtex(Array2D<Vec3f> a)
{
	gl::Texture tex(a.w, a.h, shade_h_detail::gtexfmt);
	tex.bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, a.w, a.h, GL_RGB, GL_FLOAT, a.data);
	return tex;
}
#endif