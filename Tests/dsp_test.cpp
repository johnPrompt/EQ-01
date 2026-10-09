#include "Equalizer.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <chrono>
using namespace curveshaper;
void expect(bool yes,const char* why) { if(!yes) throw std::runtime_error(why); }
float tone(int i,double fs,double f,float a=0.2f) { return a*static_cast<float>(std::sin(2*pi*f*i/fs)); }
double measure(Engine& e,double f,float amplitude=0.2f) {
    double in=0,out=0;
    for(int i=0;i<48000;++i) { float l=tone(i,e.fs,f,amplitude),r=l; const float original=l; e.process(l,r); if(i>24000) { in+=original*original; out+=l*l; } }
    return 10*std::log10(out/in);
}
int main() {
 try {
    auto e=std::make_unique<Engine>();
    e->prepare(48000);
    for(int i=0;i<4096;++i) { float x=tone(i,48000,973),l=x,r=x*0.3f; e->process(l,r); expect(l==x&&r==x*0.3f,"flat engine must be bit-exact"); }
    Band b; b.enabled=true; b.frequency=1000; b.gain=6; e->bands[0]=b; e->prepare(48000);
    expect(std::abs(measure(*e,1000)-6)<0.03,"bell centre gain incorrect");
    b.shape=Shape::HighPass; b.frequency=100; b.slope=48; b.gain=0; e->bands[0]=b; e->prepare(48000);
    expect(measure(*e,25)<-45,"48 dB/oct highpass attenuation incorrect");
    expect(std::abs(measure(*e,1000))<0.03,"highpass destroys passband");
    b.shape=Shape::AllPass; b.frequency=1000; b.slope=12; e->bands[0]=b; e->prepare(48000);
    expect(std::abs(measure(*e,1371))<0.03,"allpass changes magnitude");
    b.shape=Shape::Bell; b.placement=Placement::Side; b.gain=12; e->bands[0]=b; e->prepare(48000);
    for(int i=0;i<12000;++i) { float x=tone(i,48000,1000),l=x,r=x; e->process(l,r); expect(std::abs(l-r)<1e-7f&&std::abs(l-x)<1e-7f,"side EQ changes mono centre"); }
    b.placement=Placement::Mid; e->bands[0]=b; e->prepare(48000);
    for(int i=0;i<12000;++i) { float x=tone(i,48000,1000),l=x,r=-x; e->process(l,r); expect(std::abs(l-x)<1e-7f&&std::abs(r+x)<1e-7f,"mid EQ changes side-only signal"); }
    b.placement=Placement::Stereo; b.gain=0; b.motion=Motion::Dynamic; b.threshold=-30; b.above=-6; b.attack=1; b.release=50;
    e->bands[0]=b; e->prepare(48000); expect(measure(*e,1000)<-5.8,"above threshold compression not triggered");
    b.above=0; b.below=6; b.threshold=-20; e->bands[0]=b; e->prepare(48000);
    expect(measure(*e,1000,0.005f)>5.8,"below threshold upward compression not triggered");
    b.below=0; b.above=-6; b.external=true; e->bands[0]=b; e->prepare(48000);
    expect(std::abs(measure(*e,1000))<0.03,"missing external sidechain should not trigger above compression");
    for(int i=0;i<24000;++i) { float l=tone(i,48000,1000,0.005f),r=l,sc=tone(i,48000,1000,0.8f); e->process(l,r,sc,sc); }
    expect(e->runtime[0].currentGain<-5.8f,"external sidechain fails to trigger");
    b.external=false; b.relative=100; b.threshold=-10; b.above=-6;
    e->bands[0]=b; e->prepare(48000); const double relativeA=measure(*e,1000,0.1f);
    e->prepare(48000); const double relativeB=measure(*e,1000,0.01f);
    expect(std::abs(relativeA-relativeB)<0.05,"relative detector depends on absolute input level");
    std::cout<<"PASS: flat identity, bell gain, steep filters, allpass, M/S isolation, above/below dynamics, sidechain, relative detection\n";
    auto sp=std::make_unique<SpectralEngine>(); sp->reset(48000);
    std::array<Band,maxBands> bands{};
    std::vector<float> history(24000);
    double worst=0;
    for(int i=0;i<24000;++i) {
        float x=tone(i,48000,1371)+tone(i,48000,4210,0.07f),l=x,r=x*0.4f; history[i]=x;
        sp->process(l,r,0,0,bands);
        if(i>4096) { worst=std::max(worst,static_cast<double>(std::abs(l-history[i-SpectralEngine::size]))); expect(std::abs(r-l*0.4f)<1e-5f,"spectral identity breaks stereo linking"); }
    }
    expect(worst<1e-5,"STFT identity or reported latency incorrect");
    bands[0]=Band{}; bands[0].enabled=true; bands[0].motion=Motion::Spectral; bands[0].frequency=1500; bands[0].q=4; bands[0].threshold=-40; bands[0].above=-12; bands[0].attack=1; bands[0].density=100;
    sp->reset(48000);
    double target=0,untouched=0;
    for(int i=0;i<48000;++i) {
        float l=tone(i,48000,1500)+tone(i,48000,9000),r=l;
        sp->process(l,r,0,0,bands);
        if(i>24000) { target+=l*std::sin(2*pi*1500*(i-SpectralEngine::size)/48000); untouched+=l*std::sin(2*pi*9000*(i-SpectralEngine::size)/48000); }
    }
    expect(std::abs(target)<std::abs(untouched)*0.4,"spectral band doesn't selectively reduce target tone");
    expect(std::abs(untouched/(23999*0.1)-1)<0.02,"spectral processor damages frequencies outside band");
    std::cout<<"PASS: STFT reconstruction max error "<<worst<<", exact 2048 sample latency, stereo preservation, selective spectral suppression\n";
    // A real two-tone projection includes phase: the causal mode is minimum phase.
    e->bands={}; e->causalSpectral=true; e->bands[0]=bands[0]; e->prepare(48000);
    double ts=0,tc=0,us=0,uc=0;
    for(int i=0;i<48000;++i) {
        float l=tone(i,48000,1500)+tone(i,48000,9000),r=l; e->process(l,r);
        if(i>=24000) { ts+=l*std::sin(2*pi*1500*i/48000); tc+=l*std::cos(2*pi*1500*i/48000); us+=l*std::sin(2*pi*9000*i/48000); uc+=l*std::cos(2*pi*9000*i/48000); }
        expect(std::abs(l-r)<1e-6,"causal resonance stereo linking failed");
    }
    const double targetDb=20*std::log10(std::hypot(ts,tc)/2400),outsideDb=20*std::log10(std::hypot(us,uc)/2400);
    std::cout<<"Causal resonance target "<<targetDb<<" dB, outside "<<outsideDb<<" dB\n";
    expect(targetDb<-4&&std::abs(outsideDb)<0.3,"causal bank is not selectively suppressing resonances");
    e->prepare(48000); for(int i=0;i<4000;++i) { float l=0,r=0; e->process(l,r); expect(l==0&&r==0,"causal engine emits audio before impulse"); }
    float il=1,ir=1; e->process(il,ir); expect(std::abs(il)>0.9,"causal engine delayed the impulse");
    e->bands[0].placement=Placement::Side; e->prepare(48000);
    for(int i=0;i<12000;++i) { float x=tone(i,48000,1500),l=x,r=x; e->process(l,r); expect(std::abs(l-x)<1e-6&&std::abs(r-x)<1e-6,"causal side bank affects centre"); }
    e->bands[0].placement=Placement::Stereo; e->bands[0].external=true; e->prepare(48000);
    expect(std::abs(measure(*e,1500))<0.03,"causal missing sidechain triggers attenuation");
    e->bands[0].external=false; e->bands[0].relative=100; e->bands[0].threshold=-10; e->bands[0].density=50;
    e->prepare(48000); const auto ra=measure(*e,1500,0.1f); e->prepare(48000); const auto rb=measure(*e,1500,0.01f);
    expect(std::abs(ra-rb)<0.05,"causal relative detection is not gain invariant");
    e->bands[0].above=e->bands[0].below=0; e->bands[0].gain=6; e->prepare(48000);
    expect(std::abs(measure(*e,1500)-6)<0.04,"spectral motion must retain static EQ gain");
    std::cout<<"PASS: causal selective resonance suppression, zero audio buffer delay, stereo/M/S preservation, missing sidechain, relative invariance\n";
    e->causalSpectral=false;
    for(double fs:{44100.,48000.,96000.,192000.}) {
        for(int i=0;i<maxBands;++i) { auto& a=e->bands[i]; a=Band{}; a.enabled=true; a.frequency=static_cast<float>(40*std::pow(450.,i/23.)); a.q=i%2?0.15f:18.f; a.gain=i%2?12.f:-12.f; }
        e->prepare(fs);
        for(int i=0;i<12000;++i) {
            if(i%256==0) { auto next=e->bands; for(auto& a:next) { a.frequency=std::clamp(a.frequency*(i%512?1.07f:0.93f),20.f,20000.f); a.gain=-a.gain; } e->setBands(next); }
            float l=tone(i,fs,1973,0.01f),r=tone(i,fs,3301,0.01f); e->process(l,r);
            expect(std::isfinite(l)&&std::isfinite(r)&&std::abs(l)<50&&std::abs(r)<50,"unstable filter sweep");
        }
    }
    std::cout<<"PASS: 24-band automation stress at 44.1/48/96/192 kHz, finite bounded output\n";
    // Changing channel/shape uses a second filter path, without delaying audio.
    e->bands={}; e->bands[0]=Band{}; e->bands[0].enabled=true; e->bands[0].gain=12; e->bands[0].frequency=1000; e->prepare(48000);
    float last=0;
    for(int i=0;i<24000;++i) { float l=tone(i,48000,1000),r=l; e->process(l,r); if(i==23999) last=l; }
    auto next=e->bands; next[0].shape=Shape::HighPass; next[0].frequency=10000; next[0].slope=96; e->setBands(next);
    float first=tone(24000,48000,1000),right=first; e->process(first,right);
    expect(std::abs(first-last)<0.12,"topology change causes an abrupt reset jump");
    expect(e->transition[0]>0&&e->transition[0]<0.01f,"filter topology is not crossfaded");
    std::cout<<"PASS: 8 ms topology crossfade uses old/new filter paths without audio buffering\n";
    // Elapsed calculation time on this machine, not a DAW deadline/CPU-use guarantee.
    for(int active:{1,8,24}) {
        e->bands={}; e->causalSpectral=true;
        for(int i=0;i<active;++i) { e->bands[i]=bands[0]; e->bands[i].frequency=static_cast<float>(200*std::pow(40.,i/static_cast<double>(std::max(1,active-1)))); }
        e->prepare(48000); auto start=std::chrono::steady_clock::now(); float checksum=0;
        for(int i=0;i<48000;++i) { if(i%128==0) e->setBands(e->bands); float l=tone(i,48000,1733,0.1f),r=tone(i,48000,3741,0.1f); e->process(l,r); checksum+=l; }
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        expect(std::isfinite(checksum),"resonance benchmark output invalid");
        std::cout<<"BENCH: "<<active<<" causal resonance bands / 48 kHz stereo / 128-sample blocks: "<<ms<<" ms elapsed per 1000 ms audio\n";
    }
    return 0;
 } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
