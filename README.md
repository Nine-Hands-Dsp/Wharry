# Wharry (NINE HANDS | DSP)

Plugin VST3 (Windows / macOS, + AU sur Mac) reproduisant le fonctionnement de la pédale DigiTech Whammy 5,
au format **pédale double**, avec une pédale d'expression qui bascule à l'écran au rythme de la pédale MIDI ou des automations.

<p align="center">
  <a href="https://github.com/Nine-Hands-Dsp/Wharry/releases/latest/download/Wharry-VST3-Windows.zip"><img alt="Télécharger pour Windows" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-Windows-0078D6?style=for-the-badge"></a>
  <a href="https://github.com/Nine-Hands-Dsp/Wharry/releases/latest/download/Wharry-VST3-macOS.zip"><img alt="Télécharger pour macOS" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-macOS-111111?style=for-the-badge&logo=apple&logoColor=white"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/Wharry/releases/latest/download/Wharry-Notice.pdf"><img alt="Télécharger la notice (PDF)" src="https://img.shields.io/badge/%F0%9F%93%96%20Notice-PDF-919496?style=for-the-badge"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/Wharry/releases/latest"><img alt="Dernière version" src="https://img.shields.io/github/v/release/Nine-Hands-Dsp/Wharry?label=derni%C3%A8re%20version&color=919496"></a>
</p>

- 21 programmes : 10 Whammy, 2 Detune, 9 Harmony (ordre du sélecteur = Program Change 1 à 21)
- **CLASSIC** (notes seules, réactif) / **CHORDS** (accords, sans latence) ; option CHORDS **Studio HQ** (vocodeur de phase, latence annoncée au DAW)
- Pédale d'expression : paramètre automatisable, GLIDE (lissage), calibration talon / pointe, inversion (cochée par défaut) ; aucun CC assigné par défaut : clic droit > MIDI Learn
- Program Change façon Whammy 5 : 1-21 Classic, 22-42 Classic bypass, 43-63 Chords, 64-84 Chords bypass
- Écran : famille et moteur, programme, décalage en direct, jauge de la pédale ; clic sur l'écran = presets
- Barre de presets (◀ ▶ et menu) et ligne d'état au même endroit : les valeurs des réglages s'affichent le temps d'un réglage
- Footswitch avec anneau lumineux ; clic droit > **Glow** (25 / 50 / 75 / 100 %) dose le halo
- Clic droit > **Plugin size** : 60 % (default), 75 %, 100 % ; **Default size (new windows)** pour toutes les nouvelles fenêtres
- Suivi des notes jusqu'à ~35 Hz (7 cordes en G et plus bas)

## Visuel
Pièces photo dans `design/src/` (boîtier, plateau, caoutchouc, potard, inverseur, footswitch, jacks, vis).
`python3 tools/make_art.py` génère `art/*.png` (fond 1950 x 1800, plateau à plat, calques des potards, curseur de l'inverseur,
bouton du footswitch) et `Layout.h`. Textes, graduations, logo, écran, barre de presets et lumières sont dessinés en vectoriel.
Le plugin calcule la bascule du plateau en perspective (10° pointe levée), avec reflet et ombre qui suivent la lumière (haut gauche, 45°).

## Build
GitHub Actions (`.github/workflows/build.yml`) : artefacts Windows et macOS, puis Release (fichiers à nom fixe des boutons ci-dessus).
Tests du moteur audio : `g++ -O2 -std=c++17 tests/test_dsp.cpp -o test_dsp && ./test_dsp`

- v2.0.0 : nouvelle façade NINE HANDS | DSP (pièces photo, plateau en perspective, potards animés, anneau lumineux, menus Octopus, Glow)
