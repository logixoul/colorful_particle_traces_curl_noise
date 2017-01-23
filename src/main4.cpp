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

float mouseX, mouseY;
bool pause;
bool keys2[256];

struct Walker {
	Vec2f pos;
	Vec2f vel;

	Walker() {
		pos = Vec2f(ci::randFloat(0, wsx), ci::randFloat(0, wsy));
		vel = Vec2f::zero();
	}

	void update() {
		float speed = .1f;
		vel += ci::randVec2f()*speed;
		pos += vel;
		//pos += Vec2f::one() * .001f;
		if(pos.x < 0) pos.x += wsx;
		if(pos.y < 0) pos.y += wsy;
		pos.x = fmod(pos.x, wsx);
		pos.y = fmod(pos.y, wsy);
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
	float displaceAmt;
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
		noiseProgressSpeed=cfg1::getOpt("noiseProgressSpeed", .008f,
			[&]() { return keys['s']; },
			[&]() { return expRange(mouseY, 0.01f, 100.0f); });
		displaceAmt=cfg1::getOpt("displaceAmt", 1.0f,
			[&]() { return keys['d']; },
			[&]() { return niceExpRangeY(mouseY, 1.0f, 10000.0f); });
		
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
	Vec3f complexToColor(Vec2f comp) {
		float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
		hue /= (float)(2*M_PI);
		float lightness = comp.length();
		//lightness /= lightness + 1.0f;
		HslF hsl(hue, 1.0f, lightness);
 		return FromHSL(hsl);
	}
	Array2D<Vec3f> diskBorder76Convolve(Array2D<Vec3f> in) {
		Array2D<float> sdKernel(in.Size());
		forxy(sdKernel) {
			auto p2=p;if(p2.x>sdKernel.w/2)p2.x-=sdKernel.w;if(p2.y>sdKernel.h/2)p2.y-=sdKernel.h;
			const float r=76 / (float)scale;
			sdKernel(p)=4*(1.0-smoothstep(r, r+1, p2.length()));
			sdKernel(p)-=3*(1.0-smoothstep(r-1, r, p2.length()));
		}
		auto kernelInvSum = 1.0/(std::accumulate(sdKernel.begin(), sdKernel.end(), 0.0f));
		forxy(sdKernel) { sdKernel(p) *= kernelInvSum; }
	}
	void updateIt() {
		Array2D<Vec3f> result(sx, sy);
		Vec2f center = (Vec2f)result.Size() / 2.0f;
		forxy(result) {
			auto pf = (Vec2f)p;
			int numDetailsX = 5;
			float nscale = numDetailsX / (float)result.w;
			Vec2f comp; // complex
			comp.x = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, noiseTimeDim);
			comp.y = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale + numDetailsX, noiseTimeDim);
			result(p) = complexToColor(comp);
		}
		//result = diskBorder76Convolve(result);
		img = result;

		if(!pause) {
			noiseTimeDim += noiseProgressSpeed;

			foreach(Walker& walker, walkers) {
				walker.update();
			}
		}
	}
	void renderIt() {
		auto tex = gtex(img);
		auto walkerTex = Shade().tex(tex).expr("vec3(1.0);").scale(::scale).run();

		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		{
			glUseProgram(0);
			beginRTT(walkerTex);
			{
				glColor4f(10.0, 10.0, 10.0, 1.0);
				glBegin(GL_POINTS);
				{
					foreach(Walker& walker, walkers) {
						glVertex2f(walker.pos);
					}
				}
				glEnd();
			}
			endRTT();
		}
		glPopAttrib();

		walkerTex = gpuBlur2_4::run_longtail(walkerTex, 3, 1.0);

		if(0)tex = Shade().scale(::scale).tex(tex).tex(walkerTex).src(
			"void shade() {"
			"vec3 c = fetch3();"
			"vec3 cwalker = fetch3(tex2);"
			"_out = c * cwalker * .7;"
			//"_out = cwalker * .4;"
			"}"
			).run();
		gl::draw(tex, getWindowBounds());
	}
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return mainFuncImpl(new SApp());
}

#endif