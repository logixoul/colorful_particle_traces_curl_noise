#pragma once

#include "precompiled.h"

#define COUT_(a) cout << #a << " = " << a << endl;
inline string esc_macro_helper(string s) { return s.substr(1, s.length()-2); }
#define ESC_(s) esc_macro_helper(string(#s))

typedef unsigned char byte;

struct XSequential { template<class TArray, class TCoord> static int offset(TArray const& array, TCoord x, TCoord y)
{ return y*TCoord(array.w)+x; } };
struct YSequential { template<class TArray, class TCoord> static int offset(TArray const& array, TCoord x, TCoord y)
{ return x*TCoord(array.h)+y; } };

template<class T>
class ArrayDeleter
{
public:
	ArrayDeleter(T* arrayPtr)
	{
		refcountPtr = new int(0);
		(*refcountPtr)++;

		this->arrayPtr = arrayPtr;
	}

	ArrayDeleter(ArrayDeleter const& other)
	{
		arrayPtr = other.arrayPtr;
		refcountPtr = other.refcountPtr;
		(*refcountPtr)++;
	}

	ArrayDeleter const& operator=(ArrayDeleter const& other)
	{
		reduceRefcount();

		arrayPtr = other.arrayPtr;
		refcountPtr = other.refcountPtr;
		(*refcountPtr)++;
		
		return *this;
	}

	~ArrayDeleter()
	{
		reduceRefcount();
	}

private:
	void reduceRefcount()
	{
		(*refcountPtr)--;
		if(*refcountPtr == 0)
		{
			delete refcountPtr;
			//delete[] array;
			fftwf_free(arrayPtr);
		}
	}
	
	int* refcountPtr;
	T* arrayPtr;
};

enum nofill {};

template<class T, class MemoryLayoutPolicy = XSequential>
struct Array2D;

void copyCvtData(ci::Surface8u const& surface, Array2D<vec3> dst);
void copyCvtData(ci::SurfaceT<float> const& surface, Array2D<vec3> dst);
void copyCvtData(ci::SurfaceT<float> const& surface, Array2D<float> dst);

template<class T, class MemoryLayoutPolicy>
struct Array2D
{
	T* data;
	typedef T value_type;
	int area;
	int w, h;
	ci::ivec2 Size() const { return ci::ivec2(w, h); }
	ArrayDeleter<T> deleter;

	Array2D(int w, int h, nofill) : deleter(Init(w, h)) { }
	Array2D(ivec2 s, nofill) : deleter(Init(s.x, s.y)) { }
	Array2D(int dimension, nofill) : deleter(Init(dimension)) { }
	Array2D(int w, int h, T const& defaultValue = T()) : deleter(Init(w, h)) { fill(defaultValue); }
	Array2D(ivec2 s, T const& defaultValue = T()) : deleter(Init(s.x, s.y)) { fill(defaultValue); }
	Array2D(int dimension, T const& defaultValue = T()) : deleter(Init(dimension, dimension)) { fill(defaultValue); }
	Array2D() : deleter(Init(0, 0)) { }
	
	template<class TSrc>
	Array2D(ci::SurfaceT<TSrc> const& surface) : deleter(Init(surface.getWidth(), surface.getHeight()))
	{
		::copyCvtData(surface, *this);
	}
	
	template<class TSrc>
	Array2D(cv::Mat_<TSrc> const& mat) : deleter(nullptr)
	{
		Init(mat.cols, mat.rows, (T*)mat.data);
	}

	T* begin() { return data; }
	T* end() { return data+w*h; }
	T const* begin() const { return data; }
	T const* end() const { return data+w*h; }
	
	T& operator()(int i) { return data[i]; }
	T const& operator()(int i) const { return data[i]; }

	T& operator()(int x, int y) { return data[MemoryLayoutPolicy::offset(*this, x, y)]; }
	T const& operator()(int x, int y) const { return data[MemoryLayoutPolicy::offset(*this, x, y)]; }

	T& operator()(ivec2 const& v) { return data[MemoryLayoutPolicy::offset(*this, v.x, v.y)]; }
	T const& operator()(ivec2 const& v) const { return data[MemoryLayoutPolicy::offset(*this, v.x, v.y)]; }
	
	ivec2 wrapPoint(ivec2 p)
	{
		ivec2 wp = p;
		wp.x %= w; if(wp.x < 0) wp.x += w;
		wp.y %= h; if(wp.y < 0) wp.y += h;
		return wp;
	}
	
	T& wr(int x, int y) { return wr(ivec2(x, y)); }
	T const& wr(int x, int y) const { return wr(ivec2(x, y)); }

	T& wr(ivec2 const& v) { return (*this)(wrapPoint(v)); }
	T const& wr(ivec2 const& v) const { return (*this)(wrapPoint(v)); }

	int offsetOf(int x, int y) const { return MemoryLayoutPolicy::offset(*this, x, y); }
	int offsetOf(ci::ivec2 const& p) const { return MemoryLayoutPolicy::offset(*this, p.x, p.y); }
	bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
	bool contains(ivec2 const& p) const { return p.x >= 0 && p.y >= 0 && p.x < w && p.y < h; }

	int xStep() const { return MemoryLayoutPolicy::offset(*this, 1, 0) - MemoryLayoutPolicy::offset(*this, 0, 0); }
	int yStep() const { return MemoryLayoutPolicy::offset(*this, 0, 1) - MemoryLayoutPolicy::offset(*this, 0, 0); }

	Array2D clone(){
		Array2D result(Size());
		std::copy(begin(), end(), result.begin());
		return result;
	}

private:
	void fill(T const& value)
	{
		std::fill(begin(), end(), value);
	}
	T* Init(int w, int h) {
		// fftwf_malloc so we can use "new-array execute" fftw functions
		auto data = (T*)fftwf_malloc(w * h * sizeof(T)); // data = new T[w * h]
		Init(w, h, data);
		return data;
	}
	void Init(int w, int h, T* data) {
		this->data = data;
		area = w * h;
		this->w = w;
		this->h = h;
	}
};

inline ivec2 imod(ivec2 a, ivec2 b)
{
	return ivec2(a.x % b.x, a.y % b.y);
}

inline void rotate(vec2& p, float angle)
{
    float c = cos(angle), s = sin(angle);
    p = vec2(p.x * c + p.y * (-s), p.x * s + p.y * c);
}

inline bool isnan_(float f) { return f!=f; }

inline void check(vec3 v)
{
	if(isnan_(v.x) || isnan_(v.y) || isnan_(v.z)) throw exception();
}

void trapFP();

template<class F> vec3 apply(vec3 const& v, F f)
{
	return vec3(f(v.x), f(v.y), f(v.z));
}

// todo rm this
/*

template<class F,class T> Vec3<T> apply(Vec3<T> const& v, F f)
{
return Vec3<T>(f(v.x), f(v.y), f(v.z));
}

*/

template<class F> float apply(float v, F f)
{
	return f(v);
}

// not sure if the following is needed. it causes a macro redefinition warning.
// commenting it out until I've seen whether removing it breaks my projs.
// #define forxy(w, h) for(int i = 0; i < w; i++) for(int j = 0; j < h; j++)
#define forxy(image) \
	for(ivec2 p(0, 0); p.x < image.w; p.x++) \
		for(p.y = 0; p.y < image.h; p.y++) \
			for(bool SOME_LONG_BOOL=true; SOME_LONG_BOOL;) \
				for(decltype(image(p))& pixel=image(p); SOME_LONG_BOOL; SOME_LONG_BOOL = false)

inline float psin(float a)
{
	return sin(a)*0.5f + 0.5f;
}

const float pi = 3.14159265f;
const float twoPi = 2.0f * 3.14159265f;
const float halfPi = .5f * 3.14159265f;

void loadFile(std::vector<unsigned char>& buffer, const std::string& filename);

// todo rm this
/*template<class T>
T min(ci::Vec3<T> vec)
{
	return std::min(vec.x, std::min(vec.y, vec.z));
}

template<class T>
T max(ci::Vec3<T> vec)
{
	return std::max(vec.x, std::max(vec.y, vec.z));
}*/

template<class Source>
string ToString(const Source& Value)
{
    std::ostringstream oss;
    oss << Value;
    return oss.str();
}

template<class T>
T Parse(string const& src)
{
	std::istringstream iss(src);
	T t;
	iss >> t;
	return t;
}

template <typename T> int sgn(T val)
{
    return (val > T(0)) - (val < T(0));
}

float smoothstep(float edge0, float edge1, float x);

namespace Stopwatch
{
	void Start();
	double GetElapsedMilliseconds();
}

#define fortimes(times) for(int i = 0; i < times; i++)

#define FOR(variable, from, to) for(int variable = from; variable <= to; variable++)

void createConsole();

template<class T>
struct ListOf
{
	vector<T> data;
	ListOf(T t)
	{
		data.push_back(t);
	}
	ListOf<T>& operator()(T t)
	{
		data.push_back(t);
		return *this;
	}
	operator vector<T>()
	{
		return data;
	}
};

template<class T>
ListOf<T> list_of(T t)
{
	return ListOf<T>(t);
}

template<class T>
Array2D<T> empty_like(Array2D<T> a) {
	return Array2D<T>(a.Size(), nofill());
}

template<class T>
Array2D<T> ones_like(Array2D<T> a) {
	return Array2D<T>(a.Size(), 1.0f);
}

template<class T>
Array2D<T> zeros_like(Array2D<T> a) {
	return Array2D<T>(a.Size(), ::zero<T>());
}
