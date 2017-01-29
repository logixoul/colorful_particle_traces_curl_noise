#include "precompiled.h"
#include "easyfft.h"

Array2D<Complexf> fft(Array2D<float> in, int flags)
{
	Array2D<Complexf> in_complex(in.Size());
	forxy(in)
	{
		in_complex(p) = Complexf(in(p));
	}
	
	Array2D<Complexf> result(in.Size());
	
	auto plan = fftwf_plan_dft_2d(in.h, in.w,
		(fftwf_complex*)in_complex.data, (fftwf_complex*)result.data, FFTW_FORWARD, flags);
	fftwf_execute(plan);
	auto mul = 1.0f / sqrt((float)result.area);
	forxy(result)
	{
		result(p) *= mul;
	}
	return result;
}

Array2D<float> ifft(Array2D<Complexf> in, int flags)
{
	Array2D<Complexf> result(in.Size());
	auto plan = fftwf_plan_dft_2d(in.h, in.w,
		(fftwf_complex*)in.data, (fftwf_complex*)result.data, FFTW_BACKWARD, flags);
	fftwf_execute(plan);

	Array2D<float> out_real(in.Size());
	forxy(in)
	{
		out_real(p) = result(p).real();
	}
	auto mul = 1.0f / sqrt((float)out_real.area);
	forxy(out_real)
	{
		out_real(p) *= mul;
	}
	return out_real;
}