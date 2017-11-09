#include "precompiled.h"
#include "util.h"

void trapFP()
{
	 // Get the default control word.
   int cw = _controlfp_s(NULL, 0,0 );

   // Set the exception masks OFF, turn exceptions on.
   cw &=~(EM_OVERFLOW|EM_UNDERFLOW|/*EM_INEXACT|*/EM_ZERODIVIDE|EM_DENORMAL);

   // Set the control word.
   _controlfp_s(NULL, cw, MCW_EM );
}

float randFloat()
{
	return rand() / (float)RAND_MAX;
}

void createConsole()
{
	AllocConsole();
	std::fstream* fs = new std::fstream("CONOUT$");
	std::cout.rdbuf(fs->rdbuf());
}

void loadFile(std::vector<unsigned char>& buffer, const std::string& filename) //designed for loading files from hard disk in an std::vector
{
  std::ifstream file(filename.c_str(), std::ios::in|std::ios::binary|std::ios::ate);
  if(!file) throw new runtime_error("file doesn't exist");
  //get filesize
  std::streamsize size = 0;
  if(file.seekg(0, std::ios::end).good()) size = file.tellg();
  if(file.seekg(0, std::ios::beg).good()) size -= file.tellg();

  //read contents of the file into the vector
  if(size > 0)
  {
    buffer.resize((size_t)size);
    file.read((char*)(&buffer[0]), size);
  }
  else buffer.clear();
}

float smoothstep(float edge0, float edge1, float x)
{
	// Scale, bias and saturate x to 0..1 range
	x = (x - edge0)/(edge1 - edge0);
	x = constrain(x, 0.f, 1.f);
	// Evaluate polynomial
	return x*x*(3 - 2*x);
}

// Program tested on Microsoft Visual Studio 2008 - Zahid Ghadialy
// This program shows example of Getting Elapsed Time
//#include <windows.h>
#include <MMSystem.h>
using namespace std;
#pragma comment(lib, "winmm.lib")

LARGE_INTEGER timerFreq_;
LARGE_INTEGER counterAtStart_;
namespace Stopwatch
{
	void Start()
	{
	  QueryPerformanceFrequency(&timerFreq_);
	  QueryPerformanceCounter(&counterAtStart_);
	  TIMECAPS ptc;
	  UINT cbtc = 8;
	  MMRESULT result = timeGetDevCaps(&ptc, cbtc);
	  if (result == TIMERR_NOERROR)
	  {
	  }
	  else
	  {
		cout<<"result = TIMER ERROR"<<endl;
	  }
	}

	double GetElapsedMilliseconds()
	{
	  if (timerFreq_.QuadPart == 0)
	  {
		return -1;
	  }
	  else
	  {
		LARGE_INTEGER c;
		QueryPerformanceCounter(&c);
		double elapsed = (c.QuadPart - counterAtStart_.QuadPart) * 1000 / (double)timerFreq_.QuadPart;
		return elapsed;
	  }
	}
}

void copyCvtData(ci::Surface8u const& surface, Array2D<vec3> dst) {
	forxy(dst) {
		ColorAT<uint8_t> inPixel = surface.getPixel(p);
		dst(p) = vec3(inPixel.r, inPixel.g, inPixel.b) / 255.0f;
	}
}
void copyCvtData(ci::SurfaceT<float> const& surface, Array2D<vec3> dst) {
	forxy(dst) {
		ColorAT<float> inPixel = surface.getPixel(p);
		dst(p) = vec3(inPixel.r, inPixel.g, inPixel.b);
	}
}
void copyCvtData(ci::SurfaceT<float> const& surface, Array2D<float> dst) {
	forxy(dst) {
		ColorAT<float> inPixel = surface.getPixel(p);
		dst(p) = inPixel.r;
	}
}
