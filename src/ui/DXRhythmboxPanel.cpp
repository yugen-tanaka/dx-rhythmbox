#include "ui/DXRhythmboxPanel.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

namespace dx701::ui
{

namespace
{

namespace dx_palette
{
const juce::Colour walnutTop{0xff513727};
const juce::Colour walnutBot{0xff352318};
const juce::Colour walnutDeep{0xff1f140e};
const juce::Colour bezelEdge{0xff755741};
const juce::Colour cardBg{0xff271b14};
const juce::Colour cardBorder{0xff7e624b};
const juce::Colour wellBg{0xff0f0a07};
const juce::Colour silkCream{0xfff2e8d4};
const juce::Colour silkActive{0xfffffaed};
const juce::Colour silkMuted{0xffc6b89e};
const juce::Colour amberGlow{0xffffb84d};
const juce::Colour rubyOff{0xff591016};
const juce::Colour rubyOnCore{0xffff3246};
const juce::Colour rubyOnEdge{0xffc41224};
const juce::Colour rubySpec{0xffffccd2};
const juce::Colour cobaltNote{0xff103699};
const juce::Colour tealMeter{0xff2aa384};
const juce::Colour meterHot{0xffff4d5a};
} // namespace dx_palette

struct CapPalette
{
    juce::Colour fwTop;
    juce::Colour fwBot;
    juce::Colour fwStroke;
    juce::Colour bevelCol;
    juce::Colour topHi;
    juce::Colour topLo;
    juce::Colour topStroke;
    juce::Colour bowlLo;
    juce::Colour bowlHi;
    juce::Colour arcStroke;
    bool drawGlossyArc = false;
};

CapPalette getCapPalette(DXConcavePushButton::CapColor color, bool latched, float hoverBoost) noexcept
{
    switch (color)
    {
        case DXConcavePushButton::CapColor::Espresso:
            return {
                latched ? juce::Colour(0xff291c16) : juce::Colour(0xff3a2920),
                juce::Colour(0xff100a07),
                juce::Colour(0xff5f4839).withAlpha(latched ? 0.56f : 0.80f),
                juce::Colour(latched ? 0xff705747 : 0xff987a64),
                (latched ? juce::Colour(0xff33251d) : juce::Colour(0xff48342a)).brighter(hoverBoost),
                latched ? juce::Colour(0xff1d140f) : juce::Colour(0xff271c15),
                juce::Colour(latched ? 0xff654d3e : 0xff826653),
                juce::Colour(0xff0d0907),
                juce::Colour(latched ? 0xff3b2b22 : juce::Colour(0xff523d31).getARGB()),
                juce::Colour(0xffa68a76).withAlpha(latched ? 0.58f : 0.74f),
                true
            };
        case DXConcavePushButton::CapColor::Red:
            return {
                latched ? juce::Colour(0xffac1919) : juce::Colour(0xffd22121),
                latched ? juce::Colour(0xff600909) : juce::Colour(0xff7d0c0c),
                juce::Colour(0xff4e0808).withAlpha(0.75f),
                juce::Colour(latched ? 0xfff05b5b : 0xffff9191),
                (latched ? juce::Colour(0xffce2020) : juce::Colour(0xffec2a2a)).brighter(hoverBoost),
                latched ? juce::Colour(0xff981313) : juce::Colour(0xffb51616),
                juce::Colour(latched ? 0xffa81c1c : 0xfff86666),
                juce::Colour(0xff660909),
                juce::Colour(latched ? 0xffdd3535 : 0xfff85050),
                juce::Colour(0xffffc8c8).withAlpha(latched ? 0.54f : 0.70f),
                true
            };
        case DXConcavePushButton::CapColor::Charcoal:
            return {
                latched ? juce::Colour(0xff1a1715) : juce::Colour(0xff26211f),
                juce::Colour(0xff080706),
                juce::Colour(0xff463e3a).withAlpha(latched ? 0.52f : 0.78f),
                juce::Colour(latched ? 0xff514843 : 0xff726862),
                (latched ? juce::Colour(0xff211d1a) : juce::Colour(0xff2d2825)).brighter(hoverBoost),
                latched ? juce::Colour(0xff110f0e) : juce::Colour(0xff151211),
                juce::Colour(latched ? 0xff49413c : 0xff5e5550),
                juce::Colour(0xff060505),
                juce::Colour(latched ? 0xff292421 : 0xff393330),
                juce::Colour(0xff8c827c).withAlpha(latched ? 0.54f : 0.68f),
                true
            };
        case DXConcavePushButton::CapColor::Ivory:
        default:
            return {
                latched ? juce::Colour(0xffbdb29c) : juce::Colour(0xffdad1bd),
                latched ? juce::Colour(0xff8f8571) : juce::Colour(0xffa79c86),
                juce::Colour(0xff786d5a).withAlpha(0.70f),
                juce::Colour(latched ? 0xfff3ecdc : 0xfffffef6),
                (latched ? juce::Colour(0xffece4d2) : juce::Colour(0xfffaf5ea)).brighter(hoverBoost),
                latched ? juce::Colour(0xffd5cab3) : juce::Colour(0xffe8dfc9),
                juce::Colour(latched ? 0xffb6aa94 : 0xfffffef9),
                juce::Colour(latched ? 0xffbfb49c : 0xffcdc3ab),
                juce::Colour(latched ? 0xfff4eddc : 0xfffffdf7),
                juce::Colour(0xff948873).withAlpha(0.24f),
                false
            };
    }
}

void drawSectionBracket(juce::Graphics& g,
                        juce::Rectangle<int> area,
                        const juce::String& title,
                        float fontSize = 10.0f)
{
    if (area.getWidth() < 36 || area.getHeight() < 12)
    {
        return;
    }

    const juce::Font font(juce::FontOptions(fontSize, juce::Font::bold));
    g.setFont(font);
    const int textW = static_cast<int>(std::ceil(juce::TextLayout::getStringWidth(font, title))) + 10;
    const int cy = area.getCentreY();
    const int x0 = area.getX() + 2;
    const int x1 = area.getRight() - 2;
    const int cx = area.getCentreX();
    const int leftEnd = std::max(x0, cx - textW / 2);
    const int rightStart = std::min(x1, cx + textW / 2);

    g.setColour(dx_palette::silkCream.withAlpha(0.82f));
    if (leftEnd > x0 + 2)
    {
        g.drawHorizontalLine(cy, static_cast<float>(x0), static_cast<float>(leftEnd));
        g.drawVerticalLine(x0, static_cast<float>(cy), static_cast<float>(area.getBottom()));
    }
    if (rightStart < x1 - 2)
    {
        g.drawHorizontalLine(cy, static_cast<float>(rightStart), static_cast<float>(x1));
        g.drawVerticalLine(x1 - 1, static_cast<float>(cy), static_cast<float>(area.getBottom()));
    }

    g.setColour(dx_palette::silkCream);
    g.drawFittedText(title, area, juce::Justification::centred, 1, 0.78f);
}

void drawTiltedNotehead(juce::Graphics& g, float cx, float cy, float rx, float ry, juce::Colour col)
{
    juce::Path ellipse;
    ellipse.addEllipse(-rx, -ry, 2.0f * rx, 2.0f * ry);
    ellipse.applyTransform(juce::AffineTransform::rotation(-0.38f).translated(cx, cy));
    g.setColour(col);
    g.fillPath(ellipse);
}

} // namespace

//==============================================================================
// DXLookAndFeel
//==============================================================================
DXLookAndFeel::DXLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff201610));
    setColour(juce::ComboBox::textColourId, dx_palette::silkCream);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff745944));
    setColour(juce::ComboBox::arrowColourId, dx_palette::amberGlow);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff241812));
    setColour(juce::PopupMenu::textColourId, dx_palette::silkCream);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, dx_palette::amberGlow.withAlpha(0.28f));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::Label::textColourId, dx_palette::silkCream);
    setColour(juce::Slider::textBoxTextColourId, dx_palette::silkCream);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff19120d));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff5e4736));
}

int DXLookAndFeel::getSliderThumbRadius(juce::Slider& slider)
{
    return (slider.getSliderStyle() == juce::Slider::LinearVertical) ? 12 : LookAndFeel_V4::getSliderThumbRadius(slider);
}

void DXLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                     int x, int y, int width, int height,
                                     float sliderPos,
                                     float minSliderPos,
                                     float maxSliderPos,
                                     juce::Slider::SliderStyle style,
                                     juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos, slider);

    if (style == juce::Slider::LinearVertical)
    {
        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float margin = 12.0f;
        const float topY = static_cast<float>(y) + margin;
        const float botY = static_cast<float>(y + height) - margin;
        const float travel = std::max(24.0f, botY - topY);

        // 5 horizontal cream graduation ticks on both left & right sides (matching DX701 panel)
        const float slotHalf = 5.5f;
        for (int t = 0; t <= 4; ++t)
        {
            const float ty = topY + (static_cast<float>(t) / 4.0f) * travel;
            const float tickLen = (t == 2) ? 11.0f : 7.5f;
            const float alpha = (t == 0 || t == 2 || t == 4) ? 0.92f : 0.68f;
            g.setColour(dx_palette::silkCream.withAlpha(alpha));
            g.drawLine(cx - slotHalf - 4.0f - tickLen, ty, cx - slotHalf - 4.0f, ty, 1.4f);
            g.drawLine(cx + slotHalf + 4.0f, ty, cx + slotHalf + 4.0f + tickLen, ty, 1.4f);
        }

        // Matte black vertical slot spanning the full fader travel
        const juce::Rectangle<float> track(cx - 4.2f, topY - 4.0f, 8.4f, travel + 8.0f);
        g.setColour(juce::Colour(0xff0b0806));
        g.fillRoundedRectangle(track, 3.5f);
        g.setColour(juce::Colour(0xff6b5240).withAlpha(0.55f));
        g.drawRoundedRectangle(track, 3.5f, 0.9f);

        // Espresso bevelled fader cap with bright ivory center stripe
        const float capW = std::min(30.0f, static_cast<float>(width) - 8.0f);
        const float capH = 16.0f;
        const float clampedPos = std::clamp(sliderPos, topY, botY);
        const juce::Rectangle<float> capRect(cx - capW * 0.5f, clampedPos - capH * 0.5f, capW, capH);

        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(capRect.translated(0.0f, 2.5f), 2.5f);

        juce::ColourGradient capGrad(juce::Colour(0xff423026), capRect.getX(), capRect.getY(),
                                     juce::Colour(0xff1c1410), capRect.getX(), capRect.getBottom(), false);
        g.setGradientFill(capGrad);
        g.fillRoundedRectangle(capRect, 2.5f);

        g.setColour(juce::Colour(0xff7e624d));
        g.drawRoundedRectangle(capRect, 2.5f, 1.0f);

        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.drawHorizontalLine(static_cast<int>(capRect.getY() + 3.5f), capRect.getX() + 2.0f, capRect.getRight() - 2.0f);
        g.drawHorizontalLine(static_cast<int>(capRect.getBottom() - 3.5f), capRect.getX() + 2.0f, capRect.getRight() - 2.0f);

        g.setColour(dx_palette::silkActive);
        g.fillRect(capRect.getX() + 2.5f, clampedPos - 1.0f, capRect.getWidth() - 5.0f, 2.0f);
        return;
    }

    if (style == juce::Slider::LinearHorizontal)
    {
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float left = static_cast<float>(x) + 4.0f;
        const float right = static_cast<float>(x + width) - 4.0f;
        const float trackW = std::max(10.0f, right - left);
        g.setColour(juce::Colour(0xff110c09));
        g.fillRoundedRectangle(left, cy - 2.5f, trackW, 5.0f, 2.5f);

        const float fillW = std::clamp(sliderPos - left, 0.0f, trackW);
        g.setColour(dx_palette::amberGlow.withAlpha(0.85f));
        g.fillRoundedRectangle(left, cy - 2.5f, fillW, 5.0f, 2.5f);

        const float thumbX = std::clamp(sliderPos, left, right);
        g.setColour(juce::Colour(0xff3e2c22));
        g.fillEllipse(thumbX - 6.0f, cy - 6.0f, 12.0f, 12.0f);
        g.setColour(dx_palette::silkCream);
        g.drawEllipse(thumbX - 6.0f, cy - 6.0f, 12.0f, 12.0f, 1.1f);
        return;
    }

    juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
}

void DXLookAndFeel::drawComboBox(juce::Graphics& g,
                                 int width, int height,
                                 bool,
                                 int, int, int, int,
                                 juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    const float ax = static_cast<float>(width) - 13.0f;
    const float ay = static_cast<float>(height) * 0.5f - 1.5f;
    juce::Path arrow;
    arrow.startNewSubPath(ax - 3.5f, ay);
    arrow.lineTo(ax, ay + 4.0f);
    arrow.lineTo(ax + 3.5f, ay);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(arrow, juce::PathStrokeType(1.6f));
}

juce::Font DXLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(11.0f, juce::Font::bold));
}

void DXLookAndFeel::drawToggleButton(juce::Graphics& g,
                                     juce::ToggleButton& button,
                                     bool shouldDrawButtonAsHighlighted,
                                     bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    const float boxS = std::min(14.0f, bounds.getHeight() - 6.0f);
    const juce::Rectangle<float> chk(bounds.getX() + 3.0f, bounds.getCentreY() - boxS * 0.5f, boxS, boxS);
    g.setColour(on ? dx_palette::amberGlow.withAlpha(0.24f) : juce::Colour(0xff16100c));
    g.fillRoundedRectangle(chk, 3.0f);
    g.setColour(on ? dx_palette::amberGlow : juce::Colour(shouldDrawButtonAsHighlighted ? 0xff9f836a : 0xff705744));
    g.drawRoundedRectangle(chk, 3.0f, 1.1f);

    if (on)
    {
        juce::Path tick;
        tick.startNewSubPath(chk.getX() + boxS * 0.22f, chk.getCentreY());
        tick.lineTo(chk.getX() + boxS * 0.45f, chk.getBottom() - boxS * 0.25f);
        tick.lineTo(chk.getRight() - boxS * 0.20f, chk.getY() + boxS * 0.24f);
        g.setColour(dx_palette::amberGlow);
        g.strokePath(tick, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    auto textArea = bounds.withTrimmedLeft(chk.getRight() + 6.0f);
    g.setColour(on ? dx_palette::silkActive : dx_palette::silkCream);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawFittedText(button.getButtonText(), textArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.78f);
}

void DXLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                         juce::Button& button,
                                         const juce::Colour&,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour fill = juce::Colour(0xff2c1f17);
    if (shouldDrawButtonAsDown)
    {
        fill = fill.darker(0.22f);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        fill = fill.brighter(0.14f);
    }
    g.setColour(fill);
    g.fillRoundedRectangle(r, 3.5f);
    g.setColour(juce::Colour(0xff745944));
    g.drawRoundedRectangle(r, 3.5f, 1.0f);
}

void DXLookAndFeel::drawButtonText(juce::Graphics& g,
                                   juce::TextButton& button,
                                   bool,
                                   bool)
{
    g.setColour(dx_palette::silkCream);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(2, 1), juce::Justification::centred, 1);
}

//==============================================================================
// DXConcavePushButton -- 1982 KAWAI DX701 3D Extruded Mechanical Plunger Switch
//==============================================================================
DXConcavePushButton::DXConcavePushButton(CapColor capColor,
                                         bool momentary,
                                         PressStateCallback onPressStateChanged)
    : capColor_(capColor),
      momentary_(momentary),
      pressStateFn_(std::move(onPressStateChanged))
{
    setClickingTogglesState(!momentary_);
}

void DXConcavePushButton::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isPopupMenu() && pressStateFn_)
    {
        pressStateFn_(true);
    }
    juce::ToggleButton::mouseDown(event);
}

void DXConcavePushButton::mouseUp(const juce::MouseEvent& event)
{
    juce::ToggleButton::mouseUp(event);
    if (pressStateFn_)
    {
        pressStateFn_(false);
    }
}

void DXConcavePushButton::computeExtrusionMetrics(float& outSq, float& outZOff, float& outZOn, float& outHoleTop) const noexcept
{
    const auto full = getLocalBounds().toFloat();
    const float w = std::max(14.0f, full.getWidth() - 2.0f);
    const float h = std::max(18.0f, full.getHeight());
    outZOff = std::clamp(std::round(w * 0.22f), 7.0f, 12.0f);
    outZOn = std::clamp(std::round(outZOff * 0.28f), 2.5f, 4.0f);
    outSq = std::min(w, std::max(12.0f, h - outZOff - 3.0f));
    outHoleTop = 1.0f + outZOff;
}

juce::Rectangle<float> DXConcavePushButton::computeTopSquareRect(bool latched, bool heldDown) const noexcept
{
    const auto full = getLocalBounds().toFloat();
    float sq = 16.0f;
    float zOff = 9.0f;
    float zOn = 3.0f;
    float yHoleTop = 10.0f;
    computeExtrusionMetrics(sq, zOff, zOn, yHoleTop);

    const float z = latched ? (heldDown ? zOn * 0.60f : zOn) : (heldDown ? zOff * 0.55f : zOff);
    const float yTop = yHoleTop - z;
    const float xTop = full.getCentreX() - sq * 0.5f;
    return {xTop, yTop, sq, sq};
}

bool DXConcavePushButton::hitTest(int x, int y)
{
    float sq = 16.0f;
    float zOff = 9.0f;
    float zOn = 3.0f;
    float yHoleTop = 10.0f;
    computeExtrusionMetrics(sq, zOff, zOn, yHoleTop);

    const float yFrontBot = yHoleTop + sq;
    const bool latched = !momentary_ && getToggleState();
    const auto topRect = computeTopSquareRect(latched, isDown());
    const auto visibleBounds = juce::Rectangle<float>(topRect.getX(), topRect.getY(), topRect.getWidth(),
                                                      std::max(sq * 0.5f, yFrontBot - topRect.getY()));
    return visibleBounds.contains(static_cast<float>(x), static_cast<float>(y));
}

void DXConcavePushButton::paintButton(juce::Graphics& g,
                                      bool shouldDrawButtonAsHighlighted,
                                      bool shouldDrawButtonAsDown)
{
    const bool latched = momentary_ ? shouldDrawButtonAsDown : getToggleState();
    float sq = 16.0f;
    float zOff = 9.0f;
    float zOn = 3.0f;
    float yHoleTop = 10.0f;
    computeExtrusionMetrics(sq, zOff, zOn, yHoleTop);
    const float yFrontBot = yHoleTop + sq;

    const auto topRect = computeTopSquareRect(latched, shouldDrawButtonAsDown);
    const float x0 = topRect.getX();
    const float y0 = topRect.getY();
    const float cornerR = std::clamp(sq * 0.16f, 4.0f, 8.0f);
    const float hoverBoost = (shouldDrawButtonAsHighlighted && !latched) ? 0.05f : 0.0f;

    const CapPalette pal = getCapPalette(capColor_, latched, hoverBoost);

    // 1. Cast shadow along lower footprint inside dark cutout
    g.setColour(juce::Colours::black.withAlpha(latched ? 0.45f : 0.72f));
    g.fillRoundedRectangle(x0 + 0.5f, yFrontBot - 1.5f, sq - 1.0f, 3.0f, cornerR * 0.6f);

    // 2. Extruded 3D vertical front wall descending to the panel cutout rim (tall when OFF, short proud step when ON)
    const float frontStartY = y0 + sq * 0.58f;
    const float frontBodyH = std::max(cornerR + 2.0f, yFrontBot - frontStartY);
    const juce::Rectangle<float> frontRect(x0, frontStartY, sq, frontBodyH);

    {
        juce::ColourGradient fwGrad(pal.fwTop, frontRect.getCentreX(), frontRect.getY(),
                                    pal.fwBot, frontRect.getCentreX(), frontRect.getBottom(), false);
        g.setGradientFill(fwGrad);
        g.fillRoundedRectangle(frontRect, cornerR);

        g.setColour(pal.fwStroke);
        g.drawRoundedRectangle(frontRect.reduced(0.35f), cornerR, 0.85f);
    }

    // Crisp horizontal front bevel ridge between top concave face and front wall
    g.setColour(pal.bevelCol);
    g.drawHorizontalLine(static_cast<int>(std::round(y0 + sq - 0.5f)),
                         x0 + cornerR * 0.42f, x0 + sq - cornerR * 0.42f);

    // 3. Rounded square top face protruding cleanly above the board cutout
    {
        juce::ColourGradient topGrad(pal.topHi, x0, y0, pal.topLo, x0 + sq, y0 + sq, false);
        g.setGradientFill(topGrad);
        g.fillRoundedRectangle(topRect, cornerR);

        g.setColour(pal.topStroke);
        g.drawRoundedRectangle(topRect.reduced(0.4f), cornerR, 0.9f);
    }

    // 4. Sculpted circular concave finger bowl centered on square cap
    const float dishR = sq * 0.365f;
    const float cx = topRect.getCentreX();
    const float cy = topRect.getCentreY();
    const juce::Rectangle<float> dishRect(cx - dishR, cy - dishR, dishR * 2.0f, dishR * 2.0f);

    {
        juce::ColourGradient bowlGrad(pal.bowlLo, cx - dishR * 0.65f, cy - dishR * 0.65f,
                                      pal.bowlHi, cx + dishR * 0.72f, cy + dishR * 0.72f, false);
        g.setGradientFill(bowlGrad);
        g.fillEllipse(dishRect);

        if (pal.drawGlossyArc)
        {
            juce::Path specArc;
            specArc.addCentredArc(cx, cy, dishR * 0.86f, dishR * 0.86f, 0.0f,
                                  juce::MathConstants<float>::pi * 0.55f,
                                  juce::MathConstants<float>::pi * 1.15f, true);
            g.setColour(pal.arcStroke);
            g.strokePath(specArc, juce::PathStrokeType(1.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour(pal.arcStroke);
            g.drawEllipse(dishRect, 0.75f);
        }
    }
}

//==============================================================================
// DXTransportButton -- Standalone Cream Mini-Fill-In Play / Stop Buttons
//==============================================================================
DXTransportButton::DXTransportButton(const juce::String& name,
                                     IconType icon,
                                     HoldCallback onHoldChanged)
    : juce::Button(name),
      icon_(icon),
      holdFn_(std::move(onHoldChanged))
{
}

void DXTransportButton::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isPopupMenu())
    {
        if (holdFn_)
        {
            holdFn_(true);
        }
        repaint();
    }
    juce::Button::mouseDown(event);
}

void DXTransportButton::mouseUp(const juce::MouseEvent& event)
{
    juce::Button::mouseUp(event);
    if (holdFn_)
    {
        holdFn_(false);
    }
    repaint();
}

void DXTransportButton::paintButton(juce::Graphics& g,
                                    bool shouldDrawButtonAsHighlighted,
                                    bool shouldDrawButtonAsDown)
{
    const bool held = shouldDrawButtonAsDown || forcedHold_;
    const auto outer = getLocalBounds().toFloat().reduced(1.0f);

    if (held)
    {
        g.setColour(juce::Colour(0xffffb95c).withAlpha(0.28f));
        g.fillRoundedRectangle(outer.expanded(1.2f), 4.5f);
    }

    // Dark recessed bezel framing mini cream Fill-In cap
    g.setColour(juce::Colour(0xff140d09));
    g.fillRoundedRectangle(outer, 3.8f);

    const float pad = held ? 3.0f : 2.1f;
    const auto cap = outer.reduced(pad);
    const float capR = 3.2f;

    if (held)
    {
        juce::ColourGradient activeGrad(juce::Colour(0xfffff6d6), cap.getCentreX(), cap.getCentreY(),
                                        juce::Colour(0xffe6ba6e), cap.getX(), cap.getBottom(), true);
        g.setGradientFill(activeGrad);
        g.fillRoundedRectangle(cap, capR);

        g.setColour(juce::Colour(0xfffff1ca));
        g.drawRoundedRectangle(cap, capR, 1.0f);
    }
    else
    {
        const float boost = shouldDrawButtonAsHighlighted ? 0.06f : 0.0f;
        juce::ColourGradient creamGrad(juce::Colour(0xfff4efe2).brighter(boost), cap.getX(), cap.getY(),
                                       juce::Colour(0xffd5cbb5).brighter(boost), cap.getX(), cap.getBottom(), false);
        g.setGradientFill(creamGrad);
        g.fillRoundedRectangle(cap, capR);

        g.setColour(juce::Colour(0xfffffcf4));
        g.drawRoundedRectangle(cap, capR, 0.9f);
        g.setColour(juce::Colour(0xff9c8f78).withAlpha(0.45f));
        g.drawRoundedRectangle(cap.reduced(1.0f), capR - 0.6f, 0.7f);
    }

    const juce::Colour iconCol(0xff231710);
    g.setColour(iconCol);
    const float s = std::min(cap.getWidth(), cap.getHeight());
    const float cx = cap.getCentreX();
    const float cy = cap.getCentreY();

    if (icon_ == IconType::Play)
    {
        const float r = s * 0.25f;
        const float shiftX = r * 0.14f;
        juce::Path tri;
        tri.addTriangle(cx - r * 0.80f + shiftX, cy - r * 0.96f,
                        cx + r * 1.00f + shiftX, cy,
                        cx - r * 0.80f + shiftX, cy + r * 0.96f);
        g.fillPath(tri);
    }
    else
    {
        const float side = s * 0.42f;
        g.fillRoundedRectangle(cx - side * 0.5f, cy - side * 0.5f, side, side, 1.5f);
    }
}

//==============================================================================
// DXFillInButton
//==============================================================================
DXFillInButton::DXFillInButton(const juce::String& name, NoteSymbol symbol)
    : juce::Button(name),
      symbol_(symbol)
{
}

void DXFillInButton::setIlluminated(bool lit) noexcept
{
    if (illuminated_ != lit)
    {
        illuminated_ = lit;
        repaint();
    }
}

void DXFillInButton::stepPulse() noexcept
{
    if (clickPulseFrames_ > 0)
    {
        --clickPulseFrames_;
        if (clickPulseFrames_ == 0)
        {
            repaint();
        }
    }
}

void DXFillInButton::drawCobaltNoteSymbol(juce::Graphics& g, juce::Rectangle<float> area) const
{
    const bool isTriplet = (symbol_ == NoteSymbol::TripletSixteenth || symbol_ == NoteSymbol::TripletEighth);
    const bool isDoubleBeam = (symbol_ == NoteSymbol::TripletSixteenth || symbol_ == NoteSymbol::DoubleSixteenth);
    const juce::Colour col = dx_palette::cobaltNote;

    const float s = std::min(area.getWidth(), area.getHeight());
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();

    const float rx = isTriplet ? (s * 0.082f) : (s * 0.098f);
    const float ry = rx * 0.72f;
    const float stemW = std::max(1.6f, s * 0.036f);
    const float beamH = std::max(2.3f, s * 0.056f);
    const float beamGap = beamH * 1.68f;

    const float headYBase = cy + s * 0.17f;
    const float topYBase = cy - (isTriplet ? (s * 0.19f) : (s * 0.25f));
    const float slope = s * 0.025f;

    if (isTriplet)
    {
        const float dxSpan = s * 0.21f;
        const std::array<float, 3> nx = {cx - dxSpan - s * 0.02f, cx - s * 0.02f, cx + dxSpan - s * 0.02f};
        const std::array<float, 3> ny = {headYBase + slope * 0.5f, headYBase, headYBase - slope * 0.5f};
        const std::array<float, 3> topY = {topYBase + slope * 0.5f, topYBase, topYBase - slope * 0.5f};
        const std::array<float, 3> stemX = {nx[0] + rx * 0.72f, nx[1] + rx * 0.72f, nx[2] + rx * 0.72f};

        for (std::size_t k = 0; k < 3; ++k)
        {
            drawTiltedNotehead(g, nx[k], ny[k], rx, ry, col);
            g.setColour(col);
            g.fillRect(stemX[k] - stemW * 0.5f, topY[k], stemW, ny[k] - topY[k]);
        }

        auto drawBeam = [&](float offsetY)
        {
            juce::Path p;
            p.addQuadrilateral(stemX[0] - stemW * 0.5f, topY[0] + offsetY,
                               stemX[2] + stemW * 0.5f, topY[2] + offsetY,
                               stemX[2] + stemW * 0.5f, topY[2] + offsetY + beamH,
                               stemX[0] - stemW * 0.5f, topY[0] + offsetY + beamH);
            g.setColour(col);
            g.fillPath(p);
        };
        drawBeam(0.0f);
        if (isDoubleBeam)
        {
            drawBeam(beamGap);
        }

        const float brY = topY[1] - s * 0.095f;
        const float brL = stemX[0] - stemW * 0.5f;
        const float brR = stemX[2] + stemW * 0.5f;
        const float midGap = s * 0.082f;
        const float hookH = s * 0.045f;
        const float lineT = std::max(1.2f, s * 0.024f);

        g.setColour(col);
        g.drawLine(brL, brY + hookH, brL, brY, lineT);
        g.drawLine(brL, brY, cx - midGap, brY, lineT);
        g.drawLine(cx + midGap, brY, brR, brY, lineT);
        g.drawLine(brR, brY, brR, brY + hookH, lineT);

        g.setFont(juce::Font(juce::FontOptions(std::max(9.0f, s * 0.165f), juce::Font::bold)));
        const juce::Rectangle<float> numBox(cx - midGap, brY - s * 0.095f, 2.0f * midGap, s * 0.19f);
        g.drawFittedText("3", numBox.toNearestInt(), juce::Justification::centred, 1);
        return;
    }

    const float dx = s * 0.175f;
    const float nx1 = cx - dx - s * 0.025f;
    const float nx2 = cx + dx - s * 0.025f;
    const float ny1 = headYBase + slope * 0.45f;
    const float ny2 = headYBase - slope * 0.45f;
    const float topY1 = topYBase + slope * 0.45f;
    const float topY2 = topYBase - slope * 0.45f;
    const float stemX1 = nx1 + rx * 0.72f;
    const float stemX2 = nx2 + rx * 0.72f;

    drawTiltedNotehead(g, nx1, ny1, rx, ry, col);
    drawTiltedNotehead(g, nx2, ny2, rx, ry, col);
    g.setColour(col);
    g.fillRect(stemX1 - stemW * 0.5f, topY1, stemW, ny1 - topY1);
    g.fillRect(stemX2 - stemW * 0.5f, topY2, stemW, ny2 - topY2);

    auto drawBeam2 = [&](float offsetY)
    {
        juce::Path p;
        p.addQuadrilateral(stemX1 - stemW * 0.5f, topY1 + offsetY,
                           stemX2 + stemW * 0.5f, topY2 + offsetY,
                           stemX2 + stemW * 0.5f, topY2 + offsetY + beamH,
                           stemX1 - stemW * 0.5f, topY1 + offsetY + beamH);
        g.setColour(col);
        g.fillPath(p);
    };
    drawBeam2(0.0f);
    if (isDoubleBeam)
    {
        drawBeam2(beamGap);
    }
}

void DXFillInButton::paintButton(juce::Graphics& g,
                                 bool shouldDrawButtonAsHighlighted,
                                 bool shouldDrawButtonAsDown)
{
    const bool lit = illuminated_ || (clickPulseFrames_ > 0) || shouldDrawButtonAsDown;
    const auto outer = getLocalBounds().toFloat().reduced(1.5f);

    if (lit)
    {
        g.setColour(juce::Colour(0xffff9c38).withAlpha(0.32f));
        g.fillRoundedRectangle(outer.expanded(1.5f), 5.0f);
    }

    g.setColour(juce::Colour(0xff140d09));
    g.fillRoundedRectangle(outer, 4.0f);

    const float pad = shouldDrawButtonAsDown ? 3.5f : 2.5f;
    const auto cap = outer.reduced(pad);
    const float capR = 3.5f;

    if (lit)
    {
        juce::ColourGradient bulbGrad(juce::Colour(0xfffff8d6), cap.getCentreX(), cap.getCentreY() * 0.98f,
                                      juce::Colour(0xffeb8d36), cap.getX(), cap.getBottom(), true);
        bulbGrad.addColour(0.55, juce::Colour(0xffffcb74));
        g.setGradientFill(bulbGrad);
        g.fillRoundedRectangle(cap, capR);

        g.setColour(juce::Colour(0xffffe9b8));
        g.drawRoundedRectangle(cap, capR, 1.2f);
        g.setColour(juce::Colour(0xffd67628).withAlpha(0.55f));
        g.drawRoundedRectangle(cap.reduced(1.2f), capR - 0.8f, 0.9f);
    }
    else
    {
        const float boost = shouldDrawButtonAsHighlighted ? 0.06f : 0.0f;
        juce::ColourGradient milkyGrad(juce::Colour(0xfff0eadc).brighter(boost), cap.getX(), cap.getY(),
                                       juce::Colour(0xffcfc5b0).brighter(boost), cap.getX(), cap.getBottom(), false);
        g.setGradientFill(milkyGrad);
        g.fillRoundedRectangle(cap, capR);

        g.setColour(juce::Colour(0xfffffcf3));
        g.drawRoundedRectangle(cap, capR, 1.0f);
        g.setColour(juce::Colour(0xff9c8f78).withAlpha(0.45f));
        g.drawRoundedRectangle(cap.reduced(1.2f), capR - 0.8f, 0.8f);
    }

    drawCobaltNoteSymbol(g, cap.reduced(cap.getWidth() * 0.11f));
}

//==============================================================================
// DXAuditionPad
//==============================================================================
namespace
{

juce::String formatIsoNoteName(int midiNote)
{
    static constexpr std::array<const char*, 12> kNoteNames = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int clamped = std::clamp(midiNote, 0, 127);
    const int pc = clamped % 12;
    const int octave = (clamped / 12) - 1;
    return juce::String(kNoteNames[static_cast<std::size_t>(pc)]) + juce::String(octave);
}

} // namespace

DXAuditionPad::DXAuditionPad(const juce::String& voiceLabel, int gmMidiNote)
    : voiceLabel_(voiceLabel),
      gmMidiNote_(gmMidiNote)
{
}

bool DXAuditionPad::decayFlash() noexcept
{
    if (flashLevel_ <= 0.0f)
    {
        return false;
    }
    flashLevel_ = std::max(0.0f, flashLevel_ - 0.14f);
    repaint();
    return true;
}

void DXAuditionPad::paintButton(juce::Graphics& g,
                                bool shouldDrawButtonAsHighlighted,
                                bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat().reduced(1.0f);
    const float level = shouldDrawButtonAsDown ? 1.0f : flashLevel_;

    juce::Colour bg = juce::Colour(0xff261a14);
    if (shouldDrawButtonAsHighlighted)
    {
        bg = bg.brighter(0.14f);
    }
    if (level > 0.01f)
    {
        bg = bg.interpolatedWith(dx_palette::amberGlow, 0.40f * level);
    }

    g.setColour(bg);
    g.fillRoundedRectangle(r, 3.5f);

    const juce::Colour stroke = (level > 0.05f)
                                    ? dx_palette::amberGlow.interpolatedWith(dx_palette::silkActive, 0.35f)
                                    : juce::Colour(shouldDrawButtonAsHighlighted ? 0xff9d7f66 : 0xff6b5240);
    g.setColour(stroke);
    g.drawRoundedRectangle(r, 3.5f, level > 0.05f ? 1.4f : 1.0f);

    const auto inner = r.toNearestInt().reduced(2, 3);
    auto topArea = inner;
    const auto botArea = topArea.removeFromBottom(12);

    g.setColour(level > 0.25f ? dx_palette::silkActive : dx_palette::silkCream);
    g.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::bold)));
    g.drawFittedText(voiceLabel_, topArea, juce::Justification::centred, 1, 0.72f);

    g.setColour(level > 0.25f ? dx_palette::silkActive : dx_palette::silkMuted);
    g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
    g.drawFittedText(formatIsoNoteName(gmMidiNote_), botArea, juce::Justification::centred, 1);
}

//==============================================================================
// DXRhythmboxPanel
//==============================================================================
DXRhythmboxPanel::DXRhythmboxPanel(juce::AudioProcessorValueTreeState& apvts,
                                   AuditionCallback onAuditionVoice,
                                   FillInCallback onTriggerFillIn,
                                   HoldCallback onStandaloneStartHeld,
                                   HoldCallback onStandaloneStopHeld,
                                   TelemetryCallback getTelemetry,
                                   MidiFlashConsumeCallback consumeMidiFlash,
                                   StatusFlagsCallback getStatusFlags,
                                   bool isStandalone)
    : apvts_(apvts),
      auditionFn_(std::move(onAuditionVoice)),
      fillInFn_(std::move(onTriggerFillIn)),
      standaloneStartFn_(std::move(onStandaloneStartHeld)),
      standaloneStopFn_(std::move(onStandaloneStopHeld)),
      telemetryFn_(std::move(getTelemetry)),
      consumeMidiFlashFn_(std::move(consumeMidiFlash)),
      statusFlagsFn_(std::move(getStatusFlags)),
      isStandalone_(isStandalone)
{
    setLookAndFeel(&lookAndFeel_);

    // 1. 2x8 Rhythm Pattern Matrix Buttons (Row 0 then Row 1 so front Row 1 cleanly occludes Row 0 base in Z-order)
    for (std::size_t idx = 0; idx < dx701::dsp::kNumButtons; ++idx)
    {
        const std::size_t col = idx % 8u;
        const auto capCol = ((col % 2u) == 0u)
                                ? DXConcavePushButton::CapColor::Ivory
                                : DXConcavePushButton::CapColor::Espresso;
        auto btn = std::make_unique<DXConcavePushButton>(
            capCol,
            false,
            [this, idx](bool isDown)
            {
                handlePatternPressState(idx, isDown);
            });
        btn->setTooltip(juce::String(dx701::plugin::param_ids::kButtonParamNames[idx]));
        addAndMakeVisible(*btn);
        buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, dx701::plugin::param_ids::kButtonParamIds[idx], *btn));
        btn->onClick = [this, idx]() { handlePatternButtonClick(idx); };
        patternButtons_[idx] = std::move(btn);
    }

    // 2. Upper Right Action Switches: BREAK (Charcoal, Left) + RHYTHMER (VARIATION Espresso, RHYTHM Red)
    breakBtn_ = std::make_unique<DXConcavePushButton>(DXConcavePushButton::CapColor::Charcoal, false);
    breakBtn_->setTooltip("BREAK: When ON, rhythm starts from Beat 1 only while MIDI key or Standalone START is held");
    addAndMakeVisible(*breakBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, dx701::plugin::param_ids::dxBreak, *breakBtn_));

    variationBtn_ = std::make_unique<DXConcavePushButton>(DXConcavePushButton::CapColor::Espresso, false);
    variationBtn_->setTooltip("VARIATION: Select inner silkscreen rhythm bank");
    addAndMakeVisible(*variationBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, dx701::plugin::param_ids::dxVariation, *variationBtn_));

    rhythmBtn_ = std::make_unique<DXConcavePushButton>(DXConcavePushButton::CapColor::Red, false);
    rhythmBtn_->setTooltip("RHYTHM: Enable / Disable rhythm box function (does not auto-start)");
    addAndMakeVisible(*rhythmBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, dx701::plugin::param_ids::dxEnabled, *rhythmBtn_));

    // Standalone-only cream mini-Fill-In Play / Stop buttons right of RHYTHMER
    standaloneStartBtn_ = std::make_unique<DXTransportButton>(
        "START",
        DXTransportButton::IconType::Play,
        [this](bool held)
        {
            if (standaloneStartFn_)
            {
                standaloneStartFn_(held);
            }
        });
    standaloneStartBtn_->setTooltip("Start Rhythm (Play): Triggers armed rhythm or holds Beat 1 playback in BREAK mode");
    addChildComponent(*standaloneStartBtn_);
    standaloneStartBtn_->setVisible(isStandalone_);

    standaloneStopBtn_ = std::make_unique<DXTransportButton>(
        "STOP",
        DXTransportButton::IconType::Stop,
        [this](bool held)
        {
            if (standaloneStopFn_)
            {
                standaloneStopFn_(held);
            }
        });
    standaloneStopBtn_->setTooltip("Stop Rhythm: Stops running rhythm & inhibits start while held");
    addChildComponent(*standaloneStopBtn_);
    standaloneStopBtn_->setVisible(isStandalone_);

    // 3. 3 Vertical DX701 Sliders: TONE, VOLUME, TEMPO (VOLUME & TEMPO inverted)
    for (juce::Slider* sl : std::array<juce::Slider*, 3>{&toneSlider_, &volumeSlider_, &bpmSlider_})
    {
        sl->setLookAndFeel(&lookAndFeel_);
        sl->setSliderStyle(juce::Slider::LinearVertical);
        sl->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(*sl);
    }
    toneSlider_.setDoubleClickReturnValue(true, 0.50);
    volumeSlider_.setDoubleClickReturnValue(true, 0.85);
    bpmSlider_.setDoubleClickReturnValue(true, 112.2);

    sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, dx701::plugin::param_ids::dxTone, toneSlider_));
    sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, dx701::plugin::param_ids::dxVolume, volumeSlider_));
    sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, dx701::plugin::param_ids::dxBpm, bpmSlider_));

    bpmInput_.setLookAndFeel(&lookAndFeel_);
    bpmInput_.setMultiLine(false, false);
    bpmInput_.setReturnKeyStartsNewLine(false);
    bpmInput_.setJustification(juce::Justification::centred);
    bpmInput_.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::bold)));
    bpmInput_.setIndents(2, 0);
    bpmInput_.setInputRestrictions(7, "0123456789.");
    bpmInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff140d09));
    bpmInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff6b5240));
    bpmInput_.setColour(juce::TextEditor::focusedOutlineColourId, dx_palette::amberGlow);
    bpmInput_.setColour(juce::TextEditor::textColourId, dx_palette::silkCream);
    bpmInput_.setColour(juce::TextEditor::highlightColourId, dx_palette::amberGlow.withAlpha(0.35f));
    bpmInput_.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    bpmInput_.setColour(juce::CaretComponent::caretColourId, dx_palette::amberGlow);
    bpmInput_.setTooltip("Direct BPM input (40.0 - 300.0 BPM)");
    bpmInput_.onReturnKey = [this]()
    {
        commitBpmInput();
        unfocusAllComponents();
    };
    bpmInput_.onFocusLost = [this]()
    {
        commitBpmInput();
    };
    bpmInput_.onEscapeKey = [this]()
    {
        syncBpmInputText(true);
        unfocusAllComponents();
    };
    bpmSlider_.onValueChange = [this]()
    {
        syncBpmInputText(false);
    };
    addAndMakeVisible(bpmInput_);
    syncBpmInputText(true);

    // 4. 4 Self-Illuminating Drum Fill In Buttons (Hardware L->R order: FillIn4, FillIn3, FillIn2, FillIn1)
    constexpr std::array<dx701::dsp::FillInId, dx701::dsp::kNumFillIns> kUiSlotToFillIn = {{
        dx701::dsp::FillInId::FillIn4,
        dx701::dsp::FillInId::FillIn3,
        dx701::dsp::FillInId::FillIn2,
        dx701::dsp::FillInId::FillIn1
    }};
    constexpr std::array<DXFillInButton::NoteSymbol, dx701::dsp::kNumFillIns> kUiSlotSymbols = {{
        DXFillInButton::NoteSymbol::TripletSixteenth,
        DXFillInButton::NoteSymbol::TripletEighth,
        DXFillInButton::NoteSymbol::DoubleSixteenth,
        DXFillInButton::NoteSymbol::DoubleEighth
    }};
    constexpr std::array<const char*, dx701::dsp::kNumFillIns> kUiSlotTips = {{
        "Drum Fill In 4 (16th-note Triplets, 24 steps/bar)",
        "Drum Fill In 3 (8th-note Triplets, 12 steps/bar)",
        "Drum Fill In 2 (16th Notes, 16 steps/bar)",
        "Drum Fill In 1 (8th Notes, 8 steps/bar)"
    }};

    for (std::size_t slot = 0; slot < dx701::dsp::kNumFillIns; ++slot)
    {
        const auto fid = kUiSlotToFillIn[slot];
        auto fb = std::make_unique<DXFillInButton>(juce::String("FillIn") + juce::String(static_cast<int>(fid) + 1),
                                                   kUiSlotSymbols[slot]);
        fb->setTooltip(kUiSlotTips[slot]);
        auto* rawFb = fb.get();
        fb->onClick = [this, fid, rawFb]()
        {
            rawFb->triggerClickPulse();
            // Owner's Manual Intro: pressing Fill-In with a rhythm selected & BREAK OFF latches Rhythm ON cleanly
            const bool breakOn = breakBtn_ && breakBtn_->getToggleState();
            if (activeButtonCount_ > 0 && !breakOn && !statusFlags_.stopSwitchHeld && rhythmBtn_ && !rhythmBtn_->getToggleState())
            {
                setParameterBoolNotifyingHost(dx701::plugin::param_ids::dxEnabled, true);
            }
            if (fillInFn_)
            {
                fillInFn_(fid);
            }
        };
        addAndMakeVisible(*fb);
        fillInButtons_[slot] = std::move(fb);
    }

    // 5. Utility Strip Controls
    multiPatternBtn_.setLookAndFeel(&lookAndFeel_);
    multiPatternBtn_.setTooltip("Combine rhythm buttons via Wired-OR");
    addAndMakeVisible(multiPatternBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, dx701::plugin::param_ids::dxMultiPattern, multiPatternBtn_));
    multiPatternBtn_.onClick = [this]() { handleMultiPatternToggle(); };

    syncLbl_.setLookAndFeel(&lookAndFeel_);
    syncLbl_.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    syncLbl_.setJustificationType(juce::Justification::centredRight);
    syncLbl_.setColour(juce::Label::textColourId, dx_palette::silkCream);
    addAndMakeVisible(syncLbl_);

    syncModeBox_.setLookAndFeel(&lookAndFeel_);
    syncModeBox_.addItemList(dx701::plugin::makeSyncModeChoices(), 1);
    addAndMakeVisible(syncModeBox_);
    comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts_, dx701::plugin::param_ids::dxSyncMode, syncModeBox_));

    midiChLbl_.setLookAndFeel(&lookAndFeel_);
    midiChLbl_.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    midiChLbl_.setJustificationType(juce::Justification::centredRight);
    midiChLbl_.setColour(juce::Label::textColourId, dx_palette::silkCream);
    addAndMakeVisible(midiChLbl_);

    midiChBox_.setLookAndFeel(&lookAndFeel_);
    midiChBox_.addItemList(dx701::plugin::makeMidiChannelChoices(), 1);
    addAndMakeVisible(midiChBox_);
    comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts_, dx701::plugin::param_ids::dxMidiCh, midiChBox_));

    driftLbl_.setLookAndFeel(&lookAndFeel_);
    driftLbl_.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    driftLbl_.setJustificationType(juce::Justification::centredRight);
    driftLbl_.setColour(juce::Label::textColourId, dx_palette::silkCream);
    addAndMakeVisible(driftLbl_);

    driftSlider_.setLookAndFeel(&lookAndFeel_);
    driftSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 18);
    driftSlider_.setDoubleClickReturnValue(true, 1.0);
    addAndMakeVisible(driftSlider_);
    sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, dx701::plugin::param_ids::dxAnalogDrift, driftSlider_));

    // 6. Bottom 16 GM Drum Voice Audition Pads
    for (std::size_t v = 0; v < dx701::dsp::kNumVoices; ++v)
    {
        const auto voiceId = static_cast<dx701::dsp::VoiceId>(v);
        auto pad = std::make_unique<DXAuditionPad>(dx701::dsp::kVoiceDisplayNames[v],
                                                   dx701::dsp::kPrimaryGmMidiNotes[v]);
        auto* rawPad = pad.get();
        const int noteNum = dx701::dsp::kPrimaryGmMidiNotes[v];
        pad->setTooltip(juce::String(dx701::dsp::kVoiceNames[v]) + " - " + formatIsoNoteName(noteNum) +
                        " (MIDI Note " + juce::String(noteNum) + ")");
        pad->onClick = [this, voiceId, rawPad]()
        {
            rawPad->triggerFlash();
            if (auditionFn_)
            {
                auditionFn_(voiceId, 1.0f);
            }
        };
        addAndMakeVisible(*pad);
        padButtons_[v] = std::move(pad);
    }

    refreshTelemetryNow();
    startTimerHz(30);
}

DXRhythmboxPanel::~DXRhythmboxPanel()
{
    stopTimer();
    multiPatternBtn_.setLookAndFeel(nullptr);
    syncLbl_.setLookAndFeel(nullptr);
    syncModeBox_.setLookAndFeel(nullptr);
    midiChLbl_.setLookAndFeel(nullptr);
    midiChBox_.setLookAndFeel(nullptr);
    driftLbl_.setLookAndFeel(nullptr);
    driftSlider_.setLookAndFeel(nullptr);
    toneSlider_.setLookAndFeel(nullptr);
    volumeSlider_.setLookAndFeel(nullptr);
    bpmSlider_.setLookAndFeel(nullptr);
    bpmInput_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void DXRhythmboxPanel::rebuildWoodgrainCache()
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0)
    {
        return;
    }

    woodgrainBg_ = juce::Image(juce::Image::ARGB, w, h, true);
    juce::Graphics ig(woodgrainBg_);

    juce::ColourGradient baseGrad(dx_palette::walnutTop, 0.0f, 0.0f,
                                  dx_palette::walnutBot, 0.0f, static_cast<float>(h), false);
    ig.setGradientFill(baseGrad);
    ig.fillAll();

    // Organic horizontal walnut woodgrain stripes across the entire panel
    for (int yPos = 0; yPos < h; ++yPos)
    {
        const float yn = static_cast<float>(yPos) / static_cast<float>(h);
        const float wave = 0.48f * std::sin(yn * 86.0f)
                         + 0.30f * std::sin(yn * 219.0f + 1.1f)
                         + 0.22f * std::cos(yn * 423.0f + 2.4f);
        if (wave > 0.08f)
        {
            ig.setColour(juce::Colour(0xff6f4c34).withAlpha(0.11f * wave));
            ig.drawHorizontalLine(yPos, 0.0f, static_cast<float>(w));
        }
        else if (wave < -0.08f)
        {
            ig.setColour(juce::Colour(0xff190f0a).withAlpha(0.15f * (-wave)));
            ig.drawHorizontalLine(yPos, 0.0f, static_cast<float>(w));
        }
    }

    juce::ColourGradient vig(juce::Colours::transparentBlack, static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.42f,
                             juce::Colour(0x46100906), static_cast<float>(w) * 0.96f, static_cast<float>(h) * 0.96f, true);
    ig.setGradientFill(vig);
    ig.fillAll();

    ig.setColour(dx_palette::bezelEdge.withAlpha(0.60f));
    ig.drawRect(0, 0, w, h, 2);
}

void DXRhythmboxPanel::setParameterBoolNotifyingHost(const char* paramId, bool value)
{
    if (auto* param = apvts_.getParameter(paramId))
    {
        const float target = value ? 1.0f : 0.0f;
        if (std::abs(param->getValue() - target) > 0.01f)
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(target);
            param->endChangeGesture();
        }
    }
}

void DXRhythmboxPanel::applyPatternMaskToHost(std::uint16_t mask)
{
    const std::uint16_t capped = dx701::dsp::capButtonMask(mask, multiPatternBtn_.getToggleState());
    for (std::size_t j = 0; j < dx701::dsp::kNumButtons; ++j)
    {
        const bool on = (capped & static_cast<std::uint16_t>(1u << j)) != 0u;
        setParameterBoolNotifyingHost(dx701::plugin::param_ids::kButtonParamIds[j], on);
        patternButtons_[j]->setToggleState(on, juce::dontSendNotification);
    }
}

void DXRhythmboxPanel::handlePatternPressState(std::size_t idx, bool isPressed)
{
    const std::uint16_t bit = static_cast<std::uint16_t>(1u << idx);
    if (isPressed)
    {
        heldPatternMask_ |= bit;
        if (multiPatternBtn_.getToggleState() && std::popcount(heldPatternMask_) >= 2)
        {
            chordGestureActive_ = true;
            chordPatternMask_ = dx701::dsp::capButtonMask(
                static_cast<std::uint16_t>(chordPatternMask_ | heldPatternMask_), true);
            if (!updatingPatterns_)
            {
                const juce::ScopedValueSetter<bool> guard(updatingPatterns_, true);
                applyPatternMaskToHost(chordPatternMask_);
            }
        }
    }
    else
    {
        heldPatternMask_ = static_cast<std::uint16_t>(heldPatternMask_ & static_cast<std::uint16_t>(~bit));
        if (heldPatternMask_ == 0u)
        {
            chordGestureActive_ = false;
            chordPatternMask_ = 0u;
        }
    }
}

void DXRhythmboxPanel::handlePatternButtonClick(std::size_t clickedIndex)
{
    if (updatingPatterns_)
    {
        return;
    }
    const juce::ScopedValueSetter<bool> guard(updatingPatterns_, true);

    if (chordGestureActive_)
    {
        applyPatternMaskToHost(chordPatternMask_);
        return;
    }

    if (!multiPatternBtn_.getToggleState())
    {
        const bool nowOn = patternButtons_[clickedIndex]->getToggleState();
        const std::uint16_t targetMask = nowOn ? static_cast<std::uint16_t>(1u << clickedIndex) : 0u;
        applyPatternMaskToHost(targetMask);
        return;
    }

    // Multi-Pattern mode: enforce hardware Wired-OR limit of 4 simultaneous buttons
    int count = 0;
    for (std::size_t j = 0; j < dx701::dsp::kNumButtons; ++j)
    {
        if (patternButtons_[j]->getToggleState())
        {
            ++count;
        }
    }
    if (count > static_cast<int>(dx701::dsp::kMaxSimultaneousPatterns) &&
        patternButtons_[clickedIndex]->getToggleState())
    {
        setParameterBoolNotifyingHost(dx701::plugin::param_ids::kButtonParamIds[clickedIndex], false);
        patternButtons_[clickedIndex]->setToggleState(false, juce::dontSendNotification);
    }
}

void DXRhythmboxPanel::handleMultiPatternToggle()
{
    if (updatingPatterns_)
    {
        return;
    }
    const juce::ScopedValueSetter<bool> guard(updatingPatterns_, true);

    if (!multiPatternBtn_.getToggleState())
    {
        std::uint16_t targetMask = 0u;
        for (std::size_t j = 0; j < dx701::dsp::kNumButtons; ++j)
        {
            if (patternButtons_[j]->getToggleState())
            {
                targetMask = static_cast<std::uint16_t>(1u << j);
                break;
            }
        }
        applyPatternMaskToHost(targetMask);
    }
}

void DXRhythmboxPanel::setStandaloneVisible(bool standalone)
{
    if (isStandalone_ != standalone)
    {
        isStandalone_ = standalone;
        if (standaloneStartBtn_)
        {
            standaloneStartBtn_->setVisible(isStandalone_);
        }
        if (standaloneStopBtn_)
        {
            standaloneStopBtn_->setVisible(isStandalone_);
        }
        resized();
        repaint();
    }
}

void DXRhythmboxPanel::syncBpmInputText(bool force)
{
    if (!force && bpmInput_.hasKeyboardFocus(true))
    {
        return;
    }

    const bool hostBpm = telemetry_.lamp.usingHostBpm;
    const double shownBpm = hostBpm ? telemetry_.lamp.effectiveBpm : bpmSlider_.getValue();
    const juce::String desiredText = hostBpm ? ("EXT " + juce::String(shownBpm, 1)) : juce::String(shownBpm, 1);
    const juce::Colour desiredCol = hostBpm ? dx_palette::amberGlow : dx_palette::silkCream;

    if (bpmInput_.isReadOnly() != hostBpm)
    {
        bpmInput_.setReadOnly(hostBpm);
    }
    if (bpmInput_.getText() != desiredText)
    {
        bpmInput_.setText(desiredText, juce::dontSendNotification);
    }
    if (bpmInput_.findColour(juce::TextEditor::textColourId) != desiredCol)
    {
        bpmInput_.setColour(juce::TextEditor::textColourId, desiredCol);
        bpmInput_.applyColourToAllText(desiredCol, true);
    }
}

void DXRhythmboxPanel::commitBpmInput()
{
    if (bpmInput_.isReadOnly())
    {
        syncBpmInputText(true);
        return;
    }

    const juce::String raw = bpmInput_.getText().trim();
    if (raw.isNotEmpty())
    {
        const double parsed = raw.getDoubleValue();
        if (std::isfinite(parsed) && parsed > 0.0)
        {
            const double clamped = std::clamp(parsed, 40.0, 300.0);
            bpmSlider_.setValue(clamped, juce::sendNotificationSync);
        }
    }
    syncBpmInputText(true);
}

void DXRhythmboxPanel::refreshTelemetryNow()
{
    timerCallback();
}

void DXRhythmboxPanel::timerCallback()
{
    if (telemetryFn_)
    {
        telemetry_ = telemetryFn_();
    }
    if (statusFlagsFn_)
    {
        statusFlags_ = statusFlagsFn_();
    }
    syncBpmInputText(false);

    if (consumeMidiFlashFn_)
    {
        const std::uint32_t midiMask = consumeMidiFlashFn_();
        if (midiMask != 0u)
        {
            for (std::size_t v = 0; v < dx701::dsp::kNumVoices; ++v)
            {
                if ((midiMask & (1u << static_cast<unsigned>(v))) != 0u && padButtons_[v])
                {
                    padButtons_[v]->triggerFlash();
                }
            }
        }
    }

    const float rms = static_cast<float>(telemetry_.outputRms);
    const float norm = (rms > 1e-6f) ? juce::jlimit(0.0f, 1.0f, (20.0f * std::log10(rms) + 54.0f) / 54.0f) : 0.0f;
    meterSmooth_ = std::max(norm, meterSmooth_ * 0.78f);
    meterDb_ = (rms > 1e-6f) ? std::max(-96.0f, 20.0f * std::log10(rms)) : -96.0f;

    const bool newVar = variationBtn_ ? variationBtn_->getToggleState() : false;
    if (newVar != variationActive_)
    {
        variationActive_ = newVar;
        repaint(matrixFrameBounds_);
        repaint(variationGuideBounds_);
    }

    int countActive = 0;
    for (std::size_t j = 0; j < dx701::dsp::kNumButtons; ++j)
    {
        if (patternButtons_[j] && patternButtons_[j]->getToggleState())
        {
            ++countActive;
        }
    }
    activeButtonCount_ = countActive;

    // Update 4 Drum Fill In internal bulb glow state (slots 0..3 map to FillIn 4, 3, 2, 1)
    constexpr std::array<int, dx701::dsp::kNumFillIns> kSlotToFillIdx = {3, 2, 1, 0};
    for (std::size_t slot = 0; slot < dx701::dsp::kNumFillIns; ++slot)
    {
        if (fillInButtons_[slot])
        {
            fillInButtons_[slot]->stepPulse();
            const bool lit = telemetry_.lamp.fillInActive && (telemetry_.lamp.activeFillInIndex == kSlotToFillIdx[slot]);
            fillInButtons_[slot]->setIlluminated(lit);
        }
    }

    for (auto& pad : padButtons_)
    {
        if (pad)
        {
            pad->decayFlash();
        }
    }

    repaint(tempoLampBounds_);
    repaint(middleDeckBounds_);
    repaint(utilityBarBounds_);
}

void DXRhythmboxPanel::drawRow0Labels(juce::Graphics& g, std::size_t col, juce::Rectangle<int> slice) const
{
    const juce::Colour outerCol = !variationActive_ ? dx_palette::silkActive : dx_palette::silkMuted.withAlpha(0.70f);
    const juce::Colour innerCol = variationActive_ ? dx_palette::silkActive : dx_palette::silkMuted.withAlpha(0.70f);
    const juce::Font fontOuter(juce::FontOptions(10.0f, juce::Font::bold));
    const juce::Font fontInner(juce::FontOptions(9.5f, juce::Font::bold));

    const auto outerRow = slice.withY(row0OuterRowBounds_.getY()).withHeight(row0OuterRowBounds_.getHeight());
    const auto innerRow = slice.withY(row0InnerRowBounds_.getY()).withHeight(row0InnerRowBounds_.getHeight());

    if (col == 0u)
    {
        g.setFont(fontOuter);
        g.setColour(outerCol);
        g.drawFittedText("WALTZ", outerRow, juce::Justification::centred, 1, 0.72f);
        g.setFont(juce::FontOptions(8.8f, juce::Font::bold));
        g.setColour(innerCol);
        g.drawFittedText("JAZZ WALTZ", innerRow, juce::Justification::centred, 1, 0.64f);
        return;
    }
    if (col == 4u)
    {
        g.setFont(fontOuter);
        g.setColour(outerCol);
        g.drawFittedText("SHUFFLE", outerRow, juce::Justification::centred, 1, 0.72f);
        g.setFont(fontInner);
        g.setColour(innerCol);
        g.drawFittedText("BOLERO", innerRow, juce::Justification::centred, 1, 0.72f);
        return;
    }

    struct PrefixNum
    {
        const char* prefix;
        const char* numOuter;
        const char* numInner;
    };
    const PrefixNum mapCol[8] = {
        {"", "", ""},
        {"BALLAD", "I", "II"},
        {"SWING", "I", "II"},
        {"SWING", "III", "IV"},
        {"", "", ""},
        {"ROCK", "I", "II"},
        {"ROCK", "III", "IV"},
        {"DISCO", "I", "II"}
    };
    const auto& entry = mapCol[col];
    const auto fullBand = slice.withY(outerRow.getY()).withBottom(innerRow.getBottom()).reduced(2, 0);
    const int numW = std::clamp(fullBand.getWidth() / 4, 14, 18);
    const auto leftPrefix = fullBand.withTrimmedRight(numW);
    const auto rightNumsOuter = outerRow.withLeft(leftPrefix.getRight());
    const auto rightNumsInner = innerRow.withLeft(leftPrefix.getRight());

    g.setFont(juce::Font(juce::FontOptions(9.6f, juce::Font::bold)));
    g.setColour(dx_palette::silkCream);
    g.drawFittedText(entry.prefix, leftPrefix, juce::Justification::centredRight, 1, 0.68f);

    g.setFont(fontOuter);
    g.setColour(outerCol);
    g.drawFittedText(entry.numOuter, rightNumsOuter, juce::Justification::centred, 1, 0.75f);

    g.setFont(fontInner);
    g.setColour(innerCol);
    g.drawFittedText(entry.numInner, rightNumsInner, juce::Justification::centred, 1, 0.75f);
}

void DXRhythmboxPanel::drawRow1Labels(juce::Graphics& g, std::size_t col, juce::Rectangle<int> slice) const
{
    const juce::Colour innerCol = variationActive_ ? dx_palette::silkActive : dx_palette::silkMuted.withAlpha(0.70f);
    const juce::Colour outerCol = !variationActive_ ? dx_palette::silkActive : dx_palette::silkMuted.withAlpha(0.70f);
    const juce::Font fontOuter(juce::FontOptions(10.0f, juce::Font::bold));
    const juce::Font fontInner(juce::FontOptions(9.5f, juce::Font::bold));

    const auto innerRow = slice.withY(row1InnerRowBounds_.getY()).withHeight(row1InnerRowBounds_.getHeight());
    const auto outerRow = slice.withY(row1OuterRowBounds_.getY()).withHeight(row1OuterRowBounds_.getHeight());

    if (col >= 2u && col <= 5u)
    {
        static constexpr std::array<const char*, 4> kInner = {"HABANERA", "BAION", "MAMBO", "RHUMBA"};
        static constexpr std::array<const char*, 4> kOuter = {"TANGO", "CHA-CHA", "SAMBA", "BEGUINE"};
        g.setFont(fontInner);
        g.setColour(innerCol);
        g.drawFittedText(kInner[col - 2u], innerRow, juce::Justification::centred, 1, 0.66f);
        g.setFont(fontOuter);
        g.setColour(outerCol);
        g.drawFittedText(kOuter[col - 2u], outerRow, juce::Justification::centred, 1, 0.70f);
        return;
    }

    struct BotPrefixNum
    {
        const char* prefixTop;
        const char* prefixBot;
        const char* numInner;
        const char* numOuter;
    };
    BotPrefixNum entry{};
    if (col == 0u)
    {
        entry = {"MARCH", nullptr, "II", "I"};
    }
    else if (col == 1u)
    {
        entry = {"MARCH", nullptr, "IV", "III"};
    }
    else if (col == 6u)
    {
        entry = {"BOSSA-", "NOVA", "II", "I"};
    }
    else
    {
        entry = {"SOUL", nullptr, "II", "I"};
    }

    const auto fullBand = slice.withY(innerRow.getY()).withBottom(outerRow.getBottom()).reduced(2, 0);
    const int numW = std::clamp(fullBand.getWidth() / 4, 14, 18);
    const auto leftPrefix = fullBand.withTrimmedRight(numW);
    const auto rightNumsInner = innerRow.withLeft(leftPrefix.getRight());
    const auto rightNumsOuter = outerRow.withLeft(leftPrefix.getRight());

    g.setColour(dx_palette::silkCream);
    if (entry.prefixBot != nullptr)
    {
        g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
        g.drawFittedText(entry.prefixTop, leftPrefix.withHeight(leftPrefix.getHeight() / 2), juce::Justification::centredRight, 1, 0.70f);
        g.drawFittedText(entry.prefixBot, leftPrefix.withTrimmedTop(leftPrefix.getHeight() / 2), juce::Justification::centredRight, 1, 0.70f);
    }
    else
    {
        g.setFont(juce::Font(juce::FontOptions(9.6f, juce::Font::bold)));
        g.drawFittedText(entry.prefixTop, leftPrefix, juce::Justification::centredRight, 1, 0.70f);
    }

    g.setFont(fontInner);
    g.setColour(innerCol);
    g.drawFittedText(entry.numInner, rightNumsInner, juce::Justification::centred, 1, 0.75f);

    g.setFont(fontOuter);
    g.setColour(outerCol);
    g.drawFittedText(entry.numOuter, rightNumsOuter, juce::Justification::centred, 1, 0.75f);
}

void DXRhythmboxPanel::drawTempoJewelLamp(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    const auto area = bounds.toFloat();
    const float lampR = std::clamp(std::min(area.getWidth(), area.getHeight() - 18.0f) * 0.31f, 9.0f, 15.0f);
    const float lx = area.getCentreX();
    const float ly = area.getCentreY() - 6.0f;
    const bool hasPattern = (telemetry_.lamp.totalSteps > 0);
    const bool isFlash = hasPattern && telemetry_.lamp.beatFlash;

    if (isFlash)
    {
        const float haloAlpha = telemetry_.lamp.isDownbeat ? 0.52f : 0.32f;
        const float haloR = lampR * 1.65f;
        juce::ColourGradient haloGrad(dx_palette::rubyOnCore.withAlpha(haloAlpha), lx, ly,
                                      dx_palette::rubyOnCore.withAlpha(0.0f), lx + haloR, ly, true);
        g.setGradientFill(haloGrad);
        g.fillEllipse(lx - haloR, ly - haloR, haloR * 2.0f, haloR * 2.0f);
    }

    g.setColour(juce::Colour(0xff140c09));
    g.fillEllipse(lx - lampR - 3.0f, ly - lampR - 3.0f, (lampR + 3.0f) * 2.0f, (lampR + 3.0f) * 2.0f);
    g.setColour(juce::Colour(0xff8f7864));
    g.drawEllipse(lx - lampR - 2.0f, ly - lampR - 2.0f, (lampR + 2.0f) * 2.0f, (lampR + 2.0f) * 2.0f, 1.1f);

    if (isFlash)
    {
        juce::ColourGradient jewelGrad(dx_palette::rubySpec, lx - lampR * 0.25f, ly - lampR * 0.30f,
                                       dx_palette::rubyOnEdge, lx + lampR * 0.65f, ly + lampR * 0.65f, true);
        jewelGrad.addColour(0.45, dx_palette::rubyOnCore);
        g.setGradientFill(jewelGrad);
        g.fillEllipse(lx - lampR, ly - lampR, lampR * 2.0f, lampR * 2.0f);
    }
    else
    {
        juce::ColourGradient dimGrad(juce::Colour(0xff781721), lx - lampR * 0.25f, ly - lampR * 0.25f,
                                     dx_palette::rubyOff, lx + lampR * 0.5f, ly + lampR * 0.5f, true);
        g.setGradientFill(dimGrad);
        g.fillEllipse(lx - lampR, ly - lampR, lampR * 2.0f, lampR * 2.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(isFlash ? 0.55f : 0.16f));
    g.drawEllipse(lx - lampR * 0.60f, ly - lampR * 0.60f, lampR * 1.20f, lampR * 1.20f, 0.9f);
    g.fillEllipse(lx - lampR * 0.42f, ly - lampR * 0.52f, lampR * 0.38f, lampR * 0.28f);

    const auto lblArea = juce::Rectangle<int>(bounds.getX(), static_cast<int>(ly + lampR + 4.0f), bounds.getWidth(), 14);
    g.setColour(dx_palette::silkCream);
    g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
    g.drawFittedText("TEMPO", lblArea, juce::Justification::centred, 1);
}

void DXRhythmboxPanel::drawVariationGuide(juce::Graphics& g, bool varActive) const
{
    if (variationGuideBounds_.getWidth() < 48 || variationGuideBounds_.getHeight() < 56)
    {
        return;
    }

    // Authentic DX701 VARIATION pill badge centered vertically inside the dedicated corridor
    // between the 2x8 matrix and the RHYTHMER/BREAK block, with zero line overlaps.
    const int pillW = std::min(78, variationGuideBounds_.getWidth() - 16);
    const int pillH = 17;
    const float pillX = static_cast<float>(variationGuideBounds_.getCentreX()) - static_cast<float>(pillW) * 0.5f + 4.0f;
    const float pillY = static_cast<float>(wellBounds_.getCentreY()) - static_cast<float>(pillH) * 0.5f;
    const juce::Rectangle<float> pill(pillX, pillY, static_cast<float>(pillW), static_cast<float>(pillH));

    g.setColour(dx_palette::silkCream);
    g.fillRoundedRectangle(pill, 7.5f);
    g.setColour(dx_palette::walnutDeep);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawFittedText("VARIATION", pill.toNearestInt(), juce::Justification::centred, 1);

    // Two miniature plunger elevation icons sitting directly above the pill badge
    const float iconW = 10.5f;
    const float depressedH = 4.5f;
    const float raisedH = 8.5f;
    const float leftX = pill.getX() + pill.getWidth() * 0.30f;
    const float rightX = pill.getX() + pill.getWidth() * 0.70f;
    const float iconBotY = pill.getY() - 3.5f;

    const float startX = static_cast<float>(colLabelSlices_[7].getRight() + 4);
    const float r0OutY = static_cast<float>(row0OuterRowBounds_.getCentreY());
    const float r0InY = static_cast<float>(row0InnerRowBounds_.getCentreY());
    const float r1InY = static_cast<float>(row1InnerRowBounds_.getCentreY());
    const float r1OutY = static_cast<float>(row1OuterRowBounds_.getCentreY());

    const juce::Colour cInner = varActive ? dx_palette::silkActive : dx_palette::silkMuted.withAlpha(0.70f);
    const juce::Colour cOuter = !varActive ? dx_palette::silkActive : dx_palette::silkMuted.withAlpha(0.70f);

    // Top inner line -> left depressed glyph
    g.setColour(cInner);
    g.drawLine(startX, r0InY, leftX, r0InY, 1.3f);
    g.drawLine(leftX, r0InY, leftX, iconBotY - depressedH, 1.3f);

    // Top outer line -> right raised glyph
    g.setColour(cOuter);
    g.drawLine(startX, r0OutY, rightX, r0OutY, 1.3f);
    g.drawLine(rightX, r0OutY, rightX, iconBotY - raisedH, 1.3f);

    // Bottom inner line -> bottom-left of VARIATION pill
    g.setColour(cInner);
    g.drawLine(startX, r1InY, leftX, r1InY, 1.3f);
    g.drawLine(leftX, r1InY, leftX, pill.getBottom() + 1.5f, 1.3f);

    // Bottom outer line -> bottom-right of VARIATION pill
    g.setColour(cOuter);
    g.drawLine(startX, r1OutY, rightX, r1OutY, 1.3f);
    g.drawLine(rightX, r1OutY, rightX, pill.getBottom() + 1.5f, 1.3f);

    // Draw miniature mechanical switch icons
    g.setColour(dx_palette::silkCream);
    g.drawLine(leftX - iconW * 0.8f, iconBotY, rightX + iconW * 0.8f, iconBotY, 1.2f);
    const juce::Rectangle<float> depRect(leftX - iconW * 0.5f, iconBotY - depressedH, iconW, depressedH);
    const juce::Rectangle<float> raisedRect(rightX - iconW * 0.5f, iconBotY - raisedH, iconW, raisedH);
    g.setColour(varActive ? dx_palette::silkActive : dx_palette::silkMuted);
    g.fillRect(depRect);
    g.setColour(!varActive ? dx_palette::silkActive : dx_palette::silkMuted);
    g.fillRect(raisedRect);
}

void DXRhythmboxPanel::paint(juce::Graphics& g)
{
    if (woodgrainBg_.getWidth() != getWidth() || woodgrainBg_.getHeight() != getHeight())
    {
        rebuildWoodgrainCache();
    }
    if (woodgrainBg_.isValid())
    {
        g.drawImageAt(woodgrainBg_, 0, 0);
    }
    else
    {
        g.fillAll(dx_palette::walnutTop);
    }

    // 1. Upper Deck: Silkscreen directly on Walnut Woodgrain + Tight Dark Mechanical Cutouts
    {
        // Tight recessed slot cutout around the 2x8 rhythm switches
        g.setColour(dx_palette::wellBg);
        g.fillRoundedRectangle(wellBounds_.toFloat(), 4.0f);
        g.setColour(dx_palette::walnutDeep);
        g.drawRoundedRectangle(wellBounds_.toFloat(), 4.0f, 1.2f);

        for (std::size_t col = 0; col < 8u; ++col)
        {
            drawRow0Labels(g, col, colLabelSlices_[col]);
            drawRow1Labels(g, col, colLabelSlices_[col]);
        }

        // Right Upper: Top VARIATION bracket guide + Tempo Lamp, Bottom RHYTHMER + BREAK
        drawVariationGuide(g, variationActive_);
        drawTempoJewelLamp(g, tempoLampBounds_);

        drawSectionBracket(g, rhythmerBracketBounds_, "RHYTHMER", 9.6f);

        g.setColour(dx_palette::wellBg);
        g.fillRoundedRectangle(breakWellBounds_.toFloat(), 4.0f);
        g.fillRoundedRectangle(rhythmerWellBounds_.toFloat(), 4.0f);
        g.setColour(dx_palette::walnutDeep);
        g.drawRoundedRectangle(breakWellBounds_.toFloat(), 4.0f, 1.1f);
        g.drawRoundedRectangle(rhythmerWellBounds_.toFloat(), 4.0f, 1.1f);
        if (isStandalone_ && !standaloneTransportBezelBounds_.isEmpty())
        {
            g.setColour(dx_palette::wellBg);
            g.fillRoundedRectangle(standaloneTransportBezelBounds_.toFloat(), 4.5f);
            g.setColour(dx_palette::walnutDeep);
            g.drawRoundedRectangle(standaloneTransportBezelBounds_.toFloat(), 4.5f, 1.2f);
        }

        constexpr std::array<const char*, 3> kActionTitles = {"BREAK", "VARIATION", "RHYTHM"};
        for (std::size_t k = 0; k < 3; ++k)
        {
            g.setColour(dx_palette::silkCream);
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
            g.drawFittedText(kActionTitles[k], actionLabelBounds_[k], juce::Justification::centred, 1, 0.70f);
        }
    }

    // Subtle engraved walnut divider rule between Upper Deck and Middle Deck
    {
        const int sepY = (upperDeckBounds_.getBottom() + middleDeckBounds_.getY()) / 2;
        g.setColour(dx_palette::walnutDeep.withAlpha(0.70f));
        g.drawHorizontalLine(sepY, 12.0f, static_cast<float>(getWidth() - 12));
        g.setColour(dx_palette::bezelEdge.withAlpha(0.35f));
        g.drawHorizontalLine(sepY + 1, 12.0f, static_cast<float>(getWidth() - 12));
    }

    // 2. Middle Deck: 3 DX701 Sliders directly on Walnut + Compact 4 Self-Illuminating Drum Fill In
    {
        constexpr std::array<const char*, 3> kSliderHeaders = {"TONE", "VOLUME", "TEMPO"};
        for (std::size_t s = 0; s < 3; ++s)
        {
            g.setColour(dx_palette::silkCream);
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawFittedText(kSliderHeaders[s], sliderHeaderBounds_[s], juce::Justification::centred, 1);
        }

        // Compact recessed bezel tightly framing the 4 Drum Fill In buttons
        g.setColour(dx_palette::wellBg);
        g.fillRoundedRectangle(fillInBezelBounds_.toFloat(), 4.5f);
        g.setColour(dx_palette::walnutDeep);
        g.drawRoundedRectangle(fillInBezelBounds_.toFloat(), 4.5f, 1.3f);

        // Two-line slanted bold cream caption: "Drum" / "Fill In" (matching DX701 photo)
        g.setColour(dx_palette::silkActive);
        auto capTop = fillInCaptionBounds_.withHeight(fillInCaptionBounds_.getHeight() / 2 + 2);
        auto capBot = fillInCaptionBounds_.withTrimmedTop(fillInCaptionBounds_.getHeight() / 2 - 2);
        g.setFont(juce::Font(juce::FontOptions(17.5f, juce::Font::bold | juce::Font::italic)));
        g.drawFittedText("Drum", capTop, juce::Justification::centred, 1);
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold | juce::Font::italic)));
        g.drawFittedText("Fill In", capBot, juce::Justification::centred, 1);
    }

    // 3. Utility Bar: Multi-Pattern count / SYNC / MIDI / DRIFT / Status & Output Meter
    {
        g.setColour(dx_palette::cardBg.withAlpha(0.94f));
        g.fillRoundedRectangle(utilityBarBounds_.toFloat(), 4.5f);
        g.setColour(dx_palette::cardBorder.withAlpha(0.80f));
        g.drawRoundedRectangle(utilityBarBounds_.toFloat(), 4.5f, 1.0f);

        const bool isRun = telemetry_.lamp.isRunning;
        const bool isFill = telemetry_.lamp.fillInActive;
        const bool breakMode = breakBtn_ && breakBtn_->getToggleState();
        juce::String stText;
        juce::Colour stCol = dx_palette::silkCream;
        if (statusFlags_.stopSwitchHeld)
        {
            stText = "STOP SWITCH";
            stCol = dx_palette::meterHot;
        }
        else if (isFill)
        {
            stText = "FILL IN " + juce::String(telemetry_.lamp.activeFillInIndex + 1) + "  " +
                     juce::String(telemetry_.lamp.currentStep + 1).paddedLeft('0', 2) + "/" +
                     juce::String(telemetry_.lamp.totalSteps);
            stCol = dx_palette::amberGlow;
        }
        else if (isRun)
        {
            stText = "RUN " + juce::String(telemetry_.lamp.currentStep + 1).paddedLeft('0', 2) + "/" +
                     juce::String(telemetry_.lamp.totalSteps);
            stCol = dx_palette::silkActive;
        }
        else if (breakMode && statusFlags_.midiArmed)
        {
            stText = "BREAK HOLD READY";
            stCol = dx_palette::amberGlow;
        }
        else if (statusFlags_.midiArmed)
        {
            stText = "RHYTHM ARMED";
            stCol = dx_palette::amberGlow;
        }
        else if (breakMode)
        {
            stText = "BREAK ON";
            stCol = dx_palette::silkCream;
        }
        else if (multiPatternBtn_.getToggleState())
        {
            stText = "MULTI (" + juce::String(activeButtonCount_) + ")";
            stCol = dx_palette::amberGlow;
        }
        else
        {
            stText = (activeButtonCount_ > 0) ? "READY" : "NO PATTERN";
            stCol = dx_palette::silkMuted;
        }

        const auto stR = statusReadoutBounds_.toFloat();
        g.setColour(juce::Colour(0xff140d09));
        g.fillRoundedRectangle(stR, 3.0f);
        g.setColour(stCol);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawFittedText(stText, statusReadoutBounds_.reduced(4, 1), juce::Justification::centred, 1, 0.72f);

        const auto mb = meterBounds_.toFloat();
        g.setColour(juce::Colour(0xff100a07));
        g.fillRoundedRectangle(mb, 3.0f);
        g.setColour(juce::Colour(0xff5b4333));
        g.drawRoundedRectangle(mb, 3.0f, 0.9f);
        const float innerW = std::max(0.0f, mb.getWidth() - 4.0f);
        const auto bar = juce::Rectangle<float>(mb.getX() + 2.0f, mb.getY() + 3.0f,
                                                innerW * meterSmooth_, std::max(2.0f, mb.getHeight() - 6.0f));
        g.setColour(meterSmooth_ > 0.85f ? dx_palette::meterHot : dx_palette::tealMeter);
        g.fillRoundedRectangle(bar, 2.0f);
    }

    // 4. Bottom Deck: 16 GM Drum Voice Audition Bay
    {
        g.setColour(dx_palette::cardBg.withAlpha(0.92f));
        g.fillRoundedRectangle(auditionBayBounds_.toFloat(), 5.0f);
        g.setColour(dx_palette::cardBorder.withAlpha(0.80f));
        g.drawRoundedRectangle(auditionBayBounds_.toFloat(), 5.0f, 1.0f);
        drawSectionBracket(g, auditionBracketBounds_, "GM DRUM MAP", 9.2f);
    }
}

void DXRhythmboxPanel::resized()
{
    auto area = getLocalBounds().reduced(10, 8);
    if (area.getWidth() < 200 || area.getHeight() < 200)
    {
        return;
    }

    const int deckGap = 10;
    const int utilH = 34;
    const int auditionH = std::clamp(area.getHeight() / 8, 58, 72);

    auditionBayBounds_ = area.removeFromBottom(auditionH);
    area.removeFromBottom(7);
    utilityBarBounds_ = area.removeFromBottom(utilH);
    area.removeFromBottom(8);

    const int upperH = static_cast<int>(std::lround(static_cast<double>(area.getHeight() - deckGap) * 0.565));
    upperDeckBounds_ = area.removeFromTop(upperH);
    area.removeFromTop(deckGap);
    middleDeckBounds_ = area;

    // 1. Upper Deck: Left 2x8 Matrix (~57%) + Center VARIATION Corridor (~12%) + Far-Right Tempo Lamp, BREAK, RHYTHMER & Standalone START/STOP
    {
        auto upper = upperDeckBounds_;
        const int matrixW = static_cast<int>(std::lround(static_cast<double>(upper.getWidth()) * 0.57));
        const int guideW = std::clamp(upper.getWidth() / 9, 92, 118);
        matrixFrameBounds_ = upper.removeFromLeft(matrixW);
        variationGuideBounds_ = upper.removeFromLeft(guideW);
        rightUpperBounds_ = upper;

        auto mf = matrixFrameBounds_;
        const int lblHOuter = std::max(14, mf.getHeight() / 13);
        const int lblHInner = std::max(14, mf.getHeight() / 13);

        row0OuterRowBounds_ = mf.removeFromTop(lblHOuter);
        row0InnerRowBounds_ = mf.removeFromTop(lblHInner);
        row1OuterRowBounds_ = mf.removeFromBottom(lblHOuter);
        row1InnerRowBounds_ = mf.removeFromBottom(lblHInner);
        mf.removeFromTop(4);
        mf.removeFromBottom(3);

        // True 3D cutout protrusion: OFF buttons protrude zOff px above wellBounds_.getY() onto walnut;
        // Row 1 buttons are added after Row 0 in Z-order and overlap Row 0's lower skirt cleanly.
        const int wellInnerPadX = 4;
        const int btnGapX = 3;
        const int rowGap = 4;
        const int availW = mf.getWidth() - 2 * wellInnerPadX - 7 * btnGapX;
        const int availH = mf.getHeight() - 4;
        int sq = availW / 8;
        int zOff = std::clamp(static_cast<int>(std::lround(static_cast<float>(sq) * 0.22f)), 7, 12);
        while (sq > 24 && (zOff + 2 * sq + rowGap + 4) > availH)
        {
            --sq;
            zOff = std::clamp(static_cast<int>(std::lround(static_cast<float>(sq) * 0.22f)), 7, 12);
        }

        const int cellW = sq + 2;
        const int cellH = sq + zOff + 4;
        const int totalMatrixW = cellW * 8 + (btnGapX - 2) * 7;
        const int colStep = sq + btnGapX;
        const int totalFootprintW = sq * 8 + btnGapX * 7;
        const int startFootX = mf.getX() + (mf.getWidth() - totalFootprintW) / 2;

        const int totalSpanH = zOff + 2 * sq + rowGap + 4;
        const int holeTopY = mf.getY() + std::max(0, (mf.getHeight() - totalSpanH) / 2) + zOff + 2;
        const int row0BtnY = holeTopY - zOff - 1;
        const int row1BtnY = holeTopY + sq + rowGap - zOff - 1;

        wellBounds_ = juce::Rectangle<int>(startFootX - wellInnerPadX, holeTopY - 2,
                                           totalFootprintW + 2 * wellInnerPadX, 2 * sq + rowGap + 5);
        juce::ignoreUnused(totalMatrixW);

        for (std::size_t col = 0; col < 8u; ++col)
        {
            const int xCap = startFootX + static_cast<int>(col) * colStep;
            colLabelSlices_[col] = juce::Rectangle<int>(xCap - 3, 0, sq + 6, 16);

            if (patternButtons_[col])
            {
                patternButtons_[col]->setBounds(xCap - 1, row0BtnY, cellW, cellH);
            }
            if (patternButtons_[8u + col])
            {
                patternButtons_[8u + col]->setBounds(xCap - 1, row1BtnY, cellW, cellH);
            }
        }

        // Far-Right Block: Top centered Ruby Tempo Lamp; Bottom BREAK (left) + RHYTHMER (mid) + Cream Play/Stop (right, Standalone)
        auto ru = rightUpperBounds_.withTrimmedLeft(4);
        const int lampRowH = static_cast<int>(std::lround(static_cast<double>(ru.getHeight()) * 0.36));
        tempoLampBounds_ = ru.removeFromTop(lampRowH);

        auto ruBot = ru.withTrimmedTop(4);
        const int actSq = std::clamp(sq - 10, 32, 42);
        const int actZOff = std::clamp(static_cast<int>(std::lround(static_cast<float>(actSq) * 0.22f)), 7, 12);
        const int actCellW = actSq + 2;
        const int actCellH = actSq + actZOff + 4;
        const int actPad = 4;
        const int actGapX = 8;
        const int breakWellW = actSq + actPad * 2;
        const int rhythmerWellW = actSq * 2 + actGapX + actPad * 2;
        const int splitGapL = 8;
        const int splitGapR = isStandalone_ ? 8 : 0;
        const int ssPad = 4;
        const int ssGap = 4;
        const int ssMaxPerBtn = std::max(26, (ruBot.getWidth() - (breakWellW + splitGapL + rhythmerWellW + splitGapR + ssPad * 2 + ssGap)) / 2);
        const int ssSide = isStandalone_ ? std::min(actSq, ssMaxPerBtn) : 0;
        const int startStopW = isStandalone_ ? (ssSide * 2 + ssGap + ssPad * 2) : 0;
        const int totalGroupW = breakWellW + splitGapL + rhythmerWellW + splitGapR + startStopW;
        const int gx = ruBot.getX() + std::max(0, (ruBot.getWidth() - totalGroupW) / 2);

        const int actHoleTopY = ruBot.getY() + 16 + actZOff;
        const int actBtnY = actHoleTopY - actZOff - 1;
        const int breakX = gx;
        const int rhythmerX = breakX + breakWellW + splitGapL;

        breakWellBounds_ = juce::Rectangle<int>(breakX, actHoleTopY - 2, breakWellW, actSq + actPad * 2);
        rhythmerBracketBounds_ = juce::Rectangle<int>(rhythmerX, actHoleTopY - actZOff - 16, rhythmerWellW, 14);
        rhythmerWellBounds_ = juce::Rectangle<int>(rhythmerX, actHoleTopY - 2, rhythmerWellW, actSq + actPad * 2);

        if (breakBtn_)
        {
            breakBtn_->setBounds(breakWellBounds_.getX() + actPad - 1, actBtnY, actCellW, actCellH);
        }
        if (variationBtn_)
        {
            variationBtn_->setBounds(rhythmerWellBounds_.getX() + actPad - 1, actBtnY, actCellW, actCellH);
        }
        if (rhythmBtn_)
        {
            rhythmBtn_->setBounds(rhythmerWellBounds_.getX() + actPad + actSq + actGapX - 1, actBtnY, actCellW, actCellH);
        }

        const int lblY = breakWellBounds_.getBottom() + 2;
        actionLabelBounds_[0] = juce::Rectangle<int>(breakWellBounds_.getX() + actPad - 2, lblY, actSq + 4, 14);
        actionLabelBounds_[1] = juce::Rectangle<int>(rhythmerWellBounds_.getX() + actPad - 2, lblY, actSq + 4, 14);
        actionLabelBounds_[2] = juce::Rectangle<int>(rhythmerWellBounds_.getX() + actPad + actSq + actGapX - 2, lblY, actSq + 4, 14);

        if (isStandalone_ && standaloneStartBtn_ && standaloneStopBtn_)
        {
            const int ssX = rhythmerWellBounds_.getRight() + splitGapR;
            const int bezelH = ssSide + ssPad * 2;
            const int bezelY = actHoleTopY - 2 + ((actSq + actPad * 2) - bezelH) / 2;
            standaloneTransportBezelBounds_ = juce::Rectangle<int>(ssX, bezelY, startStopW, bezelH);
            standaloneStartBtn_->setBounds(ssX + ssPad, bezelY + ssPad, ssSide, ssSide);
            standaloneStopBtn_->setBounds(ssX + ssPad + ssSide + ssGap, bezelY + ssPad, ssSide, ssSide);
        }
        else
        {
            standaloneTransportBezelBounds_ = {};
        }
    }

    // 2. Middle Deck: Left 3 Sliders (42%) + Right Compact 4 Drum Fill In Buttons (58%)
    {
        auto mid = middleDeckBounds_;
        const int sliderW = static_cast<int>(std::lround(static_cast<double>(mid.getWidth()) * 0.42));
        sliderBayBounds_ = mid.removeFromLeft(sliderW);
        mid.removeFromLeft(8);
        fillInConsoleBounds_ = mid;

        // 3 vertical fader columns cleanly spaced on walnut panel; compact BPM text box grouped under TEMPO label
        auto sb = sliderBayBounds_.reduced(6, 4);
        const int colW = sb.getWidth() / 3;
        std::array<juce::Slider*, 3> sliders = {&toneSlider_, &volumeSlider_, &bpmSlider_};
        for (std::size_t s = 0; s < 3; ++s)
        {
            auto col = sb.withX(sb.getX() + static_cast<int>(s) * colW).withWidth(colW).reduced(4, 0);
            auto inputRow = col.removeFromBottom(20);
            col.removeFromBottom(2);
            sliderHeaderBounds_[s] = col.removeFromBottom(16);
            sliders[s]->setBounds(col.reduced(2, 2));
            if (s == 2)
            {
                const int boxW = std::clamp(inputRow.getWidth() - 42, 56, 68);
                bpmInput_.setBounds(inputRow.withSizeKeepingCentre(boxW, 19));
            }
        }

        // Compact 4 Self-Illuminating Drum Fill In Buttons + Drum Fill In caption underneath
        auto fc = fillInConsoleBounds_.reduced(8, 6);
        const int gap = 4;
        const int bezelPad = 5;
        const int maxW = (fc.getWidth() - bezelPad * 2 - gap * 3) / 4;
        const int maxH = fc.getHeight() - 44;
        const int side = std::clamp(std::min(maxW, maxH), 42, 72);

        const int bezelW = side * 4 + gap * 3 + bezelPad * 2;
        const int bezelH = side + bezelPad * 2;
        const int captionH = 38;
        const int blockH = bezelH + 6 + captionH;
        const int blockTopY = fc.getY() + std::max(0, (fc.getHeight() - blockH) / 2);
        const int bezelX = fc.getCentreX() - bezelW / 2;

        fillInBezelBounds_ = juce::Rectangle<int>(bezelX, blockTopY, bezelW, bezelH);
        fillInCaptionBounds_ = juce::Rectangle<int>(bezelX, fillInBezelBounds_.getBottom() + 5, bezelW, captionH);

        int fx = fillInBezelBounds_.getX() + bezelPad;
        const int fy = fillInBezelBounds_.getY() + bezelPad;
        for (std::size_t slot = 0; slot < dx701::dsp::kNumFillIns; ++slot)
        {
            if (fillInButtons_[slot])
            {
                fillInButtons_[slot]->setBounds(fx, fy, side, side);
            }
            fx += side + gap;
        }
    }

    // 3. Utility Bar
    {
        auto ub = utilityBarBounds_.reduced(8, 5);
        multiPatternBtn_.setBounds(ub.removeFromLeft(std::min(116, ub.getWidth() / 5)));
        ub.removeFromLeft(6);
        syncLbl_.setBounds(ub.removeFromLeft(34));
        syncModeBox_.setBounds(ub.removeFromLeft(std::min(96, ub.getWidth() / 5)));
        ub.removeFromLeft(6);
        midiChLbl_.setBounds(ub.removeFromLeft(32));
        midiChBox_.setBounds(ub.removeFromLeft(std::min(88, ub.getWidth() / 6)));
        ub.removeFromLeft(6);
        driftLbl_.setBounds(ub.removeFromLeft(38));
        driftSlider_.setBounds(ub.removeFromLeft(std::min(130, ub.getWidth() / 3)));

        ub.removeFromLeft(8);
        meterBounds_ = ub.removeFromRight(std::max(56, ub.getWidth() / 3)).reduced(0, 2);
        ub.removeFromRight(6);
        statusReadoutBounds_ = ub;
    }

    // 4. Bottom Audition Bay (16 pads in one clean row)
    {
        auto ab = auditionBayBounds_.reduced(8, 4);
        auditionBracketBounds_ = ab.removeFromTop(14);
        ab.removeFromTop(3);

        const int padGap = 4;
        const float padW = static_cast<float>(ab.getWidth() - padGap * 15) / 16.0f;
        for (std::size_t v = 0; v < dx701::dsp::kNumVoices; ++v)
        {
            const int px = ab.getX() + static_cast<int>(std::lround(static_cast<float>(v) * (padW + padGap)));
            const int pw = std::max(24, static_cast<int>(std::floor(padW)));
            if (padButtons_[v])
            {
                padButtons_[v]->setBounds(px, ab.getY(), pw, ab.getHeight());
            }
        }
    }
}

} // namespace dx701::ui
