#include "PluginEditor.h"
#include "Skin.cpp.inc"

using namespace curveshaper;
namespace {
constexpr const char* sliderKeys[]{"frequency","gain","q","output","threshold","above","below","attack","release","ratio","knee","relative","detectorFrequency","detectorQ","drive","width","density"};
constexpr const char* sliderEN[]{"Frequency","Gain","Width / Q","Output","Threshold","Above range","Below range","Attack","Release","Ratio","Knee","Relative","Detector Hz","Detector Q","Drive","Stereo width","Density"};
constexpr const char* sliderRU[]{u8"Частота",u8"Усиление",u8"Ширина / Q",u8"Выход",u8"Порог",u8"Выше порога",u8"Ниже порога",u8"Атака",u8"Возврат",u8"Соотношение",u8"Колено",u8"Относит.",u8"Частота дет.",u8"Q детектора",u8"Драйв",u8"Стереоширина",u8"Плотность"};
bool globalSlider(int i) { return i==3||i==14||i==15; }
juce::String utf(const char* s) { return juce::String::fromUTF8(s); }
constexpr const char* shapeEN[]{"Bell","Low shelf","High shelf","High pass","Low pass","Notch","Band pass","All pass","Tilt"};
constexpr const char* shapeRU[]{u8"Колокол",u8"Низкая полка",u8"Высокая полка",u8"Срез низких",u8"Срез высоких",u8"Режектор",u8"Полосовой",u8"Фазовый",u8"Наклон"};
constexpr const char* channelEN[]{"Stereo","Left","Right","Mid","Side"};
constexpr const char* channelRU[]{u8"Стерео",u8"Левый",u8"Правый",u8"Mid",u8"Side"};
bool hasGain(Shape s) { return static_cast<int>(s)<=2||s==Shape::Tilt; }
bool isCut(Shape s) { return s==Shape::HighPass||s==Shape::LowPass; }
}
EQDisplay::EQDisplay(EQ1Processor& p,EQ1Editor& o):processor(p),owner(o) {
    setWantsKeyboardFocus(true); setTitle("Frequency response and spectrum"); setComponentID("eqGraph");
    before.fill(-100); after.fill(-100); sidechain.fill(-100); stroke.reserve(512);
}
EQDisplay::~EQDisplay() {
    if(gesture) {
        processor.parameters.getParameter(EQ1Processor::id(draggedBand,"frequency"))->endChangeGesture();
        processor.parameters.getParameter(EQ1Processor::id(draggedBand,"gain"))->endChangeGesture();
    }
}
juce::Rectangle<float> EQDisplay::plot() const { return getLocalBounds().toFloat().reduced(32,20).withTrimmedBottom(14); }
float EQDisplay::frequencyX(float f) const { auto p=plot(); return p.getX()+p.getWidth()*std::log(std::clamp(f,20.f,20000.f)/20)/std::log(1000.f); }
float EQDisplay::xFrequency(float x) const { auto p=plot(); return 20*std::pow(1000.f,std::clamp((x-p.getX())/p.getWidth(),0.f,1.f)); }
float EQDisplay::gainY(float db) const { auto p=plot(); return p.getCentreY()-db*p.getHeight()/48; }
float EQDisplay::yGain(float y) const { auto p=plot(); return std::clamp((p.getCentreY()-y)*48/p.getHeight(),-24.f,24.f); }
Shape EQDisplay::creationShape(juce::Point<float> pt) const {
    const auto area=plot(); const float x=(pt.x-area.getX())/area.getWidth();
    if(x<0.09f) return Shape::HighPass;
    if(x>0.96f) return Shape::LowPass;
    if(x<0.25f) return Shape::LowShelf;
    if(x>0.78f) return Shape::HighShelf;
    if(yGain(pt.y)<-19.f) return Shape::Notch;
    return Shape::Bell;
}
float EQDisplay::stereoGainAt(float f) const {
    double sum=0;
    for(int i=0;i<maxBands;++i) {
        const auto b=processor.band(i); if(!b.enabled||b.placement!=Placement::Stereo) continue;
        if(b.motion==Motion::Spectral) sum+=20*std::log10(std::max(1e-9,magnitude(b,f,processor.analysisSampleRate())))+processor.reduction[i].load()*std::exp(-0.5*std::pow(std::log2(f/b.frequency)*b.q,2));
        else sum+=20*std::log10(std::max(1e-9,magnitude(b,f,processor.analysisSampleRate(),processor.reduction[i].load())));
    }
    return static_cast<float>(sum);
}
float EQDisplay::creationGain(juce::Point<float> pt) const {
    const float db=yGain(pt.y);
    return std::abs(db-stereoGainAt(xFrequency(pt.x)))<0.5f?0.f:db;
}
int EQDisplay::hit(juce::Point<float> pt) const {
    int found=-1; float distance=18;
    for(int i=0;i<maxBands;++i) { const auto b=processor.band(i); if(!b.enabled) continue;
        const float d=pt.getDistanceFrom({frequencyX(b.frequency),gainY(b.gain)}); if(d<distance) { found=i; distance=d; }
    }
    return found;
}
void EQDisplay::paint(juce::Graphics& g) {
    auto area=plot(); g.fillAll(panel);
    for(float f:{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f}) {
        const float x=frequencyX(f); g.setColour(line.withAlpha(0.55f)); g.drawVerticalLine(static_cast<int>(x),area.getY(),area.getBottom());
        label(g,f>=1000?juce::String(f/1000,0)+"k":juce::String(f,0),{static_cast<int>(x)-24,static_cast<int>(area.getBottom())+5,48,20},12,muted,juce::Justification::centred);
    }
    for(int db=-24;db<=24;db+=6) {
        const float y=gainY(static_cast<float>(db)); g.setColour(db==0?controlLine.withAlpha(0.6f):line.withAlpha(0.45f));
        g.drawHorizontalLine(static_cast<int>(y),area.getX(),area.getRight());
        label(g,juce::String(db),{1,static_cast<int>(y)-9,25,18},11,muted,juce::Justification::right);
    }
    auto spectrumPath=[&](const std::array<float,1025>& bins,int kind) {
        juce::Path path; path.startNewSubPath(area.getX(),area.getBottom());
        for(int x=0;x<=static_cast<int>(area.getWidth());++x) {
            const float f=xFrequency(area.getX()+x),k=f*2048/static_cast<float>(processor.analysisSampleRate());
            const int k0=std::clamp(static_cast<int>(k),1,1023); const float v=bins[k0]+(bins[k0+1]-bins[k0])*(k-k0);
            const float y=area.getBottom()-std::clamp((v+90)/84,0.f,1.f)*area.getHeight(); path.lineTo(area.getX()+x,y);
        }
        if(kind==0) { path.lineTo(area.getRight(),area.getBottom()); path.closeSubPath(); g.setColour(rail.withAlpha(0.85f)); g.fillPath(path); }
        else if(kind==2) {juce::Path dotted; const float pattern[]{2,4}; juce::PathStrokeType(1.4f).createDashedStroke(dotted,path,pattern,2); g.setColour(juce::Colour(0xff777381)); g.fillPath(dotted);}
        else { g.setColour(controlLine.withAlpha(0.6f)); g.strokePath(path,juce::PathStrokeType(1)); }
    };
    g.saveState(); g.reduceClipRegion(area.toNearestInt()); spectrumPath(before,0); spectrumPath(after,1);
    if(sidechainView) spectrumPath(sidechain,2);
    juce::Path total,selectedPath;
    const int selected=owner.selectedBand();
    for(int x=0;x<=static_cast<int>(area.getWidth());++x) {
        const float f=xFrequency(area.getX()+x); double sum=0,sel=0;
        for(int i=0;i<maxBands;++i) {
            auto b=processor.band(i); if(!b.enabled) continue;
            double db=0;
            if(b.motion==Motion::Spectral) { db=20*std::log10(std::max(1e-9,magnitude(b,f,processor.analysisSampleRate())))+processor.reduction[i].load()*std::exp(-0.5*std::pow(std::log2(f/b.frequency)*b.q,2)); }
            else db=20*std::log10(std::max(1e-9,magnitude(b,f,processor.analysisSampleRate(),processor.reduction[i].load())));
            // Main response shows the stereo domain; L/R and M/S curves are independently editable.
            if(b.placement==Placement::Stereo) sum+=db;
            if(i==selected) sel=db;
        }
        const auto pt=juce::Point<float>(area.getX()+x,gainY(static_cast<float>(sum)));
        if(x==0) { total.startNewSubPath(pt); selectedPath.startNewSubPath(area.getX(),gainY(static_cast<float>(sel))); }
        else { total.lineTo(pt); selectedPath.lineTo(area.getX()+x,gainY(static_cast<float>(sel))); }
    }
    g.setColour(controlLine.withAlpha(0.65f)); g.strokePath(selectedPath,juce::PathStrokeType(1.5f));
    g.setColour(text); g.strokePath(total,juce::PathStrokeType(2.5f));
    if(previewVisible&&!sketch&&!gesture&&!drawing) {
        Band candidate; candidate.enabled=true; candidate.shape=creationShape(previewPosition); candidate.frequency=xFrequency(previewPosition.x);
        candidate.gain=creationGain(previewPosition); candidate.q=isCut(candidate.shape)?0.7071f:1.f;
        juce::Path future;
        for(int x=0;x<=static_cast<int>(area.getWidth());++x) {
            const float f=xFrequency(area.getX()+x);
            const float db=stereoGainAt(f)+static_cast<float>(20*std::log10(std::max(1e-9,magnitude(candidate,f,processor.analysisSampleRate()))));
            if(x==0) future.startNewSubPath(area.getX(),gainY(db)); else future.lineTo(area.getX()+x,gainY(db));
        }
        juce::Path dashed; const float pattern[]{4,5}; juce::PathStrokeType(1.5f).createDashedStroke(dashed,future,pattern,2);
        g.setColour(controlLine.withAlpha(0.8f)); g.fillPath(dashed);
    }
    g.restoreState();
    if(selected>=0&&processor.band(selected).enabled) {
        const auto b=processor.band(selected);
        auto readout=owner.tr("Band ","Бэнд ")+juce::String(selected+1)+utf(u8" · ")+juce::String(b.frequency,0)+" Hz";
        if(hasGain(b.shape)) readout+=utf(u8" · ")+juce::String(b.gain,1)+" dB";
        readout+=isCut(b.shape)?utf(u8" · ")+juce::String(b.slope,0)+owner.tr(" dB/oct"," дБ/окт"):utf(u8" · Q ")+juce::String(b.q,1);
        label(g,readout,{static_cast<int>(area.getX()),1,340,20},12,muted);
    }
    bool any=false;
    for(int order=0;order<=maxBands;++order) { const int i=order==maxBands?selected:order; if(i<0||i>=maxBands||(i==selected&&order!=maxBands)) continue; const auto b=processor.band(i); if(!b.enabled) continue;
        any=true;
        const float x=frequencyX(b.frequency),y=gainY(b.gain),radius=i==selected?10.f:8.f;
        g.setColour(i==selected?text:panel); g.fillEllipse(x-radius,y-radius,2*radius,2*radius);
        g.setColour(text); g.drawEllipse(x-radius,y-radius,2*radius,2*radius,1.5f);
        g.setColour(i==selected?panel:text); g.setFont(uiFont(11));
        g.drawText(juce::String(i+1),juce::Rectangle<int>(static_cast<int>(x-radius),static_cast<int>(y-radius),static_cast<int>(2*radius),static_cast<int>(2*radius)),juce::Justification::centred);
    }
    if(!any&&!drawing) {
        label(g,owner.tr("Start with a band","Начни с бэнда"),{static_cast<int>(area.getCentreX())-170,static_cast<int>(area.getCentreY())-72,340,30},22,text,juce::Justification::centred);
        label(g,owner.tr("Move to preview. Click to add.","Наведи для просмотра. Нажми, чтобы добавить."),{static_cast<int>(area.getCentreX())-260,static_cast<int>(area.getCentreY())-40,520,24},14,muted,juce::Justification::centred);
    }
    if(previewVisible&&!sketch&&!gesture&&!drawing) {
        const int type=static_cast<int>(creationShape(previewPosition));
        auto hint=owner.tr(shapeEN[type],shapeRU[type])+utf(u8"  ·  ")+juce::String(xFrequency(previewPosition.x),0)+" Hz";
        const int hx=std::clamp(static_cast<int>(previewPosition.x)-105,static_cast<int>(area.getX()),static_cast<int>(area.getRight())-210);
        const int hy=std::clamp(static_cast<int>(previewPosition.y)-42,static_cast<int>(area.getY()),static_cast<int>(area.getBottom())-28);
        g.setColour(panel); g.fillRoundedRectangle(static_cast<float>(hx),static_cast<float>(hy),210,28,5);
        g.setColour(line); g.drawRoundedRectangle(static_cast<float>(hx),static_cast<float>(hy),210,28,5,1);
        label(g,hint,{hx+8,hy,194,28},13,text,juce::Justification::centred);
    }
    label(g,owner.tr("Pre / Post","До / После"),{static_cast<int>(area.getRight())-130,1,130,20},11,muted,juce::Justification::right);
    if(sidechainView) label(g,owner.tr("SC · control signal","SC · управляющий сигнал"),{static_cast<int>(area.getRight())-350,1,210,20},12,juce::Colour(0xff777381),juce::Justification::right);
    if(drawing&&!stroke.empty()) { juce::Path p; p.startNewSubPath(stroke.front()); for(auto pt:stroke) p.lineTo(pt); g.setColour(text.withAlpha(0.65f)); g.strokePath(p,juce::PathStrokeType(2)); }
    if(hasKeyboardFocus(false)) { g.setColour(controlLine); g.drawRect(getLocalBounds(),1); }
}
void EQDisplay::mouseDown(const juce::MouseEvent& e) {
    if(e.getNumberOfClicks()>1&&!e.mods.isPopupMenu()) return;
    grabKeyboardFocus(); int i=hit(e.position); bool created=false;
    previewVisible=false;
    if(e.mods.isPopupMenu()) { if(i>=0) owner.popupBand(i,true); return; }
    if(sketch) { owner.checkpoint(); drawing=true; stroke.clear(); stroke.push_back(e.position); repaint(); return; }
    if(i<0) {
        if(!plot().contains(e.position)) { owner.deselectBand(); return; }
        if(owner.selectedBand()>=0&&!e.mods.isCtrlDown()) { owner.deselectBand(); return; }
        owner.checkpoint(); const auto type=creationShape(e.position);
        i=processor.addBand(xFrequency(e.position.x),hasGain(type)?creationGain(e.position):0,type); created=true;
        if(i<0) return;
        lastCreated=i; lastCreatedPosition=e.position; lastCreatedTime=juce::Time::currentTimeMillis();
        if(isCut(type)) processor.setValue(EQ1Processor::id(i,"q"),0.7071f);
        if(e.mods.isAltDown()&&hasGain(type)) processor.setValue(EQ1Processor::id(i,"motion"),1);
    }
    owner.selectBand(i); mouseOrigin=e.position;
    const auto b=processor.band(i); originFrequency=b.frequency; originGain=b.gain;
    dragPending=true; gestureCreated=created; draggedBand=i;
}
void EQDisplay::mouseDrag(const juce::MouseEvent& e) {
    if(drawing) { if(stroke.size()<512&&stroke.back().getDistanceFrom(e.position)>3) stroke.push_back(e.position); repaint(); return; }
    if(!dragPending) return;
    if(!gesture) {
        if(e.position.getDistanceFrom(mouseOrigin)<3) return;
        if(!gestureCreated) owner.checkpoint();
        processor.parameters.getParameter(EQ1Processor::id(draggedBand,"frequency"))->beginChangeGesture();
        processor.parameters.getParameter(EQ1Processor::id(draggedBand,"gain"))->beginChangeGesture(); gesture=true;
    }
    const auto pos=e.mods.isShiftDown()?mouseOrigin+(e.position-mouseOrigin)*0.15f:e.position;
    const int i=draggedBand;
    processor.setValue(EQ1Processor::id(i,"frequency"),std::clamp(originFrequency*std::pow(1000.f,(pos.x-mouseOrigin.x)/plot().getWidth()),20.f,20000.f),false);
    const auto shape=processor.band(i).shape;
    if(hasGain(shape)) processor.setValue(EQ1Processor::id(i,"gain"),std::clamp(originGain+(mouseOrigin.y-pos.y)*48/plot().getHeight(),-24.f,24.f),false);
    owner.refresh(); repaint();
}
void EQDisplay::mouseUp(const juce::MouseEvent&) {
    if(drawing) { finishSketch(); drawing=false; stroke.clear(); }
    if(gesture) {
        const int i=draggedBand; processor.parameters.getParameter(EQ1Processor::id(i,"frequency"))->endChangeGesture();
        processor.parameters.getParameter(EQ1Processor::id(i,"gain"))->endChangeGesture(); gesture=false;
    }
    dragPending=false; draggedBand=-1;
    owner.refresh(); repaint();
}
void EQDisplay::mouseDoubleClick(const juce::MouseEvent& e) {
    if(sketch||e.mods.isPopupMenu()) return;
    if(lastCreated>=0&&processor.band(lastCreated).enabled&&lastCreatedPosition.getDistanceFrom(e.position)<4&&juce::Time::currentTimeMillis()-lastCreatedTime<500) {owner.openBandPanel(lastCreated); return;}
    const int existing=hit(e.position); if(existing>=0) { owner.openBandPanel(existing); return; }
    if(!plot().contains(e.position)) return;
    owner.checkpoint(); const auto type=creationShape(e.position);
    const int i=processor.addBand(xFrequency(e.position.x),hasGain(type)?creationGain(e.position):0,type);
    if(i>=0&&isCut(type)) processor.setValue(EQ1Processor::id(i,"q"),0.7071f);
    if(i>=0) { owner.selectBand(i); if(e.mods.isAltDown()) owner.changeBand("motion",1,false); }
}
void EQDisplay::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) {
    const int i=hit(e.position); if(i<0) return; owner.selectBand(i); owner.checkpoint();
    auto b=processor.band(i);
    if(isCut(b.shape)) processor.setValue(EQ1Processor::id(i,"slope"),std::clamp(b.slope/12-1+(wheel.deltaY>0?1.f:-1.f),0.f,7.f));
    else processor.setValue(EQ1Processor::id(i,"q"),std::clamp(b.q*std::pow(2.f,wheel.deltaY),0.1f,32.f));
    owner.refresh(); repaint();
}
bool EQDisplay::keyPressed(const juce::KeyPress& key) { return owner.keyPressed(key); }
void EQDisplay::mouseMove(const juce::MouseEvent& e) {
    const bool overBand=hit(e.position)>=0;
    setMouseCursor(overBand?juce::MouseCursor::DraggingHandCursor:juce::MouseCursor::CrosshairCursor);
    previewPosition=e.position; previewVisible=!overBand&&plot().contains(e.position); repaint();
}
void EQDisplay::mouseExit(const juce::MouseEvent&) { previewVisible=false; repaint(); }
void EQDisplay::finishSketch() {
    if(stroke.size()<3) return;
    // Resample the gesture into a fixed log-frequency grid. Fit a few extrema, not a fake drawn curve.
    std::array<float,96> values{}; std::array<bool,96> seen{};
    for(auto p:stroke) { int k=std::clamp(static_cast<int>((p.x-plot().getX())/plot().getWidth()*95),0,95); values[k]=yGain(p.y); seen[k]=true; }
    int start=-1; for(int i=0;i<96;++i) if(seen[i]) { if(start>=0) for(int k=start+1;k<i;++k) { values[k]=values[start]+(values[i]-values[start])*(k-start)/static_cast<float>(i-start); seen[k]=true; } start=i; }
    int last=-12,created=0;
    for(int i=2;i<94;++i) {
        if(!seen[i-1]||!seen[i]||!seen[i+1]||std::abs(values[i])<1.5f||i-last<7) continue;
        bool maximum=values[i]>0?values[i]>=values[i-1]&&values[i]>values[i+1]:values[i]<=values[i-1]&&values[i]<values[i+1];
        if(!maximum) continue;
        int left=i,right=i; while(left>0&&seen[left]&&std::abs(values[left])>std::abs(values[i])*0.5f) --left;
        while(right<95&&seen[right]&&std::abs(values[right])>std::abs(values[i])*0.5f) ++right;
        const float f=xFrequency(plot().getX()+plot().getWidth()*i/95);
        int added=processor.addBand(f,values[i]); if(added<0) break;
        processor.setValue(EQ1Processor::id(added,"q"),std::clamp(95.f/(std::max(3,right-left)*9.966f),0.2f,12.f));
        owner.selectBand(added); last=i; if(++created>=8) break;
    }
    if(created==0) { auto peak=*std::max_element(stroke.begin(),stroke.end(),[&](auto a,auto b){return std::abs(yGain(a.y))<std::abs(yGain(b.y));}); if(std::abs(yGain(peak.y))>=1.5f) { const int i=processor.addBand(xFrequency(peak.x),yGain(peak.y)); if(i>=0) owner.selectBand(i); } }
    owner.refresh();
}
void EQDisplay::analyze() {
    float a,b,sl,sr; bool updated=false; const bool frozen=processor.value("freeze")>0.5f;
    while(processor.pullSpectrum(a,b,sl,sr)) {
        if(frozen) continue;
        input[cursor]=a; output[cursor]=b; sideLeft[cursor]=sl; sideRight[cursor]=sr; cursor=(cursor+1)%2048;
        if(++count<512) continue; count=0;
        for(int c=0;c<2;++c) {
            auto& source=c==0?input:output; auto& target=c==0?before:after;
            for(int i=0;i<2048;++i) frame[i]={source[(cursor+i)%2048]*(0.5f-0.5f*std::cos(static_cast<float>(2*pi*i/2048))),0};
            fft.transform(frame);
            for(int i=0;i<=1024;++i) { float db=gainDb(std::abs(frame[i])/512); target[i]=std::max(db,target[i]-2.f); }
        }
        if(processor.sidechainConnected.load()) for(int c=0;c<2;++c) {
            const auto& source=c==0?sideLeft:sideRight;
            for(int i=0;i<2048;++i) frame[i]={source[(cursor+i)%2048]*(0.5f-0.5f*std::cos(static_cast<float>(2*pi*i/2048))),0};
            fft.transform(frame);
            for(int i=0;i<=1024;++i) {const float power=std::norm(frame[i]/512.f)*0.5f; if(c==0) sidePower[i]=power; else sidechain[i]=std::max(gainDb(std::sqrt(sidePower[i]+power)),sidechain[i]-2.f);}
        }
        else sidechain.fill(-100);
        updated=true;
    }
    if(updated) repaint();
}
float EQDisplay::sidechainLevelAt(float frequency) const {return sidechain[std::clamp(static_cast<int>(frequency*2048/processor.analysisSampleRate()),1,1024)];}

void FloatingPanel::paint(juce::Graphics& g) {
    auto r=getLocalBounds().toFloat().reduced(2);
    g.setColour(juce::Colours::black.withAlpha(0.035f)); g.fillRoundedRectangle(r.translated(0,2),8);
    g.setColour(panel); g.fillRoundedRectangle(r,8);
    g.setColour(line); g.drawRoundedRectangle(r,8,1);
    if(title.isNotEmpty()) {
        if(movable) label(g,title,{16,4,getWidth()-54,20},13,muted);
        else label(g,title,{16,10,getWidth()-60,23},16,text);
    }
    if(subtitle.isNotEmpty()) label(g,subtitle,{16,getHeight()-28,getWidth()-32,18},11,muted);
    if(getComponentID()=="bandPanel"&&getHeight()>150) { g.setColour(line.withAlpha(0.6f)); g.drawHorizontalLine(136,16.f,getWidth()-16.f); }
}
void FloatingPanel::mouseDown(const juce::MouseEvent& e) {
    if(!movable||e.mods.isPopupMenu()) return;
    dragScreen=e.getScreenPosition(); dragPosition=getPosition(); dragging=true;
}
void FloatingPanel::mouseDrag(const juce::MouseEvent& e) {
    if(!dragging) return;
    setTopLeftPosition(dragPosition+e.getScreenPosition()-dragScreen);
    if(onMove) onMove();
}
void FloatingPanel::mouseUp(const juce::MouseEvent&) {dragging=false;}

EQ1Editor::EQ1Editor(EQ1Processor& p)
 : AudioProcessorEditor(p),graph(p,*this),processor(p),
   sliders{&frequency,&gain,&q,&outputGain,&threshold,&above,&below,&attack,&release,&ratio,&knee,&relative,&detectorFrequency,&detectorQ,&drive,&width,&density} {
    setLookAndFeel(&skin); setResizable(true,true); setResizeLimits(820,560,1600,1100); setSize(1080,680);
    russian=p.parameters.state.getProperty("editorLanguage","en").toString()=="ru";
    setTitle("EQ-11"); addAndMakeVisible(graph); addChildComponent(bandPanel); addChildComponent(settingsPanel);
    bandPanel.setComponentID("bandPanel"); settingsPanel.setComponentID("settingsPanel");
    bandPanel.movable=true; bandPanel.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    bandPanel.onMove=[this]{panelPlaced=true; layoutPanels();};
    addAndMakeVisible(preset); preset.getProperties().set("compact",true); preset.setComponentID("presets");
    for(auto* b:{&undoButton,&redoButton,&bypass,&settings,&language}) addAndMakeVisible(*b);
    settingsPanel.addAndMakeVisible(closeSettings);
    for(auto* b:{&sketch,&freeze,&delta}) addAndMakeVisible(*b);
    for(auto* b:{&freeDetector,&details,&solo,&detectorSolo,&detectorSettings,&detectorSource,&shape,&placement,&motion,&moreBand,&slope,&closeBand}) bandPanel.addAndMakeVisible(*b);
    for(auto* b:{&undoButton,&redoButton,&ab,&language,&sketch,&freeze,&delta,&bypass,&instances,&external,&freeDetector,&settings,&details,&remove,&solo,&detectorSolo,&closeBand,&closeSettings,&shape,&placement,&motion,&moreBand,&slope}) {
        b->getProperties().set("compact",true); b->setWantsKeyboardFocus(true);
    }
    settings.setComponentID("settings"); details.setComponentID("bandDetails"); solo.setComponentID("bandSolo"); detectorSolo.setComponentID("detectorSolo");
    undoButton.setComponentID("undoButton"); redoButton.setComponentID("redoButton");
    language.setComponentID("language"); remove.setComponentID("removeBand");
    shape.setComponentID("bandShape"); placement.setComponentID("bandChannel"); motion.setComponentID("bandMotion"); moreBand.setComponentID("bandMore"); slope.setComponentID("cutSlope");
    detectorSettings.setComponentID("detectorSettings"); detectorSource.setComponentID("detectorSource");
    closeBand.setComponentID("closeBandPanel"); closeBand.getProperties().set("icon","close");
    sketch.setComponentID("drawCurve"); freeze.setComponentID("freezeSpectrum"); delta.setComponentID("listenDifference");
    mode.setComponentID("processingMode"); character.setComponentID("colourMode");
    for(auto* b:{&details,&detectorSettings,&detectorSource}) {b->getProperties().set("compact",true); b->getProperties().set("flat",true); b->getProperties().set("menu",true);}
    for(auto* b:{&details,&detectorSettings}) b->getProperties().set("section",true);
    undoButton.getProperties().set("icon","undo"); redoButton.getProperties().set("icon","redo"); settings.getProperties().set("icon","gear"); solo.getProperties().set("icon","headphones");
    shape.getProperties().set("icon","filter"); moreBand.getProperties().set("icon","more");
    for(auto* b:{&shape,&placement,&motion,&details,&slope,&detectorSettings}) b->getProperties().set("flat",true);
    detectorSolo.getProperties().set("icon","headphones");
    for(auto* b:{&shape,&placement,&motion,&slope,&details}) b->getProperties().set("menu",true);
    for(auto* b:{&placement,&motion,&details}) b->getProperties().set("secondary",true);
    for(auto* b:{&sketch,&freeze,&delta,&bypass,&external,&freeDetector,&solo,&detectorSolo}) b->setClickingTogglesState(true);
    preset.onChange=[this]{ if(refreshing) return; const int choice=preset.getSelectedId(); if(choice>=1&&choice<=5) { presetChoice=choice; applyPreset(choice-1); } if(choice==10) loadPreset(); if(choice==11) savePreset(); if(choice>=10) preset.setSelectedId(presetChoice,juce::dontSendNotification); };
    undoButton.onClick=[this]{undo();}; redoButton.onClick=[this]{redo();}; ab.onClick=[this]{switchAB();};
    settings.onClick=[this]{showSettings();}; closeSettings.onClick=[this]{settingsOpen=false; layoutPanels();};
    details.onClick=[this]{expanded=!(expanded&&!detectorPage); detectorPage=false; layoutPanels();}; closeBand.onClick=[this]{closeBandPanel();};
    detectorSettings.onClick=[this]{expanded=!(expanded&&detectorPage); detectorPage=true; layoutPanels();};
    detectorSource.onClick=[this]{detectorSourceMenu();};
    remove.onClick=[this]{changeBand("on",0); deselectBand();};
    solo.onClick=[this]{listen(false);}; detectorSolo.onClick=[this]{listen(true);};
    language.onClick=[this]{setRussian(!russian);};
    sketch.onClick=[this]{graph.sketch=sketch.getToggleState(); settingsOpen=false; layoutPanels(); graph.repaint();};
    freeze.onClick=[this]{processor.setValue("freeze",freeze.getToggleState()?1.f:0.f);};
    delta.onClick=[this]{checkpoint(); processor.setValue("delta",delta.getToggleState()?1.f:0.f);};
    bypass.onClick=[this]{checkpoint(); processor.setValue("bypass",bypass.getToggleState()?1.f:0.f);};
    instances.onClick=[this]{instanceMenu();};
    external.onClick=[this]{changeBand("external",external.getToggleState()?1.f:0.f);};
    freeDetector.onClick=[this]{changeBand("freeDetector",freeDetector.getToggleState()?1.f:0.f);};
    for(auto* c:{&mode,&character}) { addAndMakeVisible(*c); c->setWantsKeyboardFocus(true); c->getProperties().set("compact",true); }
    mode.onChange=[this]{if(!refreshing) { checkpoint(); processor.setValue("engine",static_cast<float>(mode.getSelectedId()-1)); refresh(); }};
    character.onChange=[this]{if(!refreshing) { checkpoint(); processor.setValue("character",static_cast<float>(character.getSelectedId()-1)); refresh(); }};
    shape.onClick=[this]{shapeMenu();}; placement.onClick=[this]{placementMenu();}; motion.onClick=[this]{motionMenu();}; slope.onClick=[this]{slopeMenu();};
    moreBand.onClick=[this]{if(selected>=0) popupBand(selected);};
    for(int i=0;i<17;++i) {
        prepareSlider(*sliders[i],sliderKeys[i],globalSlider(i),sliderEN[i],i);
        juce::Component& parent=i==3?static_cast<juce::Component&>(*this):globalSlider(i)?static_cast<juce::Component&>(settingsPanel):static_cast<juce::Component&>(bandPanel); parent.addAndMakeVisible(captions[i]);
        captions[i].setFont(uiFont(13)); captions[i].setColour(juce::Label::textColourId,muted); captions[i].setJustificationType(juce::Justification::left);
    }
    translate(); refresh(); p.getStateInformation(stateA); stateB=stateA; startTimerHz(30);
}
EQ1Editor::~EQ1Editor() { juce::PopupMenu::dismissAllActiveMenus(); stopTimer(); for(auto* s:sliders) if(auto* field=dynamic_cast<ScrubSlider*>(s)) field->finishScrub(); processor.auditionBand=-1; setLookAndFeel(nullptr); }
juce::String EQ1Editor::tr(const char* en,const char* ru) const { return utf(russian?ru:en); }
void EQ1Editor::setRussian(bool ru) { russian=ru; processor.parameters.state.setProperty("editorLanguage",ru?"ru":"en",nullptr); translate(); refresh(); }
void EQ1Editor::prepareSlider(juce::Slider& s,const char* key,bool global,const char* title,int) {
    juce::Component& parent=juce::String(key)=="output"?static_cast<juce::Component&>(*this):global?static_cast<juce::Component&>(settingsPanel):static_cast<juce::Component&>(bandPanel); parent.addAndMakeVisible(s);
    s.setComponentID(key); s.setTitle(title); s.setWantsKeyboardFocus(true); s.getProperties().set("numeric",true); s.getProperties().set("quiet",true);
    s.getProperties().set("valueFont",juce::String(key)=="frequency"||juce::String(key)=="gain"?26:18);
    auto* p=processor.parameters.getParameter(global?juce::String(key):EQ1Processor::id(0,key));
    auto range=p->getNormalisableRange(); s.setRange(range.start,range.end,range.interval); s.setSkewFactor(range.skew);
    s.setSliderStyle(juce::Slider::LinearHorizontal); s.setTextBoxStyle(juce::Slider::TextBoxLeft,false,160,30);
    s.setScrollWheelEnabled(true); s.setNumDecimalPlacesToDisplay(1);
    const juce::String k(key);
    s.valueFromTextFunction=[frequencyField=k=="frequency"||k=="detectorFrequency"](const juce::String& input) {
        const auto value=input.trim().toLowerCase().removeCharacters(" \t").replace(",",".");
        const bool kilo=frequencyField&&(value.containsChar('k')||value.containsChar(0x043a));
        return value.getDoubleValue()*(kilo?1000:1);
    };
    if(k=="frequency"||k=="detectorFrequency") { s.setTextValueSuffix(" Hz"); s.setNumDecimalPlacesToDisplay(0); }
    else if(k=="gain"||k=="output"||k=="threshold"||k=="above"||k=="below"||k=="knee"||k=="drive") s.setTextValueSuffix(" dB");
    else if(k=="attack"||k=="release") s.setTextValueSuffix(" ms");
    else if(k=="relative"||k=="width"||k=="density") { s.setTextValueSuffix(" %"); s.setNumDecimalPlacesToDisplay(0); }
    s.setDoubleClickReturnValue(true,p->convertFrom0to1(p->getDefaultValue()));
    s.onDragStart=[this,&s,key,global] {
        if(!global&&selected<0) return;
        checkpoint(); const int band=selected; s.getProperties().set("gestureBand",band);
        processor.parameters.getParameter(global?juce::String(key):EQ1Processor::id(band,key))->beginChangeGesture(); s.getProperties().set("gesture",true);
    };
    s.onValueChange=[this,&s,key,global] {
        if(refreshing||(!global&&selected<0)) return;
        const bool gesture=static_cast<bool>(s.getProperties()["gesture"]); if(!gesture) checkpoint();
        const int band=gesture?static_cast<int>(s.getProperties()["gestureBand"]):selected;
        processor.setValue(global?juce::String(key):EQ1Processor::id(band,key),static_cast<float>(s.getValue()),!gesture); graph.repaint();
    };
    s.onDragEnd=[this,&s,key,global] {
        if(!static_cast<bool>(s.getProperties()["gesture"])) return;
        processor.parameters.getParameter(global?juce::String(key):EQ1Processor::id(static_cast<int>(s.getProperties()["gestureBand"]),key))->endChangeGesture(); s.getProperties().set("gesture",false);
    };
}
void EQ1Editor::translate() {
    refreshing=true;
    preset.clear(juce::dontSendNotification); preset.setTextWhenNothingSelected(tr("Presets","Пресеты"));
    const char* presetsEN[]{"Flat","Vocal shape","Dynamic presence","Spectral soften","Gentle mastering"};
    const char* presetsRU[]{u8"Без коррекции",u8"Форма вокала",u8"Динамика присутствия",u8"Смягчить спектр",u8"Мягкий мастеринг"};
    for(int i=0;i<5;++i) preset.addItem(tr(presetsEN[i],presetsRU[i]),i+1);
    preset.addSeparator(); preset.addItem(tr("Load preset…","Загрузить пресет…"),10); preset.addItem(tr("Save preset…","Сохранить пресет…"),11); preset.setSelectedId(presetChoice,juce::dontSendNotification);
    undoButton.setButtonText(""); redoButton.setButtonText(""); undoButton.setTitle(tr("Undo last change","Отменить изменение")); redoButton.setTitle(tr("Redo last change","Повторить изменение"));
    undoButton.setTooltip(undoButton.getTitle()+" (Ctrl+Z)"); redoButton.setTooltip(redoButton.getTitle()+" (Ctrl+Shift+Z)");
    language.setButtonText(russian?"RU":"EN"); sketch.setButtonText(tr("Draw curve","Рисовать")); freeze.setButtonText(tr("Freeze","Стоп спектра"));
    delta.setButtonText(tr("Listen to delta","Слушать разницу")); bypass.setButtonText(tr("Compare with original","Сравнить с исходником")); instances.setButtonText(tr("Copy from instance","Из экземпляра"));
    bypass.setTooltip(tr("When active, hear the original at the same plugin delay. Click again for processed audio.",u8"При включении слышен исходный звук с той же задержкой плагина. Повторный клик возвращает обработку."));
    settings.setButtonText(""); settings.setTitle(tr("More settings","Дополнительные настройки")); settings.setTooltip(settings.getTitle());
    language.setTitle(tr("Interface language","Язык интерфейса")); language.setTooltip(tr("Switch English / Russian","Переключить английский / русский"));
    graph.setTooltip(tr("Click a point to select; drag for frequency/gain and use the wheel for Q. Double-click a band to open parameters, or right-click and choose Parameters.",u8"Клик выбирает точку; перетаскивание меняет частоту/усиление, колесо — Q. Двойной клик по бэнду открывает параметры. Также: правый клик → Параметры."));
    bandPanel.setTooltip(tr("Drag the panel by its title or empty background. Its position stays fixed while you edit the band.",u8"Перемещай панель за заголовок или свободное место. При редактировании бэнда она остаётся на месте."));
    closeBand.setTitle(tr("Close band parameters","Закрыть параметры бэнда")); closeBand.setTooltip(closeBand.getTitle()+" (Esc)");
    details.setButtonText(tr("Details","Детали")); remove.setButtonText(tr("Delete","Удалить"));
    solo.setButtonText(""); solo.setTitle(tr("Listen to this band","Прослушать бэнд")); detectorSolo.setButtonText(""); detectorSolo.setTitle(tr("Listen to detector","Слушать детектор")); closeBand.setButtonText(utf(u8"×")); closeSettings.setButtonText(utf(u8"×"));
    moreBand.setTitle(tr("Band actions","Действия с бэндом")); moreBand.setTooltip(moreBand.getTitle());
    external.setButtonText(tr("External sidechain","Внешний сайдчейн")); freeDetector.setButtonText(tr("Separate frequency / Q","Отдельная частота / Q"));
    mode.clear(juce::dontSendNotification); mode.addItem(tr("Live","Живой"),1); mode.addItem(tr("Spectral","Спектр"),2);
    character.clear(juce::dontSendNotification); character.addItem(tr("Colour: Clean","Окраска: Нет"),1); character.addItem(tr("Colour: Subtle","Окраска: Мягкая"),2); character.addItem(tr("Colour: Warm","Окраска: Тёплая"),3);
    for(int i=0;i<17;++i) { captions[i].setText(tr(sliderEN[i],sliderRU[i]),juce::dontSendNotification); sliders[i]->setTitle(tr(sliderEN[i],sliderRU[i])); }
    mode.setTooltip(tr("Clean Live adds zero audio delay. Resonance bands use causal filters; Spectral uses buffered FFT. Change engine and colour while stopped.",u8"Чистый живой режим без задержки аудио. Резонансы: быстрые фильтры; Спектр: FFT с буфером. Меняй движок и окраску при остановке."));
    sketch.setTooltip(tr("Draw a broad curve, release to fit real EQ bands.",u8"Нарисуй кривую: после отпускания появятся настоящие полосы EQ."));
    solo.setTooltip(tr("Listen to this frequency area. Click again to return to the mix.",u8"Прослушать область частот. Повторный клик вернёт микс."));
    detectorSolo.setTooltip(tr("Listen to the band detector, including the external sidechain.",u8"Прослушать детектор полосы, включая внешний сайдчейн."));
    external.setTooltip(tr("Trigger this band from the DAW sidechain.",u8"Управлять полосой от сайдчейна DAW."));
    freeDetector.setTooltip(tr("Use a separate detector frequency and Q without moving the EQ band.",u8"Задать частоту и Q детектора отдельно, не перемещая бэнд EQ."));
    details.setTooltip(tr("Show timing, ratio, knee and relative response. Range and threshold stay above.",u8"Раскрыть время, соотношение, колено и относительную реакцию. Диапазон и порог остаются выше."));
    detectorSettings.setTooltip(tr("Show the detector frequency settings. The signal source stays visible above.",u8"Раскрыть настройку частоты детектора. Его источник остаётся виден выше."));
    delta.setTooltip(tr("Listen to the difference between original and processed audio; click again for the mix.",u8"Слушать разницу исходного и обработанного звука. Повторный клик вернёт микс."));
    freeze.setTooltip(tr("Pause the spectrum display; audio keeps playing.",u8"Остановить изображение спектра; звук продолжает играть."));
    shape.setTooltip(tr("Filter shape determines which frequency area changes.",u8"Форма фильтра определяет область частот, которую меняет бэнд."));
    slope.setTooltip(tr("Cut steepness in dB per octave. Use the wheel over the cut point.",u8"Крутизна среза в дБ на октаву. Меняется колесом над точкой среза."));
    placement.setTooltip(tr("Stereo: both channels. Mid: centre. Side: difference between left/right.",u8"Стерео: оба канала. Mid: центр. Side: разность левого и правого."));
    motion.setTooltip(tr("Static stays fixed. Dynamic follows level across the band. Resonance control treats narrow areas independently.",u8"Статический: постоянная коррекция. Динамический: весь бэнд реагирует на уровень. Резонансы: узкие области реагируют отдельно."));
    const char* hintsEN[]{"Centre/cut frequency in Hz.","Static boost or cut. Dynamic range acts on top of this gain.","Higher Q narrows the frequency area; the mouse wheel changes Q.","Final output level.","Detector level where correction starts.","Maximum correction above threshold. Negative reduces, positive boosts.","Maximum correction below threshold. Negative reduces, positive boosts.","How quickly correction builds up.","How quickly correction returns after the signal falls.","Strength of the level-dependent response.","Softness of the transition around threshold.","Blend absolute detector level with level relative to the full signal.","Frequency heard by the separate detector.","Width of the separate detector; larger Q is narrower.","Amount of nonlinear saturation.","Side level relative to Mid: 100% preserves original width.","Higher density makes resonance correction more selective."};
    const char* hintsRU[]{u8"Центральная частота или частота среза, в Гц.",u8"Статическое усиление или ослабление. Динамика действует поверх него.",u8"Больший Q сужает область частот. Колесо над точкой меняет Q.",u8"Уровень на выходе всего эквалайзера.",u8"Уровень детектора, при котором начинается коррекция.",u8"Максимальная коррекция выше порога: минус ослабляет, плюс усиливает.",u8"Максимальная коррекция ниже порога: минус ослабляет, плюс усиливает.",u8"Как быстро набирается динамическая коррекция.",u8"Как быстро коррекция возвращается после спада сигнала.",u8"Сила реакции на изменение уровня.",u8"Плавность перехода около порога.",u8"Смесь абсолютного уровня и уровня относительно всего сигнала.",u8"Частота, которую слышит отдельный детектор.",u8"Ширина отдельного детектора: больший Q сужает область.",u8"Сила нелинейного насыщения.",u8"Уровень Side относительно Mid. 100% сохраняет исходную ширину.",u8"Большая плотность делает коррекцию резонансов более избирательной."};
    for(int i=0;i<17;++i) { sliders[i]->setTooltip(tr(hintsEN[i],hintsRU[i])+tr(" Drag up/right to increase; Shift for fine control. Double-click to type.",u8" Тяни вверх/вправо для увеличения; Shift — точнее. Двойной клик — ввод числа.")); captions[i].setTooltip(sliders[i]->getTooltip()); }
    refreshing=false;
}
void EQ1Editor::resized() {
    const int w=getWidth(),footer=getHeight()-55;
    preset.setBounds(184,16,180,32); undoButton.setBounds(w-424,16,32,32); redoButton.setBounds(w-384,16,32,32);
    bypass.setBounds(w-344,16,216,32); language.setBounds(w-120,16,56,32); settings.setBounds(w-56,16,32,32);
    graph.setBounds(20,64,w-40,getHeight()-126);
    mode.setBounds(20,footer+10,148,32); character.setBounds(178,footer+10,142,32);
    captions[3].setBounds(330,footer+1,86,16); outputGain.setBounds(330,footer+18,86,29);
    sketch.setBounds(432,footer+10,90,32); freeze.setBounds(532,footer+10,110,32); delta.setBounds(652,footer+10,148,32);
    layoutPanels();
}
void EQ1Editor::layoutPanels() {
    const bool valid=selected>=0&&processor.band(selected).enabled;
    bandPanel.setVisible(valid&&inspectorOpen&&!settingsOpen&&!graph.interacting()); settingsPanel.setVisible(settingsOpen);
    settings.setToggleState(settingsOpen,juce::dontSendNotification);
    graph.sidechainView=valid&&processor.band(selected).motion!=Motion::Static&&processor.band(selected).external&&processor.sidechainConnected.load()&&!settingsOpen&&((inspectorOpen&&expanded&&detectorPage)||(processor.auditionBand.load()==selected&&processor.auditionDetector.load()));
    std::array<bool,17> showValue{},showCaption{};
    struct Visibility {juce::Component* component; bool visible=false;};
    std::array<Visibility,8> controls{{{&external},{&freeDetector},{&detectorSolo},{&detectorSettings},{&detectorSource},{&slope},{&details},{&motion}}};
    auto show=[&](juce::Component& component,bool visible=true) {for(auto& state:controls) if(state.component==&component) state.visible=visible;};
    if(valid&&inspectorOpen) {
        const auto b=processor.band(selected); const bool dynamic=b.motion!=Motion::Static&&hasGain(b.shape);
        const bool cut=isCut(b.shape); const int pw=getWidth()<940?350:390, ph=!hasGain(b.shape)?146:dynamic?(expanded?408:310):184;
        const float nx=graph.getX()+graph.frequencyX(b.frequency),ny=graph.getY()+graph.gainY(b.gain);
        int x=std::clamp(static_cast<int>(nx)-pw/2,28,getWidth()-pw-28),y=static_cast<int>(ny)+28;
        if(panelPlaced) {x=bandPanel.getX(); y=bandPanel.getY();}
        else if(y+ph>graph.getBottom()-12) {
            y=static_cast<int>(ny)-ph-28;
            if(y<graph.getY()+14) {
                x=static_cast<int>(nx)+28;
                if(x+pw>getWidth()-28) x=static_cast<int>(nx)-pw-28;
                x=std::clamp(x,28,getWidth()-pw-28); y=static_cast<int>(ny)-60;
            }
        }
        x=std::clamp(x,28,getWidth()-pw-28); y=std::clamp(y,graph.getY()+14,graph.getBottom()-ph-12);
        bandPanel.setBounds(x,y,pw,ph); panelPlaced=true; bandPanel.title=tr("Band ","Бэнд ")+juce::String(selected+1); bandPanel.subtitle={};
        closeBand.setBounds(pw-36,2,28,24);
        shape.setBounds(16,30,pw-170,30); placement.setBounds(pw-150,30,68,30); solo.setBounds(pw-76,30,28,30); moreBand.setBounds(pw-44,30,28,30);
        auto field=[&](int i,int fx,int fy,int fw,int fh=30) {
            fy+=20;
            showCaption[i]=true; showValue[i]=true;
            captions[i].setBounds(fx,fy,fw,18); sliders[i]->setBounds(fx,fy+20,fw,fh);
        };
        const int first=pw==350?130:145,second=pw==350?108:116,secondX=first+25,thirdX=secondX+second+15;
        field(0,16,51,first,36);
        if(hasGain(b.shape)) { field(1,secondX,51,second,36); field(2,thirdX,51,pw-16-thirdX,36); }
        else if(cut) {
            show(slope); slope.setBounds(secondX,91,second+14,36);
            captions[1].setText(tr("Slope","Крутизна"),juce::dontSendNotification); showCaption[1]=true; captions[1].setBounds(secondX,71,second+14,18);
            if(b.slope==12) field(2,thirdX+8,51,pw-24-thirdX,36);
        } else field(2,pw-148,51,132,36);
        if(hasGain(b.shape)) {
            show(motion); motion.setBounds(16,139,pw-32,30);
            if(dynamic) {
                const int col=(pw-64)/3,c2=32+col,c3=48+2*col;
                field(5,16,156,col); field(4,c2,156,col);
                if(b.motion==Motion::Spectral) field(16,c3,156,col);
                show(detectorSource); detectorSource.setBounds(16,234,pw-76,28);
                show(detectorSolo); detectorSolo.setBounds(pw-48,234,32,28);
                show(details); show(detectorSettings);
                const int half=(pw-40)/2;
                details.setBounds(16,270,half,28); detectorSettings.setBounds(24+half,270,half,28);
                details.setButtonText(tr("Timing & response","Время и реакция")); detectorSettings.setButtonText(tr("Detector","Детектор"));
                details.setToggleState(expanded&&!detectorPage,juce::dontSendNotification); detectorSettings.setToggleState(expanded&&detectorPage,juce::dontSendNotification);
                details.getProperties().set("expanded",expanded&&!detectorPage); detectorSettings.getProperties().set("expanded",expanded&&detectorPage);
                if(expanded&&!detectorPage) {
                    field(7,16,283,col,26); field(8,c2,283,col,26); field(9,c3,283,col,26);
                    field(6,16,329,col,26); field(10,c2,329,col,26); field(11,c3,329,col,26);
                } else if(expanded) {
                    show(freeDetector,b.motion==Motion::Dynamic); freeDetector.setBounds(16,309,pw-32,30);
                    if(b.freeDetector&&b.motion==Motion::Dynamic) {field(12,16,328,(pw-48)/2,28); field(13,24+(pw-48)/2,328,(pw-48)/2,28);}
                    else bandPanel.subtitle=b.motion==Motion::Spectral?tr("Each resonance cell has its own detector","У каждой области резонанса свой детектор"):tr("Frequency and Q follow the EQ band","Частота и Q детектора следуют бэнду EQ");
                }
            }
        }
    }
    for(int i=0;i<17;++i) if(!globalSlider(i)) {sliders[i]->setVisible(showValue[i]); captions[i].setVisible(showCaption[i]);}
    for(const auto& state:controls) state.component->setVisible(state.visible);
    if(settingsOpen) {
        settingsPanel.setBounds(getWidth()-354,64,334,176); settingsPanel.title=tr("Colour & stereo","Окраска и стерео");
        closeSettings.setBounds(290,8,28,26);
        captions[14].setBounds(16,57,142,18); drive.setBounds(16,80,142,32);
        captions[15].setBounds(174,57,144,18); width.setBounds(174,80,144,32);
        settingsPanel.subtitle=tr("Drive is available with colour enabled","Драйв доступен при включённой окраске");
    }
    bandPanel.toFront(false); settingsPanel.toFront(false); bandPanel.repaint(); settingsPanel.repaint(); graph.repaint();
}
void EQ1Editor::paint(juce::Graphics& g) {
    g.fillAll(background); label(g,"EQ-11",{24,16,172,32},20,text);
    g.setColour(line); g.drawHorizontalLine(getHeight()-57,20.f,getWidth()-20.f);
    if(getWidth()>=1000) {
        const bool listening=processor.auditionBand.load()>=0;
        const auto status=listening?processor.auditionDetector.load()?tr("Listening to detector","Прослушивание детектора"):tr("Listening to band","Прослушивание бэнда"):graph.sketch?tr("Draw, then release","Нарисуй кривую и отпусти"):selected>=0&&!inspectorOpen?tr("Double-click band: parameters","Двойной клик: параметры"):tr("Drag values · Shift: fine","Тяни значения · Shift: точнее");
        label(g,status,{820,getHeight()-44,getWidth()-844,30},12,muted,juce::Justification::right);
    }
}
void EQ1Editor::selectBand(int i) { if(i>=0&&i<24&&processor.band(i).enabled) { if(selected!=i) { inspectorOpen=false; panelPlaced=false; expanded=false; detectorPage=false; processor.auditionBand=-1; } selected=i; settingsOpen=false; refresh(); } }
void EQ1Editor::openBandPanel(int i) { selectBand(i); if(selected!=i||i<0) return; inspectorOpen=true; settingsOpen=false; refresh(); }
void EQ1Editor::closeBandPanel() { inspectorOpen=false; processor.auditionBand=-1; graph.grabKeyboardFocus(); refresh(); }
void EQ1Editor::deselectBand() { selected=-1; inspectorOpen=false; panelPlaced=false; expanded=false; processor.auditionBand=-1; refresh(); }
void EQ1Editor::showSoundSettings() { settingsOpen=!settingsOpen; processor.auditionBand=-1; layoutPanels(); }
void EQ1Editor::showSettings() {
    juce::PopupMenu menu,versions; menu.setLookAndFeel(&skin);
    menu.addItem(1,tr("Colour & stereo…","Окраска и стерео…"));
    versions.addItem(4,tr("Switch A / B · current ","Переключить A / B · сейчас ")+(isB?"B":"A"));
    menu.addSubMenu(tr("Versions A / B","Версии A / B"),versions);
    menu.addSeparator(); menu.addItem(6,tr("Copy EQ from another instance…","Копировать EQ из другого экземпляра…"),processor.peers().size()>1);
    juce::Component::SafePointer<EQ1Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settings).withMinimumWidth(280).withStandardItemHeight(34),[safe](int result){
        if(!safe) return;
        if(result==1) safe->showSoundSettings();
        if(result==4) safe->switchAB();
        if(result==6) safe->instanceMenu();
        safe->refresh();
    });
}
void EQ1Editor::detectorSourceMenu() {
    if(selected<0) return; const int target=selected; const auto b=processor.band(selected);
    juce::PopupMenu menu; menu.setLookAndFeel(&skin);
    menu.addItem(1,tr("Own signal","Свой сигнал"),true,!b.external);
    menu.addItem(2,tr("External sidechain","Внешний сайдчейн"),true,b.external);
    juce::Component::SafePointer<EQ1Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&detectorSource).withMinimumWidth(240).withStandardItemHeight(34),[safe,target](int result){
        if(safe&&safe->selected==target&&result>0) safe->changeBand("external",result==2?1.f:0.f);
    });
}
void EQ1Editor::setShape(int type) {
    if(selected<0||type<0||type>8) return;
    checkpoint(); processor.setValue(EQ1Processor::id(selected,"shape"),static_cast<float>(type));
    if(!hasGain(static_cast<Shape>(type))||type==8&&processor.band(selected).motion==Motion::Spectral)
        processor.setValue(EQ1Processor::id(selected,"motion"),0);
    expanded=false; refresh();
}
void EQ1Editor::shapeMenu() {
    if(selected<0) return; const int target=selected,current=static_cast<int>(processor.band(selected).shape);
    juce::PopupMenu menu,other;
    menu.setLookAndFeel(&skin);
    auto add=[&](juce::PopupMenu& m,int type) {
        auto icon=std::make_unique<juce::DrawablePath>(); icon->setPath(ShaperSkin::filterPath(type,{0,0,30,24}));
        icon->setFill(juce::Colours::transparentBlack); icon->setStrokeFill(text); icon->setStrokeThickness(1.6f);
        m.addItem(juce::PopupMenu::Item(tr(shapeEN[type],shapeRU[type])).setID(type+1).setTicked(current==type).setImage(std::move(icon)));
    };
    add(menu,0); add(menu,3); add(menu,4); menu.addSeparator(); add(menu,1); add(menu,2);
    for(int type:{5,6,8,7}) add(other,type);
    menu.addSeparator(); menu.addSubMenu(tr("More shapes","Другие формы"),other);
    juce::Component::SafePointer<EQ1Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&shape).withMinimumWidth(228).withStandardItemHeight(36),[safe,target](int r){if(safe&&safe->selected==target&&r>0) safe->setShape(r-1);});
}
void EQ1Editor::placementMenu() {
    if(selected<0) return; const int target=selected; juce::PopupMenu menu;
    menu.setLookAndFeel(&skin);
    for(int i=0;i<5;++i) { if(i==1||i==3) menu.addSeparator(); menu.addItem(i+1,tr(channelEN[i],channelRU[i]),true,static_cast<int>(processor.band(selected).placement)==i); }
    juce::Component::SafePointer<EQ1Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&placement).withMinimumWidth(150).withStandardItemHeight(32),[safe,target](int r){if(safe&&safe->selected==target&&r>0) safe->changeBand("placement",static_cast<float>(r-1));});
}
void EQ1Editor::motionMenu() {
    if(selected<0) return; const int target=selected; juce::PopupMenu menu; const auto b=processor.band(selected);
    menu.setLookAndFeel(&skin);
    menu.addItem(1,tr("Off · static EQ","Выкл · статический EQ"),true,b.motion==Motion::Static);
    menu.addItem(2,tr("Dynamic EQ","Динамический EQ"),true,b.motion==Motion::Dynamic);
    menu.addItem(3,tr("Resonance control","Подавление резонансов"),b.shape!=Shape::Tilt,b.motion==Motion::Spectral);
    juce::Component::SafePointer<EQ1Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&motion).withMinimumWidth(248).withStandardItemHeight(34),[safe,target](int r){
        if(safe&&safe->selected==target&&r>0) {safe->changeBand("motion",static_cast<float>(r-1)); safe->expanded=false; safe->refresh();}
    });
}
void EQ1Editor::slopeMenu() {
    if(selected<0) return; const int target=selected; juce::PopupMenu menu;
    menu.setLookAndFeel(&skin);
    for(int i=0;i<8;++i) menu.addItem(i+1,juce::String((i+1)*12)+tr(" dB/oct"," дБ/окт"),true,processor.band(selected).slope==(i+1)*12);
    juce::Component::SafePointer<EQ1Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&slope).withMinimumWidth(160).withStandardItemHeight(30),[safe,target](int r){if(safe&&safe->selected==target&&r>0) safe->changeBand("slope",static_cast<float>(r-1));});
}
void EQ1Editor::listen(bool detector) { if(selected<0) return; const bool was=processor.auditionBand.load()==selected&&processor.auditionDetector.load()==detector; processor.auditionDetector=detector; processor.auditionBand=was?-1:selected; refresh(); }
void EQ1Editor::checkpoint() {
    juce::MemoryBlock state; processor.getStateInformation(state); undoStates.push_back(std::move(state));
    if(undoStates.size()>50) undoStates.erase(undoStates.begin()); redoStates.clear();
}
void EQ1Editor::changeBand(const char* key,float value,bool record) {
    if(selected<0) return;
    if(record) checkpoint(); processor.setValue(EQ1Processor::id(selected,key),value); refresh();
}
void EQ1Editor::refresh() {
    refreshing=true; const auto b=processor.band(std::max(0,selected));
    if(selected>=0&&!b.enabled&&!graph.interacting()) { selected=-1; inspectorOpen=false; panelPlaced=false; processor.auditionBand=-1; }
    const int type=static_cast<int>(b.shape); shape.setButtonText(tr(shapeEN[type],shapeRU[type])); shape.getProperties().set("shape",type);
    placement.setButtonText(tr(channelEN[static_cast<int>(b.placement)],channelRU[static_cast<int>(b.placement)]));
    motion.setButtonText(b.motion==Motion::Static?tr("Dynamics: Off","Динамика: Выкл"):b.motion==Motion::Dynamic?tr("Dynamics","Динамика"):tr("Resonance control","Резонансы"));
    detectorSource.setButtonText(tr("Detector: ","Детектор: ")+(b.external?processor.sidechainConnected.load()?tr("External SC","Внешний SC"):tr("External SC · input off","Внешний SC · вход выкл"):tr("Own signal","Свой сигнал")));
    detectorSource.setTooltip(tr("This signal triggers the band's dynamics. External SC must be routed in the DAW. Range and threshold remain editable even without input. The dotted SC spectrum shows the input before detector filtering.",u8"Этот сигнал управляет динамикой бэнда. Внешний SC подключается в DAW. Диапазон и порог редактируются даже без входящего сигнала. Пунктир SC показывает вход до фильтра детектора."));
    slope.setButtonText(juce::String(b.slope,0)+tr(" dB/oct"," дБ/окт"));
    captions[1].setText(tr(sliderEN[1],sliderRU[1]),juce::dontSendNotification); captions[2].setText("Q",juce::dontSendNotification);
    captions[5].setText(tr("Range","Диапазон"),juce::dontSendNotification);
    const bool spectral=processor.value("engine")>0.5f;
    const auto delay=juce::String(1000.0*processor.getLatencySamples()/processor.analysisSampleRate(),2)+" ms";
    const auto latencyText=utf(u8" · ")+delay;
    mode.changeItemText(1,tr("Live","Живой")+(!spectral?latencyText:juce::String{})); mode.changeItemText(2,tr("Spectral","Спектр")+(spectral?latencyText:juce::String{}));
    mode.setSelectedId(spectral?2:1,juce::dontSendNotification);
    character.setSelectedId(static_cast<int>(processor.value("character"))+1,juce::dontSendNotification);
    for(int i=0;i<17;++i) if((globalSlider(i)||selected>=0)&&!sliders[i]->isMouseButtonDown()&&!static_cast<bool>(sliders[i]->getProperties()["gesture"])&&!static_cast<ScrubSlider*>(sliders[i])->editing()) {
        const auto key=globalSlider(i)?juce::String(sliderKeys[i]):EQ1Processor::id(selected,sliderKeys[i]); sliders[i]->setValue(processor.value(key),juce::dontSendNotification);
    }
    gain.setEnabled(static_cast<int>(b.shape)<=2||b.shape==Shape::Tilt); drive.setEnabled(processor.value("character")>0.5f);
    motion.setEnabled(static_cast<int>(b.shape)<=2||b.shape==Shape::Tilt);
    detectorFrequency.setEnabled(b.freeDetector&&b.motion==Motion::Dynamic); detectorQ.setEnabled(detectorFrequency.isEnabled());
    external.setToggleState(b.external,juce::dontSendNotification); freeDetector.setToggleState(b.freeDetector,juce::dontSendNotification);
    freeze.setToggleState(processor.value("freeze")>0.5f,juce::dontSendNotification); delta.setToggleState(processor.value("delta")>0.5f,juce::dontSendNotification);
    bypass.setToggleState(processor.value("bypass")>0.5f,juce::dontSendNotification); ab.setButtonText(tr("Two versions / now ","Две версии / сейчас ")+(isB?"B":"A"));
    ab.setTooltip(tr("Switch between two snapshots of the EQ. Each version keeps its changes while this editor is open.",u8"Переключить два снимка настроек EQ. Каждая версия помнит изменения, пока редактор открыт."));
    solo.setToggleState(processor.auditionBand.load()==selected&&selected>=0&&!processor.auditionDetector.load(),juce::dontSendNotification);
    detectorSolo.setToggleState(processor.auditionBand.load()==selected&&selected>=0&&processor.auditionDetector.load(),juce::dontSendNotification);
    undoButton.setEnabled(!undoStates.empty()); redoButton.setEnabled(!redoStates.empty()); refreshing=false;
    layoutPanels(); repaint();
}
void EQ1Editor::timerCallback() {
    const bool ru=processor.parameters.state.getProperty("editorLanguage","en").toString()=="ru";
    if(ru!=russian) { russian=ru; translate(); }
    graph.analyze(); refresh();
}

void EQ1Editor::undo() { if(undoStates.empty()) return; juce::MemoryBlock now; processor.getStateInformation(now); redoStates.push_back(now); auto prev=undoStates.back(); undoStates.pop_back(); processor.setStateInformation(prev.getData(),static_cast<int>(prev.getSize())); refresh(); }
void EQ1Editor::redo() { if(redoStates.empty()) return; juce::MemoryBlock now; processor.getStateInformation(now); undoStates.push_back(now); auto next=redoStates.back(); redoStates.pop_back(); processor.setStateInformation(next.getData(),static_cast<int>(next.getSize())); refresh(); }
void EQ1Editor::switchAB() { checkpoint(); if(isB) { processor.getStateInformation(stateB); processor.setStateInformation(stateA.getData(),static_cast<int>(stateA.getSize())); } else { processor.getStateInformation(stateA); processor.setStateInformation(stateB.getData(),static_cast<int>(stateB.getSize())); } isB=!isB; refresh(); }
bool EQ1Editor::keyPressed(const juce::KeyPress& k) {
    if(k.getKeyCode()==juce::KeyPress::escapeKey) { if(inspectorOpen) closeBandPanel(); else if(settingsOpen) {settingsOpen=false; refresh();} else deselectBand(); return true; }
    if(k.getModifiers().isCommandDown()&&k.getKeyCode()=='Z') { if(k.getModifiers().isShiftDown()) redo(); else undo(); return true; }
    if(k.getModifiers().isCommandDown()&&k.getKeyCode()=='C'&&selected>=0) { processor.copyBand(selected); return true; }
    if(k.getModifiers().isCommandDown()&&k.getKeyCode()=='V') { checkpoint(); int i=processor.pasteBand(); if(i>=0) selectBand(i); return true; }
    if(selected>=0&&(k.getKeyCode()==juce::KeyPress::deleteKey||k.getKeyCode()==juce::KeyPress::backspaceKey)) { changeBand("on",0); return true; }
    return false;
}
void EQ1Editor::applyPreset(int i) { checkpoint(); processor.auditionBand=-1; processor.applyPreset(i); selected=-1; inspectorOpen=false; panelPlaced=false; expanded=false; settingsOpen=false; refresh(); }
void EQ1Editor::presetMenu() {
    juce::PopupMenu menu; menu.addItem(1,tr("Flat","Без коррекции")); menu.addItem(2,tr("Vocal shape","Форма вокала")); menu.addItem(3,tr("Dynamic presence","Динамика присутствия")); menu.addItem(4,tr("Spectral soften","Смягчить спектр")); menu.addItem(5,tr("Gentle mastering","Мягкий мастеринг"));
    menu.addSeparator(); menu.addItem(10,tr("Load preset...","Загрузить пресет...")); menu.addItem(11,tr("Save preset...","Сохранить пресет..."));
    juce::Component::SafePointer<EQ1Editor> safe(this); menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&preset),[safe](int result){if(!safe) return; if(result>=1&&result<=5) safe->applyPreset(result-1); if(result==10) safe->loadPreset(); if(result==11) safe->savePreset();});
}
void EQ1Editor::popupBand(int i,bool atPointer) {
    selectBand(i); juce::PopupMenu menu;
    menu.setLookAndFeel(&skin);
    if(!inspectorOpen) {menu.addItem(6,tr("Parameters…","Параметры…")); menu.addSeparator();}
    menu.addItem(4,tr("Listen to band","Слушать бэнд"),true,processor.auditionBand.load()==i&&!processor.auditionDetector.load());
    if(processor.band(i).motion!=Motion::Static) menu.addItem(5,tr("Listen to detector","Слушать детектор"),true,processor.auditionBand.load()==i&&processor.auditionDetector.load());
    menu.addSeparator(); menu.addItem(2,tr("Copy band","Копировать бэнд")); menu.addItem(3,tr("Paste band","Вставить бэнд"));
    menu.addSeparator(); menu.addItem(1,tr("Delete band","Удалить бэнд"));
    juce::Component::SafePointer<EQ1Editor> safe(this);
    auto options=juce::PopupMenu::Options().withTargetComponent(atPointer?static_cast<juce::Component*>(&graph):static_cast<juce::Component*>(&moreBand)).withMinimumWidth(220).withStandardItemHeight(32);
    if(atPointer) options=options.withMousePosition();
    menu.showMenuAsync(options,[safe,i](int result){
        if(!safe||safe->selected!=i) return;
        if(result==6) safe->openBandPanel(i);
        if(result==1) safe->changeBand("on",0); if(result==2) safe->processor.copyBand(i);
        if(result==3) {safe->checkpoint(); int added=safe->processor.pasteBand(); if(added>=0) safe->selectBand(added);}
        if(result==4) safe->listen(false); if(result==5) safe->listen(true);
    });
}
void EQ1Editor::instanceMenu() {
    const auto peers=processor.peers(); juce::PopupMenu menu; menu.addSectionHeader(tr("Copy EQ bands from instance","Копировать полосы из экземпляра"));
    menu.setLookAndFeel(&skin);
    for(size_t i=0;i<peers.size();++i) menu.addItem(static_cast<int>(i+1),peers[i].name+" #"+juce::String(static_cast<int64_t>(peers[i].id))+" ("+juce::String(peers[i].bands)+")");
    juce::Component::SafePointer<EQ1Editor> safe(this); menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settings),[safe,peers](int result){ if(!safe||result<=0||static_cast<size_t>(result)>peers.size()) return; safe->checkpoint(); safe->processor.copyFromPeer(peers[static_cast<size_t>(result-1)].id); safe->refresh(); });
}
void EQ1Editor::savePreset() {
    fileChooser=std::make_unique<juce::FileChooser>(tr("Save EQ-11 preset","Сохранить пресет EQ-11"),juce::File{},"*.eq11preset");
    juce::Component::SafePointer<EQ1Editor> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe](const juce::FileChooser& c){ if(!safe) return; auto file=c.getResult(); if(file==juce::File{}) return; juce::MemoryBlock state; safe->processor.getStateInformation(state); if(!file.withFileExtension("eq11preset").replaceWithData(state.getData(),state.getSize())) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"EQ-11",safe->tr("Could not save preset. Choose a writable folder.",u8"Не удалось сохранить пресет. Выбери доступную для записи папку.")); });
}
void EQ1Editor::loadPreset() {
    fileChooser=std::make_unique<juce::FileChooser>(tr("Load EQ-11 preset","Загрузить пресет EQ-11"),juce::File{},"*.eq11preset;*.eq01preset;*.cspreset");
    juce::Component::SafePointer<EQ1Editor> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& c){ if(!safe) return; auto file=c.getResult(); if(file==juce::File{}) return; juce::MemoryBlock state; if(!file.loadFileAsData(state)||state.getSize()>4*1024*1024) return; auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize())); if(!xml||!xml->hasTagName("CurveShaperState")) {juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"EQ-11",safe->tr("This is not an EQ-11 preset.",u8"Это не пресет EQ-11.")); return;} safe->checkpoint(); safe->processor.setStateInformation(state.getData(),static_cast<int>(state.getSize())); safe->refresh(); });
}
