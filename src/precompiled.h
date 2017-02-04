#define BOOST_RESULT_OF_USE_DECLTYPE 
#include <cmath>
#include <iostream>
#include <string>
#include <cinder/ip/Resize.h>
#include <complex>
#include <cinder/app/AppBasic.h>
#include <cinder/gl/GlslProg.h>
#include <cinder/gl/Texture.h>
#include <cinder/gl/Fbo.h>
#include <cinder/gl/gl.h>
#include <cinder/ImageIo.h>
#include <cinder/Vector.h>
#include <cinder/Rand.h>
#include <cinder/Camera.h>
#include <boost/foreach.hpp>
#include <fftw3.h>
#include <opencv2/imgproc.hpp>
#include <numeric>
#include <tuple>
#define foreach BOOST_FOREACH
using namespace ci;
using namespace std;
using namespace ci::app;
