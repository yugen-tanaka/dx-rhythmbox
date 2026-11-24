// 1982 KAWAI DX701 hardware-styled JUCE 8 UI panel for DX Rhythmbox.
#pragma once

#include "dsp/DXRhythmboxEngine.hpp"
#include "plugin/DXRhythmboxPluginParams.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace dx701::ui
{

class DXLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DXLookAndFeel();

    int getSliderThumbRadius(juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
};

class DXConcavePushButton final : public juce::ToggleButton
{
public:
    enum class CapColor
    {
        Ivory,
        Espresso,
        Red,
        Charcoal
    };

    using PressStateCallback = std::function<void(bool isDown)>;

    DXConcavePushButton(CapColor capColor,
                        bool momentary = false,
                        PressStateCallback onPressStateChanged = {});

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    bool hitTest(int x, int y) override;

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    juce::Rectangle<float> computeTopSquareRect(bool latched, bool heldDown) const noexcept;
    void computeExtrusionMetrics(float& outSq, float& outZOff, float& outZOn, float& outHoleTop) const noexcept;
    CapColor capColor_ = CapColor::Ivory;
    bool momentary_ = false;
    PressStateCallback pressStateFn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXConcavePushButton)
};

class DXTransportButton final : public juce::Button
{
public:
    enum class IconType
    {
        Play,
        Stop
    };

    using HoldCallback = std::function<void(bool isDown)>;

    DXTransportButton(const juce::String& name,
                      IconType icon,
                      HoldCallback onHoldChanged = {});

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void setHoldCallback(HoldCallback cb) { holdFn_ = std::move(cb); }
    void triggerHoldForTest(bool isDown)
    {
        forcedHold_ = isDown;
        if (holdFn_)
        {
            holdFn_(isDown);
        }
        repaint();
    }

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    IconType icon_ = IconType::Play;
    bool forcedHold_ = false;
    HoldCallback holdFn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXTransportButton)
};

class DXInvertedVerticalSlider final : public juce::Slider
{
public:
    DXInvertedVerticalSlider()
        : juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox)
    {
    }

    double proportionOfLengthToValue(double proportion) override
    {
        return juce::Slider::proportionOfLengthToValue(1.0 - proportion);
    }

    double valueToProportionOfLength(double value) override
    {
        return 1.0 - juce::Slider::valueToProportionOfLength(value);
    }
};

class DXFillInButton final : public juce::Button
{
public:
    enum class NoteSymbol
    {
        TripletSixteenth, // Slot 0 (leftmost): FILL_IN_4
        TripletEighth,    // Slot 1: FILL_IN_3
        DoubleSixteenth,  // Slot 2: FILL_IN_2
        DoubleEighth      // Slot 3 (rightmost): FILL_IN_1
    };

    DXFillInButton(const juce::String& name, NoteSymbol symbol);

    void setIlluminated(bool lit) noexcept;
    bool isIlluminated() const noexcept { return illuminated_; }
    void triggerClickPulse() noexcept { clickPulseFrames_ = 6; repaint(); }
    void stepPulse() noexcept;

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    void drawCobaltNoteSymbol(juce::Graphics& g, juce::Rectangle<float> area) const;

    NoteSymbol symbol_;
    bool illuminated_ = false;
    int clickPulseFrames_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXFillInButton)
};

class DXAuditionPad final : public juce::TextButton
{
public:
    DXAuditionPad(const juce::String& voiceLabel, int gmMidiNote);

    void triggerFlash() noexcept { flashLevel_ = 1.0f; repaint(); }
    bool decayFlash() noexcept;

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    juce::String voiceLabel_;
    int gmMidiNote_ = 36;
    float flashLevel_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXAuditionPad)
};

class DXRhythmboxPanel final : public juce::Component,
                               private juce::Timer
{
public:
    using AuditionCallback = std::function<void(dx701::dsp::VoiceId, float)>;
    using FillInCallback = std::function<void(dx701::dsp::FillInId)>;
    using HoldCallback = std::function<void(bool held)>;
    using TelemetryCallback = std::function<dx701::dsp::DXRhythmboxTelemetry()>;
    using MidiFlashConsumeCallback = std::function<std::uint32_t()>;
    struct TransportStatusFlags
    {
        bool latched = false;
        bool stopSwitchHeld = false;
        bool midiArmed = false;
    };
    using StatusFlagsCallback = std::function<TransportStatusFlags()>;

    DXRhythmboxPanel(juce::AudioProcessorValueTreeState& apvts,
                     AuditionCallback onAuditionVoice,
                     FillInCallback onTriggerFillIn,
                     HoldCallback onStandaloneStartHeld,
                     HoldCallback onStandaloneStopHeld,
                     TelemetryCallback getTelemetry,
                     MidiFlashConsumeCallback consumeMidiFlash = {},
                     StatusFlagsCallback getStatusFlags = {},
                     bool isStandalone = false);
    ~DXRhythmboxPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Synchronous telemetry refresh helper for headless visual tests
    void refreshTelemetryNow();
    void setStandaloneVisible(bool standalone);

    // Public accessors for automated GUI tests
    DXConcavePushButton& rhythmButton() noexcept { return *rhythmBtn_; }
    DXConcavePushButton& variationButton() noexcept { return *variationBtn_; }
    DXConcavePushButton& breakButton() noexcept { return *breakBtn_; }
    DXTransportButton& standaloneStartButton() noexcept { return *standaloneStartBtn_; }
    DXTransportButton& standaloneStopButton() noexcept { return *standaloneStopBtn_; }
    juce::ToggleButton& multiPatternButton() noexcept { return multiPatternBtn_; }
    DXConcavePushButton& patternButton(std::size_t index) noexcept { return *patternButtons_[index]; }
    DXFillInButton& fillInButton(std::size_t uiSlot) noexcept { return *fillInButtons_[uiSlot]; }
    DXAuditionPad& auditionPad(std::size_t index) noexcept { return *padButtons_[index]; }
    juce::Slider& toneSlider() noexcept { return toneSlider_; }
    juce::Slider& volumeSlider() noexcept { return volumeSlider_; }
    juce::Slider& bpmSlider() noexcept { return bpmSlider_; }
    juce::TextEditor& bpmInput() noexcept { return bpmInput_; }
    void handlePatternPressState(std::size_t idx, bool isPressed);

private:
    void timerCallback() override;
    void syncBpmInputText(bool force = false);
    void commitBpmInput();
    void rebuildWoodgrainCache();
    void drawTempoJewelLamp(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void drawVariationGuide(juce::Graphics& g,
                            bool varActive) const;
    void drawRow0Labels(juce::Graphics& g, std::size_t col, juce::Rectangle<int> area) const;
    void drawRow1Labels(juce::Graphics& g, std::size_t col, juce::Rectangle<int> area) const;
    void handlePatternButtonClick(std::size_t clickedIndex);
    void handleMultiPatternToggle();
    void applyPatternMaskToHost(std::uint16_t mask);
    void setParameterBoolNotifyingHost(const char* paramId, bool value);

    juce::AudioProcessorValueTreeState& apvts_;
    AuditionCallback auditionFn_;
    FillInCallback fillInFn_;
    HoldCallback standaloneStartFn_;
    HoldCallback standaloneStopFn_;
    TelemetryCallback telemetryFn_;
    MidiFlashConsumeCallback consumeMidiFlashFn_;
    StatusFlagsCallback statusFlagsFn_;
    DXLookAndFeel lookAndFeel_;

    // Upper left: 2x8 rhythm pattern buttons
    std::array<std::unique_ptr<DXConcavePushButton>, dx701::dsp::kNumButtons> patternButtons_;

    // Upper right: BREAK (left) + RHYTHMER (VARIATION, RHYTHM) + Standalone cream Play / Stop (right)
    std::unique_ptr<DXConcavePushButton> breakBtn_;
    std::unique_ptr<DXConcavePushButton> variationBtn_;
    std::unique_ptr<DXConcavePushButton> rhythmBtn_;
    std::unique_ptr<DXTransportButton> standaloneStartBtn_;
    std::unique_ptr<DXTransportButton> standaloneStopBtn_;

    // Middle left: 3 vertical faders (TONE normal; VOLUME & TEMPO inverted MAX-at-bottom) + compact direct BPM field
    juce::Slider toneSlider_{juce::Slider::LinearVertical, juce::Slider::NoTextBox};
    DXInvertedVerticalSlider volumeSlider_;
    DXInvertedVerticalSlider bpmSlider_;
    juce::TextEditor bpmInput_;

    // Middle right: 4 self-illuminating Drum Fill In buttons (FILL_IN_4..FILL_IN_1 left-to-right)
    std::array<std::unique_ptr<DXFillInButton>, dx701::dsp::kNumFillIns> fillInButtons_;

    // Utility strip controls
    juce::ToggleButton multiPatternBtn_{"Multi-Pattern"};
    juce::Label syncLbl_{{}, "SYNC"};
    juce::ComboBox syncModeBox_;
    juce::Label midiChLbl_{{}, "MIDI"};
    juce::ComboBox midiChBox_;
    juce::Label driftLbl_{{}, "DRIFT"};
    juce::Slider driftSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};

    // Bottom deck: 16 GM voice audition pads
    std::array<std::unique_ptr<DXAuditionPad>, dx701::dsp::kNumVoices> padButtons_;

    // Cached layout frames for paint()
    juce::Image woodgrainBg_;
    juce::Rectangle<int> upperDeckBounds_;
    juce::Rectangle<int> matrixFrameBounds_;
    juce::Rectangle<int> row0OuterRowBounds_;
    juce::Rectangle<int> row0InnerRowBounds_;
    juce::Rectangle<int> wellBounds_;
    juce::Rectangle<int> row1InnerRowBounds_;
    juce::Rectangle<int> row1OuterRowBounds_;
    std::array<juce::Rectangle<int>, 8> colLabelSlices_{};

    juce::Rectangle<int> rightUpperBounds_;
    juce::Rectangle<int> variationGuideBounds_;
    juce::Rectangle<int> tempoLampBounds_;
    juce::Rectangle<int> rhythmerBracketBounds_;
    juce::Rectangle<int> rhythmerWellBounds_;
    juce::Rectangle<int> breakWellBounds_;
    juce::Rectangle<int> standaloneTransportBezelBounds_;
    std::array<juce::Rectangle<int>, 3> actionLabelBounds_{};

    juce::Rectangle<int> middleDeckBounds_;
    juce::Rectangle<int> sliderBayBounds_;
    std::array<juce::Rectangle<int>, 3> sliderHeaderBounds_{};

    juce::Rectangle<int> fillInConsoleBounds_;
    juce::Rectangle<int> fillInBezelBounds_;
    juce::Rectangle<int> fillInCaptionBounds_;

    juce::Rectangle<int> utilityBarBounds_;
    juce::Rectangle<int> statusReadoutBounds_;
    juce::Rectangle<int> meterBounds_;

    juce::Rectangle<int> auditionBayBounds_;
    juce::Rectangle<int> auditionBracketBounds_;

    dx701::dsp::DXRhythmboxTelemetry telemetry_{};
    TransportStatusFlags statusFlags_{};
    float meterSmooth_ = 0.0f;
    float meterDb_ = -96.0f;
    bool variationActive_ = false;
    bool isStandalone_ = false;
    bool updatingPatterns_ = false;
    std::uint16_t heldPatternMask_ = 0u;
    std::uint16_t chordPatternMask_ = 0u;
    bool chordGestureActive_ = false;
    int activeButtonCount_ = 1;

    // Declared LAST so attachments unlink before widgets are destroyed
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXRhythmboxPanel)
};

} // namespace dx701::ui
