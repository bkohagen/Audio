// Shared visual design system: colours, fonts, cards and header.
// Change the look of every plugin here.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kit::theme
{
    const juce::Colour bg0       { 0xff111317 };   // window background (bottom)
    const juce::Colour bg1       { 0xff181b20 };   // window background (top)
    const juce::Colour card      { 0xff1c2026 };   // card fill
    const juce::Colour cardEdge  { 0x14ffffff };   // card outline
    const juce::Colour text      { 0xffe9ebee };
    const juce::Colour dim       { 0xff8a919b };
    const juce::Colour faint     { 0xff4a5059 };
    const juce::Colour accent    { 0xff4f9dff };   // primary controls (blue)
    const juce::Colour warm      { 0xffffb547 };   // character / engage (amber)
    const juce::Colour hot       { 0xffff5a4f };   // warnings / heavy GR (red)
    const juce::Colour display   { 0xff0b0d10 };   // meter glass

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    /** Rounded card with optional small-caps title in its top-left corner. */
    inline void drawCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
    {
        using namespace juce;
        g.setGradientFill (ColourGradient (card.brighter (0.06f), r.getX(), r.getY(), card, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (cardEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

        if (title.isNotEmpty())
        {
            g.setColour (dim);
            g.setFont (font (10.5f, true));
            g.drawText (title, r.reduced (16.0f, 12.0f).removeFromTop (14.0f), Justification::centredLeft);
        }
    }

    /** Window background + header: product name, subtitle and a right-aligned pill badge. */
    inline void drawBackgroundAndHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& name,
                                         const juce::String& subtitle, const juce::String& badge = "ZERO LATENCY")
    {
        using namespace juce;
        g.setGradientFill (ColourGradient (bg1, 0.0f, 0.0f, bg0, 0.0f, (float) area.getHeight(), false));
        g.fillRect (area);

        auto header = area.withHeight (68).reduced (24, 18).toFloat();
        g.setColour (text);
        g.setFont (font (24.0f, true));
        const float nameW = GlyphArrangement::getStringWidth (font (24.0f, true), name) + 16.0f;
        g.drawText (name, header.removeFromLeft (nameW), Justification::centredLeft);
        g.setColour (dim);
        g.setFont (font (11.0f, true));
        g.drawText (subtitle, header.withTrimmedTop (2.0f), Justification::centredLeft);

        if (badge.isNotEmpty())
        {
            const auto pill = Rectangle<float> (130.0f, 24.0f).withPosition ((float) area.getRight() - 154.0f, 22.0f);
            g.setColour (accent.withAlpha (0.12f));
            g.fillRoundedRectangle (pill, 12.0f);
            g.setColour (accent.withAlpha (0.6f));
            g.drawRoundedRectangle (pill, 12.0f, 1.0f);
            g.setColour (accent.brighter (0.3f));
            g.setFont (font (10.5f, true));
            g.drawText (badge, pill, Justification::centred);
        }
    }
}
