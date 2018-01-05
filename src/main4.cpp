#include "precompiled.h"
#if 1
#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_4.h"
#include "cfg1.h"
#include "sw.h"
#include "stefanfw.h"

#include "hdrwrite.h"
#include <float.h>
#include "simplexnoise.h"

#include "colorspaces.h"
#include "easyfft.h"

int wsx=1280, wsy = 720;
int scale = 1;
int sx = wsx / ::scale;
int sy = wsy / ::scale;


float noiseTimeDim = 0.0f;
const int MAX_AGE = 100;

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

// https://www.shadertoy.com/view/4dK3zG
vec3 complexToColor_fromShaderToy(vec2 comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float normal_value = hue;
    vec3 color;
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

struct Walker {
	vec2 pos;
	int age;
	vec3 color;
	vec2 lastMove;

	float alpha() {
		return min((age/(float)MAX_AGE)*5.0, 1.0);
	}

	Walker() {
		pos = vec2(ci::randFloat(0, sx), ci::randFloat(0, sy));
		age = ci::randInt(0, MAX_AGE);
	}
	float noiseXAt(vec2 p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)sx;
		float noiseX = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, noiseTimeDim);
		return noiseX;
	}
	
	float noiseYAt(vec2 p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)sx;
		float noiseY = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale + numDetailsX, noiseTimeDim);
		return noiseY;
	}
	vec2 noisevec2At(vec2 p) {
		return vec2(noiseXAt(p), noiseYAt(p));
	}
	vec2 curlNoisevec2At(vec2 p) {
		float eps = 1;
		float noiseXAbove = noiseXAt(p - vec2(0, eps));
		float noiseXBelow = noiseXAt(p + vec2(0, eps));
		float noiseYOnLeft = noiseYAt(p - vec2(eps, 0));
		float noiseYOnRight = noiseYAt(p + vec2(eps, 0));
		return vec2(noiseXBelow - noiseXAbove, -(noiseYOnRight - noiseYOnLeft)) / (2.0f * eps);
	}
	void update() {
		vec2 toAdd = curlNoisevec2At(pos) * 50.0f;
		//toAdd.y -= 1.0f;
		pos += toAdd / float(::scale);
		color = complexToColor_HSV(toAdd);
		lastMove = toAdd;
		//color = vec3::one();

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

struct SApp : App {
	void setup()
	{
		enableDenormalFlushToZero();

		createConsole();
		disableGLReadClamp();
		stefanfw::eventHandler.subscribeToEvents(*this);
		setWindowSize(wsx, wsy);

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
#if 0
	void renderComplexImg(Array2D<Complexf> in) {
		Array2D<vec3> colored(in.Size());
		forxy(in) {
			vec2 vec2(in(p).real(), in(p).imag());
			colored(p) = complexToColor_HSV(vec2)/* * 10000.0f*/;
		}
		::texToDraw = gtex(colored);
		::texOverride = true;
	}
	void printMinMax(string desc, Array2D<Complexf> in) {
		auto lengths = Array2D<float>(in.Size());
		forxy(in) {
			lengths(p) = abs(in(p));
		}
		printMinMax(desc, lengths);
	}
	void printMinMax(string desc, Array2D<float> arr) {
		auto maxEl = *std::max_element(arr.begin(), arr.end());
		auto minEl = *std::min_element(arr.begin(), arr.end());
		qDebug() << "array '" << desc << "': min=" << minEl << ", max=" << maxEl;
	}
	Array2D<Complexf> getFdKernel(ivec2 size) {
		Array2D<float> sdKernel(size);
		forxy(sdKernel) {
			auto p2=p;if(p2.x>sdKernel.w/2)p2.x-=sdKernel.w;if(p2.y>sdKernel.h/2)p2.y-=sdKernel.h;
			//sdKernel(p) = 1.0 / (.01f + (p2-ivec2(3, 3)).length()/10.0f);
			//sdKernel(p) = 1.0 / (1.f + p2.length()/10.0f);
			float dist = p2.length();
			//sdKernel(p) = powf(max(1.0f - p2.length() / 20.0f, 0.0f), 4.0);
			//sdKernel(p) = 1.0 / pow((1.f + dist*5.0f), 3.0f);
			sdKernel(p) = p2.length() > 10 ? 0 : 1;
			//sdKernel(p) = expf(-p2.lengthSquared()*.02f);
			//if(p == ivec2::zero()) sdKernel(p) = 1.0f;
			//else sdKernel(p) = 0.0f;
		}
		auto kernelInvSum = 1.0/(std::accumulate(sdKernel.begin(), sdKernel.end(), 0.0f));
		forxy(sdKernel) { sdKernel(p) *= kernelInvSum; }
		auto fdKernel = fft(sdKernel, FFTW_MEASURE);
		return fdKernel;
	}
	Array2D<vec3> convolveLongtail(Array2D<vec3> in) {
		/*static*/ Array2D<Complexf> fdKernel = getFdKernel(in.Size());
		auto inChans = ::split(in);
		for(int i = 0; i < inChans.size(); i++) {
			auto& inChan = inChans[i];
			auto inChanFd = fft(inChan, FFTW_MEASURE);
			//renderComplexImg(inChanFd);
			forxy(inChanFd) {
				auto p2=p;if(p2.x>in.w/2)p2.x-=in.w;if(p2.y>in.h/2)p2.y-=in.h;
				inChanFd(p) *= fdKernel(p);
				//if(p != vec2::zero())
				//	inChanFd(p) /= 10.0f + sqrt(p2.length());
				//inChanFd(p) *= .1f;
				//inChanFd(p) *= expf(-.01*p2.lengthSquared());
				//if(p2.length() > 10)
				//	inChanFd(p) *= 0.0f;
			}
			//renderComplexImg(fdKernel);
			inChan = ifft(inChanFd, FFTW_MEASURE);
		}
		return ::merge(inChans);
	}
	void renderIt() {
		static Array2D<float> sizeSource(sx, sy);
		static auto sizeSourceTex = gtex(sizeSource);
		static auto walkerTex = Shade().tex(sizeSourceTex).expr("vec3(0.0);").run();
		if(!pause) {
			walkerTex = Shade().tex(walkerTex).expr("fetch3()*.99;").run();
			glPushAttrib(GL_ALL_ATTRIB_BITS);
			glUseProgram(0);
			glPointSize(2);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
								glColor4f(c.x, c.y, c.z, walker.alpha());
								glVertex2f(walker.pos);
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

		auto walkerImg = gettexdata<vec3>(walkerTex, GL_RGB, GL_FLOAT, walkerTex.getCleanBounds());
		walkerImg = convolveLongtail(walkerImg);
		auto walkerTex2 = gtex(walkerImg);

		//walkerTex = gpuBlur2_4::run_longtail(walkerTex, 3, 1.0);
		auto walkerTex3 = shade2(walkerTex, walkerTex2, "_out = tc.x > .5 ? fetch3() : fetch3(tex2) * 600.0;");

		if(::texOverride) {
			gl::draw(texToDraw, getWindowBounds());
		} else {
			gl::draw(walkerTex3, getWindowBounds());
		}
	}
#else
	Array2D<float> getKernel(ivec2 size) {
		Array2D<float> sdKernel(size);
		forxy(sdKernel) {
			//auto p2=p;if(p2.x>sdKernel.w/2)p2.x-=sdKernel.w;if(p2.y>sdKernel.h/2)p2.y-=sdKernel.h;
			float dist = distance(vec2(p), vec2(sdKernel.Size()/2));
			//sdKernel(p) = 1.0 / (.01f + (p2-ivec2(3, 3)).length()/10.0f);
			sdKernel(p) = 1.0 / pow((1.f + dist*5.0f), 3.0f);
			//sdKernel(p) = dist > 10 ? 0 : 1;
			//sdKernel(p) = expf(-p2.lengthSquared()*.02f);
			//if(p == ivec2::zero()) sdKernel(p) = 1.0f;
			//else sdKernel(p) = 0.0f;
		}
		auto kernelInvSum = 1.0/(std::accumulate(sdKernel.begin(), sdKernel.end(), 0.0f));
		forxy(sdKernel) { sdKernel(p) *= kernelInvSum; }
		return sdKernel;
	}
	void stefanDraw() {
		gl::clear(Color(0, 0, 0));
		static Array2D<float> sizeSource(sx, sy);
		static auto sizeSourceTex = gtex(sizeSource);
		string bg = "vec3 bg = vec3(0.0);";
		static auto walkerTex = shade2(sizeSourceTex, "_out = bg;", ShadeOpts(), bg);
		if(!pause) {
			walkerTex = shade2(walkerTex, "_out = mix(fetch3(), bg, 0.007);", ShadeOpts(), bg);
			
			glPointSize(2.5);
			std::vector<vec4> color;
			std::vector<vec2> pos;
			{
				foreach(Walker& walker, walkers) {
					auto c = vec4(walker.color, walker.alpha());
					color.push_back(c); pos.push_back(walker.pos);
				}
				gl::popMatrices();
			}
			{
				gl::ScopedBlend sb1(true);
				gl::ScopedBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				gl::ScopedViewport(0, 0, sx, sy);
				//gl::ScopedBlend(GL_SRC_ALPHA, GL_ONE);
				gl::pushMatrices();
				gl::setMatricesWindow(sx, sy, true);
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
		auto walkerTex2 = shade2(walkerTex, walkerTexB,
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
		gl::draw(walkerTex2, getWindowBounds());

		//CameraPersp camera;
	}
#endif
};

CINDER_APP(SApp, RendererGl)

#endif
