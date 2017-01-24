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

typedef Array2D<Vec3f> Image;
int wsx=800, wsy = 800 * (800.0f / 1280.0f);
int scale = 4;
int sx = wsx / scale;
int sy = wsy / scale;
Image img(sx, sy);
Array2D<float> whiteNoiseState(sx, sy, 0.0f);
Array2D<Vec2f> velocity(sx, sy, Vec2f::zero());
bool mouseDown_[3];
bool keys[256];
gl::Texture::Format gtexfmt;
float noiseTimeDim = 0.0f;
const int MAX_AGE = 100;

float mouseX, mouseY;
bool pause;
bool keys2[256];

Vec3f complexToColor(Vec2f comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float lightness = comp.length();
	lightness = .5f;
	//lightness /= lightness + 1.0f;
	HslF hsl(hue, 1.0f, lightness);
 	return FromHSL(hsl);
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
		pos = Vec2f(ci::randFloat(0, wsx), ci::randFloat(0, wsy));
		age = ci::randInt(0, MAX_AGE);
		lastMove = Vec2f::zero();
	}
	float noiseXAt(Vec2f p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)wsx;
		float noiseX = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, noiseTimeDim);
		return noiseX;
	}
	
	float noiseYAt(Vec2f p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)wsx;
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
		pos += toAdd;
		color = complexToColor(toAdd);
		lastMove = toAdd;
		//color = Vec3f::one();

		if(pos.x < 0) pos.x += wsx;
		if(pos.y < 0) pos.y += wsy;
		pos.x = fmod(pos.x, wsx);
		pos.y = fmod(pos.y, wsy);

		age++;
	}
};

vector<Walker> walkers;

void updateConfig() {
}

struct SApp : AppBasic {
	Rectf area;
		
	void setup()
	{
		//keys2['0']=keys2['1']=keys2['2']=keys2['3']=true;
		//_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

		_controlfp(_DN_FLUSH, _MCW_DN);

		area = Rectf(0, 0, (float)sx-1, (float)sy-1).inflated(Vec2f::zero());

		glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
		gtexfmt.setInternalFormat(hdrFormat);
		setWindowSize(wsx, wsy);

		glEnable(GL_POINT_SMOOTH);

		for(int i = 0; i < 4000; i++) {
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
			//std::fill(img.begin(), img.end(), 0.0f);
			std::fill(velocity.begin(), velocity.end(), Vec2f::zero());
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

		/*Sleep(50);*/my_console::clr();
		sw::endFrame();
		cfg1::print();
		my_console::endFrame();

		if(pause)
			Sleep(50);
	}
	void updateIt() {
		Array2D<Vec3f> result(sx, sy);
		img = result;

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
		auto tex = gtex(img);
		static auto walkerTex = Shade().tex(tex).expr("vec3(0.0);").scale(::scale).run();
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
				endRTT();
			}
			glPopAttrib();
		}

		//auto walkerTex2 = Shade().tex(walkerTex).expr("1.0-fetch3()").run();

		//walkerTex = gpuBlur2_4::run_longtail(walkerTex, 3, 1.0);

		gl::draw(walkerTex, getWindowBounds());
	}
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return mainFuncImpl(new SApp());
}

#endif