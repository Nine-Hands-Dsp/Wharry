#pragma once
#include "PluginProcessor.h"
#include "NineHandsWindow.h"

class PedalView;

class WharryEditor : public juce::AudioProcessorEditor
{
public:
    explicit WharryEditor (WharryProcessor&);
    ~WharryEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    int  getScaleIndex() const;          // 0, 1, 2 = 60 % (défaut d'origine), 75 %, 100 %  (-1 = taille libre)
    void setScaleIndex (int idx);        // choisit la taille de cette instance (sauvegardée avec le projet)
    void applyScale (int idx);           // redimensionne seulement (0 = 60 %, aussi la taille minimale)
    juce::PopupMenu buildContextMenu (int learnIdx = -1);   // menu clic droit (aussi pour l'outil de contrôle des menus)
    NineHandsWindow::HostLock hostLock;  // fenêtre du DAW : jamais sous 60 %, jamais au-delà de 150 %

private:
    friend class PedalView;              // son timer surveille le moteur de rendu (anti-gel v2)
    void useSoftwareRenderer();          // applique le moteur logiciel (Windows) ; repaint si la fenêtre hôte a changé
    void checkRenderer();                // depuis le timer : seulement si la fenêtre a changé ou au plus 1 fois / s
    void resetRenderer();                // chien de garde : oublie la fenêtre mémorisée et réapplique tout
    void* lastPeer = nullptr;
    double lastRendererCheckMs = 0.0;
    WharryProcessor& proc;
    std::unique_ptr<PedalView> view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WharryEditor)
};
