#pragma once
#include "WigaWindowSize.h"

// Reglage « Glow » (halo des lumieres) commun a toutes les fenetres du plugin : settings.xml, attribut glowPercent (25-100, 100 par defaut).
namespace NineHandsGlow
{
    inline int percent (const juce::String& plugin)
    {
        const auto f = WigaWindowSize::file (plugin);
        if (f.existsAsFile())
            if (auto x = juce::XmlDocument::parse (f))
                return juce::jlimit (25, 100, x->getIntAttribute ("glowPercent", 100));
        return 100;
    }

    inline void setPercent (const juce::String& plugin, int pc)
    {
        const auto f = WigaWindowSize::file (plugin);
        juce::XmlElement x ("NineHandsSettings");
        if (f.existsAsFile())
            if (auto old = juce::XmlDocument::parse (f))
                x = *old;
        x.setAttribute ("glowPercent", juce::jlimit (25, 100, pc));
        f.getParentDirectory().createDirectory();
        x.writeTo (f);
    }
}
