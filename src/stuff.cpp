#include "precompiled.h"
#include "stuff.h"

int denormal_check::num;

std::map<string,string> FileCache::db;

string FileCache::get(string filename) {
	if(db.find(filename)==db.end()) {
		std::vector<unsigned char> buffer;
		loadFile(buffer,filename);
		string bufferStr(buffer.data(), buffer.data() + buffer.size());
		db[filename]=bufferStr;
	}
	return db[filename];
}

Array2D<Vec3f> resize(Array2D<Vec3f> src, Vec2i dstSize, const ci::FilterBase &filter)
{
	ci::SurfaceT<float> tmpSurface(
		(float*)src.data, src.w, src.h, /*rowBytes*/sizeof(Vec3f) * src.w, ci::SurfaceChannelOrder::RGB);
	auto resizedSurface = ci::ip::resizeCopy(tmpSurface, tmpSurface.getBounds(), dstSize, filter);
	Array2D<Vec3f> resultArray = resizedSurface;
	return resultArray;
}

Array2D<float> resize(Array2D<float> src, Vec2i dstSize, const ci::FilterBase &filter)
{
#if 0
	ci::ChannelT<float> tmpSurface(
		(float*)src.data, src.w, src.h, /*rowBytes*/sizeof(float) * src.w, ci::SurfaceChannelOrder::ABGR);
	auto resizedSurface = ci::ip::resizeCopy(tmpSurface, tmpSurface.getBounds(), dstSize, filter);
	Array2D<float> resultArray = resizedSurface;
	return resultArray;
#endif
	auto srcRgb = ::merge(list_of(src)(src)(src));
	auto resized = resize(srcRgb, dstSize, filter);
	return ::split(resized)[0];
}

float sq(float f) {
	return f * f;
}

vector<float> getGaussianKernel(int ksize, float sigma) {
	vector<float> result;
	int r=ksize/2;
	float sum=0.0f;
	for(int i=-r;i<=r;i++) {
		float exponent = -(i*i/sq(2*sigma));
		float val = exp(exponent);
		sum += val;
		result.push_back(val);
	}
	for(int i=0; i<result.size(); i++) {
		result[i] /= sum;
	}
	return result;
}

float sigmaFromKsize(int ksize) {
	float sigma = 0.3*((ksize-1)*0.5 - 1) + 0.8;
	return sigma;
}

float ksizeFromSigma(float sigma) {
	// ceil just to be sure
	int ksize = ceil(((sigma - 0.8) / 0.3 + 1) / 0.5 + 1);
	if(ksize % 2 == 0)
		ksize++;
	return ksize;
}
