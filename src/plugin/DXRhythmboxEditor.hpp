#pragma once

#include "plugin/DXRhythmboxProcessor.hpp"
#include "ui/DXRhythmboxPanel.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace dx701::plugin
{

class DXSettingsGearButton final : public juce::ToggleButton
{
public:
    DXSettingsGearButton();

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXSettingsGearButton)
};

class DXSettingsOverlay final : public juce::Component
{
public:
    DXSettingsOverlay(juce::AudioProcessorValueTreeState& apvts,
                      std::function<void()> onCloseRequested);
    ~DXSettingsOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    juce::AudioProcessorValueTreeState& apvts_;
    std::function<void()> closeFn_;
    dx701::ui::DXLookAndFeel laf_;

    juce::Rectangle<int> cardBounds_;
    juce::Label titleLabel_{{}, "PERFORMANCE & MIDI SETTINGS"};
    juce::TextButton closeBtn_{"X"};

    juce::Label midiStartSectionLbl_{{}, "MIDI KEY START (SYNCHRO-START)"};
    juce::ToggleButton midiStartEnableBtn_{"Enable MIDI Key Start (Lower / Pedal Auto-Start)"};
    juce::Label midiStartChLbl_{{}, "Trigger MIDI Ch"};
    juce::ComboBox midiStartChBox_;
    juce::Label midiStartHintLbl_{{}, "When RHYTHM is ON, waits for Note-On on the trigger channel to start Beat 1."};

    juce::Label stopCcSectionLbl_{{}, "EXPRESSION PEDAL RHYTHM STOP SWITCH"};
    juce::ToggleButton stopCcEnableBtn_{"Enable Rhythm Stop Switch (MIDI CC)"};
    juce::Label stopCcNumLbl_{{}, "Stop CC #"};
    juce::Slider stopCcNumSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label stopCcChLbl_{{}, "CC Channel"};
    juce::ComboBox stopCcChBox_;
    juce::Label stopCcHintLbl_{{}, "CC >= 64 stops a running rhythm and inhibits rhythm start while held."};

    // Declared LAST so attachments unlink before widgets are destroyed
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXSettingsOverlay)
};

class DXRhythmboxAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit DXRhythmboxAudioProcessorEditor(DXRhythmboxAudioProcessor& owner);
    ~DXRhythmboxAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Accessors for automated GUI & visual inspection tests
    dx701::ui::DXRhythmboxPanel& rhythmboxPanel() noexcept { return panel_; }
    DXSettingsGearButton& settingsGearButton() noexcept { return settingsGearBtn_; }
    bool isSettingsOpen() const noexcept { return settingsOverlay_.isVisible(); }
    void syncUiNow();

private:
    void timerCallback() override;

    DXRhythmboxAudioProcessor& processor_;
    dx701::ui::DXLookAndFeel presetBarLaf_;

    juce::Label titleLabel_{{}, "DX RHYTHMBOX"};
    juce::Label presetLabel_{{}, "PROGRAM"};
    juce::TextButton prevProgramBtn_{"<"};
    juce::ComboBox programBox_;
    juce::TextButton nextProgramBtn_{">"};
    DXSettingsGearButton settingsGearBtn_;

    juce::Rectangle<int> presetBarBounds_;
    dx701::ui::DXRhythmboxPanel panel_;
    DXSettingsOverlay settingsOverlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXRhythmboxAudioProcessorEditor)
};

} // namespace dx701::plugin
