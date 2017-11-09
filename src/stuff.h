#pragma once
#include "precompiled.h"
#include "util.h"
#include "qdebug.h"

const GLenum hdrFormat = GL_RGBA16F;

template<class F>
struct Transformed {
	F f;
	Transformed(F f) : f(f)
	{
	}
};

template<class Range, class F>
auto operator|(Array2D<Range> r, Transformed<F> transformed_) -> Array2D<decltype(transformed_.f(r(0, 0)))>
{
	Array2D<decltype(transformed_.f(r(0, 0)))> r2(r.w, r.h);
	forxy(r2)
	{
		r2(p) = transformed_.f(r(p));
	}
	return r2;
}

template<class F>
Transformed<F> transformed(F f)
{
	return Transformed<F>(f);
}

template<class T>
ivec2 wrapPoint(Array2D<T> const& src, ivec2 p)
{
	ivec2 wp = p;
	wp.x %= src.w; if(wp.x < 0) wp.x += src.w;
	wp.y %= src.h; if(wp.y < 0) wp.y += src.h;
	return wp;
}
template<class T>
T const& getWrapped(Array2D<T> const& src, ivec2 p)
{
	return src(wrapPoint(src, p));
}
template<class T>
T const& getWrapped(Array2D<T> const& src, int x, int y)
{
	return getWrapped(src, ivec2(x, y));
}
template<class T>
T& getWrapped(Array2D<T>& src, ivec2 p)
{
	return src(wrapPoint(src, p));
}
template<class T>
T& getWrapped(Array2D<T>& src, int x, int y)
{
	return getWrapped(src, ivec2(x, y));
}
struct WrapModes {
	struct GetWrapped {
		template<class T>
		static T& fetch(Array2D<T>& src, int x, int y)
		{
			return ::getWrapped(src, x, y);
		}
	};
	struct Get_WrapZeros {
		template<class T>
		static T& fetch(Array2D<T>& src, int x, int y)
		{
			return ::get_wrapZeros(src, x, y);
		}
	};
	struct NoWrap {
		template<class T>
		static T& fetch(Array2D<T>& src, int x, int y)
		{
			return src(x, y);
		}
	};
	typedef GetWrapped DefaultImpl;
};
template<class T, class FetchFunc>
Array2D<T> blur(Array2D<T>& src, int r, T zero = T())
{
	Array2D<T> newImg(src.w, src.h);
	float divide = pow(2.0f * r + 1.0f, 2.0f);
	for(int x = 0; x < src.w; x++)
	{
		T sum = zero;
		for(int y = -r; y <= r; y++)
		{
			sum += FetchFunc::fetch(src, x, y);
		}
		for(int y = 0; y < src.h; y++)
		{
			newImg(x, y) = sum;
			sum -= FetchFunc::fetch(src, x, y - r);
			sum += FetchFunc::fetch(src, x, y + r + 1);
		}
	}
	Array2D<T> newImg2(src.w, src.h);
	for(int y = 0; y < src.h; y++)
	{
		T sum = zero;
		for(int x = -r; x <= r; x++)
		{
			sum += FetchFunc::fetch(newImg, x, y);
		}
		for(int x = 0; x < src.w; x++)
		{
			newImg2(x, y) = sum;
			sum -= FetchFunc::fetch(newImg, x - r, y);
			sum += FetchFunc::fetch(newImg, x + r + 1, y);
		}
	}
	forxy(img)
		newImg2(p) /= divide;
	return newImg2;
}
template<class T>
Array2D<T> blur(Array2D<T>& src, int r, T zero = T())
{
	return blur<T, WrapModes::DefaultImpl>(src, r, zero);
}
template<class T, class FetchFunc>
void blurFaster_helper1d(Array2D<T>& arr, int r, ivec2 startXY, ivec2 endXY, ivec2 stepXY)
{
	// init stuff
	T zero = ::zero<T>();
	int d = 2 * r + 1;
	float normMul = 1.0f / float(d);
	
	// setup buffer
	struct BufferEntry
	{
		T val;
		BufferEntry* next;
	};
	vector<BufferEntry> buffer(d);
	for(int i = 0; i < buffer.size(); i++)
	{
		if(i == buffer.size() - 1)
		{
			buffer[i].next = &buffer[0];
			continue;
		}
		buffer[i].next = &buffer[i+1];
	}
	
	// fill beginning of buffer
	BufferEntry* b = &buffer[0];
	for(int i = -r; i <= r; i++)
	{
		ivec2 fetchPos = startXY + i * stepXY;
		b->val = FetchFunc::fetch(arr, fetchPos.x, fetchPos.y);
		b = b->next;
	}
	
	// init sum
	T sum=zero;
	foreach(auto& entry, buffer)
	{
		sum += entry.val;
	}
	
	// do main work
	BufferEntry* oldest = &buffer[0];
	BufferEntry* newest = &buffer[buffer.size() - 1];
	//const T* startPtr = &arr(startXY);
	//int ptrStep = &arr(stepXY) - &arr(0, 0);
	//T* outP = startPtr - r * ptrStep;
	ivec2 fetchOffset = stepXY * (r + 1);
	ivec2 outP;
	for(outP = startXY; outP + fetchOffset != endXY; outP += stepXY) {
		arr(outP) = sum * normMul;
		sum -= oldest->val;
		oldest = oldest->next;
		newest = newest->next;
		ivec2 fetchPos = outP + fetchOffset;
		newest->val = WrapModes::NoWrap::fetch(arr, fetchPos.x, fetchPos.y);
		sum += newest->val;
	}

	// do last part
	for(/*outP already initted*/; outP != endXY; outP += stepXY) {
		arr(outP) = sum * normMul;
		sum -= oldest->val;
		oldest = oldest->next;
		newest = newest->next;
		ivec2 fetchPos = outP + fetchOffset;
		newest->val = FetchFunc::fetch(arr, fetchPos.x, fetchPos.y);
		sum += newest->val;
	}
}
template<class T, class FetchFunc>
Array2D<T> blurFaster(Array2D<T>& src, int r, T zero = T())
{
	auto newImg = src.clone();
	// blur columns
	for(int x = 0; x < src.w; x++)
	{
		blurFaster_helper1d<T, FetchFunc>(newImg, r,
			/*startXY*/ivec2(x, 0),
			/*endXY*/ivec2(x, src.h),
			/*stepXY*/ivec2(0, 1));
	}
	// blur rows
	for(int y = 0; y < src.h; y++)
	{
		blurFaster_helper1d<T, FetchFunc>(newImg, r,
			/*startXY*/ivec2(0, y),
			/*endXY*/ivec2(src.w, y),
			/*stepXY*/ivec2(1, 0));
	}
	return newImg;
}
/*template<class T>
Array2D<T> blur2(Array2D<T> const& src, int r)
{
	T zero = ::zero<T>();
	Array2D<T> newImg(sx, sy);
	float mul = 1.0f/pow(2.0f * r + 1.0f, 2.0f);
	auto blur1d = [&](int imax, T* p0, T* pEnd, int step) {
		vector<T> buffer;
		int windowSize = 2*r+1;
		buffer.resize(windowSize, zero);
		T* back = &buffer[0];
		T* front = &buffer[buffer.size()-1];
		T* dst=p0;
		for(int i = 0; i <= r; i++)
		{
			*(back+i+r)=*dst;
			dst += step;
		}
		dst=p0;
		T sum=zero;
		T* rStep=r*step;
		for(dst=p0; dst!=pEnd; dst+=step)
		{
			*dst=sum;
			sum-=*back;
			front++;if(front==&*buffer.end())front=&buffer[0];
			back++;if(back==&*buffer.end())back=&buffer[0];
			*front=*(dst+rStep);
			sum+=*front;
		}
	};

	for(int x = 0; x < sx; x++)
	{
		T sum = zero;
		for(int y = 0; y <= r; y++)
		{
			sum += src(x, y);
		}
		for(int y = 0; y < sy; y++)
		{
			newImg(x, y) = sum;
			sum -= get_wrapZeros(src, x, y - r);
			sum += get_wrapZeros(src, x, y + r + 1);
		}
	}
	Array2D<T> newImg2(sx, sy);
	for(int y = 0; y < sy; y++)
	{
		T sum = zero;
		for(int x = 0; x <= r; x++)
		{
			sum += newImg(x, y);
		}
		for(int x = 0; x < sx; x++)
		{
			newImg2(x, y) = sum;
			sum -= get_wrapZeros(newImg, x - r, y);
			sum += get_wrapZeros(newImg, x + r + 1, y);
		}
	}
	forxy(img)
		newImg2(p) *= mul;
	return newImg2;
}*/
template<class T>
void aaPoint_i2(Array2D<T>& dst, ivec2 p, T value)
{
	if(dst.contains(p))
		dst(p) += value;
}
template<class T>
void aaPoint_i2(Array2D<T>& dst, int x, int y, T value)
{
	if(dst.contains(x, y))
		dst(p) += value;
}
template<class T>
void aaPoint2(Array2D<T>& dst, vec2 p, T value)
{
	aaPoint2(dst, p.x, p.y, value);
}
template<class T>
void aaPoint2(Array2D<T>& dst, float x, float y, T value)
{
	int ix = x, iy = y;
	float fx = ix, fy = iy;
	if(x < 0.0f && fx != x) { fx--; ix--; }
	if(y < 0.0f && fy != y) { fy--; iy--; }
	float fractx = x - fx;
	float fracty = y - fy;
	float fractx1 = 1.0 - fractx;
	float fracty1 = 1.0 - fracty;
	get2(dst, ix, iy) += (fractx1 * fracty1) * value;
	get2(dst, ix, iy+1) += (fractx1 * fracty) * value;
	get2(dst, ix+1, iy) += (fractx * fracty1) * value;
	get2(dst, ix+1, iy+1) += (fractx * fracty) * value;
}
template<class T>
void aaPoint2_fast(Array2D<T>& dst, vec2 p, T const& value)
{
	aaPoint2_fast(dst, p.x, p.y, value);
}
inline void my_assert_func(bool isTrue, string desc) {
	if(!isTrue) {
		cout << "assert failure: " << desc << endl;
		system("pause");
		throw std::runtime_error(desc.c_str());
	}
}
#define my_assert(isTrue) my_assert_func(isTrue, #isTrue);
//#define AAPOINT_DEBUG
template<class T>
void aaPoint2_fast(Array2D<T>& dst, float x, float y, T const& value)
{
	int ix = x, iy = y;
	float fx = ix, fy = iy;
#ifdef AAPOINT_DEBUG
	my_assert(x>=0.0f);
	my_assert(y>=0.0f);
	my_assert(ix+1<=dst.w-1);
	my_assert(iy+1<=dst.h-1);
#endif
	//if(x < 0.0f && fx != x) { fx--; ix--; }
	//if(y < 0.0f && fy != y) { fy--; iy--; }
	float fractx = x - fx;
	float fracty = y - fy;
	//float fractx1 = 1.0 - fractx;
	//float fracty1 = 1.0 - fracty;
	
	T partB = fracty*value;
	T partT = value - partB;
	T partBR = fractx * partB;
	T partBL = partB - partBR;
	T partTR = fractx * partT;
	T partTL = partT - partTR;
	auto basePtr = &dst(ix, iy);
	basePtr[0] += partTL;
	basePtr[1] += partTR;
	basePtr[dst.w] += partBL;
	basePtr[dst.w+1] += partBR;
}
template<class T>
T& get2(Array2D<T>& src, int x, int y)
{
	static T t;
	if(src.contains(x, y))
	{
		return src(x, y);
	}
	else
	{
		return t;
	}
}
template<class T>
void aaPoint_i(Array2D<T>& dst, ivec2 p, T value)
{
	dst.wr(p) += value;
}
template<class T>
void aaPoint_i(Array2D<T>& dst, int x, int y, T value)
{
	dst.wr(x, y) += value;
}
/// BEGIN CODE FOR BACKWARD COMPATIBILITY

	template<class T>
	void aaPoint_wrapZeros(Array2D<T>& dst, vec2 p, T value)
	{
		aaPoint_wrapZeros(dst, p.x, p.y, value);
	}
	template<class T>
	void aaPoint_wrapZeros(Array2D<T>& dst, float x, float y, T value)
	{
		int ix = x, iy = y;
		float fx = ix, fy = iy;
		if(x < 0.0f && fx != x) { fx--; ix--; }
		if(y < 0.0f && fy != y) { fy--; iy--; }
		float fractx = x - fx;
		float fracty = y - fy;
		float fractx1 = 1.0 - fractx;
		float fracty1 = 1.0 - fracty;
		get_wrapZeros(dst, ix, iy) += (fractx1 * fracty1) * value;
		get_wrapZeros(dst, ix, iy+1) += (fractx1 * fracty) * value;
		get_wrapZeros(dst, ix+1, iy) += (fractx * fracty1) * value;
		get_wrapZeros(dst, ix+1, iy+1) += (fractx * fracty) * value;
	}

/// END CODE FOR BACKWARD COMPATIBILITY

template<class T, class FetchFunc>
void aaPoint(Array2D<T>& dst, vec2 p, T value)
{
	aaPoint<T, FetchFunc>(dst, p.x, p.y, value);
}
template<class T, class FetchFunc>
void aaPoint(Array2D<T>& dst, float x, float y, T value)
{
	int ix = x, iy = y;
	float fx = ix, fy = iy;
	if(x < 0.0f && fx != x) { fx--; ix--; }
	if(y < 0.0f && fy != y) { fy--; iy--; }
	float fractx = x - fx;
	float fracty = y - fy;
	float fractx1 = 1.0 - fractx;
	float fracty1 = 1.0 - fracty;
	FetchFunc::fetch(dst, ix, iy) += (fractx1 * fracty1) * value;
	FetchFunc::fetch(dst, ix, iy+1) += (fractx1 * fracty) * value;
	FetchFunc::fetch(dst, ix+1, iy) += (fractx * fracty1) * value;
	FetchFunc::fetch(dst, ix+1, iy+1) += (fractx * fracty) * value;
}
template<class T>
void aaPoint(Array2D<T>& dst, float x, float y, T value)
{
	aaPoint<T, WrapModes::DefaultImpl>(dst, p.x, p.y, value);
}
template<class T>
void aaPoint(Array2D<T>& dst, vec2 p, T value)
{
	aaPoint<T, WrapModes::DefaultImpl>(dst, p, value);
}
template<class T, class FetchFunc>
T getBilinear(Array2D<T> src, vec2 p)
{
	return getBilinear<T, FetchFunc>(src, p.x, p.y);
}
template<class T, class FetchFunc>
T getBilinear(Array2D<T> src, float x, float y)
{
	int ix = x, iy = y;
	float fx = ix, fy = iy;
	if(x < 0.0f && fx != x) { fx--; ix--; }
	if(y < 0.0f && fy != y) { fy--; iy--; }
	float fractx = x - fx;
	float fracty = y - fy;
	return lerp(
		lerp(FetchFunc::fetch(src, ix, iy), FetchFunc::fetch(src, ix + 1, iy), fractx),
		lerp(FetchFunc::fetch(src, ix, iy + 1), FetchFunc::fetch(src, ix + 1, iy + 1), fractx),
		fracty);
}
template<class T>
T getBilinear(Array2D<T> src, float x, float y)
{
	return getBilinear<T, WrapModes::DefaultImpl>(src, x, y);
}
template<class T>
T getBilinear(Array2D<T> src, vec2 p)
{
	return getBilinear<T, WrapModes::DefaultImpl>(src, p);
}

inline gl::TextureRef gtex(Array2D<float> a)
{
	gl::Texture::Format fmt;
	fmt.setInternalFormat(hdrFormat);
	gl::TextureRef tex = gl::Texture2d::create(a.w, a.h, fmt);
	tex->bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, a.w, a.h, GL_RED, GL_FLOAT, a.data);
	return tex;
}
inline gl::TextureRef gtex(Array2D<vec2> a)
{
	gl::Texture::Format fmt;
	fmt.setInternalFormat(hdrFormat);
	gl::TextureRef tex = gl::Texture2d::create(a.w, a.h, fmt);
	tex->bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, a.w, a.h, GL_RG, GL_FLOAT, a.data);
	return tex;
}
inline gl::TextureRef gtex(Array2D<vec3> a)
{
	gl::Texture::Format fmt;
	fmt.setInternalFormat(hdrFormat);
	gl::TextureRef tex = gl::Texture2d::create(a.w, a.h, fmt);
	tex->bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, a.w, a.h, GL_RGB, GL_FLOAT, a.data);
	return tex;
}
inline Array2D<float> to01(Array2D<float> a) {
	auto minn = *std::min_element(a.begin(), a.end());
	auto maxx = *std::max_element(a.begin(), a.end());
	auto b = a.clone();
	forxy(b) {
		b(p) -= minn;
		b(p) /= (maxx - minn);
	}
	return b;
}
template<class T>
T& zero() {
	static T val=T()*0.0f;
	val = T()*0.0f;
	return val;
}

inline ivec2 clampPoint(ivec2 p, int w, int h)
{
	ivec2 wp = p;
	if(wp.x < 0) wp.x = 0;
	if(wp.x > w-1) wp.x = w-1;
	if(wp.y < 0) wp.y = 0;
	if(wp.y > h-1) wp.y = h-1;
	return wp;
}
template<class T>
T& get_clamped(Array2D<T>& src, int x, int y)
{
	return src(clampPoint(ivec2(x, y), src.w, src.h));
}
template<class T>
T const& get_clamped(Array2D<T> const& src, int x, int y)
{
	return src(clampPoint(ivec2(x, y), src.w, src.h));
}

template<class T>
Array2D<T> gauss3(Array2D<T> src) {
	T zero=::zero<T>();
	Array2D<T> dst1(src.w, src.h);
	Array2D<T> dst2(src.w, src.h);
	forxy(dst1)
		dst1(p) = .25f * (2 * get_clamped(src, p.x, p.y) + get_clamped(src, p.x-1, p.y) + get_clamped(src, p.x+1, p.y));
	forxy(dst2)
		dst2(p) = .25f * (2 * get_clamped(dst1, p.x, p.y) + get_clamped(dst1, p.x, p.y-1) + get_clamped(dst1, p.x, p.y+1));
	return dst2;
}

inline int sign(float f)
{
	if(f < 0)
		return -1;
	if(f > 0)
		return 1;
	return 0;
}
inline float expRange(float x, float min, float max) {
	return exp(lerp(log(min),log(max), x));
}

inline float niceExpRangeX(float mouseX, float min, float max) {
	float x2=sign(mouseX)*std::max(0.0f,abs(mouseX)-40.0f/(float)App::get()->getWindowWidth());
	return sign(x2)*expRange(abs(x2), min, max);
}

inline float niceExpRangeY(float mouseY, float min, float max) {
	float y2=sign(mouseY)*std::max(0.0f,abs(mouseY)-40.0f/(float)App::get()->getWindowHeight());
	return sign(y2)*expRange(abs(y2), min, max);
}

template<class Func>
class MapHelper {
private:
	static Func* func;
public:
	typedef typename decltype((*func)(ivec2(0, 0))) result_dtype;
};

template<class TSrc, class Func>
auto map(Array2D<TSrc> a, Func func) -> Array2D<typename MapHelper<Func>::result_dtype> {
	auto result = Array2D<typename MapHelper<Func>::result_dtype>(a.w, a.h);
	forxy(a) {
		result(p) = func(p);
	}
	return result;
}

template<class T>
vec2 gradient_i2(Array2D<T> src, ivec2 p)
{
	T nbs[3][3];
	for(int x = -1; x <= 1; x++) {
		for(int y = -1; y <= 1; y++) {
			nbs[x+1][y+1] = get_clamped(src, p.x + x, p.y + y);
		}
	}
	vec2 gradient;
	T aTL = nbs[0][0];
	T aTC = nbs[1][0];
	T aTR = nbs[2][0];
	T aML = nbs[0][1];
	T aMR = nbs[2][1];
	T aBL = nbs[0][2];
	T aBC = nbs[1][2];
	T aBR = nbs[2][2];
	// removed '2' distance-denominator for backward compat for now
	float norm = 1.0f/16.f; //1.0f / 32.0f;
	gradient.x = ((3.0f * aTR + 10.0f * aMR + 3.0f * aBR) - (3.0f * aTL + 10.0f * aML + 3.0f * aBL)) * norm;
	gradient.y = ((3.0f * aBL + 10.0f * aBC + 3.0f * aBR) - (3.0f * aTL + 10.0f * aTC + 3.0f * aTR)) * norm;
	//gradient.x = get_clamped(src, p.x + 1, p.y) - get_clamped(src, p.x - 1, p.y);
	//gradient.y = get_clamped(src, p.x, p.y + 1) - get_clamped(src, p.x, p.y - 1);
	return gradient;
}

inline void mm(Array2D<float> arr, string desc="") {
	std::stringstream prepend;
	if(desc!="") {
		 prepend << "[" << desc << "] ";
	}
	qDebug()<<prepend.str()<<"min: " << *std::min_element(arr.begin(),arr.end()) << ", "
		<<"max: " << *std::max_element(arr.begin(),arr.end());
}
//get_wrapZeros
template<class T>
T& get_wrapZeros(Array2D<T>& src, int x, int y)
{
	if(x < 0 || y < 0 || x >= src.w || y >= src.h)
	{
		return zero<T>();
	}
	return src(x, y);
}
template<class T>
T const& get_wrapZeros(Array2D<T> const& src, int x, int y)
{
	if(x < 0 || y < 0 || x >= src.w || y >= src.h)
	{
		return zero<T>();
	}
	return src(x, y);
}
template<class T, class FetchFunc>
vec2 gradient_i(Array2D<T> src, ivec2 p)
{
	//if(p.x<1||p.y<1||p.x>=src.w-1||p.y>=src.h-1)
	//	return vec2::zero();
	vec2 gradient;
	gradient.x = (FetchFunc::fetch(src,p.x + 1, p.y) - FetchFunc::fetch(src, p.x - 1, p.y)) / 2.0f;
	gradient.y = (FetchFunc::fetch(src,p.x, p.y + 1) - FetchFunc::fetch(src, p.x, p.y - 1)) / 2.0f;
	return gradient;
}
template<class T, class FetchFunc>
vec2 gradient_i_nodiv(Array2D<T> src, ivec2 p)
{
	//if(p.x<1||p.y<1||p.x>=src.w-1||p.y>=src.h-1)
	//	return vec2::zero();
	vec2 gradient;
	gradient.x = FetchFunc::fetch(src,p.x + 1, p.y) - FetchFunc::fetch(src, p.x - 1, p.y);
	gradient.y = FetchFunc::fetch(src,p.x, p.y + 1) - FetchFunc::fetch(src, p.x, p.y - 1);
	return gradient;
}
template<class T, class FetchFunc>
Array2D<vec2> get_gradients(Array2D<T> src)
{
	auto src2=src.clone();
	forxy(src2)
		src2(p) /= 2.0f;
	Array2D<vec2> gradients(src.w, src.h);
	for(int x = 0; x < src.w; x++)
	{
		gradients(x, 0) = gradient_i_nodiv<T, FetchFunc>(src2, ivec2(x, 0));
		gradients(x, src.h-1) = gradient_i_nodiv<T, FetchFunc>(src2, ivec2(x, src.h-1));
	}
	for(int y = 1; y < src.h-1; y++)
	{
		gradients(0, y) = gradient_i_nodiv<T, FetchFunc>(src2, ivec2(0, y));
		gradients(src.w-1, y) = gradient_i_nodiv<T, FetchFunc>(src2, ivec2(src.w-1, y));
	}
	for(int x=1; x < src.w-1; x++)
	{
		for(int y=1; y < src.h-1; y++)
		{
			gradients(x, y) = gradient_i_nodiv<T, WrapModes::NoWrap>(src2, ivec2(x, y));
		}
	}
	return gradients;
}
template<class T>
Array2D<vec2> get_gradients(Array2D<T> src)
{
	return get_gradients<T, WrapModes::DefaultImpl>(src);
}

inline gl::TextureRef maketex(int w, int h, GLint internalFormat) {
	gl::Texture::Format fmt; fmt.setInternalFormat(internalFormat); return gl::Texture::create(NULL, GL_RGBA, w, h, fmt);
}

template<class T>
Array2D<T> gettexdata(gl::TextureRef tex, GLenum format, GLenum type) {
	return gettexdata<T>(tex, format, type, tex.getBounds());
}

inline void checkGLError(string place)
{
	GLenum errCode;
	if ((errCode = glGetError()) != GL_NO_ERROR) 
	{
		qDebug()<<"GL error "<<hex<<errCode<<dec<< " at " << place;
	}
	else {
		qDebug() << "NO error at " << place;
	}
}
#define MY_STRINGIZE_DETAIL(x) #x
#define MY_STRINGIZE(x) MY_STRINGIZE_DETAIL(x)
#define CHECK_GL_ERROR() checkGLError(__FILE__ ": " MY_STRINGIZE(__LINE__))

template<class T>
Array2D<T> gettexdata(gl::TextureRef tex, GLenum format, GLenum type, ci::Area area) {
	Array2D<T> data(area.getWidth(), area.getHeight());
	tex.bind();
	//glGetTexImage(GL_TEXTURE_2D, 0, format, type, data.data);
	beginRTT(tex);
	glReadPixels(area.x1, area.y1, area.getWidth(), area.getHeight(), format, type, data.data);
	endRTT();
	//glGetTexS
	GLenum errCode;
	if ((errCode = glGetError()) != GL_NO_ERROR) 
	{
		qDebug() <<"ERROR"<<errCode;
	}
	return data;
}

float sq(float f);

vector<float> getGaussianKernel(int ksize, float sigma); // ksize must be odd

float sigmaFromKsize(float ksize);

float ksizeFromSigma(float sigma);

// KS means it has ksize and sigma args
template<class T,class FetchFunc>
Array2D<T> separableConvolve(Array2D<T> src, vector<float>& kernel) {
	int ksize = kernel.size();
	int r = ksize / 2;
	
	T zero=::zero<T>();
	Array2D<T> dst1(src.w, src.h);
	Array2D<T> dst2(src.w, src.h);
	
	int w = src.w, h = src.h;
	
	// vertical

	auto runtime_fetch = FetchFunc::fetch<T>;
	for(int y = 0; y < h; y++)
	{
		auto blurVert = [&](int x0, int x1) {
			// guard against w<r
			x0 = max(x0, 0);
			x1 = min(x1, w);

			for(int x = x0; x < x1; x++)
			{
				T sum = zero;
				for(int xadd = -r; xadd <= r; xadd++)
				{
					sum += kernel[xadd + r] * (runtime_fetch(src, x + xadd, y));
				}
				dst1(x, y) = sum;
			}
		};

		
		blurVert(0, r);
		blurVert(w-r, w);
		for(int x = r; x < w-r; x++)
		{
			T sum = zero;
			for(int xadd = -r; xadd <= r; xadd++)
			{
				sum += kernel[xadd + r] * src(x + xadd, y);
			}
			dst1(x, y) = sum;
		}
	}
	
	// horizontal
	for(int x = 0; x < w; x++)
	{
		auto blurHorz = [&](int y0, int y1) {
			// guard against h<r
			y0 = max(y0, 0);
			y1 = min(y1, h);
			for(int y = y0; y < y1; y++)
			{
				T sum = zero;
				for(int yadd = -r; yadd <= r; yadd++)
				{
					sum += kernel[yadd + r] * runtime_fetch(dst1, x, y + yadd);
				}
				dst2(x, y) = sum;
			}
		};

		blurHorz(0, r);
		blurHorz(h-r, h);
		for(int y = r; y < h-r; y++)
		{
			T sum = zero;
			for(int yadd = -r; yadd <= r; yadd++)
			{
				sum += kernel[yadd + r] * dst1(x, y + yadd);
			}
			dst2(x, y) = sum;
		}
	}
	return dst2;
}
// one-arg version for backward compatibility
template<class T,class FetchFunc>
Array2D<T> gaussianBlur(Array2D<T> src, int ksize) { // ksize must be odd.
	auto kernel = getGaussianKernel(ksize, sigmaFromKsize(ksize));
	return separableConvolve<T, FetchFunc>(src, kernel);
}
template<class T>
Array2D<T> gaussianBlur(Array2D<T> src, int ksize) {
	return gaussianBlur<T, WrapModes::DefaultImpl>(src, ksize);
}
struct denormal_check {
	static int num;
	static void begin_frame() {
		num = 0;
	}
	static void check(float f) {
		if ( f != 0 && fabsf( f ) < numeric_limits<float>::min() ) {
			// it's denormalized
			num++;
		}
	}
	static void end_frame() {
		qDebug() << "denormals detected: " << num;
	}
};

inline vector<Array2D<float> > split(Array2D<vec3> arr) {
	Array2D<float> r(arr.w, arr.h);
	Array2D<float> g(arr.w, arr.h);
	Array2D<float> b(arr.w, arr.h);
	forxy(arr) {
		r(p) = arr(p).x;
		g(p) = arr(p).y;
		b(p) = arr(p).z;
	}
	vector<Array2D<float> > result;
	result.push_back(r);
	result.push_back(g);
	result.push_back(b);
	return result;
}
inline void setWrapBlack(gl::TextureRef tex) {
	tex->bind();
	float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, black);
	tex->setWrap(GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);
}

inline Array2D<vec3> merge(vector<Array2D<float> > channels) {
	Array2D<float>& r = channels[0];
	Array2D<float>& g = channels[1];
	Array2D<float>& b = channels[2];
	Array2D<vec3> result(r.w, r.h);
	forxy(result) {
		result(p) = vec3(r(p), g(p), b(p));
	}
	return result;
}

inline Array2D<float> div(Array2D<vec2> a) {
	return ::map(a, [&](ivec2 p) -> float {
		auto dGx_dx = (a.wr(p.x+1,p.y).x-a.wr(p.x-1,p.y).x) / 2.0f;
		auto dGy_dy = (a.wr(p.x,p.y+1).y-a.wr(p.x,p.y-1).y) / 2.0f;
		return dGx_dx + dGy_dy;
	});
}

class FileCache {
public:
	static string get(string filename);
private:
	static std::map<string,string> db;
};

Array2D<vec3> resize(Array2D<vec3> src, ivec2 dstSize, const ci::FilterBase &filter);
Array2D<float> resize(Array2D<float> src, ivec2 dstSize, const ci::FilterBase &filter);


inline Array2D<vec2> gradientForward(Array2D<float> a) {
	return ::map(a, [&](ivec2 p) -> vec2 {
		return vec2(
			(a.wr(p.x + 1, p.y) - a.wr(p.x, p.y)) / 1.0f,
			(a.wr(p.x, p.y + 1) - a.wr(p.x, p.y)) / 1.0f
		);
	});
}

inline Array2D<float> divBackward(Array2D<vec2> a) {
	return ::map(a, [&](ivec2 p) -> float {
		auto dGx_dx = (a.wr(p.x, p.y).x - a.wr(p.x - 1, p.y).x);
		auto dGy_dy = (a.wr(p.x, p.y).y - a.wr(p.x, p.y - 1).y);
		return dGx_dx + dGy_dy;
	});
}

void disableGLReadClamp();

void enableDenormalFlushToZero();

void draw(const gl::TextureRef &texture, const Area &srcArea, const Rectf &dstRect, gl::GlslProgRef const& glsl);

void draw(const gl::TextureRef &texture, const Rectf &dstRect, gl::GlslProgRef const& glsl);

template<class TVec>
TVec safeNormalized(TVec const& vec) {
	TVec::value_type len = length(vec);
	if (len == 0.0f) {
		return vec;
	}
	return vec / len;
}

gl::TextureRef redToLuminance(gl::TextureRef const& in);