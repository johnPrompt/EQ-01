#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>

// No heap allocation, locks, framework dependencies or I/O in the audio engine.
namespace curveshaper {
constexpr int maxBands = 24;
constexpr double pi = 3.14159265358979323846;
inline float dbGain(float x) { return std::pow(10.0f, x * 0.05f); }
inline float gainDb(float x) { return 20.0f * std::log10(std::max(x, 1.0e-9f)); }
enum class Shape { Bell, LowShelf, HighShelf, HighPass, LowPass, Notch, BandPass, AllPass, Tilt };
enum class Placement { Stereo, Left, Right, Mid, Side };
enum class Motion { Static, Dynamic, Spectral };
struct Band {
    bool enabled = false;
    Shape shape = Shape::Bell;
    Placement placement = Placement::Stereo;
    Motion motion = Motion::Static;
    float frequency = 1000, gain = 0, q = 1, slope = 12;
    float threshold = -24, above = -6, below = 0, ratio = 2, knee = 6;
    float attack = 15, release = 150, relative = 0;
    float detectorFrequency = 1000, detectorQ = 1, density = 50;
    bool external = false, freeDetector = false;
};
inline float dynamicGain(const Band& b,float level) {
    const float x=level-b.threshold, half=b.knee*0.5f;
    auto soft=[half](float v) { if(half<0.001f) return std::max(0.f,v); if(v<=-half) return 0.f; if(v>=half) return v; return (v+half)*(v+half)/(4*half); };
    const float factor=1-1/std::max(1.f,b.ratio);
    return std::copysign(std::min(std::abs(b.above),soft(x)*factor),b.above)+std::copysign(std::min(std::abs(b.below),soft(-x)*factor),b.below);
}
struct Coefficients {
    double b0=1, b1=0, b2=0, a1=0, a2=0;
    double magnitude(double f, double sr) const {
        const auto z=std::polar(1.0, -2*pi*f/sr);
        return std::abs((b0+b1*z+b2*z*z)/(1.0+a1*z+a2*z*z));
    }
};
inline Coefficients coefficients(Shape type, double fs, double f, double q, double db) {
    f=std::clamp(f, 5.0, fs*0.475); q=std::clamp(q, 0.1, 32.0);
    const double w=2*pi*f/fs, c=std::cos(w), s=std::sin(w), a=s/(2*q), A=std::pow(10.0,db/40), t=2*std::sqrt(A)*a;
    double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
    switch(type) {
        case Shape::Bell: b0=1+a*A; b1=-2*c; b2=1-a*A; a0=1+a/A; a1=-2*c; a2=1-a/A; break;
        case Shape::LowShelf: b0=A*((A+1)-(A-1)*c+t); b1=2*A*((A-1)-(A+1)*c); b2=A*((A+1)-(A-1)*c-t); a0=(A+1)+(A-1)*c+t; a1=-2*((A-1)+(A+1)*c); a2=(A+1)+(A-1)*c-t; break;
        case Shape::HighShelf: b0=A*((A+1)+(A-1)*c+t); b1=-2*A*((A-1)+(A+1)*c); b2=A*((A+1)+(A-1)*c-t); a0=(A+1)-(A-1)*c+t; a1=2*((A-1)-(A+1)*c); a2=(A+1)-(A-1)*c-t; break;
        case Shape::HighPass: b0=(1+c)/2; b1=-(1+c); b2=b0; a0=1+a; a1=-2*c; a2=1-a; break;
        case Shape::LowPass: b0=(1-c)/2; b1=1-c; b2=b0; a0=1+a; a1=-2*c; a2=1-a; break;
        case Shape::Notch: b0=1; b1=-2*c; b2=1; a0=1+a; a1=-2*c; a2=1-a; break;
        case Shape::BandPass: b0=a; b1=0; b2=-a; a0=1+a; a1=-2*c; a2=1-a; break;
        case Shape::AllPass: b0=1-a; b1=-2*c; b2=1+a; a0=1+a; a1=-2*c; a2=1-a; break;
        case Shape::Tilt: return coefficients(Shape::HighShelf,fs,f,q,db);
    }
    return {b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
}
struct Biquad {
    Coefficients c;
    double z1=0,z2=0;
    float process(float x) {
        const double y=c.b0*x+z1; z1=c.b1*x-c.a1*y+z2; z2=c.b2*x-c.a2*y;
        // Prevent denormal accumulation without altering normal audio.
        if(std::abs(z1)<1e-25) z1=0;
        if(std::abs(z2)<1e-25) z2=0;
        return static_cast<float>(y);
    }
    void reset() { z1=z2=0; }
};
inline int stages(const Band& b) {
    return (b.shape==Shape::HighPass||b.shape==Shape::LowPass) ? std::clamp(static_cast<int>(b.slope/12),1,8) : 1;
}
inline double magnitude(const Band& b,double f,double fs,float dynamic=0) {
    if(!b.enabled) return 1;
    const int count=stages(b); double value=1;
    for(int s=0;s<count;++s) {
        const double q=count>1?1/(2*std::cos(pi*(2*s+1)/(4*count))):b.q;
        value*=coefficients(b.shape,fs,b.frequency,q,b.gain+dynamic).magnitude(f,fs);
    }
    if(b.shape==Shape::Tilt) value*=coefficients(Shape::LowShelf,fs,b.frequency,b.q,-b.gain-dynamic).magnitude(f,fs);
    return value;
}
struct BandRuntime {
    std::array<std::array<Biquad,8>,2> eq;
    std::array<Biquad,2> tilt, detector;
    double envelope=1e-18, broadEnvelope=1e-18;
    float currentGain=0, smoothGain=0, smoothFrequency=1000, smoothQ=1, active=0;
    Shape lastShape=Shape::Bell;
    Placement lastPlacement=Placement::Stereo;
    Motion lastMotion=Motion::Static;
    Band settings;
    double sampleRate=48000;
    int count=1;
    double envelopeTime=0, attackTime=0, releaseTime=0;
    float coefficientTime=0, activationTime=0;
    void reset(double fs) {
        sampleRate=fs; envelope=broadEnvelope=1e-18; currentGain=0; active=0;
        envelopeTime=std::exp(-1/(fs*0.003));
        coefficientTime=static_cast<float>(1-std::exp(-32/(fs*0.008)));
        activationTime=static_cast<float>(1-std::exp(-1/(fs*0.005)));
        for(auto& lane:eq) for(auto& f:lane) f.reset();
        for(auto& f:tilt) f.reset();
        for(auto& f:detector) f.reset();
    }
    void configure(const Band& b,bool initial=false) {
        if(initial) { smoothGain=b.gain; smoothFrequency=b.frequency; smoothQ=b.q; }
        if(b.shape!=lastShape||b.placement!=lastPlacement||b.motion!=lastMotion) {
            for(auto& lane:eq) for(auto& f:lane) f.reset();
            for(auto& f:tilt) f.reset();
            for(auto& f:detector) f.reset();
            envelope=1e-18; currentGain=0;
        }
        lastShape=b.shape; lastPlacement=b.placement; lastMotion=b.motion;
        settings=b; count=stages(b);
        attackTime=std::exp(-1/(sampleRate*std::max(0.1f,b.attack)*0.001));
        releaseTime=std::exp(-1/(sampleRate*std::max(0.1f,b.release)*0.001));
        const double df=b.freeDetector?b.detectorFrequency:b.frequency;
        const double dq=b.freeDetector?b.detectorQ:b.q;
        for(auto& f:detector) f.c=coefficients(Shape::BandPass,sampleRate,df,dq,0);
    }
    void updateCoefficients() {
        const float smoothing=coefficientTime;
        smoothGain+=(settings.gain-smoothGain)*smoothing;
        smoothFrequency+=(settings.frequency-smoothFrequency)*smoothing;
        smoothQ+=(settings.q-smoothQ)*smoothing;
        for(int s=0;s<count;++s) {
            // Butterworth cascades for steep cut filters; Q remains meaningful for one stage.
            double q=smoothQ;
            if(count>1) q=1/(2*std::cos(pi*(2*s+1)/(4*count)));
            auto c=coefficients(settings.shape,sampleRate,smoothFrequency,q,smoothGain+currentGain);
            for(auto& lane:eq) lane[s].c=c;
        }
        for(auto& f:tilt) f.c=coefficients(Shape::LowShelf,sampleRate,smoothFrequency,smoothQ,-smoothGain-currentGain);
    }
    float dynamics(float level) const {
        return dynamicGain(settings,level);
    }
    void detect(float l,float r,float sl,float sr) {
        if(settings.motion!=Motion::Dynamic||!settings.enabled) { currentGain*=0.99f; return; }
        if(settings.external) { l=sl; r=sr; }
        if(settings.placement==Placement::Mid) l=r=(l+r)*0.5f;
        else if(settings.placement==Placement::Side) { const float s=(l-r)*0.5f; l=r=s; }
        else if(settings.placement==Placement::Left) r=l;
        else if(settings.placement==Placement::Right) l=r;
        const double a=detector[0].process(l), b=detector[1].process(r);
        const double power=(a*a+b*b)*0.5, broad=(l*l+r*r)*0.5;
        const double envTime=envelopeTime;
        envelope=envTime*envelope+(1-envTime)*power;
        broadEnvelope=envTime*broadEnvelope+(1-envTime)*broad;
        float db=10*static_cast<float>(std::log10(std::max(envelope,1e-18)));
        db-=settings.relative*0.01f*10*static_cast<float>(std::log10(std::max(broadEnvelope,1e-18)));
        const float target=dynamics(db);
        const double aTime=std::abs(target)>std::abs(currentGain)?attackTime:releaseTime;
        currentGain=static_cast<float>(aTime*currentGain+(1-aTime)*target);
    }
    float filter(int lane,float x) {
        for(int s=0;s<count;++s) x=eq[lane][s].process(x);
        if(settings.shape==Shape::Tilt) x=tilt[lane].process(x);
        return x;
    }
    void process(float& l,float& r) {
        const float target=settings.enabled?1.0f:0.0f;
        active+=(target-active)*activationTime;
        if(active<1e-6f&&target==0) return;
        float a=l,b=r;
        switch(settings.placement) {
            case Placement::Stereo: a=filter(0,l); b=filter(1,r); break;
            case Placement::Left: a=filter(0,l); break;
            case Placement::Right: b=filter(1,r); break;
            case Placement::Mid: { float m=(l+r)*0.5f,s=(l-r)*0.5f; m=filter(0,m); a=m+s; b=m-s; break; }
            case Placement::Side: { float m=(l+r)*0.5f,s=(l-r)*0.5f; s=filter(0,s); a=m+s; b=m-s; break; }
        }
        l+=(a-l)*active; r+=(b-r)*active;
    }
};

// A causal alternative to buffered spectral processing. A log-spaced bank follows
// resonant energy with minimum-phase filters. Detection takes time, audio is never delayed.
struct ResonanceBank {
    static constexpr int cells=13;
    std::array<std::array<Biquad,2>,cells> detector{}, filters{};
    std::array<std::array<double,2>,cells> energy{};
    std::array<std::array<float,2>,cells> gains{};
    std::array<float,cells> frequencies{}, weights{};
    Band settings;
    double fs=48000, envTime=0, broad=1e-18;
    float attackTime=0, releaseTime=0, active=0, activationTime=0, reduction=0;
    uint64_t tick=0;
    void reset(double rate) {
        fs=rate; broad=1e-18; tick=0; active=reduction=0;
        envTime=std::exp(-1/(fs*0.003)); activationTime=static_cast<float>(1-std::exp(-1/(fs*0.005)));
        for(auto& c:detector) for(auto& f:c) f.reset();
        for(auto& c:filters) for(auto& f:c) f.reset();
        for(auto& c:energy) c.fill(1e-18);
        for(auto& c:gains) c.fill(0);
    }
    void configure(const Band& b) {
        settings=b;
        attackTime=static_cast<float>(std::exp(-32/(fs*std::max(0.1f,b.attack)*0.001)));
        releaseTime=static_cast<float>(std::exp(-32/(fs*std::max(0.1f,b.release)*0.001)));
        const float step=std::clamp(1/(3*b.q),0.035f,0.8f);
        const float filterQ=std::clamp(1/(2*std::sinh(step*0.693147f*0.5f))*(0.75f+b.density*0.01f),2.f,96.f);
        for(int k=0;k<cells;++k) {
            const float oct=(k-cells/2)*step, f=b.frequency*std::pow(2.f,oct);
            frequencies[k]=std::clamp(f,20.f,static_cast<float>(fs*0.475));
            float w=std::exp(-0.5f*oct*oct*b.q*b.q);
            if(b.shape==Shape::LowShelf) w=1/(1+std::exp(5*oct));
            if(b.shape==Shape::HighShelf) w=1/(1+std::exp(-5*oct));
            weights[k]=(f>=20&&f<fs*0.475)?w:0;
            for(int lane=0;lane<2;++lane) {
                detector[k][lane].c=coefficients(Shape::BandPass,fs,frequencies[k],filterQ,0);
                filters[k][lane].c=coefficients(Shape::Bell,fs,frequencies[k],filterQ,gains[k][lane]);
            }
        }
    }
    void domain(float& l,float& r) const {
        if(settings.placement==Placement::Mid||settings.placement==Placement::Side) { const float m=(l+r)*0.5f,s=(l-r)*0.5f; l=m; r=s; }
        else if(settings.placement==Placement::Left) r=l;
        else if(settings.placement==Placement::Right) l=r;
    }
    void process(float& l,float& r,float sl,float sr,float inL,float inR) {
        const bool on=settings.enabled&&settings.motion==Motion::Spectral;
        active+=((on?1.f:0.f)-active)*activationTime;
        if(!on&&active<1e-6f) { reduction=0; return; }
        float dl=settings.external?sl:inL, dr=settings.external?sr:inR; domain(dl,dr);
        const double broadPower=settings.placement==Placement::Mid?dl*dl:settings.placement==Placement::Side?dr*dr:(dl*dl+dr*dr)*0.5;
        broad=envTime*broad+(1-envTime)*broadPower;
        for(int k=0;k<cells;++k) for(int lane=0;lane<2;++lane) {
            const float x=detector[k][lane].process(lane==0?dl:dr);
            energy[k][lane]=envTime*energy[k][lane]+(1-envTime)*x*x;
        }
        if(tick++%32==0) {
            reduction=0; const float rel=settings.relative*0.01f*10*static_cast<float>(std::log10(std::max(broad,1e-18)));
            const float step=std::clamp(1/(3*settings.q),0.035f,0.8f);
            const float filterQ=std::clamp(1/(2*std::sinh(step*0.693147f*0.5f))*(0.75f+settings.density*0.01f),2.f,96.f);
            for(int k=0;k<cells;++k) for(int lane=0;lane<2;++lane) {
                auto power=[&](int j) { return settings.placement==Placement::Stereo?(energy[j][0]+energy[j][1])*0.5:energy[j][lane]; };
                const double p=power(k), local=(power(std::max(0,k-2))+power(std::min(cells-1,k+2)))*0.5;
                const float prominence=10*static_cast<float>(std::log10(std::max(p,1e-18)/std::max(local,1e-18)));
                const float selectivity=std::clamp((prominence+3-settings.density*0.07f)/6,0.f,1.f);
                const float level=10*static_cast<float>(std::log10(std::max(p,1e-18)))-rel;
                const float target=on?dynamicGain(settings,level)*weights[k]*selectivity*0.7f:0;
                float& db=gains[k][lane]; const float t=std::abs(target)>std::abs(db)?attackTime:releaseTime;
                db=t*db+(1-t)*target;
                filters[k][lane].c=coefficients(Shape::Bell,fs,frequencies[k],filterQ,db);
                if(std::abs(db)>std::abs(reduction)) reduction=db;
            }
        }
        float a=l,z=r; domain(a,z);
        for(int k=0;k<cells;++k) {
            if(settings.placement!=Placement::Right&&settings.placement!=Placement::Side) a=filters[k][0].process(a);
            if(settings.placement!=Placement::Left&&settings.placement!=Placement::Mid) z=filters[k][1].process(z);
        }
        if(settings.placement==Placement::Mid||settings.placement==Placement::Side) { const float m=a,s=z; a=m+s; z=m-s; }
        else if(settings.placement==Placement::Left) z=r;
        else if(settings.placement==Placement::Right) a=l;
        l+=(a-l)*active; r+=(z-r)*active;
    }
};

template<int N> struct FFT {
    std::array<int,N> reverse{};
    std::array<std::complex<float>,N/2> roots{};
    FFT() {
        int bits=0; for(int x=N;x>1;x>>=1) ++bits;
        for(int i=0;i<N;++i) { int a=i,b=0; for(int j=0;j<bits;++j) { b=(b<<1)|(a&1); a>>=1; } reverse[i]=b; }
        for(int i=0;i<N/2;++i) roots[i]=std::polar(1.0f,static_cast<float>(-2*pi*i/N));
    }
    void transform(std::array<std::complex<float>,N>& a,bool inverse=false) const {
        for(int i=0;i<N;++i) if(i<reverse[i]) std::swap(a[i],a[reverse[i]]);
        for(int len=2;len<=N;len*=2) for(int start=0;start<N;start+=len)
            for(int j=0;j<len/2;++j) {
                const auto root=inverse?std::conj(roots[j*N/len]):roots[j*N/len];
                const auto u=a[start+j],v=a[start+j+len/2]*root;
                a[start+j]=u+v; a[start+j+len/2]=u-v;
            }
        if(inverse) for(auto& v:a) v/=static_cast<float>(N);
    }
};

// Fixed-resolution, fourfold overlap STFT. Exactly N samples latency, including identity.
struct SpectralEngine {
    static constexpr int size=2048,hop=512;
    FFT<size> fft;
    std::array<float,size> window{};
    std::array<std::array<float,size>,4> input{};
    std::array<std::array<float,size>,2> output{};
    std::array<std::array<std::complex<float>,size>,4> bins{};
    std::array<std::array<std::complex<float>,size>,4> source{};
    std::array<std::array<std::array<float,size/2+1>,2>,maxBands> gain{};
    int cursor=0,hopCounter=0;
    double fs=48000;
    SpectralEngine() { for(int i=0;i<size;++i) window[i]=std::sqrt(0.5f-0.5f*std::cos(static_cast<float>(2*pi*i/size))); }
    void reset(double sampleRate) {
        fs=sampleRate; cursor=hopCounter=0;
        for(auto& a:input) a.fill(0);
        for(auto& a:output) a.fill(0);
        for(auto& band:gain) for(auto& lane:band) lane.fill(0);
    }
    float weight(const Band& b,float f) const {
        if(f<5) return 0;
        const float oct=std::log2(f/b.frequency);
        if(b.shape==Shape::LowShelf) return 1/(1+std::exp(5*oct));
        if(b.shape==Shape::HighShelf) return 1/(1+std::exp(-5*oct));
        const float width=std::max(0.04f,1.0f/b.q);
        return std::exp(-0.5f*oct*oct/(width*width));
    }
    void frame(const std::array<Band,maxBands>& bands) {
        for(int c=0;c<4;++c) {
            for(int i=0;i<size;++i) bins[c][i]={input[c][(cursor+i)%size]*window[i],0};
            fft.transform(bins[c]);
        }
        source=bins; // All bands detect the same signal; editing order must not change detection.
        for(int band=0;band<maxBands;++band) {
            const auto& b=bands[band];
            if(!b.enabled||b.motion!=Motion::Spectral) { for(auto& lane:gain[band]) lane.fill(0); continue; }
            const int dc=b.external?2:0;
            double broadPower=0;
            for(int k=0;k<=size/2;++k) broadPower+=(std::norm(source[dc][k])+std::norm(source[dc+1][k]));
            const float relative=b.relative*0.01f*10*static_cast<float>(std::log10(std::max(broadPower/(size*size*0.5),1e-18)));
            const float attack=static_cast<float>(std::exp(-hop/(fs*std::max(0.1f,b.attack)*0.001)));
            const float release=static_cast<float>(std::exp(-hop/(fs*std::max(0.1f,b.release)*0.001)));
            for(int k=0;k<=size/2;++k) {
                const float w=weight(b,static_cast<float>(k*fs/size));
                auto detectorL=source[dc][k],detectorR=source[dc+1][k];
                if(b.placement==Placement::Mid||b.placement==Placement::Side) {
                    const auto dl=detectorL,dr=detectorR; detectorL=(dl+dr)*0.5f; detectorR=(dl-dr)*0.5f;
                }
                for(int lane=0;lane<2;++lane) {
                    bool use=true;
                    if((b.placement==Placement::Left||b.placement==Placement::Mid)&&lane==1) use=false;
                    if((b.placement==Placement::Right||b.placement==Placement::Side)&&lane==0) use=false;
                    if(!use) { gain[band][lane][k]=0; continue; }
                    float amplitude=std::abs(lane==0?detectorL:detectorR)*2/(size*0.63662f);
                    if(b.placement==Placement::Stereo) amplitude=std::sqrt((std::norm(detectorL)+std::norm(detectorR))*0.5f)*2/(size*0.63662f);
                    const float target=dynamicGain(b,gainDb(amplitude)-relative)*w;
                    float& state=gain[band][lane][k];
                    const float t=std::abs(target)>std::abs(state)?attack:release;
                    state=t*state+(1-t)*target;
                }
            }
            constexpr int radius=4;
            const float blur=(100-b.density)*0.01f;
            for(int k=0;k<=size/2;++k) {
                const auto l=bins[0][k],r=bins[1][k]; auto a=l,z=r;
                if(b.placement==Placement::Mid||b.placement==Placement::Side) { a=(l+r)*0.5f; z=(l-r)*0.5f; }
                for(int lane=0;lane<2;++lane) {
                    float db=0; int count=0;
                    for(int j=std::max(0,k-radius);j<=std::min(size/2,k+radius);++j) { db+=gain[band][lane][j]; ++count; }
                    db=gain[band][lane][k]*(1-blur)+db/std::max(1,count)*blur;
                    (lane==0?a:z)*=dbGain(db);
                }
                if(b.placement==Placement::Mid||b.placement==Placement::Side) { bins[0][k]=a+z; bins[1][k]=a-z; }
                else { bins[0][k]=a; bins[1][k]=z; }
            }
        }
        for(int c=0;c<2;++c) {
            for(int k=1;k<size/2;++k) bins[c][size-k]=std::conj(bins[c][k]);
            fft.transform(bins[c],true);
            for(int i=0;i<size;++i) output[c][(cursor+i)%size]+=bins[c][i].real()*window[i]*0.5f;
        }
    }
    void process(float& l,float& r,float scL,float scR,const std::array<Band,maxBands>& bands) {
        input[0][cursor]=l; input[1][cursor]=r; input[2][cursor]=scL; input[3][cursor]=scR;
        l=output[0][cursor]; r=output[1][cursor]; output[0][cursor]=output[1][cursor]=0;
        cursor=(cursor+1)%size;
        if(++hopCounter==hop) { hopCounter=0; frame(bands); }
    }
};

class Engine {
public:
    std::array<Band,maxBands> bands{};
    std::array<BandRuntime,maxBands> runtime{};
    std::array<BandRuntime,maxBands> previous{};
    std::array<float,maxBands> transition{};
    std::array<ResonanceBank,maxBands> resonance{};
    SpectralEngine spectral;
    double fs=48000;
    bool spectralMode=false, causalSpectral=false;
    uint64_t tick=0;
    float transitionStep=1;
    void prepare(double sampleRate) {
        fs=sampleRate; tick=0; transition.fill(1); transitionStep=static_cast<float>(1/(fs*0.008));
        for(int i=0;i<maxBands;++i) { runtime[i].reset(fs); runtime[i].configure(bands[i],true); runtime[i].updateCoefficients(); }
        for(int i=0;i<maxBands;++i) { resonance[i].reset(fs); resonance[i].configure(bands[i]); }
        spectral.reset(fs);
    }
    void setBands(const std::array<Band,maxBands>& next) {
        bands=next; for(int i=0;i<maxBands;++i) {
            const auto& old=runtime[i].settings; const auto& b=bands[i];
            if(old.shape!=b.shape||old.placement!=b.placement||old.motion!=b.motion||old.slope!=b.slope) { previous[i]=runtime[i]; transition[i]=0; }
            runtime[i].configure(b);
            if(causalSpectral&&((b.enabled&&b.motion==Motion::Spectral)||resonance[i].active>1e-6f)) resonance[i].configure(b);
        }
    }
    void process(float& l,float& r,float sl=0,float sr=0) {
        const float detectorL=l,detectorR=r;
        for(auto& band:runtime) band.detect(l,r,sl,sr);
        if(tick%32==0) for(auto& band:runtime) if(band.settings.enabled||band.active>1e-6f) band.updateCoefficients();
        ++tick;
        for(int i=0;i<maxBands;++i) {
            if(transition[i]<1) {
                float a=l,b=r; previous[i].process(a,b); runtime[i].process(l,r);
                const float blend=transition[i]; l=a+(l-a)*blend; r=b+(r-b)*blend;
                transition[i]=std::min(1.f,blend+transitionStep);
            } else runtime[i].process(l,r);
        }
        if(causalSpectral&&!spectralMode) for(auto& bank:resonance) bank.process(l,r,sl,sr,detectorL,detectorR);
        if(spectralMode) spectral.process(l,r,sl,sr,bands);
    }
};
} // namespace curveshaper
