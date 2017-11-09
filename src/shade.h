#pragma once

#include "precompiled.h"
#include "util.h"

void beginRTT(gl::TextureRef fbotex);
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
	operator std::string() {
		return s;
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
	//ShadeOpts& tex(gl::TextureRef val) { _texv.push_back(val); }
	optional<GLenum> _ifmt;
	float _scaleX, _scaleY;
	//vector<gl::TextureRef> _texv;
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
	Shade& tex(gl::TextureRef val) { _texv.push_back(val); return *this; }
	Shade& ifmt(GLenum val) { _ifmt=val; return *this; }
	Shade& scale(float val) { _scaleX=val; _scaleY=val; return *this; }
	Shade& scale(float valX, float valY) { _scaleX=valX; _scaleY=valY; return *this; }
	Shade& operator()(gl::TextureRef val) { tex(val); return *this; }

	gl::TextureRef run();

	string _src;
	vector<gl::TextureRef> _texv;
	optional<GLenum> _ifmt;
	float _scaleX, _scaleY;
};
gl::TextureRef shade(vector<gl::TextureRef> const& texv, const char* fshader_constChar, ShadeOpts const& opts=ShadeOpts());
inline gl::TextureRef shade(vector<gl::TextureRef> const& texv, const char* fshader_constChar, float resScale)
{
	return shade(texv, fshader_constChar, ShadeOpts().scale(resScale));
}
