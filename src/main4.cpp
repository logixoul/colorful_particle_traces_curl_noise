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

int wsx=800, wsy = 800 * (800.0f / 1280.0f);
int scale = 2;
int sx = wsx / ::scale;
int sy = wsy / ::scale;

float noiseTimeDim = 0.0f;
float heightmapTimeDim = 0.0f;
const int MAX_AGE = 100;
gl::GlslProgRef shader;

Array2D<float> img2(sx, sy); // heightmap based on tex rgb


bool pause;


vec3 complexToColor_HSV(vec2 comp) {
	float hue = (float)M_PI+(float)atan2(comp.y,comp.x);
	hue /= (float)(2*M_PI);
	float lightness = length(comp);
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
	static float noiseXAt(vec2 p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)sx;
		float noiseX = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, noiseTimeDim);
		return noiseX;
	}
	
	static float noiseYAt(vec2 p) {
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

// begin 3d heightmap

	Array2D<vec3> normals;
	struct Triangle
	{
		vec3 a, b, c;
		vec3 a0, b0, c0;
		ivec2 ia, ib, ic;
		Triangle(vec3 const& a, vec3 const& b, vec3 const& c,
			vec3 a0, vec3 b0, vec3 c0, ivec2 ia, ivec2 ib, ivec2 ic) : a(a), b(b), c(c), a0(a0), b0(b0), c0(c0), ia(ia), ib(ib), ic(ic)
		{

		}
	};
	vector<Triangle> triangles2;
	Array2D<Triangle> triangles;
	
	vec3 getNormal(Triangle t)
	{
		return normalize(cross(t.b-t.c, t.b-t.a));
	}

	void calcNormals()
	{
		normals = Array2D<vec3>(sx, sy);
		
		foreach(auto& triangle, triangles2)
		{
			auto n6 = getNormal(triangle);
			normals(triangle.ia) += n6;
			normals(triangle.ib) += n6;
			normals(triangle.ic) += n6;
		}
		forxy(normals)
		{
			normals(p) = safeNormalized(normals(p));
		}
	}

	Triangle getTriangle(ivec2 ai, ivec2 bi, ivec2 ci)
	{
		vec3 a(ai.x, ai.y, img2(ai));
		vec3 b(bi.x, bi.y, img2(bi));
		vec3 c(ci.x, ci.y, img2(ci));
		return Triangle(a, b, c, a, b, c, ai, bi, ci);
	}
// end 3d heightmap

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

		for(int i = 0; i < 4000 /*/ sq(::scale)*/; i++) {
			walkers.push_back(Walker());
		}

		shader = gl::GlslProg::create(loadFile("heightmap.vs"), loadFile("heightmap.fs"));
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
			heightmapTimeDim += 0.01f;
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
	Array2D<float> getKernel(vec2 size) {
		Array2D<float> sdKernel(size);
		forxy(sdKernel) {
			//auto p2=p;if(p2.x>sdKernel.w/2)p2.x-=sdKernel.w;if(p2.y>sdKernel.h/2)p2.y-=sdKernel.h;
			float dist = distance(p, sdKernel.Size()/2);
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
		static Array2D<float> sizeSource(sx, sy);
		static auto sizeSourceTex = gtex(sizeSource);
		string bg = "vec3 bg = vec3(0.0);";
		static auto walkerTex = shade2(sizeSourceTex, "_out = bg;", ShadeOpts(), bg);
		if(!pause) {
			walkerTex = shade2(walkerTex, "_out = mix(fetch3(), bg, .01);", ShadeOpts(), bg);
			
			glPointSize(1);
			std::vector<vec4> color;
			std::vector<vec2> pos;
			{
				foreach(Walker& walker, walkers) {
					auto c = vec4(walker.color * 20.0f, walker.alpha());
					// todo: change rotMat to a rotated unit vector here
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
		if(getElapsedFrames() > 50)
		{
			auto walkerImg = gettexdata<vec3>(walkerTex, GL_RGB, GL_FLOAT/*, walkerTex->getCleanBounds()*/);
			cv::Mat_<cv::Vec3f> walkerMat(walkerImg.h, walkerImg.w, (cv::Vec3f*)walkerImg.data);
			static Array2D<float> kernel = getKernel(ivec2(20, 20));
			static cv::Mat_<float> kernelMat(kernel.h, kernel.w, kernel.data);
				cv::filter2D(walkerMat, walkerMat, -1, kernelMat, cv::Point(-1, -1), 0, cv::BORDER_WRAP);
			walkerTex = gtex(walkerImg);
		}
		auto walkerImg = gettexdata<vec3>(walkerTex, GL_RGB, GL_FLOAT/*, walkerTex->getCleanBounds()*/);
		forxy(img2)
		{
			
			/*int numDetailsX = 5;
			float nscale = numDetailsX / (float)sx;
			float f = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, heightmapTimeDim);
			//float f = Walker::noiseXAt(p);
			f = f * .5 + .5;
			f = pow(f, 4.0f);
			img2(p) = f * 40.0;*/
			float f = dot(walkerImg(p), vec3(1.0/3.0));
			f = pow(f, 1.0f / 3.0f) * 2.0f;
			img2(p) = f;
		}
		
		CameraPersp camera;
		vec3 toLookAt(sx/2, sy/2+60, 0.0f);
		vec3 cameraPos(toLookAt.x, toLookAt.y+50.0f, sx / 4.0/*-f*/-10.0f);
		camera.lookAt(cameraPos, toLookAt, vec3(0, 0, 1)/*vec3::zAxis()*/);
		camera.setAspectRatio(getWindowAspectRatio());
		camera.setFov(90.0f); // degrees

		triangles2.clear();
		for(int x = 0; x < sx - 1; x++)
		{
			for(int y = 0; y < sy - 1; y++)
			{
				ivec2 index(x, y);
				triangles2.push_back(getTriangle(ivec2(x, y + 1), ivec2(x, y), ivec2(x + 1, y)));
				triangles2.push_back(getTriangle(ivec2(x, y + 1), ivec2(x + 1, y), ivec2(x + 1, y + 1)));
			}
		}
		calcNormals();
		shader->bind();
		shader->uniform("tex", 0); walkerTex->bind(0);
		shader->uniform("mouse", vec2(mouseX, mouseY));
		shader->uniform("time", (float)getElapsedSeconds());
		shader->uniform("viewportSize", (vec2)getWindowSize());
		shader->uniform("lightOrbitCenter", vec2(sx, sy) / 2.0f);
		
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_CULL_FACE);
		gl::pushMatrices();
		gl::setMatrices(camera);
		glBegin(GL_TRIANGLES);
		foreach(auto& triangle, triangles2)
		{
			vec3 c = normals(triangle.ia);
			glColor3f(c.x, c.y, c.z);
			glTexCoord2f(vec2(triangle.ia)/vec2(sx, sy)); glNormal3f(normals(triangle.ia)); glVertex3f(triangle.a);
			glTexCoord2f(vec2(triangle.ib)/vec2(sx, sy)); glNormal3f(normals(triangle.ib)); glVertex3f(triangle.b);
			glTexCoord2f(vec2(triangle.ib)/vec2(sx, sy)); glNormal3f(normals(triangle.ic)); glVertex3f(triangle.c);
		}
		glEnd();
		gl::popMatrices();
		glPopAttrib();
		//gl::GlslProg::unbind();

		//gl::draw(walkerTex2, getWindowBounds());
	}
#endif
};

CINDER_APP(SApp, RendererGl)