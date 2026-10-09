#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
class ScrubSlider final : public juce::Slider {
public:
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool scrubbing() const { return gesture!=nullptr; }
    bool editing() const;
    void finishScrub();
private:
    void nudge(double);
    std::unique_ptr<juce::Slider::ScopedDragNotification> gesture;
    juce::Point<float> origin,lastPosition;
    double initialValue=0;
    bool pending=false,horizontal=false;
};
class ShaperSkin final : public juce::LookAndFeel_V4 {
public:
    ShaperSkin();
    static juce::Path filterPath(int,juce::Rectangle<float>);
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, const juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&,int,int) override;
    void drawPopupMenuItem(juce::Graphics&,const juce::Rectangle<int>&,bool,bool,bool,bool,bool,const juce::String&,const juce::String&,const juce::Drawable*,const juce::Colour*) override;
    void drawPopupMenuSectionHeader(juce::Graphics&,const juce::Rectangle<int>&,const juce::String&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawComboBoxTextWhenNothingSelected(juce::Graphics&, juce::ComboBox&, juce::Label&) override;
};

