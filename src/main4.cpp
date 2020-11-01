#include "precompiled.h"
#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_4.h"
#include "cfg1.h"
#include "sw.h"
#include "stefanfw.h"


#include <float.h>
#include "simplexnoise.h"

#include "colorspaces.h"
#include "easyfft.h"
//#include <opencv2/videoio.hpp>

// baseline: 18fps

int wsx=1280, wsy = 720;
int scale = 1;
int sx() { return wsx / ::scale; }
int sy() { return wsy / ::scale; }


float noiseTimeDim = 0.0f;
const int MAX_AGE = 200;


bool pause;


vec3 complexToColor_HSV(vec2 comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float lightness = length(comp);
	lightness = .5f;
	//lightness /= lightness + 1.0f;
	HslF hsl(hue, 1.0f, lightness);
 	return FromHSL(hsl);
}


vec3 complexToColor(vec2 comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float lightness = length(comp);
	//lightness = .5f;
	//lightness /= lightness + 1.0f;
	//HslF hsl(hue, 1.0f, lightness);
 	//return FromHSL(hsl);
	vec3 colors[] = {
		//vec3(0,0,0),
		vec3(15,131,174),
		vec3(246,24,199),
		vec3(148,255,171),
		vec3(251,253,84)
	};
	int colorCount = 4;
	float pos = hue*(colorCount - .01f);
	int posint1 = floor(pos);
	int posint2 = (posint1 + 1) % colorCount;
	float posFract = pos - posint1;
	vec3 color1 = colors[posint1];
	vec3 color2 = colors[posint2];
	return lerp(color1, color2, posFract) / 255.0f;
}

int numDetailsX = 5;
float nscale = numDetailsX / (float)sx();

struct Walker {
	vec2 pos;
	int age;
	vec3 color;
	vec2 lastMove;

	float alpha() {
		return min((age/(float)MAX_AGE)*5.0, 1.0);
	}

	Walker() {
		pos = vec2(ci::randFloat(0, sx()), ci::randFloat(0, sy()));
		age = ci::randInt(0, MAX_AGE);
	}
	static float noiseXAt(vec2 p, float z) {
		float noiseX = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, z);
		return noiseX;
	}
	
	static float noiseYAt(vec2 p, float z) {
		float noiseY = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale + numDetailsX, z);
		return noiseY;
	}
	static vec2 noisevec2At(vec2 p, float z) {
		return vec2(noiseXAt(p, z), noiseYAt(p, z));
	}
	static vec2 curlNoisevec2At(vec2 p, float z) {
		float eps = 1;
		float noiseXAbove = noiseXAt(p - vec2(0, eps), z);
		float noiseXBelow = noiseXAt(p + vec2(0, eps), z);
		float noiseYOnLeft = noiseYAt(p - vec2(eps, 0), z);
		float noiseYOnRight = noiseYAt(p + vec2(eps, 0), z);
		return vec2(noiseXBelow - noiseXAbove, -(noiseYOnRight - noiseYOnLeft)) / (2.0f * eps);
	}
	void update() {
		vec2 toAdd = curlNoisevec2At(pos, noiseTimeDim) * 50.0f;
		//toAdd.y -= 1.0f;
		pos += toAdd / float(::scale);
		color = complexToColor_HSV(toAdd);
		lastMove = toAdd;
		//color = vec3::one();

		if(pos.x < 0) pos.x += sx();
		if(pos.y < 0) pos.y += sy();
		pos.x = fmod(pos.x, sx());
		pos.y = fmod(pos.y, sy());

		age++;
	}
};

vector<Walker> walkers;

void updateConfig() {
}

struct SApp : App {
	//cv::VideoWriter videoWriter = cv::VideoWriter("outVideo", CV_FOURCC_MACRO('P', 'I', 'M', '1'), 30, cv::Size(1280,720), true);
	void setup()
	{
		enableDenormalFlushToZero();

		//createConsole();
		disableGLReadClamp();
		stefanfw::eventHandler.subscribeToEvents(*this);
		//setWindowSize(wsx, wsy);
		setFullScreen(true);
		getWindow()->setBorderless(true);
		wsx = getWindowWidth();
		wsy = getWindowHeight();


		for(int i = 0; i < 4000 / sq(::scale); i++) {
			walkers.push_back(Walker());
		}
	}
	void keyDown(KeyEvent e)
	{
		
		if(keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	float noiseProgressSpeed;
	
	void update()
	{
		stefanfw::beginFrame();
		stefanUpdate();
		stefanDraw();
		stefanfw::endFrame();
	}


	void stefanUpdate() {
		noiseProgressSpeed = cfg1::getOpt("noiseProgressSpeed", .00008f,
			[&]() { return keys['s']; },
			[&]() { return expRange(mouseY, 0.01f, 100.0f); });

		if(!pause) {
			noiseTimeDim += noiseProgressSpeed;

			foreach(Walker& walker, walkers) {
				walker.update();
				if(walker.age > MAX_AGE) {
					walker = Walker();
				}
			}
		}

		if (pause)
			Sleep(50);
	}
	void stefanDraw() {
		gl::clear(Color(0, 0, 0));

		const float t = getElapsedFrames();
		static Array2D<float> sizeSource(sx(), sy());
		static auto sizeSourceTex = gtex(sizeSource);
		string bg = "vec3 bg = vec3(0.0);";
		static auto walkerTex = shade2(sizeSourceTex, "_out = bg;", ShadeOpts(), bg);
		mat2 rotMat;
		mat3 rotMat3;
		rotMat3 = glm::rotate(rotMat3, std::sin(t / 10.0f));
		rotMat = mat2(rotMat3);
		if(!pause) {
			walkerTex = shade2(walkerTex, "_out = mix(fetch3(), bg, 0.007);", ShadeOpts(), bg);
			
			glPointSize(2.5);
			std::vector<vec4> color;
			std::vector<vec2> pos;
			{
				foreach(Walker& walker, walkers) {
					auto c = vec4(walker.color, walker.alpha()*.3f);
					// todo: change rotMat to a rotated unit vector here
					auto rotated = safeNormalized(rotMat * walker.lastMove) * 30.0f;
					color.push_back(c); pos.push_back(walker.pos+rotated);
					color.push_back(c); pos.push_back(walker.pos-rotated);
				}
				//gl::popMatrices();
			}
			{
				gl::ScopedBlend sb1(true);
				gl::ScopedBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				gl::ScopedViewport(0, 0, sx(), sy());
				//gl::ScopedBlend(GL_SRC_ALPHA, GL_ONE);
				gl::pushMatrices();
				gl::setMatricesWindow(sx(), sy(), true);
				static auto colorDef = gl::ShaderDef().color();
				static auto colorProg = gl::getStockShader(colorDef);
				gl::ScopedGlslProg sgp(colorProg);

				beginRTT(walkerTex);
				{
					gl::VboRef colorVbo = gl::Vbo::create(GL_ARRAY_BUFFER, color, GL_STATIC_DRAW);
					gl::VboRef posVbo = gl::Vbo::create(GL_ARRAY_BUFFER, pos, GL_STATIC_DRAW);
					geom::BufferLayout colorLayout, posLayout;
					colorLayout.append(geom::COLOR, 4, sizeof(decltype(color[0])), 0);
					posLayout.append(geom::POSITION, 2, sizeof(decltype(pos[0])), 0);

					gl::VboMeshRef vboMesh = gl::VboMesh::create(color.size(), GL_POINTS,
					{ std::make_pair(colorLayout, colorVbo), std::make_pair(posLayout, posVbo) });
					//glUseProgram(0);
					gl::draw(vboMesh);
				}
				endRTT();
			}
		}
		
		auto walkerTexB = gpuBlur2_4::run(walkerTex, 2);
		auto walkerTex3 = shade2(walkerTex, walkerTexB,
			"vec3 c = fetch3();"
			"vec3 hsl = rgb2hsl(c);"
			"hsl.z /= .5;"
			"hsl.z = min(hsl.z, 1.0);"
			"hsl.z = pow(hsl.z, 3.0);"
			"c = hsl2rgb(hsl);"
			"c += fetch3(tex2);"
			"_out = c;",
			ShadeOpts(),
			FileCache::get("stuff.fs")
			);

		auto walkerTexM = walkerTex3;
		for(int i = 0; i < 6; i++) {
			walkerTexM = medianFilter(walkerTexM);
		}

		auto walkerTex4 = shade2(walkerTex3, walkerTexM,
			"vec3 c3 = fetch3();"
			"c3 = mix(vec3(28.0/255.0), vec3(232.0/255.0), c3);"
			"vec3 cM = fetch3(tex2);"
			"cM = mix(vec3(36.0/255.0), vec3(219.0/255.0), cM);"
			"_out = blendOverlay(c3, cM);"
			//"_out = cM;"
			,
			ShadeOpts(),
			"float blendOverlay(float base, float blend) {"
			"	return base<0.5?(2.0*base*blend):(1.0-2.0*(1.0-base)*(1.0-blend));"
			"}"
			""
			"vec3 blendOverlay(vec3 base, vec3 blend) {"
			"	return vec3(blendOverlay(base.r,blend.r),blendOverlay(base.g,blend.g),blendOverlay(base.b,blend.b));"
			"}"
			);

		gl::draw(walkerTex4, getWindowBounds());
	}

	gl::TextureRef medianFilter(gl::TextureRef in) {
		return shade2(in,
			"float toSort[9];"
			"\n"
			"#define CSWAP(a,b) { float t = max(toSort[a],toSort[b]); toSort[a] = min(toSort[a],toSort[b]); toSort[b] = t; }"
			"\n"
			"int i = 0;"
			"for(int x = -1; x <= 1; x++) {"
			"	for(int y = -1; y <= 1; y++) {"
			"		vec3 c = fetch3(tex, tc + tsize * vec2(x, y));"
			"		toSort[i] = pack(c);"
			"		i++;"
			"	}"
			"}"
			"CSWAP(0,1)"
			"CSWAP(2,3)"
			"CSWAP(4,5)"
			"CSWAP(7,8)"
			"CSWAP(0,2)"
			"CSWAP(1,3)"
			"CSWAP(6,8)"
			"CSWAP(1,2)"
			"CSWAP(6,7)"
			"CSWAP(5,8)"
			"CSWAP(4,7)"
			"CSWAP(3,8)"
			"CSWAP(4,6)"
			"CSWAP(5,7)"
			"CSWAP(5,6)"
			"CSWAP(2,7)"
			"CSWAP(0,5)"
			"CSWAP(1,6)"
			"CSWAP(3,7)"
			"CSWAP(0,4)"
			"CSWAP(1,5)"
			"CSWAP(3,6)"
			"CSWAP(1,4)"
			"CSWAP(2,5)"
			"CSWAP(2,4)"
			"CSWAP(3,5)"
			"CSWAP(3,4)"
			"vec3 median = unpack(toSort[4]);"
			"_out = median;"
			, ShadeOpts(),
			FileCache::get("stuff.fs") +
				"float quant(float x)"
				"{"
				"	x = clamp(x,0.,1.);"
				"	return floor(x*255.);"
				"}"
				""
				"float pack(vec3 c)"
				"{	"
				"	float lum = (c.x+c.y+c.z)*(1./3.);"
				""
				"	return quant(c.x) + quant(c.y)*256. + quant(lum) * 65536.;"
				"}"
				""
				"vec3 unpack(float x)"
				"{"
				"	float lum = floor(x * (1./65536.)) * (1./255.);"
				"	vec3 c;"
				"	c.x = floor(mod(x,256.)) 			* (1./255.);"
				"	c.y = floor(mod(x*(1./256.),256.)) * (1./255.);"
				"	c.z = lum * 3. - c.y - c.x;"
				"	return c;"
				"}"
			);
	}
};

CINDER_APP(SApp, RendererGl)
