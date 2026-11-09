# Wharry (NINE HANDS | DSP)

Plugin VST3 (Windows / macOS, + AU sur Mac) reproduisant le fonctionnement de la pédale DigiTech Whammy 5,
au format **pédale double** (fenêtre aux hauteurs de Ring Think : 544 x 495 par défaut et au minimum, puis 75 % et 100 %).

- 21 programmes : 10 Whammy, 2 Detune, 9 Harmony (ordre du sélecteur = Program Change 1 à 21)
- **CLASSIC** (notes seules, réactif, caractère Whammy) / **CHORDS** (accords, sans latence) ; option CHORDS **Studio HQ** (vocodeur de phase, latence annoncée au DAW)
- Pédale d'expression : paramètre automatisable, lissage réglable, calibration talon/pointe, inversion ; aucun CC assigné par défaut : clic droit > MIDI > Learn
- Program Change façon Whammy 5 : 1-21 Classic, 22-42 Classic bypass, 43-63 Chords, 64-84 Chords bypass
- Inverseur CLASSIC / CHORDS, footswitch ON / OFF (MIDI learn par clic droit)
- Boutons GLIDE (lissage de la pédale), TONE (filtre de la voix traitée), MIX (dosage de l'effet)
- Sélecteur 21 crans sans LED : l'écran affiche famille, numéro, programmes voisins, programme choisi, décalage en direct et course de la pédale ; clic sur l'écran = menu des presets
- Suivi des notes jusqu'à ~35 Hz (7 cordes en G et plus bas)
- Clic droit > **Plugin size** : 60 %, 75 %, 100 % (la taille par défaut porte la mention « (default) ») ; sous-menu **Default size (new windows)** = taille d'ouverture des nouvelles fenêtres, commune à toutes les instances (une taille déjà choisie pour une instance et sauvegardée avec le projet reste prioritaire)

v1.8.1 : pédale d'expression MIDI inversée par défaut (option « Invert pedal direction » cochée à l'ouverture)
v1.8.0 : anti-gel v2 (JUCE 8.0.15, caches refaits par le timer, chien de garde, aucun appel à l'hôte depuis le thread audio)

<p align="center">
  <a href="https://github.com/Nine-Hands-Dsp/Wharry/releases/latest/download/Wharry-VST3-Windows.zip"><img alt="Télécharger pour Windows" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-Windows-0078D6?style=for-the-badge"></a>
  <a href="https://github.com/Nine-Hands-Dsp/Wharry/releases/latest/download/Wharry-VST3-macOS.zip"><img alt="Télécharger pour macOS" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-macOS-111111?style=for-the-badge&logo=apple&logoColor=white"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/Wharry/releases/latest"><img alt="Dernière version" src="https://img.shields.io/github/v/release/Nine-Hands-Dsp/Wharry?label=derni%C3%A8re%20version&color=919496"></a>
</p>

## Visuel
Design de l'utilisateur : `design/wharry_design_source.png`. `python3 tools/design_art.py` génère `background.png`,
`treadle_down.png` (plateau à plat), les sprites et `tools/layout.txt` (coordonnées reprises dans `PluginEditor.cpp`).
Le plugin calcule la bascule du plateau en perspective (orteil levé -> enfoncé).

## Build
GitHub Actions (`.github/workflows/build.yml`) : artefacts `Wharry-VST3-Windows` et `Wharry-VST3-macOS`.
Tests du moteur audio : `g++ -O2 -std=c++17 tests/test_dsp.cpp -o test_dsp && ./test_dsp`

- v1.8.2 : menu de taille de fenêtre : « 60 % (default) », 75 %, 100 % (plus de taille en pixels)
- v1.9.0 : MIDI Learn au clic droit sur chaque commande (Clear MIDI, assignations sauvegardées avec le projet)
- v1.10.0 : clic droit > Plugin size > Default size (new windows) : taille par défaut des nouvelles fenêtres au choix, comme dans Octopus
- v1.10.1 : plus aucun CC MIDI assigné par défaut (tout passe par le MIDI Learn)
