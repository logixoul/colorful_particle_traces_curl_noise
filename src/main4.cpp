#include "precompiled.h"
#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_4.h"
#include "cfg1.h"
#include "sw.h"
#include "my_console.h"
#include "hdrwrite.h"
#include <float.h>
#include "simplexnoise.h"
#include "mainfunc_impl.h"
#include "colorspaces.h"
#include "easyfft.h"

// baseline: 18fps

int wsx=700, wsy = 700;
int scale = 1;
int sx = wsx / scale;
int sy = wsy / scale;
bool mouseDown_[3];
bool keys[256];
float noiseTimeDim = 0.0f;
const int MAX_AGE = 200;

float mouseX, mouseY;
bool pause;
bool keys2[256];

Vec3f complexToColor_HSV(Vec2f comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float lightness = comp.length();
	lightness = 1.0f;
	//lightness /= lightness + 1.0f;
	/*HslF hsl(hue, 1.0f, lightness);
 	return FromHSL(hsl);*/
	return (Vec3f&)ci::hsvToRGB(Vec3f(hue, 1.0f, lightness));
}

// https://www.shadertoy.com/view/4dK3zG
Vec3f complexToColor_fromShaderToy(Vec2f comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float normal_value = hue;
    Vec3f color;
    if(normal_value<0.0) normal_value = 0.0;
    if(normal_value>1.0) normal_value = 1.0;
    float v1 = 1.0/7.0;
    float v2 = 2.0/7.0;
    float v3 = 3.0/7.0;
    float v4 = 4.0/7.0;
    float v5 = 5.0/7.0;
    float v6 = 6.0/7.0;
    //compute color
    if(normal_value<v1)
    {
      float c = normal_value/v1;
      color.x = 70.*(1.-c);
      color.y = 70.*(1.-c);
      color.z = 219.*(1.-c) + 91.*c;
    }
    else if(normal_value<v2)
    {
      float c = (normal_value-v1)/(v2-v1);
      color.x = 0.;
      color.y = 255.*c;
      color.z = 91.*(1.-c) + 255.*c;
    }
    else if(normal_value<v3)
    {
      float c = (normal_value-v2)/(v3-v2);
      color.x =  0.*c;
      color.y = 255.*(1.-c) + 128.*c;
      color.z = 255.*(1.-c) + 0.*c;
    }
    else if(normal_value<v4)
    {
      float c = (normal_value-v3)/(v4-v3);
      color.x = 255.*c;
      color.y = 128.*(1.-c) + 255.*c;
      color.z = 0.;
    }
    else if(normal_value<v5)
    {
      float c = (normal_value-v4)/(v5-v4);
      color.x = 255.*(1.-c) + 255.*c;
      color.y = 255.*(1.-c) + 96.*c;
      color.z = 0.;
    }
    else if(normal_value<v6)
    {
      float c = (normal_value-v5)/(v6-v5);
      color.x = 255.*(1.-c) + 107.*c;
      color.y = 96.*(1.-c);
      color.z = 0.;
    }
    else
    {
      float c = (normal_value-v6)/(1.-v6);
      color.x = 107.*(1.-c) + 223.*c;
      color.y = 77.*c;
      color.z = 77.*c;
    }
    return color * 1.5f / 255.0f;
}

Vec3f complexToColor(Vec2f comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float lightness = comp.length();
	//lightness = .5f;
	//lightness /= lightness + 1.0f;
	//HslF hsl(hue, 1.0f, lightness);
 	//return FromHSL(hsl);
	Vec3f colors[] = {
		//Vec3f(0,0,0),
		Vec3f(15,131,174),
		Vec3f(246,24,199),
		Vec3f(148,255,171),
		Vec3f(251,253,84)
	};
	int colorCount = 4;
	float pos = hue*(colorCount - .01f);
	int posint1 = floor(pos);
	int posint2 = (posint1 + 1) % colorCount;
	float posFract = pos - posint1;
	Vec3f color1 = colors[posint1];
	Vec3f color2 = colors[posint2];
	return lerp(color1, color2, posFract) / 255.0f;
}

int numDetailsX = 5;
float nscale = numDetailsX / (float)sx;

struct Walker {
	Vec2f pos;
	int age;
	Vec3f color;
	Vec2f lastMove;

	float alpha() {
		return min((age/(float)MAX_AGE)*5.0, 1.0);
	}

	Walker() {
		pos = Vec2f(ci::randFloat(0, sx), ci::randFloat(0, sy));
		age = ci::randInt(0, MAX_AGE);
		lastMove = Vec2f::zero();
	}
	static float noiseXAt(Vec2f p, float z) {
		float noiseX = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, z);
		return noiseX;
	}
	
	static float noiseYAt(Vec2f p, float z) {
		float noiseY = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale + numDetailsX, z);
		return noiseY;
	}
	static Vec2f noiseVec2fAt(Vec2f p, float z) {
		return Vec2f(noiseXAt(p, z), noiseYAt(p, z));
	}
	static Vec2f curlNoiseVec2fAt(Vec2f p, float z) {
		float eps = 1;
		float noiseXAbove = noiseXAt(p - Vec2f(0, eps), z);
		float noiseXBelow = noiseXAt(p + Vec2f(0, eps), z);
		float noiseYOnLeft = noiseYAt(p - Vec2f(eps, 0), z);
		float noiseYOnRight = noiseYAt(p + Vec2f(eps, 0), z);
		return Vec2f(noiseXBelow - noiseXAbove, -(noiseYOnRight - noiseYOnLeft)) / (2.0f * eps);
	}
	void update() {
		Vec2f toAdd = curlNoiseVec2fAt(pos, noiseTimeDim) * 50.0f;
		//toAdd.y -= 1.0f;
		pos += toAdd / scale;
		color = complexToColor_HSV(toAdd);
		lastMove = toAdd;
		//color = Vec3f::one();

		if(pos.x < 0) pos.x += sx;
		if(pos.y < 0) pos.y += sy;
		pos.x = fmod(pos.x, sx);
		pos.y = fmod(pos.y, sy);

		age++;
	}
};

vector<Walker> walkers;

void updateConfig() {
}

struct SApp : AppBasic {
	void setup()
	{
		_controlfp(_DN_FLUSH, _MCW_DN);

		glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
		setWindowSize(wsx, wsy);

		glEnable(GL_POINT_SMOOTH);

		for(int i = 0; i < 4000 / sq(scale); i++) {
			walkers.push_back(Walker());
		}
	}
	void keyDown(KeyEvent e)
	{
		keys[e.getChar()] = true;
		if(e.isControlDown()&&e.getCode()!=KeyEvent::KEY_LCTRL)
		{
			keys2[e.getChar()] = !keys2[e.getChar()];
			return;
		}
		if(keys['r'])
		{
		}
		if(keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	void keyUp(KeyEvent e)
	{
		keys[e.getChar()] = false;
	}
	
	void mouseDown(MouseEvent e)
	{
		mouseDown_[e.isLeft() ? 0 : e.isMiddle() ? 1 : 2] = true;
	}
	void mouseUp(MouseEvent e)
	{
		mouseDown_[e.isLeft() ? 0 : e.isMiddle() ? 1 : 2] = false;
	}
	float noiseProgressSpeed;
	
	void draw()
	{
		my_console::beginFrame();
		sw::beginFrame();

		wsx = getWindowSize().x;
		wsy = getWindowSize().y;

		mouseX = getMousePos().x / (float)wsx;
		mouseY = getMousePos().y / (float)wsy;
		noiseProgressSpeed=cfg1::getOpt("noiseProgressSpeed", .00008f,
			[&]() { return keys['s']; },
			[&]() { return expRange(mouseY, 0.01f, 100.0f); });
		
		gl::clear(Color(0, 0, 0));

		updateIt();
		
		renderIt();

		sw::endFrame();
		cfg1::print();
		my_console::endFrame();

		if(pause)
			Sleep(50);
	}
	void updateIt() {
		if(!pause) {
			noiseTimeDim += noiseProgressSpeed;

			foreach(Walker& walker, walkers) {
				walker.update();
				if(walker.age > MAX_AGE) {
					walker = Walker();
				}
			}
		}
	}
	void renderIt() {
		const float t = getElapsedFrames();
		static Array2D<float> sizeSource(sx, sy);
		static auto sizeSourceTex = gtex(sizeSource);
		string bg = "vec3 bg = vec3(0.0);";
		static auto walkerTex = shade2(sizeSourceTex, "_out = bg;", ShadeOpts(), bg);
		Matrix22f rotMat = Matrix22f::createRotation(sin(t/10.0));
		if(!pause) {
			walkerTex = shade2(walkerTex, "_out = mix(fetch3(), bg, 0.007);", ShadeOpts(), bg);
			glPushAttrib(GL_ALL_ATTRIB_BITS);
			glUseProgram(0);
			glPointSize(2.5);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			//glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glEnable(GL_BLEND);
			{
				beginRTT(walkerTex);
				{
					gl::pushMatrices();
					gl::setMatricesWindow(sx, sy, true);
					{
						glBegin(GL_POINTS);
						{
							foreach(Walker& walker, walkers) {
								auto& c = walker.color;
								glColor4f(c.x, c.y, c.z, walker.alpha()*.3f);
								auto rotated = rotMat.transformVec(walker.lastMove).safeNormalized() * 30.0f;
								glVertex2f(walker.pos+rotated);
								glVertex2f(walker.pos-rotated);
							}
						}
						glEnd();
					}
					gl::popMatrices();
				}
				endRTT();
			}
			glPopAttrib();
		}
		globaldict["noiseTimeDim100"] = noiseTimeDim * 100;
		auto velocity = shade2(walkerTex,
			"float nscale = 5;"
			"vec2 pf = tc * texSize / texSize.x;"
			"vec2 vel = vec2(0.0);"
			"for(int i = 0; i < 1; i++) {"
			"	nscale *= 2.0f;"
			/*"	vel.x += raw_noise_3d(pf.x * nscale, pf.y * nscale, noiseTimeDim100) / nscale;"
			"	vel.y += raw_noise_3d(pf.x * nscale, pf.y * nscale, noiseTimeDim100 + 1.0) / nscale;"*/
			"	vel = curlNoise(pf, noiseTimeDim100) * 1.0;"
			"}"
			"_out.rg = vel * 0.6;"
			,
			ShadeOpts(),
			FileCache::get("simplexnoise3d.fs.glsl") +
				"int numDetailsX = 5;"
				"int nscale = numDetailsX;"
				"float noiseXAt(vec2 p, float z) {"
				"	float noiseX = raw_noise_3d(p.x * nscale, p.y * nscale, z);"
				"	return noiseX;"
				"}"
				""
				"float noiseYAt(vec2 p, float z) {"
				"	float noiseY = raw_noise_3d(p.x * nscale, p.y * nscale + numDetailsX, z);"
				"	return noiseY;"
				"}"
				"float raw_noise_3d(vec2 xy, float z) {"
				"	return raw_noise_3d(xy.x, xy.y, z);"
				"}"
				"vec2 curlNoise(vec2 p, float z) {"
				"	vec2 eps = tsize * 1.0;"
					"float noiseXAbove = raw_noise_3d(p * nscale - vec2(0, 1) * eps, z);"
					"float noiseXBelow = raw_noise_3d(p * nscale + vec2(0, 1) * eps, z);"
					"float noiseYOnLeft = raw_noise_3d(p * nscale - vec2(1, 0) * eps + numDetailsX, z);"
					"float noiseYOnRight = raw_noise_3d(p * nscale + vec2(1, 0) * eps + numDetailsX, z);"
					"return vec2(noiseXBelow - noiseXAbove, -(noiseYOnRight - noiseYOnLeft)) / (2.0 * eps);"
				"}"
			);
		walkerTex = shade2(walkerTex, velocity, FileCache::get("forward_convect.glsl"));
		//velocity = shade2(velocity, velocity, FileCache::get("forward_convect.glsl"));
		walkerTex = shade2(walkerTex,
			"vec3 c = fetch3();"
			"vec3 hsv = rgb2hsv(c);"
			"hsv[1] = mix(0.5, 1.0, hsv[1]);"
			"c = hsv2rgb(hsv);"
			"_out = c;",
			ShadeOpts(),
			FileCache::get("stuff.fs"));

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
		//gl::draw(velocity, getWindowBounds());
	}

	gl::Texture medianFilter(gl::Texture in) {
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
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return mainFuncImpl(new SApp());
}
