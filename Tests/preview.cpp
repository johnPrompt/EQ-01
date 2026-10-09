#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
void expect(bool ok,const char* msg) { if(!ok) throw std::runtime_error(msg); }
juce::Component* find(juce::Component& c,const juce::String& id) {
    if(c.getComponentID()==id) return &c;
    for(auto* child:c.getChildren()) if(auto* match=find(*child,id)) return match;
    return nullptr;
}
void save(EQ1Editor& editor,const juce::File& file) {
    auto image=editor.createComponentSnapshot(editor.getLocalBounds(),true,1.f);
    juce::MemoryOutputStream out;
    juce::PNGImageFormat png; expect(png.writeImageToStream(image,out),"cannot render screenshot");
    expect(file.replaceWithData(out.getData(),out.getDataSize()),"cannot save screenshot");
}
void saveMenu(juce::TextButton& button,const juce::File& file) {
    button.onClick(); juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
    bool saved=false;
    if(auto* c=juce::Component::getCurrentlyModalComponent(); c&&c->isVisible()&&c->getWidth()>=100&&c->getHeight()>=40) {
        auto image=c->createComponentSnapshot(c->getLocalBounds(),true,1.f); juce::MemoryOutputStream out; juce::PNGImageFormat png;
        expect(png.writeImageToStream(image,out)&&file.replaceWithData(out.getData(),out.getDataSize()),"cannot render popup menu"); saved=true;
    }
    juce::PopupMenu::dismissAllActiveMenus(); juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    if(!saved) throw std::runtime_error("actual popup menu is not visible: "+button.getComponentID().toStdString());
}
juce::Label& numericLabel(juce::Slider& s) {
    for(auto* child:s.getChildren()) if(auto* value=dynamic_cast<juce::Label*>(child)) return *value;
    throw std::runtime_error("numeric label missing");
}
juce::MouseEvent pointerEvent(juce::Component& c,juce::Point<float> p,juce::Point<float> origin,int flags,bool drag,int clicks=1) {
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(flags),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),origin,juce::Time::getCurrentTime(),clicks,drag);
}
void scrub(juce::Slider& s,juce::Point<float> displacement,bool fine=false,std::function<void()> during={}) {
    auto& value=numericLabel(s); const juce::Point<float> start{12,12};
    const int flags=juce::ModifierKeys::leftButtonModifier|(fine?juce::ModifierKeys::shiftModifier:0);
    value.mouseDown(pointerEvent(value,start,start,flags,false));
    value.mouseDrag(pointerEvent(value,start+displacement*0.5f,start,flags,true));
    if(during) during();
    value.mouseDrag(pointerEvent(value,start+displacement,start,flags,true));
    static_cast<juce::Component&>(value).mouseUp(pointerEvent(value,start+displacement,start,0,true));
}
struct GestureProbe final : juce::AudioProcessorParameter::Listener {
    int starts=0,ends=0,values=0; bool active=false,grouped=true;
    void parameterValueChanged(int,float) override {++values; grouped=grouped&&active;}
    void parameterGestureChanged(int,bool beginning) override {active=beginning; if(beginning) ++starts; else ++ends;}
};
int main(int argc,char** argv) {
 try {
    juce::ScopedJuceInitialiser_GUI gui;
    expect(argc>=2,"usage: preview output_directory");
    const auto root=juce::File(juce::String::fromUTF8(argv[1])); root.createDirectory();
    auto processor=std::make_unique<EQ1Processor>(); processor->prepareToPlay(48000,256); processor->applyPreset(1);
    processor->setValue(EQ1Processor::id(2,"threshold"),-30);
    processor->setValue(EQ1Processor::id(2,"above"),-4); processor->setValue(EQ1Processor::id(2,"relative"),25);
    auto editor=std::make_unique<EQ1Editor>(*processor); editor->selectBand(2);
    juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer midi;
    uint32_t seed=12345;
    for(int block=0;block<200;++block) {
        for(int i=0;i<256;++i) { seed=1664525*seed+1013904223; float noise=(static_cast<float>(seed)/4294967296.f-0.5f)*0.05f;
            float sample=noise; for(float f:{137.f,274.f,548.f,1096.f,3500.f}) sample+=0.06f*std::sin(static_cast<float>(2*curveshaper::pi*f*(block*256+i)/48000));
            audio.setSample(0,i,sample); audio.setSample(1,i,sample*0.85f);
        }
        processor->processBlock(audio,midi); editor->graph.analyze();
    }
    const auto selectedPoint=juce::Point<float>(editor->graph.frequencyX(processor->band(2).frequency),editor->graph.gainY(processor->band(2).gain));
    GestureProbe selectionProbe; auto* selectedFrequency=processor->parameters.getParameter(EQ1Processor::id(2,"frequency")); selectedFrequency->addListener(&selectionProbe);
    editor->graph.mouseDown(pointerEvent(editor->graph,selectedPoint,selectedPoint,juce::ModifierKeys::leftButtonModifier,false));
    editor->graph.mouseUp(pointerEvent(editor->graph,selectedPoint,selectedPoint,0,false)); selectedFrequency->removeListener(&selectionProbe);
    expect(editor->selectedBand()==2&&editor->contextBounds().isEmpty(),"single click automatically opens band parameters");
    expect(selectionProbe.starts==0&&selectionProbe.ends==0&&!find(*editor,"undoButton")->isEnabled(),"selection creates a host gesture or undo action");
    editor->refresh(); save(*editor,root.getChildFile("Preview-selected.png"));
    editor->setRussian(true); save(*editor,root.getChildFile("Preview-selected-RU.png")); editor->setRussian(false);
    editor->graph.mouseDoubleClick(pointerEvent(editor->graph,selectedPoint,selectedPoint,juce::ModifierKeys::leftButtonModifier,false,2));
    expect(!editor->contextBounds().isEmpty(),"double-click on existing band does not open parameters");
    save(*editor,root.getChildFile("Preview.png"));
    editor->setRussian(true); save(*editor,root.getChildFile("Preview-RU.png")); editor->setRussian(false);
    editor->setRussian(true);
    saveMenu(*dynamic_cast<juce::TextButton*>(find(*editor,"bandShape")),root.getChildFile("Preview-shapes.png"));
    saveMenu(*dynamic_cast<juce::TextButton*>(find(*editor,"settings")),root.getChildFile("Preview-menu.png")); editor->setRussian(false);
    processor->setValue(EQ1Processor::id(2,"motion"),1); editor->refresh(); save(*editor,root.getChildFile("Preview-dynamic.png"));
    expect(find(*editor,"band1")==nullptr,"empty band slot strip should be removed");
    auto* frequency=dynamic_cast<juce::Slider*>(find(*editor,"frequency")); expect(frequency,"frequency field missing");
    auto* panel=find(*editor,"bandPanel"); expect(panel&&panel->isVisible(),"selected band context missing");
    expect(editor->graph.getHeight()>editor->getHeight()*0.75f,"graph is not the main workspace");
    auto* presets=dynamic_cast<juce::ComboBox*>(find(*editor,"presets")); expect(presets&&presets->getNumItems()==7,"presets should be a real dropdown");
    auto* language=find(*editor,"language"); expect(language&&language->getParentComponent()==editor.get()&&language->isVisible(),"language toggle must stay in header");
    for(const char* key:{"frequency","gain","q","output","threshold","above","below","attack","release","ratio","knee","relative","detectorFrequency","detectorQ","drive","width","density"}) {
        auto* s=dynamic_cast<juce::Slider*>(find(*editor,key)); expect(s&&s->getSliderStyle()!=juce::Slider::RotaryHorizontalVerticalDrag,"rotary control survived redesign");
    }
    auto& text=numericLabel(*frequency); static_cast<juce::Component&>(text).mouseDoubleClick(pointerEvent(text,{12,12},{12,12},0,false,2));
    expect(text.getCurrentTextEditor()!=nullptr,"double-click does not open numeric entry");
    // This offscreen editor has no OS keyboard focus. Do not queue JUCE's
    // focus-loss auto-commit; exercise real refresh with pending text instead.
    text.getCurrentTextEditor()->setText("4,1k",false); editor->refresh(); juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
    expect(text.getCurrentTextEditor()&&text.getCurrentTextEditor()->getText()=="4,1k","refresh interrupts numeric text entry");
    frequency->hideTextBox(false);
    expect(std::abs(processor->band(2).frequency-4100)<1,"typed comma/kHz entry not connected");
    const auto anchored=panel->getBounds();
    scrub(*frequency,{20,0},false,[&]{editor->refresh(); expect(panel->getBounds()==anchored,"numeric frequency drag moves panel under pointer");});
    expect(processor->band(2).frequency>4100,"frequency scrub failed");
    editor->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));
    expect(std::abs(processor->band(2).frequency-4100)<1,"scrub undo needs more than one action");
    for(const char* key:{"processingMode","colourMode","output","drawCurve","freezeSpectrum","listenDifference"}) {
        auto* control=find(*editor,key); expect(control&&control->isVisible()&&control->getParentComponent()==editor.get(),"frequent global action not in bottom bar");
    }
    editor->checkpoint(); editor->changeBand("placement",4,false); expect(processor->band(2).placement==curveshaper::Placement::Side,"placement not connected");
    auto before=processor->band(2); processor->copyBand(2); int copy=processor->pasteBand(); expect(copy>=0&&processor->band(copy).frequency==before.frequency,"band clipboard failed");
    auto second=std::make_unique<EQ1Processor>(); auto peers=second->peers(); expect(peers.size()>=2,"instance registry not connected");
    expect(second->copyFromPeer(peers[0].id),"cross-instance band copy failed"); expect(second->band(2).frequency==processor->band(2).frequency,"wrong peer state");
    juce::MemoryBlock state; processor->getStateInformation(state); processor->clearBands(); processor->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    expect(processor->band(2).enabled&&std::abs(processor->band(2).frequency-4100)<1,"project state does not restore");
    auto legacy=processor->parameters.copyState(); legacy.setProperty("schema",1,nullptr);
    for(int i=legacy.getNumChildren()-1;i>=0;--i) if(legacy.getChild(i).getProperty("id").toString().endsWith(".density")) legacy.removeChild(i,nullptr);
    juce::MemoryBlock oldState; auto oldXml=legacy.createXml(); juce::AudioProcessor::copyXmlToBinary(*oldXml,oldState);
    processor->setValue(EQ1Processor::id(2,"density"),17);
    processor->setStateInformation(oldState.getData(),static_cast<int>(oldState.getSize()));
    expect(processor->value(EQ1Processor::id(2,"density"))==50&&std::abs(processor->band(2).frequency-4100)<1,"legacy schema 1 does not migrate missing density");
    auto* details=dynamic_cast<juce::TextButton*>(find(*editor,"bandDetails")); expect(details,"context details action missing");
    details->onClick(); editor->refresh(); save(*editor,root.getChildFile("Preview-details.png"));
    expect(panel->getBottom()<=editor->graph.getBottom(),"expanded panel outside graph");
    auto* detectorPage=dynamic_cast<juce::TextButton*>(find(*editor,"detectorSettings")); expect(detectorPage,"detector settings missing");
    editor->changeBand("freeDetector",1,false); detectorPage->onClick(); editor->refresh();
    expect(find(*editor,"detectorFrequency")->isVisible()&&!find(*editor,"knee")->isVisible(),"detector page does not expose its own controls");
    save(*editor,root.getChildFile("Preview-detector.png")); details->onClick(); editor->refresh();
    expect(find(*editor,"knee")->isVisible()&&find(*editor,"below")->isVisible(),"independent detector prevents editing dynamics");
    detectorPage->onClick(); editor->changeBand("external",1,false);
    auto* range=dynamic_cast<juce::Slider*>(find(*editor,"above")); auto* threshold=dynamic_cast<juce::Slider*>(find(*editor,"threshold"));
    const auto rangeBounds=range->getBounds(),thresholdBounds=threshold->getBounds(); const float initialRange=processor->band(2).above;
    GestureProbe probe; auto* rangeParameter=processor->parameters.getParameter(EQ1Processor::id(2,"above")); rangeParameter->addListener(&probe);
    scrub(*range,{36,0}); rangeParameter->removeListener(&probe);
    expect(processor->band(2).above>initialRange&&probe.starts==1&&probe.ends==1&&probe.values>=2&&probe.grouped,"external SC range scrub not grouped into one host gesture");
    const float initialThreshold=processor->band(2).threshold; scrub(*threshold,{0,-24});
    const float coarseChange=processor->band(2).threshold-initialThreshold; expect(coarseChange>1,"external SC threshold vertical scrub failed");
    editor->changeBand("threshold",initialThreshold,false); scrub(*threshold,{0,-24},true);
    expect(processor->band(2).threshold-initialThreshold<coarseChange*0.2f,"Shift does not provide fine scrub");
    details->onClick(); expect(range->isVisible()&&threshold->isVisible()&&range->getBounds()==rangeBounds&&threshold->getBounds()==thresholdBounds,"section switching relocates primary dynamics controls");
    expect(find(*editor,"knee")->isVisible(),"timing section unreachable from detector section");
    detectorPage->onClick();
    editor->setRussian(true); save(*editor,root.getChildFile("Preview-sidechain-RU.png")); editor->setRussian(false);
    saveMenu(*dynamic_cast<juce::TextButton*>(find(*editor,"detectorSource")),root.getChildFile("Preview-source.png"));
    editor->changeBand("external",0,false);
    {
        auto scProcessor=std::make_unique<EQ1Processor>(); auto layout=scProcessor->getBusesLayout();
        layout.inputBuses.set(1,juce::AudioChannelSet::stereo()); expect(scProcessor->setBusesLayout(layout),"native SC layout failed");
        scProcessor->prepareToPlay(48000,256); const int scBand=scProcessor->addBand(1200,0);
        scProcessor->setValue(EQ1Processor::id(scBand,"motion"),1); scProcessor->setValue(EQ1Processor::id(scBand,"external"),1);
        auto scEditor=std::make_unique<EQ1Editor>(*scProcessor); scEditor->openBandPanel(scBand);
        dynamic_cast<juce::TextButton*>(find(*scEditor,"detectorSettings"))->onClick();
        juce::AudioBuffer<float> routed(4,256);
        for(int block=0;block<120;++block) {
            for(int i=0;i<256;++i) {
                const int t=block*256+i; const float main=0.06f*std::sin(static_cast<float>(2*curveshaper::pi*220*t/48000));
                const float sc=0.12f*std::sin(static_cast<float>(2*curveshaper::pi*1200*t/48000));
                routed.setSample(0,i,main); routed.setSample(1,i,main); routed.setSample(2,i,sc); routed.setSample(3,i,-sc);
            }
            scProcessor->processBlock(routed,midi); scEditor->graph.analyze();
        }
        scEditor->refresh();
        expect(scEditor->graph.sidechainView&&scEditor->graph.sidechainLevelAt(1200)>-30&&scEditor->graph.sidechainLevelAt(9000)<-65,"SC spectrum is missing, cancels opposite polarity, or shows wrong signal");
        expect(scProcessor->getLatencySamples()==0,"SC visualization changes audio latency");
        auto* scRange=dynamic_cast<juce::Slider*>(find(*scEditor,"above")); auto* scThreshold=dynamic_cast<juce::Slider*>(find(*scEditor,"threshold"));
        const float oldRange=scProcessor->band(scBand).above,oldThreshold=scProcessor->band(scBand).threshold;
        scrub(*scRange,{12,0}); scrub(*scThreshold,{0,-12});
        expect(scProcessor->band(scBand).above>oldRange&&scProcessor->band(scBand).threshold>oldThreshold,"connected SC prevents numeric adjustment");
        scEditor->setRussian(true); save(*scEditor,root.getChildFile("Preview-sidechain-spectrum.png"));
        dynamic_cast<juce::TextButton*>(find(*scEditor,"bandDetails"))->onClick();
        expect(!scEditor->graph.sidechainView,"SC spectrum clutters normal timing view");
    }
    auto* solo=dynamic_cast<juce::TextButton*>(find(*editor,"bandSolo")); solo->onClick(); expect(processor->auditionBand.load()==2,"band listen not connected"); solo->onClick(); expect(processor->auditionBand.load()==-1,"listen does not exit");
    editor->showSoundSettings(); editor->refresh();
    expect(!panel->isVisible()&&find(*editor,"settingsPanel")->isVisible(),"global settings should replace context"); save(*editor,root.getChildFile("Preview-settings.png"));
    editor->showSoundSettings(); editor->deselectBand(); expect(!panel->isVisible(),"context remains after deselection");
    save(*editor,root.getChildFile("Preview-graph.png"));
    editor->setSize(820,560); editor->openBandPanel(2); details->onClick(); editor->refresh(); save(*editor,root.getChildFile("Preview-small.png"));
    for(auto* child:editor->getChildren()) if(child->isVisible()) expect(child->getBounds().getBottom()<=editor->getHeight()&&child->getBounds().getRight()<=editor->getWidth(),"visible control extends outside editor");
    if(panel->getHeight()<300) details->onClick();
    const auto fixedPanel=panel->getBounds();
    for(float f:{30.f,500.f,2000.f,18000.f}) for(float db:{-18.f,0.f,18.f}) {
        editor->changeBand("frequency",f,false); editor->changeBand("gain",db,false);
        expect(panel->getBounds()==fixedPanel,"parameter changes make the panel follow its band");
    }
    // The audition is an actual audio path, not just a button state.
    processor->clearBands(); const int listenBand=processor->addBand(1000,0); processor->auditionBand=listenBand; processor->prepareToPlay(48000,256);
    double ls=0,lc=0,hs=0,hc=0;
    for(int block=0;block<160;++block) {
        for(int i=0;i<256;++i) { const int t=block*256+i; float x=0.1f*std::sin(static_cast<float>(2*curveshaper::pi*1000*t/48000))+0.1f*std::sin(static_cast<float>(2*curveshaper::pi*9000*t/48000)); audio.setSample(0,i,x); audio.setSample(1,i,x); }
        processor->processBlock(audio,midi);
        if(block>=80) for(int i=0;i<256;++i) { const int t=block*256+i; const float x=audio.getSample(0,i); ls+=x*std::sin(2*curveshaper::pi*1000*t/48000); lc+=x*std::cos(2*curveshaper::pi*1000*t/48000); hs+=x*std::sin(2*curveshaper::pi*9000*t/48000); hc+=x*std::cos(2*curveshaper::pi*9000*t/48000); }
    }
    expect(std::hypot(ls,lc)>std::hypot(hs,hc)*5,"audition does not isolate the band");
    processor->setValue(EQ1Processor::id(listenBand,"external"),1); processor->auditionDetector=true;
    double quiet=0;
    for(int block=0;block<50;++block) { audio.clear(); for(int i=0;i<256;++i) { audio.setSample(0,i,0.1f); audio.setSample(1,i,0.1f); } processor->processBlock(audio,midi); if(block>25) quiet+=audio.getMagnitude(0,0,256); }
    expect(quiet<1e-5,"detector audition ignores missing external sidechain"); processor->auditionBand=-1; processor->auditionDetector=false;
    // Exercise actual drawing handlers and their resulting filter parameters.
    processor->clearBands(); editor->graph.sketch=true;
    auto event=[&](float f,float db,bool dragged) {
        auto pt=juce::Point<float>(editor->graph.frequencyX(f),editor->graph.gainY(db));
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),pt,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&editor->graph,&editor->graph,juce::Time::getCurrentTime(),pt,juce::Time::getCurrentTime(),1,dragged);
    };
    editor->graph.sketch=false; editor->deselectBand();
    for(auto [f,type]:{std::pair<float,curveshaper::Shape>{30,curveshaper::Shape::HighPass},{80,curveshaper::Shape::LowShelf},{1000,curveshaper::Shape::Bell},{8000,curveshaper::Shape::HighShelf},{18000,curveshaper::Shape::LowPass}}) {
        processor->clearBands(); editor->deselectBand();
        editor->graph.mouseMove(event(f,4,false));
        if(f==18000) save(*editor,root.getChildFile("Preview-add.png"));
        editor->graph.mouseDown(event(f,4,false)); editor->graph.mouseUp(event(f,4,false));
        const int created=editor->selectedBand(); expect(created>=0&&processor->band(created).shape==type,"click chooses wrong filter form");
        expect(!panel->isVisible(),"creating a band automatically opens parameters");
        editor->graph.mouseDoubleClick(event(f,4,false));
        int bands=0; for(int i=0;i<24;++i) bands+=processor->band(i).enabled?1:0;
        expect(bands==1,"double-click after click duplicated a new band");
        if(type==curveshaper::Shape::LowPass) {
            expect(!find(*editor,"gain")->isVisible()&&find(*editor,"cutSlope")->isVisible(),"cut exposes irrelevant gain or hides slope");
            juce::MouseWheelDetails wheel; wheel.deltaY=0.1f;
            editor->graph.mouseWheelMove(event(f,0,false),wheel);
            expect(processor->band(created).slope==24&&!find(*editor,"q")->isVisible(),"cut wheel fails slope or exposes inactive Q");
            save(*editor,root.getChildFile("Preview-cut.png"));
        }
        editor->graph.mouseDown(event(1000,-8,false)); expect(editor->selectedBand()<0,"click background should deselect existing band");
    }
    processor->clearBands(); editor->deselectBand();
    const int existingCurve=processor->addBand(1000,6); const auto existingFilter=processor->band(existingCurve);
    const float curveGain=static_cast<float>(20*std::log10(curveshaper::magnitude(existingFilter,2500,48000)));
    editor->graph.mouseDown(event(2500,curveGain,false)); editor->graph.mouseUp(event(2500,curveGain,false));
    expect(editor->selectedBand()!=existingCurve&&std::abs(processor->band(editor->selectedBand()).gain)<0.001f,"adding on curve unexpectedly changes audio");
    editor->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));
    int restoredCount=0; for(int i=0;i<24;++i) restoredCount+=processor->band(i).enabled?1:0;
    expect(restoredCount==1,"creating a band requires multiple undo actions");
    processor->clearBands(); editor->deselectBand(); editor->graph.mouseExit(event(1000,0,false)); editor->graph.sketch=true;
    editor->graph.mouseDown(event(500,0,false));
    for(int i=0;i<=60;++i) editor->graph.mouseDrag(event(500*std::pow(6.f,i/60.f),6*std::sin(static_cast<float>(curveshaper::pi*i/60)),true));
    editor->graph.mouseUp(event(3000,0,true));
    int fitted=0; for(int i=0;i<24;++i) fitted+=processor->band(i).enabled?1:0;
    expect(fitted>0&&processor->band(0).gain>5,"EQ Sketch did not fit the drawn peak");
    expect(editor->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0)),"undo shortcut ignored");
    expect(!processor->band(0).enabled,"sketch undo did not restore previous state");
    processor->setValue("engine",1); processor->prepareToPlay(48000,128);
    juce::AudioBuffer<float> huge(2,4096); huge.clear(); huge.setSample(0,0,1); huge.setSample(1,0,1);
    processor->processBlock(huge,midi); int peak=0;
    for(int i=1;i<4096;++i) if(std::abs(huge.getSample(0,i))>std::abs(huge.getSample(0,peak))) peak=i;
    expect(peak==processor->getLatencySamples(),"bounded oversized native processing or latency failed");
    editor->graph.sketch=false; editor->deselectBand(); save(*editor,root.getChildFile("Preview-empty.png"));
    // Double-click empty graph adds; double-click its existing point opens parameters.
    editor->graph.mouseDoubleClick(event(2000,3,false)); expect(editor->selectedBand()>=0&&!panel->isVisible(),"empty-graph double-click opens a panel before explicit band editing");
    const int editBand=editor->selectedBand();
    GestureProbe directProbe; auto* directFrequency=processor->parameters.getParameter(EQ1Processor::id(editBand,"frequency")); directFrequency->addListener(&directProbe);
    editor->graph.mouseDown(event(2000,3,false)); editor->graph.mouseDrag(event(3000,4,true)); editor->graph.mouseDrag(event(4000,6,true)); editor->graph.mouseUp(event(4000,6,true)); directFrequency->removeListener(&directProbe);
    expect(directProbe.starts==1&&directProbe.ends==1&&directProbe.values>=2&&directProbe.grouped,"graph drag does not group actual movement in one host gesture");
    expect(!panel->isVisible(),"direct band drag automatically opens parameters on release");
    editor->graph.mouseDoubleClick(event(4000,6,false)); expect(panel->isVisible(),"double-click on a moved band does not open parameters");
    const auto anchor=panel->getBounds(); editor->changeBand("frequency",8000); expect(panel->getBounds()==anchor,"opened panel follows numeric frequency changes");
    panel->mouseDown(pointerEvent(*panel,{70,14},{70,14},juce::ModifierKeys::leftButtonModifier,false));
    panel->mouseDrag(pointerEvent(*panel,{40,14},{70,14},juce::ModifierKeys::leftButtonModifier,true));
    panel->mouseUp(pointerEvent(*panel,{40,14},{70,14},0,true));
    expect(panel->getX()!=anchor.getX(),"panel title cannot be dragged"); const auto moved=panel->getBounds(); editor->refresh(); expect(panel->getBounds()==moved,"refresh discards the manual panel position");
    auto* close=dynamic_cast<juce::TextButton*>(find(*editor,"closeBandPanel")); expect(close&&close->isVisible(),"parameters have no visible close control");
    close->onClick(); expect(editor->selectedBand()==editBand&&!panel->isVisible(),"closing parameters loses band selection");
    editor->graph.mouseDown(event(8000,6,false)); editor->graph.mouseUp(event(8000,6,false)); expect(!panel->isVisible(),"closed panel reopens on a single click");
    editor->graph.mouseDoubleClick(event(8000,6,false)); expect(panel->isVisible()&&panel->getBounds()==moved,"reopening loses manual position");
    editor->graph.mouseDown(event(8000,6,false)); editor->graph.mouseDrag(event(6000,5,true)); expect(!panel->isVisible(),"open inspector covers direct dragging"); editor->graph.mouseUp(event(6000,5,true));
    expect(panel->isVisible()&&panel->getBounds()==moved,"explicitly opened inspector changes position after a graph drag");
    editor->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)); expect(editor->selectedBand()==editBand&&!panel->isVisible(),"Escape does not close only the inspector");
    editor->popupBand(editBand,true); juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
    auto* bandMenu=juce::Component::getCurrentlyModalComponent(); expect(bandMenu&&bandMenu->isVisible(),"right-click menu closes while inspector is hidden");
    {auto image=bandMenu->createComponentSnapshot(bandMenu->getLocalBounds(),true,1.f); juce::MemoryOutputStream out; juce::PNGImageFormat png; expect(png.writeImageToStream(image,out)&&root.getChildFile("Preview-band-menu.png").replaceWithData(out.getData(),out.getDataSize()),"cannot capture closed-inspector band menu");}
    juce::AccessibilityHandler* parametersAction=nullptr;
    for(auto* item:bandMenu->getChildren()) if(auto* handler=item->getAccessibilityHandler(); handler&&handler->getTitle().startsWith("Parameters")) {parametersAction=handler; break;}
    expect(parametersAction&&parametersAction->getActions().invoke(juce::AccessibilityActionType::press),"native Parameters menu action is unavailable");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
    expect(panel->isVisible(),"Parameters menu action does not open the inspector"); editor->closeBandPanel();
    const int other=processor->addBand(300,0); editor->openBandPanel(editBand); editor->selectBand(other); expect(editor->selectedBand()==other&&!panel->isVisible(),"selecting another band automatically opens parameters");
    editor->setRussian(true); save(*editor,root.getChildFile("Preview-switch-band-RU.png"));
    processor->clearBands(); editor->refresh();
    std::cout<<"PASS: native 1080x680 / 820x560, RU/EN, preset dropdown, actual shape/settings/source popup rendering, graph >80% default / >75% minimum height, permanent bottom bar, no knobs/empty slots, contextual create/select/deselect, region-based Bell/Shelf/Cut creation, no duplicate double-click, adding on curve preserves audio, one-step create undo, cut slope wheel and relevant fields, single click selects without opening parameters or recording an undo/host gesture, direct dragging stays free, double-click opens existing-band parameters, movable inspector retains its position, close/Escape preserve selection, switching bands closes inspector, stable timing/detector disclosures and primary controls, actual label-level horizontal/vertical scrubbing with external SC connected/disconnected, Shift fine control, one grouped host gesture, frozen numeric drag context, uninterrupted double-click comma/kHz text entry, real stereo SC spectrum without opposite-polarity cancellation or added audio delay, contextual SC visibility, band/detector audio audition, M/S, clipboard, peer copy, schema migration, Sketch undo, oversized native block and exact latency\n";
    return 0;
 } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
