#include "precompiled.h"
#if 1
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

int wsx=1000, wsy = 1000 * (800.0f / 1280.0f);
int scale = 1;
int sx = wsx / scale;
int sy = wsy / scale;
bool mouseDown_[3];
bool keys[256];
gl::Texture::Format gtexfmt;
float noiseTimeDim = 0.0f;
const int MAX_AGE = 100;
gl::Texture texToDraw;
bool texOverride = false;

float mouseX, mouseY;
bool pause;
bool keys2[256];

Vec3f complexToColor_HSV(Vec2f comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float lightness = comp.length();
	lightness = .5f;
	//lightness /= lightness + 1.0f;
	HslF hsl(hue, 1.0f, lightness);
 	return FromHSL(hsl);
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
	float noiseXAt(Vec2f p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)sx;
		float noiseX = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, noiseTimeDim);
		return noiseX;
	}
	
	float noiseYAt(Vec2f p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)sx;
		float noiseY = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale + numDetailsX, noiseTimeDim);
		return noiseY;
	}
	Vec2f noiseVec2fAt(Vec2f p) {
		return Vec2f(noiseXAt(p), noiseYAt(p));
	}
	Vec2f curlNoiseVec2fAt(Vec2f p) {
		float eps = 1;
		float noiseXAbove = noiseXAt(p - Vec2f(0, eps));
		float noiseXBelow = noiseXAt(p + Vec2f(0, eps));
		float noiseYOnLeft = noiseYAt(p - Vec2f(eps, 0));
		float noiseYOnRight = noiseYAt(p + Vec2f(eps, 0));
		return Vec2f(noiseXBelow - noiseXAbove, -(noiseYOnRight - noiseYOnLeft)) / (2.0f * eps);
	}
	void update() {
		Vec2f toAdd = curlNoiseVec2fAt(pos) * 50.0f;
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

// begin 3d heightmap

	Array2D<Vec3f> normals;
	struct Triangle
	{
		Vec3f a, b, c;
		Vec3f a0, b0, c0;
		Vec2i ia, ib, ic;
		Triangle(Vec3f const& a, Vec3f const& b, Vec3f const& c,
			Vec3f a0, Vec3f b0, Vec3f c0, Vec2i ia, Vec2i ib, Vec2i ic) : a(a), b(b), c(c), a0(a0), b0(b0), c0(c0), ia(ia), ib(ib), ic(ic)
		{

		}
	};
	vector<Triangle> triangles2;
	Array2D<Triangle> triangles;
	
	Vec3f getNormal(Triangle t)
	{
		return (t.b-t.c).cross(t.b-t.a).normalized();
	}

	void calcNormals()
	{
		normals = Array2D<Vec3f>(sx, sy);
		
		foreach(auto& triangle, triangles2)
		{
			auto n6 = getNormal(triangle);
			normals(triangle.ia) += n6;
			normals(triangle.ib) += n6;
			normals(triangle.ic) += n6;
		}
		forxy(normals)
		{
			normals(p) = normals(p).safeNormalized();
		}
	}
// end 3d heightmap

void updateConfig() {
}

struct SApp : AppBasic {
	void setup()
	{
		_controlfp(_DN_FLUSH, _MCW_DN);

		glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
		gtexfmt.setInternalFormat(hdrFormat);
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
	Vec2f direction;
	Vec2f lastm;
	void mouseDrag(MouseEvent e)
	{
		mm();
	}
	void mouseMove(MouseEvent e)
	{
		mm();
	}
	void mm()
	{
		direction = getMousePos() - lastm;
		lastm = getMousePos();
	}
	Vec2f reflect(Vec2f const & I, Vec2f const & N)
	{
		return I - N * N.dot(I) * 2.0f;
	}
	float noiseProgressSpeed;
	
	void draw()
	{
		::texOverride = false;

		my_console::beginFrame();
		sw::beginFrame();
		static bool first = true;
		first = false;

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
		//Sleep(2000);
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
#if 0
	void renderComplexImg(Array2D<Complexf> in) {
		Array2D<Vec3f> colored(in.Size());
		forxy(in) {
			Vec2f vec2f(in(p).real(), in(p).imag());
			colored(p) = complexToColor_HSV(vec2f)/* * 10000.0f*/;
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
	Array2D<Complexf> getFdKernel(Vec2i size) {
		Array2D<float> sdKernel(size);
		forxy(sdKernel) {
			auto p2=p;if(p2.x>sdKernel.w/2)p2.x-=sdKernel.w;if(p2.y>sdKernel.h/2)p2.y-=sdKernel.h;
			//sdKernel(p) = 1.0 / (.01f + (p2-Vec2i(3, 3)).length()/10.0f);
			//sdKernel(p) = 1.0 / (1.f + p2.length()/10.0f);
			float dist = p2.length();
			//sdKernel(p) = powf(max(1.0f - p2.length() / 20.0f, 0.0f), 4.0);
			//sdKernel(p) = 1.0 / pow((1.f + dist*5.0f), 3.0f);
			sdKernel(p) = p2.length() > 10 ? 0 : 1;
			//sdKernel(p) = expf(-p2.lengthSquared()*.02f);
			//if(p == Vec2i::zero()) sdKernel(p) = 1.0f;
			//else sdKernel(p) = 0.0f;
		}
		auto kernelInvSum = 1.0/(std::accumulate(sdKernel.begin(), sdKernel.end(), 0.0f));
		forxy(sdKernel) { sdKernel(p) *= kernelInvSum; }
		auto fdKernel = fft(sdKernel, FFTW_MEASURE);
		return fdKernel;
	}
	Array2D<Vec3f> convolveLongtail(Array2D<Vec3f> in) {
		/*static*/ Array2D<Complexf> fdKernel = getFdKernel(in.Size());
		auto inChans = ::split(in);
		for(int i = 0; i < inChans.size(); i++) {
			auto& inChan = inChans[i];
			auto inChanFd = fft(inChan, FFTW_MEASURE);
			//renderComplexImg(inChanFd);
			forxy(inChanFd) {
				auto p2=p;if(p2.x>in.w/2)p2.x-=in.w;if(p2.y>in.h/2)p2.y-=in.h;
				inChanFd(p) *= fdKernel(p);
				//if(p != Vec2f::zero())
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

		auto walkerImg = gettexdata<Vec3f>(walkerTex, GL_RGB, GL_FLOAT, walkerTex.getCleanBounds());
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
	Array2D<float> getKernel(Vec2i size) {
		Array2D<float> sdKernel(size);
		forxy(sdKernel) {
			//auto p2=p;if(p2.x>sdKernel.w/2)p2.x-=sdKernel.w;if(p2.y>sdKernel.h/2)p2.y-=sdKernel.h;
			float dist = p.distance(sdKernel.Size()/2);
			//sdKernel(p) = 1.0 / (.01f + (p2-Vec2i(3, 3)).length()/10.0f);
			sdKernel(p) = 1.0 / pow((1.f + dist*5.0f), 3.0f);
			//sdKernel(p) = dist > 10 ? 0 : 1;
			//sdKernel(p) = expf(-p2.lengthSquared()*.02f);
			//if(p == Vec2i::zero()) sdKernel(p) = 1.0f;
			//else sdKernel(p) = 0.0f;
		}
		auto kernelInvSum = 1.0/(std::accumulate(sdKernel.begin(), sdKernel.end(), 0.0f));
		forxy(sdKernel) { sdKernel(p) *= kernelInvSum; }
		return sdKernel;
	}
	void renderIt() {
		static Array2D<float> sizeSource(sx, sy);
		static auto sizeSourceTex = gtex(sizeSource);
		string bg = "vec3 bg = vec3(0.0);";
		static auto walkerTex = shade2(sizeSourceTex, "_out = bg;", ShadeOpts(), bg);
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
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return mainFuncImpl(new SApp());
}

#endif