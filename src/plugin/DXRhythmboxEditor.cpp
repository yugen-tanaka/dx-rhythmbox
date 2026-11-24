#include "plugin/DXRhythmboxEditor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace dx701::plugin
{

namespace
{

juce::Path makeGearPath(float cx, float cy, float outerR, float innerR, float holeR, int numTeeth)
{
    juce::Path p;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const int steps = numTeeth * 4;
    for (int i = 0; i < steps; ++i)
    {
        const float angle = (static_cast<float>(i) / static_cast<float>(steps)) * twoPi;
        const int phase = i % 4;
        const float rad = (phase == 0 || phase == 1) ? outerR : innerR;
        const float x = cx + std::cos(angle) * rad;
        const float y = cy + std::sin(angle) * rad;
        if (i == 0)
        {
            p.startNewSubPath(x, y);
        }
        else
        {
            p.lineTo(x, y);
        }
    }
    p.closeSubPath();
    p.setUsingNonZeroWinding(false);
    p.addEllipse(cx - holeR, cy - holeR, holeR * 2.0f, holeR * 2.0f);
    return p;
}

} // namespace

//==============================================================================
// DXSettingsGearButton
//==============================================================================
DXSettingsGearButton::DXSettingsGearButton()
{
    setClickingTogglesState(true);
    setTooltip("Performance & MIDI Settings (MIDI Key Start & Rhythm Stop CC)");
}

void DXSettingsGearButton::paintButton(juce::Graphics& g,
                                       bool shouldDrawButtonAsHighlighted,
                                       bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat().reduced(1.0f);
    const bool open = getToggleState();
    const juce::Colour accent(0xffffb84d);

    juce::Colour bg = open ? accent.withAlpha(0.24f) : juce::Colour(0xff2a1e16);
    if (shouldDrawButtonAsDown)
    {
        bg = bg.darker(0.18f);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        bg = bg.brighter(0.14f);
    }
    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f);

    g.setColour(open ? accent : juce::Colour(shouldDrawButtonAsHighlighted ? 0xff9b7e65 : 0xff745944));
    g.drawRoundedRectangle(r, 4.0f, open ? 1.4f : 1.0f);

    const float side = std::min(r.getWidth(), r.getHeight());
    const auto gear = makeGearPath(r.getCentreX(), r.getCentreY(), side * 0.35f, side * 0.25f, side * 0.11f, 8);
    g.setColour(open ? accent : juce::Colour(0xfff1e8d4));
    g.fillPath(gear);
}

//==============================================================================
// DXRhythmboxAudioProcessorEditor
//==============================================================================
DXSettingsOverlay::DXSettingsOverlay(juce::AudioProcessorValueTreeState& apvts,
                                     std::function<void()> onCloseRequested)
    : apvts_(apvts),
      closeFn_(std::move(onCloseRequested))
{
    setLookAndFeel(&laf_);
    setWantsKeyboardFocus(true);

    titleLabel_.setLookAndFeel(&laf_);
    titleLabel_.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xfffff2d8));
    addAndMakeVisible(titleLabel_);

    closeBtn_.setLookAndFeel(&laf_);
    closeBtn_.onClick = [this]()
    {
        if (closeFn_)
        {
            closeFn_();
        }
    };
    addAndMakeVisible(closeBtn_);

    const juce::Colour accentAmber(0xffffb84d);
    const juce::Colour hintCol(0xffc9b89c);

    // MIDI Key Start controls
    midiStartSectionLbl_.setLookAndFeel(&laf_);
    midiStartSectionLbl_.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    midiStartSectionLbl_.setColour(juce::Label::textColourId, accentAmber);
    addAndMakeVisible(midiStartSectionLbl_);

    midiStartEnableBtn_.setLookAndFeel(&laf_);
    addAndMakeVisible(midiStartEnableBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, param_ids::dxMidiStartEn, midiStartEnableBtn_));

    midiStartChLbl_.setLookAndFeel(&laf_);
    midiStartChLbl_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(midiStartChLbl_);

    midiStartChBox_.setLookAndFeel(&laf_);
    midiStartChBox_.addItemList(makeMidiTriggerChannelChoices(), 1);
    addAndMakeVisible(midiStartChBox_);
    comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts_, param_ids::dxMidiStartCh, midiStartChBox_));

    midiStartHintLbl_.setLookAndFeel(&laf_);
    midiStartHintLbl_.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    midiStartHintLbl_.setColour(juce::Label::textColourId, hintCol);
    addAndMakeVisible(midiStartHintLbl_);

    // Rhythm Stop CC controls
    stopCcSectionLbl_.setLookAndFeel(&laf_);
    stopCcSectionLbl_.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    stopCcSectionLbl_.setColour(juce::Label::textColourId, accentAmber);
    addAndMakeVisible(stopCcSectionLbl_);

    stopCcEnableBtn_.setLookAndFeel(&laf_);
    addAndMakeVisible(stopCcEnableBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, param_ids::dxStopCcEn, stopCcEnableBtn_));

    stopCcNumLbl_.setLookAndFeel(&laf_);
    stopCcNumLbl_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(stopCcNumLbl_);

    stopCcNumSlider_.setLookAndFeel(&laf_);
    stopCcNumSlider_.setRange(0.0, 127.0, 1.0);
    stopCcNumSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 22);
    stopCcNumSlider_.setDoubleClickReturnValue(true, 67.0);
    addAndMakeVisible(stopCcNumSlider_);
    sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, param_ids::dxStopCcNum, stopCcNumSlider_));

    stopCcChLbl_.setLookAndFeel(&laf_);
    stopCcChLbl_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(stopCcChLbl_);

    stopCcChBox_.setLookAndFeel(&laf_);
    stopCcChBox_.addItemList(makeMidiTriggerChannelChoices(), 1);
    addAndMakeVisible(stopCcChBox_);
    comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts_, param_ids::dxStopCcCh, stopCcChBox_));

    stopCcHintLbl_.setLookAndFeel(&laf_);
    stopCcHintLbl_.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    stopCcHintLbl_.setColour(juce::Label::textColourId, hintCol);
    addAndMakeVisible(stopCcHintLbl_);
}

DXSettingsOverlay::~DXSettingsOverlay()
{
    titleLabel_.setLookAndFeel(nullptr);
    closeBtn_.setLookAndFeel(nullptr);
    midiStartSectionLbl_.setLookAndFeel(nullptr);
    midiStartEnableBtn_.setLookAndFeel(nullptr);
    midiStartChLbl_.setLookAndFeel(nullptr);
    midiStartChBox_.setLookAndFeel(nullptr);
    midiStartHintLbl_.setLookAndFeel(nullptr);
    stopCcSectionLbl_.setLookAndFeel(nullptr);
    stopCcEnableBtn_.setLookAndFeel(nullptr);
    stopCcNumLbl_.setLookAndFeel(nullptr);
    stopCcNumSlider_.setLookAndFeel(nullptr);
    stopCcChLbl_.setLookAndFeel(nullptr);
    stopCcChBox_.setLookAndFeel(nullptr);
    stopCcHintLbl_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void DXSettingsOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xaa0b0705));

    const auto card = cardBounds_.toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRoundedRectangle(card.translated(0.0f, 4.0f), 8.0f);

    juce::ColourGradient bg(juce::Colour(0xff38261c), card.getX(), card.getY(),
                            juce::Colour(0xff231711), card.getX(), card.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(card, 8.0f);

    g.setColour(juce::Colour(0xff8e6e54));
    g.drawRoundedRectangle(card, 8.0f, 1.4f);

    // Header divider + mid-section divider
    g.setColour(juce::Colour(0xff5c4433));
    const float hdrY = static_cast<float>(cardBounds_.getY() + 42);
    const float midY = static_cast<float>(cardBounds_.getY() + cardBounds_.getHeight() / 2 + 12);
    g.drawHorizontalLine(static_cast<int>(hdrY), card.getX() + 14.0f, card.getRight() - 14.0f);
    g.drawHorizontalLine(static_cast<int>(midY), card.getX() + 14.0f, card.getRight() - 14.0f);
}

void DXSettingsOverlay::resized()
{
    const int cardW = std::min(540, getWidth() - 40);
    const int cardH = std::min(300, getHeight() - 36);
    cardBounds_ = getLocalBounds().withSizeKeepingCentre(cardW, cardH);

    auto area = cardBounds_.reduced(18, 12);
    auto hdr = area.removeFromTop(26);
    closeBtn_.setBounds(hdr.removeFromRight(28));
    titleLabel_.setBounds(hdr);

    area.removeFromTop(14);
    auto sec1 = area.removeFromTop((area.getHeight() - 16) / 2);
    area.removeFromTop(16);
    auto sec2 = area;

    // Group 1: MIDI Key Start
    midiStartSectionLbl_.setBounds(sec1.removeFromTop(20));
    sec1.removeFromTop(4);
    midiStartEnableBtn_.setBounds(sec1.removeFromTop(24));
    sec1.removeFromTop(6);
    auto r1 = sec1.removeFromTop(24);
    midiStartChLbl_.setBounds(r1.removeFromLeft(116));
    midiStartChBox_.setBounds(r1.removeFromLeft(110));
    sec1.removeFromTop(4);
    midiStartHintLbl_.setBounds(sec1.removeFromTop(18));

    // Group 2: Rhythm Stop CC
    stopCcSectionLbl_.setBounds(sec2.removeFromTop(20));
    sec2.removeFromTop(4);
    stopCcEnableBtn_.setBounds(sec2.removeFromTop(24));
    sec2.removeFromTop(6);
    auto r2 = sec2.removeFromTop(24);
    stopCcNumLbl_.setBounds(r2.removeFromLeft(74));
    stopCcNumSlider_.setBounds(r2.removeFromLeft(190));
    r2.removeFromLeft(14);
    stopCcChLbl_.setBounds(r2.removeFromLeft(84));
    stopCcChBox_.setBounds(r2.removeFromLeft(105));
    sec2.removeFromTop(4);
    stopCcHintLbl_.setBounds(sec2.removeFromTop(18));
}

void DXSettingsOverlay::mouseDown(const juce::MouseEvent& event)
{
    if (!cardBounds_.contains(event.getPosition()) && closeFn_)
    {
        closeFn_();
    }
}

bool DXSettingsOverlay::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && closeFn_)
    {
        closeFn_();
        return true;
    }
    return false;
}

//==============================================================================
// DXRhythmboxAudioProcessorEditor
//==============================================================================
DXRhythmboxAudioProcessorEditor::DXRhythmboxAudioProcessorEditor(DXRhythmboxAudioProcessor& owner)
    : AudioProcessorEditor(&owner),
      processor_(owner),
      panel_(
          owner.apvts(),
          [this](dx701::dsp::VoiceId voice, float)
          {
              processor_.triggerVoiceFromUi(voice);
          },
          [this](dx701::dsp::FillInId fillIn)
          {
              processor_.triggerFillInFromUi(fillIn);
          },
          [this](bool held)
          {
              processor_.setStandaloneStartHeldFromUi(held);
          },
          [this](bool held)
          {
              processor_.setStandaloneStopHeldFromUi(held);
          },
          [this]()
          {
              return processor_.getTelemetry();
          },
          [this]()
          {
              return processor_.consumeMidiFlashMask();
          },
          [this]()
          {
              return dx701::ui::DXRhythmboxPanel::TransportStatusFlags{
                  processor_.isRhythmLatched(),
                  processor_.isStopSwitchHeld(),
                  processor_.isMidiArmed()
              };
          },
          owner.isStandaloneMode()),
      settingsOverlay_(
          owner.apvts(),
          [this]()
          {
              settingsGearBtn_.setToggleState(false, juce::dontSendNotification);
              settingsOverlay_.setVisible(false);
          })
{
    setLookAndFeel(&presetBarLaf_);

    titleLabel_.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffefe6d2));
    addAndMakeVisible(titleLabel_);

    presetLabel_.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    presetLabel_.setJustificationType(juce::Justification::centredRight);
    presetLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffbcae96));
    addAndMakeVisible(presetLabel_);

    prevProgramBtn_.setLookAndFeel(&presetBarLaf_);
    prevProgramBtn_.onClick = [this]()
    {
        const int n = processor_.getNumPrograms();
        if (n > 0)
        {
            const int prevIdx = (processor_.getCurrentProgram() - 1 + n) % n;
            processor_.setCurrentProgram(prevIdx);
            programBox_.setSelectedId(prevIdx + 1, juce::dontSendNotification);
            panel_.refreshTelemetryNow();
        }
    };
    addAndMakeVisible(prevProgramBtn_);

    programBox_.setLookAndFeel(&presetBarLaf_);
    for (int p = 0; p < processor_.getNumPrograms(); ++p)
    {
        programBox_.addItem(processor_.getProgramName(p), p + 1);
    }
    programBox_.setSelectedId(processor_.getCurrentProgram() + 1, juce::dontSendNotification);
    programBox_.onChange = [this]()
    {
        const int sel = programBox_.getSelectedId();
        if (sel > 0)
        {
            processor_.setCurrentProgram(sel - 1);
            panel_.refreshTelemetryNow();
        }
    };
    addAndMakeVisible(programBox_);

    nextProgramBtn_.setLookAndFeel(&presetBarLaf_);
    nextProgramBtn_.onClick = [this]()
    {
        const int n = processor_.getNumPrograms();
        if (n > 0)
        {
            const int nextIdx = (processor_.getCurrentProgram() + 1) % n;
            processor_.setCurrentProgram(nextIdx);
            programBox_.setSelectedId(nextIdx + 1, juce::dontSendNotification);
            panel_.refreshTelemetryNow();
        }
    };
    addAndMakeVisible(nextProgramBtn_);

    settingsGearBtn_.onClick = [this]()
    {
        const bool open = settingsGearBtn_.getToggleState();
        settingsOverlay_.setVisible(open);
        if (open)
        {
            settingsOverlay_.toFront(true);
        }
    };
    addAndMakeVisible(settingsGearBtn_);

    addAndMakeVisible(panel_);
    addChildComponent(settingsOverlay_);

    setResizable(true, true);
    setResizeLimits(800, 520, 1440, 900);
    setSize(960, 600);

    syncUiNow();
    startTimerHz(15);
}

DXRhythmboxAudioProcessorEditor::~DXRhythmboxAudioProcessorEditor()
{
    stopTimer();
    prevProgramBtn_.setLookAndFeel(nullptr);
    programBox_.setLookAndFeel(nullptr);
    nextProgramBtn_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void DXRhythmboxAudioProcessorEditor::syncUiNow()
{
    timerCallback();
    panel_.refreshTelemetryNow();
}

void DXRhythmboxAudioProcessorEditor::timerCallback()
{
    if (!programBox_.isPopupActive())
    {
        const int wantId = processor_.getCurrentProgram() + 1;
        if (programBox_.getSelectedId() != wantId && wantId > 0)
        {
            programBox_.setSelectedId(wantId, juce::dontSendNotification);
        }
    }

    panel_.setStandaloneVisible(processor_.isStandaloneMode());
}

void DXRhythmboxAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1b130e));
    juce::ColourGradient barGrad(juce::Colour(0xff2c1f17), 0.0f, 0.0f,
                                 juce::Colour(0xff1b130e), 0.0f, static_cast<float>(presetBarBounds_.getBottom()), false);
    g.setGradientFill(barGrad);
    g.fillRect(presetBarBounds_);
    g.setColour(juce::Colour(0xff5b4333));
    g.drawHorizontalLine(presetBarBounds_.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));
}

void DXRhythmboxAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    presetBarBounds_ = area.removeFromTop(34);

    auto bar = presetBarBounds_.reduced(10, 4);
    titleLabel_.setBounds(bar.removeFromLeft(130));

    settingsGearBtn_.setBounds(bar.removeFromRight(26));
    bar.removeFromRight(8);

    nextProgramBtn_.setBounds(bar.removeFromRight(24));
    bar.removeFromRight(3);
    programBox_.setBounds(bar.removeFromRight(std::min(210, std::max(130, bar.getWidth() - 120))));
    bar.removeFromRight(3);
    prevProgramBtn_.setBounds(bar.removeFromRight(24));
    bar.removeFromRight(6);
    presetLabel_.setBounds(bar.removeFromRight(60));

    panel_.setBounds(area);
    settingsOverlay_.setBounds(area);
}

} // namespace dx701::plugin
