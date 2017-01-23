#include "precompiled.h"
#include "shade.h"
#include "util.h"

void beginRTT(gl::Texture fbotex)
{
	static unsigned int fboid = 0;
	if(fboid == 0)
	{
		glGenFramebuffersEXT(1, &fboid);
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboid);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, fbotex.getId(), 0);
}
void endRTT()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

gl::Texture Shade::run() {
	auto opts = ShadeOpts().scale(_scaleX, _scaleY);
	if(_ifmt.exists)
		opts.ifmt(_ifmt.val);
	return shade(_texv, _src.c_str(), opts);
}

string removeEndlines(string s)
{
	string s2 = s;
	FOR(i, 0, s2.size() - 1) {
		char& c = s2[i];
		if(c == '\n' || c == '\r')
			c = ' ';
	}
	return s2;
}

map<string, float> globaldict;
void globaldict_default(string s, float f) {
	if(globaldict.find(s) == globaldict.end())
	{
		globaldict[s] = f;
	}
}

gl::Texture shade(vector<gl::Texture> texv, const char* fshader_constChar, ShadeOpts const& opts)
{
	const string fshader0(fshader_constChar);
	/*if(fshader0.find('\t') == string::npos)
		throw 0;*/
	string fshader;
	FOR(i, 0, fshader0.size() - 1) {
		//if(fshader0[i] == '\t' && (i == 0 || fshader0[i-1] != '\t'))
		//	fshader += '\n';
		fshader += fshader0[i];
	}

	auto texIndex = [&](gl::Texture t) {
		return ToString(
			1 + (std::find(texv.begin(), texv.end(), t) - texv.begin())
			);
	};
	auto samplerSuffix = [&](int i) -> string {
		return (i == 0) ? "" : ToString(1 + i);
	};
	auto samplerName = [&](int i) -> string {
		return "tex" + samplerSuffix(i);
	};
	string uniformDeclarations;
	FOR(i,0,texv.size()-1)
	{
		uniformDeclarations += "uniform sampler2D " + samplerName(i) + ";\n";
		uniformDeclarations += "uniform vec2 " + samplerName(i) + "Size;\n";
		uniformDeclarations += "uniform vec2 tsize" + samplerSuffix(i) + ";\n";
	}
	foreach(auto& p, globaldict)
	{
		uniformDeclarations += "uniform float " + p.first + ";\n";
	}
	//cout << "sampler2Ddeclarations: " << sampler2Ddeclarations << endl;
	string intro =
		Str()
		<< "#version 130"
		<< uniformDeclarations
		<< "vec3 _out = vec3(0.0);"
		<< "varying vec2 tc;"
		<< "uniform vec2 mouse;"
		<< "vec4 fetch4(sampler2D tex_, vec2 tc_) {"
		<< "	return texture2D(tex_, tc_).rgba;"
		<< "}"
		<< "vec3 fetch3(sampler2D tex_, vec2 tc_) {"
		<< "	return texture2D(tex_, tc_).rgb;"
		<< "}"
		<< "vec2 fetch2(sampler2D tex_, vec2 tc_) {"
		<< "	return texture2D(tex_, tc_).rg;"
		<< "}"
		<< "float fetch1(sampler2D tex_, vec2 tc_) {"
		<< "	return texture2D(tex_, tc_).r;"
		<< "}"
		<< "vec4 fetch4(sampler2D tex_) {"
		<< "	return texture2D(tex_, tc).rgba;"
		<< "}"
		<< "vec3 fetch3(sampler2D tex_) {"
		<< "	return texture2D(tex_, tc).rgb;"
		<< "}"
		<< "vec2 fetch2(sampler2D tex_) {"
		<< "	return texture2D(tex_, tc).rg;"
		<< "}"
		<< "float fetch1(sampler2D tex_) {"
		<< "	return texture2D(tex_, tc).r;"
		<< "}"
		<< "vec4 fetch4() {"
		<< "	return texture2D(tex, tc).rgba;"
		<< "}"
		<< "vec3 fetch3() {"
		<< "	return texture2D(tex, tc).rgb;"
		<< "}"
		<< "vec2 fetch2() {"
		<< "	return texture2D(tex, tc).rg;"
		<< "}"
		<< "float fetch1() {"
		<< "	return texture2D(tex, tc).r;"
		<< "}"
		<< "vec2 safeNormalized(vec2 v) { return length(v)==0.0 ? v : normalize(v); }"
		<< "vec3 safeNormalized(vec3 v) { return length(v)==0.0 ? v : normalize(v); }"
		<< "vec4 safeNormalized(vec4 v) { return length(v)==0.0 ? v : normalize(v); }";
	string outro =
		Str()
		<< "void main()"
		<< "{"
		<< "	gl_FragColor.a = 1.0;"
		<< "	shade();"
		<< "	gl_FragColor.rgb = _out;"
		<< "}";
	string completeFshader = intro + fshader + outro;
	string completeFshader_noEndlines = removeEndlines(intro) + fshader + outro;
	static map<string, gl::GlslProg> shaders;
	gl::GlslProg shader;
	if(shaders.find(completeFshader) == shaders.end())
	{
		try{
			shader = gl::GlslProg(
				Str()
				<< "varying vec2 tc;"

				<< "void main()"
				<< "{"
				<< "	gl_Position = ftransform();"
				<< "	tc=gl_MultiTexCoord0.xy;"
				<< "}",
				completeFshader/*_noEndlines*/.c_str()
				);
			shaders[completeFshader] = shader;
			cout << "compiling" << endl;
		} catch(gl::GlslProgCompileExc const& e) {
			cout << "gl::GlslProgCompileExc: " << e.what() << endl;
			cout << "source:" << endl;
			cout << completeFshader << endl;
			throw;
		}
	} else {
		shader = shaders[completeFshader];
	}
	auto tex0 = texv[0];
	shader.bind();
	auto app=ci::app::AppBasic::get();
	float mouseX=app->getMousePos().x/float(app->getWindowWidth());
	float mouseY=app->getMousePos().y/float(app->getWindowHeight());
	shader.uniform("mouse", Vec2f(mouseX, mouseY));
	shader.uniform("tex", 0); tex0.bind(0);
	shader.uniform("texSize", Vec2f(tex0.getSize()));
	shader.uniform("tsize", Vec2f::one()/Vec2f(tex0.getSize()));
	foreach(auto& p, globaldict)
	{
		shader.uniform(p.first, p.second);
	}
	
	FOR(i, 1, texv.size()-1) {
		//shader.
		//string index = texIndex(texv[i]);
		shader.uniform(samplerName(i), i); texv[i].bind(i);
		shader.uniform(samplerName(i) + "Size", Vec2f(texv[i].getSize()));
		shader.uniform("tsize"+samplerSuffix(i), Vec2f::one()/Vec2f(texv[i].getSize()));
	}
	gl::Texture::Format fmt;
	fmt.setInternalFormat(opts._ifmt.exists ? opts._ifmt.val : tex0.getInternalFormat());
	gl::Texture result(tex0.getWidth() * opts._scaleX, tex0.getHeight() * opts._scaleY, fmt);
	beginRTT(result);
	
	gl::pushMatrices();
	glPushAttrib(GL_VIEWPORT_BIT);
	gl::setMatricesWindow(result.getSize(), false);
	gl::draw(tex0, result.getBounds());
	glPopAttrib();
	gl::popMatrices();

	endRTT();

	gl::GlslProg::unbind();

	return result;
}
