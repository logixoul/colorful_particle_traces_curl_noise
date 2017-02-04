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

int wsx=800, wsy = 800 * (800.0f / 1280.0f);
int scale = 2;
int sx = wsx / scale;
int sy = wsy / scale;
bool mouseDown_[3];
bool keys[256];
gl::Texture::Format gtexfmt;
float noiseTimeDim = 0.0f;
float heightmapTimeDim = 0.0f;
const int MAX_AGE = 100;
gl::Texture texToDraw;
bool texOverride = false;
gl::GlslProg shader;

Array2D<float> img2(sx, sy); // heightmap based on tex rgb

float mouseX, mouseY;
bool pause;
bool keys2[256];

Vec3f complexToColor_HSV(Vec2f comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float lightness = comp.length();
	//lightness /= lightness + 1.0f;
	HslF hsl(hue, 1.0f, lightness);
 	return FromHSL(hsl);
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
	static float noiseXAt(Vec2f p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)sx;
		float noiseX = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, noiseTimeDim);
		return noiseX;
	}
	
	static float noiseYAt(Vec2f p) {
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

	Triangle getTriangle(Vec2i ai, Vec2i bi, Vec2i ci)
	{
		Vec3f a(ai.x, ai.y, img2(ai));
		Vec3f b(bi.x, bi.y, img2(bi));
		Vec3f c(ci.x, ci.y, img2(ci));
		return Triangle(a, b, c, a, b, c, ai, bi, ci);
	}
// end 3d heightmap

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

		for(int i = 0; i < 4000 /*/ sq(scale)*/; i++) {
			walkers.push_back(Walker());
		}

		shader = gl::GlslProg(loadFile("heightmap.vs"), loadFile("heightmap.fs"));
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

		/*Sleep(50);*/my_console::clr();
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
			heightmapTimeDim += 0.01f;
			foreach(Walker& walker, walkers) {
				walker.update();
				if(walker.age > MAX_AGE) {
					walker = Walker();
				}
			}
		}
	}
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
			walkerTex = shade2(walkerTex, "_out = mix(fetch3(), bg, .01);", ShadeOpts(), bg);
			glPushAttrib(GL_ALL_ATTRIB_BITS);
			glUseProgram(0);
			glPointSize(1);
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
								c *= 20.0;
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
		if(getElapsedFrames() > 50)
		{
			auto walkerImg = gettexdata<Vec3f>(walkerTex, GL_RGB, GL_FLOAT, walkerTex.getCleanBounds());
			cv::Mat_<cv::Vec3f> walkerMat(walkerImg.h, walkerImg.w, (cv::Vec3f*)walkerImg.data);
			static Array2D<float> kernel = getKernel(Vec2i(20, 20));
			static cv::Mat_<float> kernelMat(kernel.h, kernel.w, kernel.data);
				cv::filter2D(walkerMat, walkerMat, -1, kernelMat, cv::Point(-1, -1), 0, cv::BORDER_WRAP);
			walkerTex = gtex(walkerImg);
		}
		auto walkerImg = gettexdata<Vec3f>(walkerTex, GL_RGB, GL_FLOAT, walkerTex.getCleanBounds());
		forxy(img2)
		{
			
			/*int numDetailsX = 5;
			float nscale = numDetailsX / (float)sx;
			float f = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, heightmapTimeDim);
			//float f = Walker::noiseXAt(p);
			f = f * .5 + .5;
			f = pow(f, 4.0f);
			img2(p) = f * 40.0;*/
			float f = walkerImg(p).dot(Vec3f::one()*1.0/3.0) * 1.0;
			f = pow(f, 1.0f / 3.0f) * 2.0f;
			img2(p) = f;
		}
		
		CameraPersp camera;
		Vec3f toLookAt(sx/2, sy/2+60, 0.0f);
		Vec3f cameraPos(toLookAt.x, toLookAt.y+50, sx / 4-10);
		camera.lookAt(cameraPos, toLookAt, Vec3f::zAxis());
		camera.setAspectRatio(getWindowAspectRatio());
		camera.setFov(90.0f); // degrees

		triangles2.clear();
		for(int x = 0; x < sx - 1; x++)
		{
			for(int y = 0; y < sy - 1; y++)
			{
				Vec2i index(x, y);
				triangles2.push_back(getTriangle(Vec2i(x, y + 1), Vec2i(x, y), Vec2i(x + 1, y)));
				triangles2.push_back(getTriangle(Vec2i(x, y + 1), Vec2i(x + 1, y), Vec2i(x + 1, y + 1)));
			}
		}
		calcNormals();
		shader.bind();
		shader.uniform("tex", 0); walkerTex.bind(0);
		shader.uniform("mouse", Vec2f(mouseX, mouseY));
		shader.uniform("time", (float)getElapsedSeconds());
		shader.uniform("viewportSize", (Vec2f)getWindowSize());
		shader.uniform("lightOrbitCenter", Vec2f(sx, sy) / 2.0);
		
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_CULL_FACE);
		gl::pushMatrices();
		gl::setMatrices(camera);
		glBegin(GL_TRIANGLES);
		foreach(auto& triangle, triangles2)
		{
			Vec3f c = normals(triangle.ia);
			glColor3f(c.x, c.y, c.z);
			glTexCoord2f(Vec2f(triangle.ia)/Vec2f(sx, sy)); glNormal3f(normals(triangle.ia)); glVertex3f(triangle.a);
			glTexCoord2f(Vec2f(triangle.ib)/Vec2f(sx, sy)); glNormal3f(normals(triangle.ib)); glVertex3f(triangle.b);
			glTexCoord2f(Vec2f(triangle.ib)/Vec2f(sx, sy)); glNormal3f(normals(triangle.ic)); glVertex3f(triangle.c);
		}
		glEnd();
		gl::popMatrices();
		glPopAttrib();
		gl::GlslProg::unbind();

		//gl::draw(walkerTex2, getWindowBounds());
	}
#endif
};
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return mainFuncImpl(new SApp());
}

