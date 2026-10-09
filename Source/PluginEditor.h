#pragma once
#include "PluginProcessor.h"
#include "Skin.h"

class EQ1Editor;
class EQDisplay final : public juce::Component, public juce::SettableTooltipClient {
public:
    EQDisplay(EQ1Processor&,EQ1Editor&);
    ~EQDisplay() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void analyze();
    bool interacting() const { return gesture||drawing; }
    int gestureBand() const { return draggedBand; }
    float xFrequency(float) const;
    float yGain(float) const;
    float frequencyX(float) const;
    float gainY(float) const;
    juce::Rectangle<float> plot() const;
    curveshaper::Shape creationShape(juce::Point<float>) const;
    float sidechainLevelAt(float) const;
    bool sketch=false,sidechainView=false;
private:
    int hit(juce::Point<float>) const;
    float stereoGainAt(float) const;
    float creationGain(juce::Point<float>) const;
    void finishSketch();
    EQ1Processor& processor;
    EQ1Editor& owner;
    curveshaper::FFT<2048> fft;
    std::array<std::complex<float>,2048> frame{};
    std::array<float,2048> input{},output{},sideLeft{},sideRight{};
    std::array<float,1025> before{},after{},sidechain{},sidePower{};
    int cursor=0,count=0;
    bool drawing=false,gesture=false;
    bool dragPending=false,gestureCreated=false;
    bool previewVisible=false;
    juce::Point<float> previewPosition,lastCreatedPosition;
    int lastCreated=-1;
    int64_t lastCreatedTime=0;
    int draggedBand=-1;
    std::vector<juce::Point<float>> stroke;
    juce::Point<float> mouseOrigin;
    float originFrequency=1000,originGain=0;
};

class FloatingPanel final : public juce::Component, public juce::SettableTooltipClient {
public:
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    juce::String title,subtitle;
    bool movable=false;
    std::function<void()> onMove;
private:
    juce::Point<int> dragScreen,dragPosition;
    bool dragging=false;
};

class EQ1Editor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit EQ1Editor(EQ1Processor&);
    ~EQ1Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void selectBand(int);
    void openBandPanel(int);
    void closeBandPanel();
    int selectedBand() const { return selected; }
    juce::Rectangle<int> contextBounds() const { return bandPanel.isVisible()?bandPanel.getBounds():juce::Rectangle<int>{}; }
    void checkpoint();
    void changeBand(const char*,float,bool record=true);
    void popupBand(int,bool atPointer=false);
    void refresh();
    void applyPreset(int);
    void setRussian(bool);
    void deselectBand();
    void showSoundSettings();
    juce::String tr(const char*,const char*) const;
    EQDisplay graph;
private:
    void timerCallback() override;
    void translate();
    void undo();
    void redo();
    void switchAB();
    void presetMenu();
    void instanceMenu();
    void loadPreset();
    void savePreset();
    void prepareSlider(juce::Slider&,const char*,bool,const char*,int);
    void layoutPanels();
    void showSettings();
    void listen(bool detector);
    void shapeMenu();
    void placementMenu();
    void motionMenu();
    void slopeMenu();
    void detectorSourceMenu();
    void setShape(int);
    EQ1Processor& processor;
    ShaperSkin skin;
    juce::TooltipWindow tooltips{this,700};
    FloatingPanel bandPanel,settingsPanel;
    juce::TextButton undoButton,redoButton,ab,language,sketch,freeze,delta,bypass,instances,settings,details,remove,solo,detectorSolo,detectorSettings,detectorSource,closeBand,closeSettings,shape,placement,motion,moreBand,slope;
    juce::ComboBox preset,mode,character;
    ScrubSlider frequency,gain,q,outputGain,threshold,above,below,attack,release,ratio,knee,relative,detectorFrequency,detectorQ,drive,width,density;
    juce::TextButton external,freeDetector;
    std::array<juce::Label,17> captions;
    std::array<juce::Slider*,17> sliders;
    bool refreshing=false,russian=false,isB=false,expanded=false,settingsOpen=false,detectorPage=false;
    bool inspectorOpen=false,panelPlaced=false;
    int selected=-1;
    int presetChoice=0;
    juce::MemoryBlock stateA,stateB;
    std::vector<juce::MemoryBlock> undoStates,redoStates;
    std::unique_ptr<juce::FileChooser> fileChooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQ1Editor)
};
