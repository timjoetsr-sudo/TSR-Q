#pragma once
#include "TsrqDsp.h"
#include <cstdio>
#include <vector>
#include <random>
#include <chrono>
#include <complex>
using namespace tsrq;
static BandParams band(int t,double f,double g,double q,int sl=12,int pl=Stereo){BandParams b;b.used=1;b.type=t;b.f=f;b.gain=g;b.q=q;b.slope=sl;b.place=pl;return b;}
// response of a real sequence at frequency f (DFT eval)
static std::complex<double> dft(const std::vector<double>& h,double f,double fs,int off=0){ std::complex<double> s=0; const double w=-2*kPi*f/fs; for(size_t i=0;i<h.size();++i){ if(h[i]==0) continue; s+=h[i]*std::polar(1.0,w*((double)i-off)); } return s; }
// naive FFT magnitude via own FFT
static std::vector<double> magdb_fft(const std::vector<double>& h, int Nfft){ Fft f; f.init(Nfft); std::vector<Cpx> a(Nfft); for(int i=0;i<(int)h.size()&&i<Nfft;++i) a[i]=h[i]; f.run(a.data(),false); std::vector<double> m(Nfft/2+1); for(int k=0;k<=Nfft/2;++k) m[k]=20*std::log10(std::abs(a[k])+1e-30); return m; }
static double anaDb(const BandParams& b,double f){ SecSet s; sectionsFor(b,hasGain(b.type)?b.gain:0,s); return 10*std::log10(anaMag2(s,f)); }
