#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr const char* keys[]{"on","shape","placement","motion","frequency","gain","q","slope","threshold","above","below","ratio","knee","attack","release","relative","detectorFrequency","detectorQ","external","freeDetector","density"};
constexpr const char* globalKeys[]{"output","bypass","engine","character","drive","width","delta","freeze"};
using namespace curveshaper;
Band readBand(const std::array<std::atomic<float>*,21>& p) {
    Band b; b.enabled=p[0]->load()>0.5f; b.shape=static_cast<Shape>(static_cast<int>(p[1]->load()));
    b.placement=static_cast<Placement>(static_cast<int>(p[2]->load())); b.motion=static_cast<Motion>(static_cast<int>(p[3]->load()));
    b.frequency=p[4]->load(); b.gain=p[5]->load(); b.q=p[6]->load(); b.slope=p[7]->load();
    b.threshold=p[8]->load(); b.above=p[9]->load(); b.below=p[10]->load(); b.ratio=p[11]->load();
    b.knee=p[12]->load(); b.attack=p[13]->load(); b.release=p[14]->load(); b.relative=p[15]->load();
    b.detectorFrequency=p[16]->load(); b.detectorQ=p[17]->load(); b.external=p[18]->load()>0.5f; b.freeDetector=p[19]->load()>0.5f;
    b.density=p[20]->load();
    return b;
}
std::array<float,21> bandValues(const Band& b) {
    return {b.enabled?1.f:0.f,static_cast<float>(b.shape),static_cast<float>(b.placement),static_cast<float>(b.motion),
            b.frequency,b.gain,b.q,b.slope,b.threshold,b.above,b.below,b.ratio,b.knee,b.attack,b.release,b.relative,
            b.detectorFrequency,b.detectorQ,b.external?1.f:0.f,b.freeDetector?1.f:0.f,b.density};
}
}
juce::CriticalSection EQ1Processor::registryLock;
std::vector<EQ1Processor*> EQ1Processor::registry;
std::atomic<uint64_t> EQ1Processor::nextId{1};
curveshaper::Band EQ1Processor::clipboard;
bool EQ1Processor::clipboardValid=false;
juce::String EQ1Processor::id(int band,const char* key) { return "b"+juce::String(band+1)+"."+key; }

juce::AudioProcessorValueTreeState::ParameterLayout EQ1Processor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    auto floating=[&](juce::String key,juce::String name,float low,float high,float def,float skew=1.f) {
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{key,1},name,juce::NormalisableRange<float>(low,high,0,skew),def));
    };
    auto boolean=[&](juce::String key,juce::String name,bool def=false) {
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{key,1},name,def));
    };
    auto choice=[&](juce::String key,juce::String name,juce::StringArray choices,int def=0) {
        result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{key,1},name,choices,def));
    };
    // Stable IDs, order and version hints are a project compatibility contract.
    for(int i=0;i<maxBands;++i) {
        const auto prefix="Band "+juce::String(i+1)+" ";
        boolean(id(i,"on"),prefix+"Enabled");
        choice(id(i,"shape"),prefix+"Shape",{"Bell","Low shelf","High shelf","High pass","Low pass","Notch","Band pass","All pass","Tilt"});
        choice(id(i,"placement"),prefix+"Placement",{"Stereo","Left","Right","Mid","Side"});
        choice(id(i,"motion"),prefix+"Motion",{"Static","Dynamic","Spectral"});
        floating(id(i,"frequency"),prefix+"Frequency",20,20000,1000,0.22f);
        floating(id(i,"gain"),prefix+"Gain",-24,24,0);
        floating(id(i,"q"),prefix+"Q",0.1f,32,1,0.35f);
        choice(id(i,"slope"),prefix+"Slope index",{"12","24","36","48","60","72","84","96"});
        floating(id(i,"threshold"),prefix+"Threshold",-90,0,-24);
        floating(id(i,"above"),prefix+"Above range",-24,24,-6);
        floating(id(i,"below"),prefix+"Below range",-24,24,0);
        floating(id(i,"ratio"),prefix+"Ratio",1,20,2,0.4f);
        floating(id(i,"knee"),prefix+"Knee",0,24,6);
        floating(id(i,"attack"),prefix+"Attack ms",0.1f,500,15,0.3f);
        floating(id(i,"release"),prefix+"Release ms",5,3000,150,0.35f);
        floating(id(i,"relative"),prefix+"Relative detection",0,100,0);
        floating(id(i,"detectorFrequency"),prefix+"Detector frequency",20,20000,1000,0.22f);
        floating(id(i,"detectorQ"),prefix+"Detector Q",0.1f,32,1,0.35f);
        boolean(id(i,"external"),prefix+"External sidechain"); boolean(id(i,"freeDetector"),prefix+"Free detector");
    }
    floating("output","Output gain",-24,24,0);
    boolean("bypass","Bypass"); choice("engine","Engine",{"Live","Spectral"});
    choice("character","Character",{"Clean","Subtle","Warm"});
    floating("drive","Drive",0,18,0); floating("width","Stereo width",0,200,100);
    boolean("delta","Delta monitor"); boolean("freeze","Analyzer freeze");
    // Append new controls after every 0.1 parameter to preserve host parameter order.
    for(int i=0;i<maxBands;++i) floating(id(i,"density"),"Band "+juce::String(i+1)+" Resonance density",0,100,50);
    return result;
}
EQ1Processor::EQ1Processor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                  .withOutput("Output",juce::AudioChannelSet::stereo(),true)
                                  .withInput("Sidechain",juce::AudioChannelSet::stereo(),false)),
   parameters(*this,nullptr,"CurveShaperState",layout()),
   engine(std::make_unique<Engine>()),spectral(std::make_unique<SpectralEngine>()) {
    for(int i=0;i<maxBands;++i) for(int k=0;k<fields;++k) cached[i][k]=parameters.getRawParameterValue(id(i,keys[k]));
    for(int k=0;k<8;++k) globals[k]=parameters.getRawParameterValue(globalKeys[k]);
    parameters.addParameterListener("engine",this);
    parameters.addParameterListener("character",this);
    instanceId=nextId.fetch_add(1);
    const juce::ScopedLock lock(registryLock); registry.push_back(this);
}
EQ1Processor::~EQ1Processor() {
    cancelPendingUpdate(); parameters.removeParameterListener("engine",this);
    parameters.removeParameterListener("character",this);
    const juce::ScopedLock lock(registryLock);
    registry.erase(std::remove(registry.begin(),registry.end(),this),registry.end());
}
void EQ1Processor::parameterChanged(const juce::String&,float) { triggerAsyncUpdate(); }
void EQ1Processor::handleAsyncUpdate() {
    setLatencySamples(requestedLatency());
}
int EQ1Processor::requestedLatency() const { return (value("character")>0.5f?baseLatency:0)+(value("engine")>0.5f?SpectralEngine::size:0); }
bool EQ1Processor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto main=l.getMainOutputChannelSet();
    if(main!=juce::AudioChannelSet::mono()&&main!=juce::AudioChannelSet::stereo()) return false;
    if(main!=l.getMainInputChannelSet()) return false;
    if(l.inputBuses.size()>1&&!l.inputBuses[1].isDisabled()&&l.inputBuses[1]!=juce::AudioChannelSet::mono()&&l.inputBuses[1]!=juce::AudioChannelSet::stereo()) return false;
    return true;
}
void EQ1Processor::prepareToPlay(double fs,int block) {
    stereoWork.setSize(2,std::max(32,block));
    auditionWork.setSize(2,std::max(32,block));
    oversampling.initProcessing(static_cast<size_t>(std::max(32,block)));
    baseLatency=static_cast<int>(std::round(oversampling.getLatencyInSamples()));
    outputGain.reset(fs,0.02); bypassMix.reset(fs,0.01); width.reset(fs,0.02); drive.reset(fs*2,0.02);
    outputGain.setCurrentAndTargetValue(dbGain(value("output"))); bypassMix.setCurrentAndTargetValue(value("bypass")>0.5f?0.f:1.f);
    width.setCurrentAndTargetValue(value("width")*0.01f); drive.setCurrentAndTargetValue(dbGain(value("drive")));
    auditionMix.reset(fs,0.015); auditionMix.setCurrentAndTargetValue(0); currentAudition=-1;
    for(auto& f:auditionFilter) f.reset();
    std::array<Band,maxBands> config;
    for(int i=0;i<maxBands;++i) config[i]=band(i);
    engine->bands=config; engine->prepare(fs); spectral->reset(fs);
    currentMode=static_cast<int>(value("engine")); audioMode=currentMode;
    currentCharacter=static_cast<int>(value("character")); engine->causalSpectral=currentMode==0;
    setLatencySamples(requestedLatency());
    delayCursor=0; for(auto& lane:delay) lane.fill(0);
    spectrumRead=0; spectrumWrite=0; inputPeak=outputPeak=0;
}
void EQ1Processor::reset() {
    oversampling.reset(); engine->prepare(analysisSampleRate()); spectral->reset(analysisSampleRate());
    for(auto& f:auditionFilter) f.reset(); auditionMix.setCurrentAndTargetValue(0); currentAudition=-1;
    for(auto& lane:delay) lane.fill(0); delayCursor=0;
    inputPeak=outputPeak=0;
    for(auto& v:reduction) v=0;
}
float EQ1Processor::value(const juce::String& key) const {
    if(auto* p=parameters.getRawParameterValue(key)) return p->load();
    return 0;
}
void EQ1Processor::setValue(const juce::String& key,float v,bool gesture) {
    if(auto* p=parameters.getParameter(key)) {
        if(gesture) p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(v));
        if(gesture) p->endChangeGesture();
    }
}
Band EQ1Processor::band(int i) const {
    auto b=readBand(cached[static_cast<size_t>(i)]); b.slope=(b.slope+1)*12; return b;
}
int EQ1Processor::addBand(float f,float g,Shape shape) {
    for(int i=0;i<maxBands;++i) if(!band(i).enabled) {
        Band b; b.enabled=true; b.frequency=f; b.gain=g; b.shape=shape; b.detectorFrequency=f;
        auto v=bandValues(b); v[7]=0;
        for(int k=0;k<fields;++k) setValue(id(i,keys[k]),v[k]);
        return i;
    }
    return -1;
}
void EQ1Processor::clearBands() { for(int i=0;i<maxBands;++i) setValue(id(i,"on"),0); }
void EQ1Processor::copyBand(int i) { clipboard=band(i); clipboardValid=true; }
int EQ1Processor::pasteBand() {
    if(!clipboardValid) return -1;
    for(int i=0;i<maxBands;++i) if(!band(i).enabled) {
        auto v=bandValues(clipboard); v[0]=1; v[7]=clipboard.slope/12-1;
        for(int k=0;k<fields;++k) setValue(id(i,keys[k]),v[k]);
        return i;
    }
    return -1;
}
void EQ1Processor::applyPreset(int preset) {
    clearBands(); setValue("engine",0); setValue("character",0); setValue("drive",0); setValue("width",100); setValue("output",0); setValue("delta",0);
    if(preset==0) return;
    if(preset==1) { addBand(80,0,Shape::HighPass); addBand(280,-2.5f); addBand(3500,1.5f); addBand(11000,2,Shape::HighShelf); }
    if(preset==2) { int i=addBand(3200,0); setValue(id(i,"motion"),1); setValue(id(i,"threshold"),-30); setValue(id(i,"above"),-5); setValue(id(i,"attack"),5); setValue(id(i,"release"),100); }
    if(preset==3) { int i=addBand(4200,0); setValue(id(i,"motion"),2); setValue(id(i,"q"),0.7f); setValue(id(i,"threshold"),-45); setValue(id(i,"above"),-5); setValue("engine",1); }
    if(preset==4) { addBand(100,1,Shape::LowShelf); addBand(12000,1,Shape::HighShelf); }
}
void EQ1Processor::pushSpectrum(float pre,float post,float sideLeft,float sideRight) {
    const unsigned w=spectrumWrite.load(std::memory_order_relaxed),r=spectrumRead.load(std::memory_order_acquire);
    if(w-r>=16384) return;
    spectrumPre[w&16383]=pre; spectrumPost[w&16383]=post;
    spectrumSideLeft[w&16383]=sideLeft; spectrumSideRight[w&16383]=sideRight;
    spectrumWrite.store(w+1,std::memory_order_release);
}
bool EQ1Processor::pullSpectrum(float& pre,float& post,float& sideLeft,float& sideRight) {
    const unsigned r=spectrumRead.load(std::memory_order_relaxed),w=spectrumWrite.load(std::memory_order_acquire);
    if(r==w) return false;
    pre=spectrumPre[r&16383]; post=spectrumPost[r&16383]; sideLeft=spectrumSideLeft[r&16383]; sideRight=spectrumSideRight[r&16383];
    spectrumRead.store(r+1,std::memory_order_release); return true;
}
void EQ1Processor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { render(b,false); }
void EQ1Processor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { render(b,true); }
void EQ1Processor::render(juce::AudioBuffer<float>& buffer,bool hostBypass) {
    juce::ScopedNoDenormals noDenormals;
    const int n=buffer.getNumSamples(); if(n==0) return;
    if(n>stereoWork.getNumSamples()) {
        // Some hosts exceed the preparation hint. Borrow bounded sub-buffers without allocation.
        std::array<float*,4> pointers{};
        for(int offset=0;offset<n;offset+=stereoWork.getNumSamples()) {
            for(int c=0;c<buffer.getNumChannels();++c) pointers[c]=buffer.getWritePointer(c,offset);
            juce::AudioBuffer<float> segment(pointers.data(),buffer.getNumChannels(),std::min(stereoWork.getNumSamples(),n-offset));
            render(segment,hostBypass);
        }
        return;
    }
    auto main=getBusBuffer(buffer,false,0);
    std::array<Band,maxBands> config;
    for(int i=0;i<maxBands;++i) config[i]=band(i);
    const int mode=globals[2]->load()>0.5f?1:0;
    if(mode!=currentMode) { spectral->reset(analysisSampleRate()); engine->prepare(analysisSampleRate()); currentMode=mode; audioMode=mode; }
    engine->causalSpectral=mode==0; engine->setBands(config);
    const int character=static_cast<int>(globals[3]->load());
    if(character!=currentCharacter) { oversampling.reset(); currentCharacter=character; }
    const int latency=(character>0?baseLatency:0)+(mode?SpectralEngine::size:0);
    const bool scAvailable=getBusCount(true)>1&&getBus(true,1)->isEnabled();
    sidechainConnected=scAvailable;
    auto side=getBusBuffer(buffer,true,scAvailable?1:0);
    float peakIn=0,peakOut=0;
    const bool mono=main.getNumChannels()==1;
    const int listen=auditionBand.load(); const bool listening=listen>=0&&listen<maxBands&&config[listen].enabled;
    const bool detectorListen=auditionDetector.load();
    if(listening) {
        const auto& b=config[listen];
        const auto c=coefficients(Shape::BandPass,analysisSampleRate(),detectorListen&&b.freeDetector?b.detectorFrequency:b.frequency,detectorListen&&b.freeDetector?b.detectorQ:b.q,0);
        for(auto& f:auditionFilter) f.c=c;
        if(currentAudition!=listen) { for(auto& f:auditionFilter) f.reset(); currentAudition=listen; }
    }
    auditionMix.setTargetValue(listening?1.f:0.f);
    for(int i=0;i<n;++i) {
        const float l=main.getSample(0,i),r=mono?l:main.getSample(1,i);
        const float sl=scAvailable?side.getSample(0,i):0, sr=scAvailable?side.getSample(side.getNumChannels()>1?1:0,i):0;
        float a=l,z=r; engine->process(a,z,sl,sr);
        stereoWork.setSample(0,i,a); stereoWork.setSample(1,i,z);
        float al=l,ar=r;
        if(listening||auditionMix.getCurrentValue()>0) {
            const auto& b=config[std::clamp(currentAudition,0,maxBands-1)];
            if(detectorListen&&b.external) { al=sl; ar=sr; }
            if(b.placement==Placement::Mid) al=ar=(al+ar)*0.5f;
            else if(b.placement==Placement::Side) { const float s=(al-ar)*0.5f; al=s; ar=-s; }
            else if(b.placement==Placement::Left) ar=al;
            else if(b.placement==Placement::Right) al=ar;
            al=auditionFilter[0].process(al); ar=auditionFilter[1].process(ar);
        }
        auditionWork.setSample(0,i,al); auditionWork.setSample(1,i,ar);
        peakIn=std::max({peakIn,std::abs(l),std::abs(r)});
    }
    auto block=juce::dsp::AudioBlock<float>(stereoWork).getSubBlock(0,static_cast<size_t>(n));
    drive.setTargetValue(dbGain(globals[4]->load()));
    if(character>0) {
      auto up=oversampling.processSamplesUp(block);
      for(size_t i=0;i<up.getNumSamples();++i) {
        float& l=up.getChannelPointer(0)[i]; float& r=up.getChannelPointer(1)[i];
        const float d=drive.getNextValue();
        if(character>0) {
            const float amount=(character==1?0.25f:0.65f)*d;
            const float normalization=1/std::max(0.001f,amount);
            auto colour=[amount,normalization](float x) { return std::tanh(x*amount)*normalization; };
            l=colour(l); r=colour(r);
        }
      }
      oversampling.processSamplesDown(block);
    }
    outputGain.setTargetValue(dbGain(globals[0]->load())); width.setTargetValue(globals[5]->load()*0.01f);
    bypassMix.setTargetValue(hostBypass||globals[1]->load()>0.5f?0.f:1.f);
    const bool delta=globals[6]->load()>0.5f;
    for(int i=0;i<n;++i) {
        const float preL=main.getSample(0,i),preR=mono?preL:main.getSample(1,i);
        delay[0][delayCursor]=preL; delay[1][delayCursor]=preR;
        const int read=(delayCursor-latency+8192)%8192;
        const float dryL=delay[0][read],dryR=delay[1][read]; delayCursor=(delayCursor+1)%8192;
        float l=stereoWork.getSample(0,i),r=stereoWork.getSample(1,i);
        if(mode) spectral->process(l,r,scAvailable?side.getSample(0,i):0,scAvailable?side.getSample(side.getNumChannels()>1?1:0,i):0,config);
        const float w=width.getNextValue(),m=(l+r)*0.5f,s=(l-r)*0.5f*w,g=outputGain.getNextValue(),mix=bypassMix.getNextValue();
        l=(m+s)*g; r=(m-s)*g;
        if(delta) { l=dryL-l; r=dryR-r; }
        const float listenMix=auditionMix.getNextValue();
        l+=(auditionWork.getSample(0,i)-l)*listenMix; r+=(auditionWork.getSample(1,i)-r)*listenMix;
        l=dryL+(l-dryL)*mix; r=dryR+(r-dryR)*mix;
        main.setSample(0,i,l); if(!mono) main.setSample(1,i,r);
        peakOut=std::max({peakOut,std::abs(l),std::abs(r)});
        pushSpectrum((preL+preR)*0.5f,(l+r)*0.5f,scAvailable?side.getSample(0,i):0,scAvailable?side.getSample(side.getNumChannels()>1?1:0,i):0);
    }
    for(int i=0;i<maxBands;++i) {
        float gain=engine->runtime[i].currentGain;
        if(!mode&&config[i].motion==Motion::Spectral) gain=engine->resonance[i].reduction;
        if(mode&&config[i].enabled&&config[i].motion==Motion::Spectral) {
            gain=0; for(const auto& lane:spectral->gain[i]) for(float db:lane) if(std::abs(db)>std::abs(gain)) gain=db;
        }
        reduction[i].store(gain,std::memory_order_relaxed);
    }
    inputPeak.store(std::max(peakIn,inputPeak.load()*0.92f)); outputPeak.store(std::max(peakOut,outputPeak.load()*0.92f));
}
void EQ1Processor::getStateInformation(juce::MemoryBlock& dest) {
    auto state=parameters.copyState(); state.setProperty("schema",2,nullptr);
    if(auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void EQ1Processor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) {
        auto state=juce::ValueTree::fromXml(*xml); if(state.isValid()) {
            for(int i=0;i<maxBands;++i) if(!state.getChildWithProperty("id",id(i,"density")).isValid()) {
                juce::ValueTree p("PARAM"); p.setProperty("id",id(i,"density"),nullptr); p.setProperty("value",50.f,nullptr); state.appendChild(p,nullptr);
            }
            parameters.replaceState(state);
        }
    }
}
void EQ1Processor::updateTrackProperties(const TrackProperties& p) {
    const juce::ScopedLock lock(registryLock); trackName=p.name.isEmpty()?"Untitled":p.name;
}
std::vector<EQ1Processor::Peer> EQ1Processor::peers() const {
    const juce::ScopedLock lock(registryLock); std::vector<Peer> peers;
    for(auto* p:registry) { int count=0; for(int i=0;i<maxBands;++i) count+=p->band(i).enabled?1:0; peers.push_back({p->instanceId,p->trackName,count}); }
    return peers;
}
bool EQ1Processor::copyFromPeer(uint64_t other) {
    const juce::ScopedLock lock(registryLock);
    for(auto* p:registry) if(p->instanceId==other&&p!=this) {
        for(int i=0;i<maxBands;++i) for(int k=0;k<fields;++k) setValue(id(i,keys[k]),p->cached[i][k]->load());
        return true;
    }
    return false;
}
juce::AudioProcessorEditor* EQ1Processor::createEditor() { return new EQ1Editor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EQ1Processor(); }
