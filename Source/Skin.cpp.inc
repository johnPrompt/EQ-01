// Shared visual vocabulary from NoteShaper 0.5.2.
#include "Skin.h"

namespace {
const juce::Colour background{0xfff8f8fa}, panel{0xffffffff}, line{0xffdadce2};
const juce::Colour text{0xff202127}, muted{0xff626570}, accent{0xff202127};
const juce::Colour controlLine{0xff808591}, rail{0xffedeef2};
constexpr float captionSize = 14, bodySize = 18, valueSize = 20;
constexpr float corner = 6;
juce::Font uiFont(float size, int style = juce::Font::plain) { return juce::Font("Segoe UI", size, style); }
const juce::StringArray names{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
void label(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> bounds,
           float size = 14, juce::Colour colour = muted, int alignment = juce::Justification::left) {
    g.setColour(colour); g.setFont(uiFont(size)); g.drawText(value, bounds, alignment);
}
void identify(juce::Component& c, const juce::String& id, const juce::String& title, int order) {
    c.setComponentID(id); c.setTitle(title); c.setExplicitFocusOrder(order);
    c.setWantsKeyboardFocus(true);
}
juce::String tr(const char* value) { return juce::String::fromUTF8(value); }
}

namespace {
class ScrubLabel final : public juce::Label {
public:
    explicit ScrubLabel(ScrubSlider& s):slider(s) {
        setEditable(false,true,false); setJustificationType(juce::Justification::left);
        setBorderSize(juce::BorderSize<int>(5)); setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }
    void mouseDown(const juce::MouseEvent& e) override {slider.mouseDown(e.getEventRelativeTo(&slider));}
    void mouseDrag(const juce::MouseEvent& e) override {slider.mouseDrag(e.getEventRelativeTo(&slider));}
    void mouseUp(const juce::MouseEvent& e) override {slider.mouseUp(e.getEventRelativeTo(&slider));}
    void mouseDoubleClick(const juce::MouseEvent& e) override {slider.mouseDoubleClick(e.getEventRelativeTo(&slider));}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w) override {slider.mouseWheelMove(e.getEventRelativeTo(&slider),w);}
private:
    ScrubSlider& slider;
};
}
void ScrubSlider::finishScrub() {pending=false; gesture.reset(); repaint();}
bool ScrubSlider::editing() const {
    for(int i=0;i<getNumChildComponents();++i)
        if(const auto* field=dynamic_cast<const juce::Label*>(getChildComponent(i)); field&&field->isBeingEdited()) return true;
    return false;
}
void ScrubSlider::mouseDown(const juce::MouseEvent& e) {
    if(!isEnabled()||e.mods.isPopupMenu()||e.getNumberOfClicks()>1) return;
    finishScrub(); hideTextBox(false); grabKeyboardFocus();
    origin=lastPosition=e.position; initialValue=getValue(); pending=true;
}
void ScrubSlider::mouseDrag(const juce::MouseEvent& e) {
    if(!pending||!isEnabled()) return;
    if(!gesture) {
        const auto offset=e.position-origin; if(offset.getDistanceFromOrigin()<3) return;
        horizontal=std::abs(offset.x)>=std::abs(offset.y);
        gesture=std::make_unique<juce::Slider::ScopedDragNotification>(*this);
    }
    const auto delta=e.position-lastPosition; lastPosition=e.position;
    const double movement=horizontal?delta.x:-delta.y;
    const double fine=e.mods.isShiftDown()?0.12:1.0;
    setValue(proportionOfLengthToValue(std::clamp(valueToProportionOfLength(getValue())+movement*fine/240.0,0.0,1.0)),juce::sendNotificationSync);
    repaint();
}
void ScrubSlider::mouseUp(const juce::MouseEvent&) {finishScrub();}
void ScrubSlider::mouseDoubleClick(const juce::MouseEvent&) {if(isEnabled()) {finishScrub(); showTextBox();}}
void ScrubSlider::nudge(double amount) {
    if(!isEnabled()) return;
    juce::Slider::ScopedDragNotification change(*this);
    setValue(proportionOfLengthToValue(std::clamp(valueToProportionOfLength(getValue())+amount,0.0,1.0)),juce::sendNotificationSync);
}
void ScrubSlider::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w) {
    if(!e.mods.isAnyMouseButtonDown()&&(w.deltaY!=0||w.deltaX!=0)) nudge((w.deltaY!=0?w.deltaY:w.deltaX)*(e.mods.isShiftDown()?0.02:0.16));
}
bool ScrubSlider::keyPressed(const juce::KeyPress& key) {
    const int code=key.getKeyCode();
    if(code==juce::KeyPress::escapeKey&&gesture) {setValue(initialValue,juce::sendNotificationSync); finishScrub(); return true;}
    if(code==juce::KeyPress::returnKey) {if(isEnabled()) showTextBox(); return true;}
    if(code==juce::KeyPress::upKey||code==juce::KeyPress::rightKey||code==juce::KeyPress::downKey||code==juce::KeyPress::leftKey) {
        nudge((code==juce::KeyPress::upKey||code==juce::KeyPress::rightKey?1:-1)*(key.getModifiers().isShiftDown()?0.001:0.01)); return true;
    }
    return juce::Slider::keyPressed(key);
}

ShaperSkin::ShaperSkin() {
    setDefaultSansSerifTypefaceName("Segoe UI");
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, controlLine);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, rail);
    setColour(juce::PopupMenu::highlightedTextColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.3f));
    setColour(juce::TextEditor::focusedOutlineColourId, accent);
    setColour(juce::TextEditor::backgroundColourId, panel);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::CaretComponent::caretColourId, text);
    setColour(juce::TooltipWindow::backgroundColourId, panel);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, line);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, text);
}
juce::Path ShaperSkin::filterPath(int shape,juce::Rectangle<float> r) {
    juce::Path p; curveshaper::Band b; b.enabled=true; b.shape=static_cast<curveshaper::Shape>(shape); b.frequency=1000; b.gain=10; b.q=1;
    for(int i=0;i<=40;++i) {
        const float x=r.getX()+r.getWidth()*i/40, f=40*std::pow(400.f,i/40.f);
        float db=static_cast<float>(20*std::log10(std::max(1e-9,curveshaper::magnitude(b,f,48000))));
        if(shape==7) db=8*std::tanh(std::log2(f/1000));
        const float y=r.getCentreY()-std::clamp(db,-18.f,18.f)*r.getHeight()/40;
        if(i==0) p.startNewSubPath(x,y); else p.lineTo(x,y);
    }
    return p;
}

void ShaperSkin::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                         const juce::Colour&, bool hover, bool down) {
    const bool selected = b.getToggleState();
    auto bounds = b.getLocalBounds().toFloat().reduced(0.7f);
    const auto id = b.getComponentID();
    const auto icon=b.getProperties()["icon"].toString();
    const bool toggle = id == "natural";
    if (id == "language") {
        if (hover || down) { g.setColour(rail); g.fillRoundedRectangle(bounds, corner); }
    } else if (static_cast<bool>(b.getProperties()["flat"])||(icon.isNotEmpty()&&icon!="headphones")) {
        if(hover||down||selected) { g.setColour(rail); g.fillRoundedRectangle(bounds,corner); }
    } else if (toggle) {
        if (hover || down) { g.setColour(rail); g.fillRoundedRectangle(bounds, corner); }
        const float y = b.getHeight() * 0.5f;
        const juce::Rectangle<float> check(1, y - 10, 20, 20);
        g.setColour(selected ? accent : panel); g.fillRoundedRectangle(check, 4);
        g.setColour(selected ? accent : controlLine); g.drawRoundedRectangle(check, 4, 1);
        if (selected) {
            juce::Path mark; mark.startNewSubPath(6, y); mark.lineTo(10, y + 4); mark.lineTo(17, y - 4);
            g.setColour(panel); g.strokePath(mark, juce::PathStrokeType(1.8f));
        }
    } else {
        const bool noteKey = id.startsWith("note");
        auto fill = selected ? accent : (noteKey && static_cast<bool>(b.getProperties()["accidental"]) ? rail : panel);
        if (hover || down) fill = selected ? fill.brighter(0.1f) : rail;
        g.setColour(fill.withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle(bounds, corner);
        g.setColour(selected ? accent : controlLine.withAlpha(0.6f));
        g.drawRoundedRectangle(bounds, corner, 0.8f);
        if (noteKey && !selected && static_cast<bool>(b.getProperties()["targetMember"])) {
            g.setColour(controlLine.withAlpha(b.isEnabled() ? 1.0f : 0.4f));
            g.fillRoundedRectangle(bounds.getCentreX() - 7, bounds.getBottom() - 7, 14, 2, 1);
        }
    }
    if (b.hasKeyboardFocus(true)) {
        g.setColour(accent); g.drawRoundedRectangle(bounds.reduced(1), corner, 2);
    }
    if(selected&&static_cast<bool>(b.getProperties()["section"])) {g.setColour(text); g.drawHorizontalLine(b.getHeight()-2,6.f,b.getWidth()-6.f);}
}
void ShaperSkin::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool hover, bool down) {
    const auto id = b.getComponentID();
    if (id == "language") {
        g.setFont(uiFont(captionSize));
        g.setColour(hover || down || b.hasKeyboardFocus(true) ? text : muted);
        g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
        return;
    }
    const auto icon=b.getProperties()["icon"].toString();
    const bool menu=static_cast<bool>(b.getProperties()["menu"]);
    if(menu) {
        const float cx=b.getWidth()-10.f,cy=b.getHeight()*0.5f,sign=static_cast<bool>(b.getProperties()["expanded"])?-1.f:1.f;
        juce::Path chevron; chevron.startNewSubPath(cx-3,cy-1*sign); chevron.lineTo(cx,cy+2*sign); chevron.lineTo(cx+3,cy-1*sign);
        g.setColour(muted); g.strokePath(chevron,juce::PathStrokeType(1.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
    if(icon=="filter") {
        g.setColour(text); g.strokePath(filterPath(static_cast<int>(b.getProperties()["shape"]),{4,4,30.f,static_cast<float>(b.getHeight()-8)}),juce::PathStrokeType(1.7f));
        g.setFont(uiFont(16)); g.drawFittedText(b.getButtonText(),b.getLocalBounds().withTrimmedLeft(42).withTrimmedRight(18),juce::Justification::left,1);
        return;
    }
    if(icon.isNotEmpty()) {
        g.setColour((icon=="headphones"&&b.getToggleState()?panel:text).withAlpha(b.isEnabled()?1.f:0.28f));
        const float cx=icon=="headphones"&&b.getButtonText().isNotEmpty()?18.f:b.getWidth()*0.5f,cy=b.getHeight()*0.5f;
        juce::Path p;
        if(icon=="undo"||icon=="redo") {
            // Familiar bent arrows: a half-circle, with the arrow head at its leading end.
            p.startNewSubPath(cx-8,cy-3); p.cubicTo(cx-1,cy-10,cx+10,cy-5,cx+8,cy+4);
            p.startNewSubPath(cx-8,cy-3); p.lineTo(cx-8,cy-9);
            p.startNewSubPath(cx-8,cy-3); p.lineTo(cx-2,cy-3);
            if(icon=="redo") p.applyTransform(juce::AffineTransform::scale(-1,1).translated(2*cx,0));
        } else if(icon=="close") {
            p.startNewSubPath(cx-4,cy-4); p.lineTo(cx+4,cy+4);
            p.startNewSubPath(cx+4,cy-4); p.lineTo(cx-4,cy+4);
        } else if(icon=="gear") {
            for(int k=0;k<32;++k) { const float a=juce::MathConstants<float>::twoPi*k/32, r=(k%4==1||k%4==2)?9.f:7.f; const auto pt=juce::Point<float>(cx+std::sin(a)*r,cy-std::cos(a)*r); if(k==0) p.startNewSubPath(pt); else p.lineTo(pt); }
            p.closeSubPath(); p.addEllipse(cx-3,cy-3,6,6);
        } else if(icon=="headphones") {
            p.startNewSubPath(cx-7,cy); p.cubicTo(cx-7,cy-10,cx+7,cy-10,cx+7,cy);
            p.addRoundedRectangle(cx-8,cy-1,4,7,1); p.addRoundedRectangle(cx+4,cy-1,4,7,1);
        } else if(icon=="more") {
            for(int j=-1;j<=1;++j) p.addEllipse(cx+j*5-0.9f,cy-0.9f,1.8f,1.8f);
        }
        g.strokePath(p,juce::PathStrokeType(1.7f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        if(icon!="headphones"||b.getButtonText().isEmpty()) return;
        g.setFont(uiFont(14)); g.drawFittedText(b.getButtonText(),b.getLocalBounds().withTrimmedLeft(31).withTrimmedRight(5),juce::Justification::centred,1);
        return;
    }
    const bool toggle = id == "natural";
    const bool selected = b.getToggleState();
    g.setFont(uiFont(static_cast<bool>(b.getProperties()["compact"])?(id=="bandChannel"?14:16):bodySize));
    const bool flat=static_cast<bool>(b.getProperties()["flat"]);
    g.setColour((selected && !toggle && !flat ? panel : static_cast<bool>(b.getProperties()["secondary"])?muted:text).withAlpha(b.isEnabled() ? 1.0f : 0.45f));
    auto bounds = b.getLocalBounds().reduced(6);
    if(menu) { bounds=bounds.withTrimmedRight(14); g.drawFittedText(b.getButtonText(),bounds,juce::Justification::left,1); return; }
    if (toggle) { bounds.setLeft(32); g.drawFittedText(b.getButtonText(), bounds, juce::Justification::left, 1); }
    else g.drawFittedText(b.getButtonText(), bounds, juce::Justification::centred, 1);
}
void ShaperSkin::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                     float value, float start, float end, juce::Slider& s) {
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float r = juce::jmin(w, h) * 0.5f - 9;
    const float angle = start + value * (end - start);
    juce::Path track; track.addCentredArc(cx, cy, r, r, 0, start, end, true);
    const juce::PathStrokeType stroke(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(line); g.strokePath(track, stroke);
    if (value > 0) {
        juce::Path active; active.addCentredArc(cx, cy, r, r, 0, start, angle, true);
        g.setColour(text); g.strokePath(active, stroke);
    }
    const float body = r * 0.76f;
    g.setColour(background);
    g.fillEllipse(cx - body, cy - body, body * 2, body * 2);
    juce::Path pointer;
    pointer.startNewSubPath(cx + std::sin(angle) * r * 0.30f, cy - std::cos(angle) * r * 0.30f);
    pointer.lineTo(cx + std::sin(angle) * r * 0.64f, cy - std::cos(angle) * r * 0.64f);
    g.setColour(text); g.strokePath(pointer, juce::PathStrokeType(3, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (s.hasKeyboardFocus(true)) {
        g.setColour(accent); g.drawEllipse(cx - r - 7, cy - r - 7, r * 2 + 14, r * 2 + 14, 1.2f);
    }
}
void ShaperSkin::drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float position,float,float,const juce::Slider::SliderStyle,juce::Slider& s) {
    if(static_cast<bool>(s.getProperties()["numeric"])) return;
    const float cy=y+h*0.5f;
    g.setColour(line.withAlpha(s.isEnabled()?1.f:0.5f));
    g.fillRoundedRectangle(static_cast<float>(x),cy-2,static_cast<float>(w),4,2);
    if(s.isEnabled()) {
        g.setColour(text); g.fillRoundedRectangle(static_cast<float>(x),cy-2,std::max(0.f,position-x),4,2);
        g.setColour(panel); g.fillEllipse(position-6,cy-6,12,12);
        g.setColour(text); g.drawEllipse(position-6,cy-6,12,12,2);
    } else { g.setColour(controlLine.withAlpha(0.4f)); g.fillEllipse(position-4,cy-4,8,8); }
    if(s.hasKeyboardFocus(true)) { g.setColour(text); g.drawRoundedRectangle(s.getLocalBounds().toFloat().reduced(1),6,1); }
}
juce::Slider::SliderLayout ShaperSkin::getSliderLayout(juce::Slider& s) {
    if(static_cast<bool>(s.getProperties()["numeric"])) { juce::Slider::SliderLayout layout; layout.textBoxBounds=s.getLocalBounds(); return layout; }
    return juce::LookAndFeel_V4::getSliderLayout(s);
}
juce::Label* ShaperSkin::createSliderTextBox(juce::Slider& slider) {
    auto* box = dynamic_cast<ScrubSlider*>(&slider)?static_cast<juce::Label*>(new ScrubLabel(*static_cast<ScrubSlider*>(&slider))):juce::LookAndFeel_V4::createSliderTextBox(slider);
    box->setColour(juce::Label::textColourId,text); box->setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
    box->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return box;
}
juce::Font ShaperSkin::getLabelFont(juce::Label& l) {
    if(auto* s=dynamic_cast<juce::Slider*>(l.getParentComponent())) return uiFont(s->getProperties().getWithDefault("valueFont",static_cast<bool>(s->getProperties()["numeric"])?16:valueSize));
    return l.getFont();
}
void ShaperSkin::drawLabel(juce::Graphics& g, juce::Label& l) {
    if (dynamic_cast<juce::Slider*>(l.getParentComponent())) {
        if(static_cast<bool>(l.getParentComponent()->getProperties()["numeric"])&&!static_cast<bool>(l.getParentComponent()->getProperties()["quiet"])) {
            g.setColour(background); g.fillRoundedRectangle(l.getLocalBounds().toFloat().reduced(0.5f),4);
            g.setColour(line); g.drawRoundedRectangle(l.getLocalBounds().toFloat().reduced(0.5f),4,0.8f);
        }
        if (!l.isBeingEdited()) {
            auto* field=dynamic_cast<ScrubSlider*>(l.getParentComponent());
            if (l.isMouseOverOrDragging()||(field&&field->scrubbing())) {
                g.setColour(rail); g.fillRoundedRectangle(l.getLocalBounds().toFloat().reduced(1), corner);
                g.setColour(controlLine.withAlpha(0.6f)); g.drawHorizontalLine(l.getHeight()-2,5.f,l.getWidth()-5.f);
            }
            g.setColour(text.withAlpha(l.getParentComponent()->isEnabled()?1.f:0.4f)); g.setFont(getLabelFont(l));
            g.drawFittedText(l.getText(), l.getLocalBounds().reduced(5, 1),static_cast<bool>(l.getParentComponent()->getProperties()["quiet"])?juce::Justification::left:juce::Justification::centred, 1);
        }
        if (l.hasKeyboardFocus(true)) {
            g.setColour(accent); g.drawRoundedRectangle(l.getLocalBounds().toFloat().reduced(1), 6, 1);
        }
    } else juce::LookAndFeel_V4::drawLabel(g, l);
}
void ShaperSkin::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                 int, int, int, int, juce::ComboBox& box) {
    auto bounds = juce::Rectangle<float>(0.7f, 0.7f, w - 1.4f, h - 1.4f);
    g.setColour((box.isMouseOverOrDragging() ? rail : panel).withAlpha(box.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(box.hasKeyboardFocus(true) ? accent : controlLine.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds, corner, box.hasKeyboardFocus(true) ? 2 : 0.8f);
    juce::Path arrow; arrow.startNewSubPath(w - 24.0f, h * 0.5f - 2);
    arrow.lineTo(w - 19.0f, h * 0.5f + 3); arrow.lineTo(w - 14.0f, h * 0.5f - 2);
    g.setColour(muted); g.strokePath(arrow, juce::PathStrokeType(1.6f));
}
juce::Font ShaperSkin::getComboBoxFont(juce::ComboBox& b) { return uiFont(static_cast<bool>(b.getProperties()["compact"])?14:bodySize); }
juce::Font ShaperSkin::getPopupMenuFont() { return uiFont(16); }
void ShaperSkin::drawPopupMenuBackground(juce::Graphics& g,int w,int h) {
    g.fillAll(panel); g.setColour(line); g.drawRect(0,0,w,h,1);
}
void ShaperSkin::drawPopupMenuSectionHeader(juce::Graphics& g,const juce::Rectangle<int>& area,const juce::String& name) {
    label(g,name,area.reduced(14,0),13,muted);
}
void ShaperSkin::drawPopupMenuItem(juce::Graphics& g,const juce::Rectangle<int>& area,bool separator,bool active,bool highlighted,bool ticked,bool submenu,const juce::String& name,const juce::String& shortcut,const juce::Drawable* icon,const juce::Colour*) {
    if(separator) {g.setColour(line); g.drawHorizontalLine(area.getCentreY(),14.f,area.getRight()-14.f); return;}
    if(highlighted&&active) {g.setColour(rail); g.fillRoundedRectangle(area.toFloat().reduced(3,1),4);}
    g.setColour(text.withAlpha(active?1.f:0.4f)); auto r=area.reduced(14,0);
    if(icon) {icon->drawWithin(g,r.removeFromLeft(30).toFloat(),juce::RectanglePlacement::centred|juce::RectanglePlacement::onlyReduceInSize,active?1.f:0.4f); r.removeFromLeft(12);}
    const float cx=area.getRight()-16.f,cy=area.getCentreY(); juce::Path mark;
    if(submenu) {mark.startNewSubPath(cx-2,cy-4); mark.lineTo(cx+2,cy); mark.lineTo(cx-2,cy+4); r.removeFromRight(16);}
    else if(ticked) {mark.startNewSubPath(cx-6,cy); mark.lineTo(cx-2,cy+3); mark.lineTo(cx+5,cy-4); r.removeFromRight(18);}
    g.strokePath(mark,juce::PathStrokeType(1.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setFont(getPopupMenuFont());
    if(shortcut.isNotEmpty()) {auto key=r.removeFromRight(80); g.drawText(shortcut,key,juce::Justification::right);}
    g.drawFittedText(name,r,juce::Justification::left,1);
}
void ShaperSkin::positionComboBoxText(juce::ComboBox& box, juce::Label& l) {
    l.setBounds(12, 0, box.getWidth() - 44, box.getHeight());
    l.setBorderSize(juce::BorderSize<int>(0)); l.setFont(getComboBoxFont(box));
}
void ShaperSkin::drawComboBoxTextWhenNothingSelected(juce::Graphics& g, juce::ComboBox& box, juce::Label& l) {
    g.setColour(text); g.setFont(getComboBoxFont(box));
    g.drawText(box.getTextWhenNothingSelected(), l.getBounds(), juce::Justification::left);
}

