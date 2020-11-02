#include "precompiled.h"
#include "easyfft.h"
#include "sw.h"
#if 1
ivec2 toHermitianSize(ivec2 in) {
	return ivec2(in.x / 2 + 1, in.y);
}

ivec2 fromHermitianSize(ivec2 in) {
	return ivec2((in.x - 1) * 2, in.y);
}

class PlanCache {
public:
	static fftwf_plan getPlan(ivec2 logicalSize, int direction, int flags) {
		Key key = { logicalSize, direction };
		if(cache.find(key) == cache.end()) {
			Array2D<vec2> in(logicalSize, nofill());
			Array2D<vec2> out(logicalSize, nofill());
			fftwf_set_timelimit(1.0f);
			fftwf_plan plan;
			if(direction == FFTW_FORWARD)
				plan = fftwf_plan_dft_r2c_2d(logicalSize.y, logicalSize.x, (float*)in.data, (fftwf_complex*)out.data, flags);
			else
				plan = fftwf_plan_dft_c2r_2d(logicalSize.y, logicalSize.x, (fftwf_complex*)in.data, (float*)out.data, flags);
			cache[key] = plan;
			return plan;
		}
		return cache[key];
	}
private:
	struct Key {
		ivec2 arrSize;
		int direction;
	};
	struct KeyComparator {
		bool operator()(Key const& l, Key const& r) const {
			auto tiedL = std::tie(l.arrSize.x, l.arrSize.y, l.direction);
			auto tiedR = std::tie(r.arrSize.x, r.arrSize.y, r.direction);
			return tiedL < tiedR;
		}
	};
	static std::map<Key, fftwf_plan, KeyComparator> cache;
};

std::map<PlanCache::Key, fftwf_plan, PlanCache::KeyComparator> PlanCache::cache;

Array2D<vec2> fft(Array2D<float> in, int flags)
{
	Array2D<vec2> result(toHermitianSize(in.Size()));
	
	auto plan = PlanCache::getPlan(in.Size(), FFTW_FORWARD, flags);
	fftwf_execute_dft_r2c(plan, in.data, (fftwf_complex*)result.data);
	return result;
}

Array2D<float> ifft(Array2D<vec2> in, ivec2 outSize, int flags)
{
	Array2D<float> result(outSize);
	auto plan = PlanCache::getPlan(outSize, FFTW_BACKWARD, flags);
	fftwf_execute_dft_c2r(plan, (fftwf_complex*)in.data, result.data);
	return result;
}
#endif