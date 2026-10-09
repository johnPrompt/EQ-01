#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>
#include <cmath>
void expect(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
juce::AudioProcessorParameter* parameter(juce::AudioPluginInstance& p,const juce::String& name) {
    for(auto* value:p.getParameters()) if(value->getName(100)==name) return value;
    throw std::runtime_error("missing parameter: "+name.toStdString());
}
void set(juce::AudioPluginInstance& p,const juce::String& name,float value) { auto* v=parameter(p,name); v->beginChangeGesture(); v->setValueNotifyingHost(value); v->endChangeGesture(); }
int main(int argc,char** argv) {
 try {
    std::cerr<<"host: initialize\n";
    juce::ScopedJuceInitialiser_GUI gui; expect(argc>=2,"usage: host_check bundle");
    juce::AudioPluginFormatManager formats; formats.addDefaultFormats();
    juce::KnownPluginList list; juce::OwnedArray<juce::PluginDescription> descriptions;
    for(auto* format:formats.getFormats()) list.scanAndAddFile(juce::String::fromUTF8(argv[1]),true,descriptions,*format);
    std::cerr<<"host: scan\n";
    expect(descriptions.size()==1,"VST3 scan failed"); expect(descriptions[0]->name=="EQ-01","wrong product name");
    juce::String error; auto plugin=formats.createPluginInstance(*descriptions[0],48000,256,error); expect(plugin!=nullptr,error.toRawUTF8());
    plugin->setPlayConfigDetails(2,2,48000,256); plugin->prepareToPlay(48000,256);
    const int liveLatency=plugin->getLatencySamples(); expect(liveLatency==0,"clean Live must report zero delay");
    juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer midi;
    double energy=0;
    for(int block=0;block<100;++block) {
        for(int i=0;i<256;++i) { float sample=0.2f*std::sin(static_cast<float>(2*3.141592653589793*1000*(block*256+i)/48000)); audio.setSample(0,i,sample); audio.setSample(1,i,sample*0.7f); }
        plugin->processBlock(audio,midi);
        for(int i=0;i<256;++i) { const float l=audio.getSample(0,i),r=audio.getSample(1,i); expect(std::isfinite(l)&&std::isfinite(r),"invalid output"); expect(std::abs(r-l*0.7f)<1e-5f,"stereo image changed in flat mode"); if(block>30) energy+=l*l; }
    }
    expect(energy>10,"plugin silent");
    set(*plugin,"Band 1 Enabled",1); set(*plugin,"Band 1 Gain",0.625f); // +6 dB in [-24,+24].
    juce::MemoryBlock state; plugin->getStateInformation(state); expect(state.getSize()>100,"state empty");
    set(*plugin,"Band 1 Gain",0.5f); plugin->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    expect(std::abs(parameter(*plugin,"Band 1 Gain")->getValue()-0.625f)<0.001f,"host state restore failed");
    // Measure the binary's actual EQ gain, not just its parameter value.
    double boosted=0,reference=0;
    for(int block=0;block<160;++block) {
        for(int i=0;i<256;++i) { float sample=0.05f*std::sin(static_cast<float>(2*3.141592653589793*1000*(block*256+i)/48000)); audio.setSample(0,i,sample); audio.setSample(1,i,sample); }
        plugin->processBlock(audio,midi);
        if(block>80) for(int i=0;i<256;++i) { boosted+=audio.getSample(0,i)*audio.getSample(0,i); reference+=0.05*0.05*0.5; }
    }
    expect(std::abs(10*std::log10(boosted/reference)-6)<0.15,"VST3 EQ does not deliver +6 dB");
    std::cerr<<"host: measured gain\n";
    auto buses=plugin->getBusesLayout(); expect(buses.inputBuses.size()==2,"sidechain bus missing");
    buses.inputBuses.getReference(1)=juce::AudioChannelSet::stereo();
    plugin->releaseResources(); expect(plugin->setBusesLayout(buses),"sidechain bus cannot be enabled"); plugin->prepareToPlay(48000,256);
    std::cerr<<"host: sidechain prepared\n";
    set(*plugin,"Band 1 Gain",0.5f); set(*plugin,"Band 1 Motion",0.5f); set(*plugin,"Band 1 External sidechain",1); set(*plugin,"Band 1 Threshold",2.f/3.f);
    juce::AudioBuffer<float> sideAudio(4,256); double sideEnergy=0;
    for(int block=0;block<200;++block) {
        for(int i=0;i<256;++i) { float s=std::sin(static_cast<float>(2*3.141592653589793*1000*(block*256+i)/48000)); sideAudio.setSample(0,i,s*0.01f); sideAudio.setSample(1,i,s*0.01f); sideAudio.setSample(2,i,s*0.4f); sideAudio.setSample(3,i,s*0.4f); }
        plugin->processBlock(sideAudio,midi);
        if(block>120) for(int i=0;i<256;++i) sideEnergy+=sideAudio.getSample(0,i)*sideAudio.getSample(0,i);
    }
    std::cerr<<"host: sidechain audio\n";
    expect(10*std::log10(sideEnergy/(79*256*0.01*0.01*0.5))<-5.7,"VST3 sidechain failed to drive attenuation");
    plugin->releaseResources(); buses.inputBuses.getReference(1)=juce::AudioChannelSet::disabled(); expect(plugin->setBusesLayout(buses),"cannot disable sidechain");
    set(*plugin,"Band 1 Enabled",0); set(*plugin,"Band 1 External sidechain",0);
    set(*plugin,"Engine",1); juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    plugin->releaseResources(); plugin->prepareToPlay(48000,256);
    expect(plugin->getLatencySamples()==liveLatency+2048,"spectral latency wrong");
    std::cerr<<"host: spectral prepared\n";
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor()); expect(editor!=nullptr&&editor->getWidth()==1080,"native editor failed"); editor.reset();
    std::cerr<<"host: editor ready\n";
    audio.clear(); plugin->processBlockBypassed(audio,midi);
    std::array<float,4096> impulse{};
    plugin->reset();
    for(int block=0;block<16;++block) {
        audio.clear(); if(block==0) { audio.setSample(0,0,1); audio.setSample(1,0,1); }
        plugin->processBlock(audio,midi); for(int i=0;i<256;++i) impulse[block*256+i]=audio.getSample(0,i);
    }
    int peak=0; for(int i=1;i<4096;++i) if(std::abs(impulse[i])>std::abs(impulse[peak])) peak=i;
    expect(peak==liveLatency+2048,"reported spectral latency differs from impulse peak");
    // Measure the unbuffered mode in the binary at different rates and host block sizes.
    set(*plugin,"Engine",0); juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    for(double fs:{44100.,48000.,96000.,192000.}) for(int blockSize:{32,128,512}) {
        plugin->releaseResources(); plugin->setPlayConfigDetails(2,2,fs,blockSize); plugin->prepareToPlay(fs,blockSize);
        expect(plugin->getLatencySamples()==0,"clean latency changes with sample rate/block size");
        juce::AudioBuffer<float> probe(2,blockSize); probe.clear(); probe.setSample(0,0,1); probe.setSample(1,0,1); plugin->processBlock(probe,midi);
        expect(std::abs(probe.getSample(0,0)-1)<1e-6,"Live impulse is delayed");
        for(int i=1;i<blockSize;++i) expect(std::abs(probe.getSample(0,i))<1e-6,"flat Live impulse is smeared");
    }
    // Adaptive spectral bands remain active in clean Live and keep zero buffer delay.
    plugin->releaseResources(); plugin->setPlayConfigDetails(2,2,48000,256); plugin->prepareToPlay(48000,256);
    set(*plugin,"Band 1 Enabled",1); set(*plugin,"Band 1 Motion",1); set(*plugin,"Band 1 Threshold",5.f/9.f); // -40 dB.
    set(*plugin,"Band 1 Above range",0.25f); // -12 dB.
    set(*plugin,"Band 1 Resonance density",1);
    double adaptive=0;
    for(int block=0;block<200;++block) {
        for(int i=0;i<256;++i) { float x=0.1f*std::sin(static_cast<float>(2*3.141592653589793*1000*(block*256+i)/48000)); audio.setSample(0,i,x); audio.setSample(1,i,x); }
        plugin->processBlock(audio,midi); if(block>120) for(int i=0;i<256;++i) adaptive+=audio.getSample(0,i)*audio.getSample(0,i);
    }
    expect(plugin->getLatencySamples()==0&&10*std::log10(adaptive/(79*256*0.1*0.1*0.5))<-4,"live causal spectral band not working in VST3");
    set(*plugin,"Band 1 Enabled",0); set(*plugin,"Band 1 Motion",0); set(*plugin,"Character",0.5f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50); plugin->releaseResources(); plugin->prepareToPlay(48000,256);
    const int colourLatency=plugin->getLatencySamples(); expect(colourLatency>0&&colourLatency<16,"colour latency too high");
    std::array<float,256> colourImpulse{}; audio.clear(); audio.setSample(0,0,0.01f); audio.setSample(1,0,0.01f); plugin->processBlock(audio,midi);
    int colourPeak=0; for(int i=1;i<256;++i) if(std::abs(audio.getSample(0,i))>std::abs(audio.getSample(0,colourPeak))) colourPeak=i;
    std::cout<<"MEASURE: colour reports "<<colourLatency<<" samples; impulse peak "<<colourPeak<<" samples (IIR phase is frequency dependent)\n";
    set(*plugin,"Character",0); juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    juce::AudioBuffer<float> empty(2,0); plugin->processBlock(empty,midi);
    plugin->releaseResources();
    // Hosts with mono tracks must also load the same binary.
    plugin->setPlayConfigDetails(1,1,44100,128); plugin->prepareToPlay(44100,128);
    juce::AudioBuffer<float> mono(1,128); mono.clear(); plugin->processBlock(mono,midi); plugin->releaseResources();
    std::cout<<"PASS: real VST3 scan/load, mono/stereo, +6 dB EQ, external SC, automation/state, native editor, bypass, empty blocks, exact impulse latency "<<liveLatency<<" Live / "<<liveLatency+2048<<" Spectral, zero Live latency at 44.1/48/96/192 kHz and 32/128/512 samples, active causal spectral band, colour latency "<<colourLatency<<" samples\n";
    return 0;
 } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
