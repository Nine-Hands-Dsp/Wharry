#pragma once
// genere par tools/make_art.py : positions dans le repere du plugin (image / 2)
#include <juce_graphics/juce_graphics.h>
namespace lay
{
    constexpr int kW = 975;
    constexpr int kH = 900;
    const juce::Rectangle<float> plate (63.00f, 32.00f, 426.00f, 797.00f);
    const juce::Rectangle<float> cavity (55.79f, 25.00f, 440.00f, 847.50f);
    const juce::Point<float> selC (595.61f, 276.57f);
    constexpr float selD = 115.000f;
    constexpr float selTickR0 = 64.000f;
    constexpr float selTickR1 = 69.000f;
    constexpr float selTickR2 = 72.000f;
    const juce::Point<float> small[3] = { { 577.37f, 518.06f }, { 719.87f, 518.06f }, { 862.37f, 518.06f } };
    constexpr float smallD = 75.000f;
    constexpr float smallTickR0 = 42.000f;
    constexpr float smallTickR1 = 49.000f;
    constexpr float knobCanvas = 1.257f;
    constexpr float labelY = 573.064f;
    constexpr float labelSize = 15.000f;
    constexpr float selLabelE = 50.912f;
    constexpr float selLabelDy = 15.000f;
    constexpr float selLabelTop = 84.000f;
    constexpr float selLabelSize = 12.500f;
    const juce::Rectangle<float> screen (696.61f, 142.78f, 217.53f, 250.00f);
    const juce::Rectangle<float> pill (539.87f, 612.07f, 360.00f, 34.00f);
    const juce::Rectangle<float> fsRect (672.50f, 679.50f, 94.50f, 83.50f);
    constexpr float ringX = 719.75f, ringY = 720.96f, ringR = 33.93f, ringW = 1.51f;
    constexpr float capR = 32.774f;
    const juce::Rectangle<float> sw (821.42f, 53.50f, 78.00f, 28.00f);
    constexpr float sliderL = 842.62f, sliderR = 881.75f, sliderY = 67.50f, sliderW = 38.00f, sliderH = 25.00f;
    constexpr float swLabelY = 98.500f;
    constexpr float swLabelX0 = 803.718f;
    constexpr float swLabelSize = 12.000f;
    constexpr float logoX = 520.61f, logoCY = 67.50f, logoW = 200.00f;
    constexpr float titleX = 719.87f, titleY = 812.09f, titleSize = 43.00f, titleSpacing = 11.00f;
    constexpr float subX = 719.87f, subY = 848.09f, subSize = 13.00f;
    constexpr float jackY = 455.000f;
    constexpr float inX = 931.945f;
    constexpr float outX = 42.186f;
    constexpr float ioSize = 15.000f;
}
