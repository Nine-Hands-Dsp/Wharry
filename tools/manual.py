# Wharry (NINE HANDS | DSP) - notice PDF en français, illustrée avec le rendu réel du plugin (tools/render).
# Usage : python3 tools/manual.py <dossier des captures de tools/render> <fichier PDF de sortie>
# Couverture « minimale » commune à toutes les notices (gabarit fixe de la skill notice).
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, NextPageTemplate, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS, OUT = sys.argv[1], sys.argv[2]
VERSION = next(l.split("VERSION")[1].split()[0].strip(")") for l in open(os.path.join(ROOT, "CMakeLists.txt")) if l.startswith("project("))
TMP = os.path.join(os.path.dirname(os.path.abspath(OUT)), "notice_tmp"); os.makedirs(TMP, exist_ok=True)
FD = os.path.join(ROOT, "fonts")
for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold"),
             ("Barlow", "BarlowCondensed-ExtraBold-ascii")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FD, f + ".ttf")))

ACC = colors.HexColor("#E0262F"); ACC_L = colors.HexColor("#FF7A6E")
DARK = colors.HexColor("#1D1B1C"); GREY = colors.HexColor("#6B6466"); LIGHT = colors.HexColor("#F6EFEF")
W_, H_ = A4

# ---------------------------------------------------------------- illustrations (rendu réel du plugin)
def load(n): return Image.open(os.path.join(SHOTS, n)).convert("RGB")
mid = load("wharry_100pc_milieu.png"); k = mid.height / 900.0
marks = [(1, 276, 430), (2, 595.6, 276.6), (3, 805.4, 267.8), (4, 860.4, 67.5), (5, 577.4, 518.1), (6, 719.9, 518.1),
         (7, 862.4, 518.1), (8, 719.9, 629.1), (9, 719.8, 721.0), (10, 948, 455), (11, 27, 455)]
ann = mid.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(FD, "Poppins-Bold.ttf"), 17)
for n, x, y in marks:
    cx, cy, r = x * k, y * k, 13
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(224, 38, 47), width=3)
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(224, 38, 47), anchor="mm")
annotated = os.path.join(TMP, "annotated.png"); ann.save(annotated)

a, b = load("wharry_100pc_haute.png"), load("wharry_100pc_basse.png")
cw_ = int(505 * k)
pa, pb = a.crop((0, 0, cw_, a.height)), b.crop((0, 0, cw_, b.height))
both = Image.new("RGB", (pa.width * 2 + 24, pa.height), (255, 255, 255)); both.paste(pa, (0, 0)); both.paste(pb, (pa.width + 24, 0))
updown = os.path.join(TMP, "updown.png"); both.save(updown)

# couverture : pédale détourée (alpha du fond = boîtier + jacks), plateau à plat
big = load("wharry_big_basse.png")
alpha = Image.open(os.path.join(ROOT, "art", "background.png")).split()[3].resize(big.size, Image.LANCZOS)
cut = Image.new("RGBA", big.size, (0, 0, 0, 0)); cut.paste(big, (0, 0), alpha); cut = cut.crop(cut.getbbox())
cw, ch = cut.size; pad = int(cw * 0.16)
canvas = Image.new("RGBA", (cw + 2 * pad, ch + 2 * pad), (0, 0, 0, 255))
yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
glow = np.exp(-(((xx - canvas.width / 2) / (cw * 0.50)) ** 2 + ((yy - (pad + ch * 0.48)) / (ch * 0.46)) ** 2)) * 40
edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
canvas = Image.alpha_composite(canvas, Image.fromarray(np.dstack([glow, glow * 0.12, glow * 0.14, np.full_like(glow, 255)]).astype(np.uint8), "RGBA"))
refl = cut.transpose(Image.FLIP_TOP_BOTTOM); ra = np.array(refl).astype(np.float64)
ra[..., 3] *= (np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22)[:, None]
canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
canvas.alpha_composite(cut, (pad, pad))
cover_img = os.path.join(TMP, "cover.png"); canvas.convert("RGB").save(cover_img)
COVER_PX = (pad, pad, cw, ch, canvas.width, canvas.height)

LOGO_W = os.path.join(TMP, "logo_white.png")
if not os.path.exists(LOGO_W):
    # rendu du SVG officiel (blanc) par Chromium
    import subprocess, json
    svg = open(os.path.join(ROOT, "NineHandsLogoWhite.svg")).read().replace("<svg ", '<svg width="2510" height="236" ', 1)
    js = ("const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();const p=await b.newPage();"
          "await p.setViewportSize({width:2510,height:236});await p.setContent(%s);"
          "await p.screenshot({path:%s,omitBackground:true});await b.close();})();") % (json.dumps(
          '<html><body style="margin:0;background:transparent">' + svg + '</body></html>'), json.dumps(LOGO_W))
    open(os.path.join(TMP, "logo.js"), "w").write(js)
    subprocess.run(["node", os.path.join(TMP, "logo.js")], check=True)
lg = Image.open(LOGO_W); lg.crop(lg.getbbox()).save(LOGO_W)

# ---------------------------------------------------------------- styles
st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=19, leading=23, textColor=ACC, spaceAfter=8, spaceBefore=2, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=12.5, leading=16, textColor=DARK, spaceAfter=3, spaceBefore=9, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.5, leading=14, textColor=DARK, spaceAfter=5),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items): return [P("&bull;&nbsp;&nbsp;" + i) for i in items]
def img(path, width):
    im = Image.open(path); return RLImage(path, width=width, height=width * im.height / im.width)
def table(rows, widths, header=True):
    data = [[P(c, "cellb" if (header and r == 0) else "cell") for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#DDD4D4")),
             ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]
    t.setStyle(TableStyle(style)); return t

# ---------------------------------------------------------------- données (identiques au code livré)
PROGRAMS = [
    ("Whammy", "2 OCT UP", "0", "+24"), ("Whammy", "OCT UP", "0", "+12"), ("Whammy", "5TH UP", "0", "+7"), ("Whammy", "4TH UP", "0", "+5"),
    ("Whammy", "2ND DN", "0", "-2"), ("Whammy", "4TH DN", "0", "-5"), ("Whammy", "5TH DN", "0", "-7"), ("Whammy", "OCT DN", "0", "-12"),
    ("Whammy", "2 OCT DN", "0", "-24"), ("Whammy", "DIVE BOMB", "0", "-36"),
    ("Detune", "SHALLOW", "10 cents", "10 cents"), ("Detune", "DEEP", "25 cents", "25 cents"),
    ("Harmony", "OCT DN / OCT UP", "-12", "+12"), ("Harmony", "5TH DN / 4TH DN", "-7", "-5"), ("Harmony", "5TH / 7TH", "+7", "+10"),
    ("Harmony", "5TH / 6TH", "+7", "+9"), ("Harmony", "4TH / 5TH", "+5", "+7"), ("Harmony", "3RD / 4TH", "+4", "+5"),
    ("Harmony", "b3RD / 3RD", "+3", "+4"), ("Harmony", "2ND / 3RD", "+2", "+4"), ("Harmony", "4TH DN / 3RD DN", "-5", "-4")]
FACTORY = [
    ("Octave Scream (2 Oct Up)", "2 OCT UP", "Classic", 20, 100, 100, "Le fameux cri à deux octaves, rapide et brut."),
    ("Classic Octave Up", "OCT UP", "Classic", 25, 100, 100, "Le glissement d'une octave vers le haut, pour solos et riffs."),
    ("Power Fifth Rise", "5TH UP", "Chords", 30, 90, 100, "Quinte montante sur power chords, propre en mode Chords."),
    ("Half-Step Drop (2nd Dn chords)", "2ND DN", "Chords", 40, 100, 100, "Toute la guitare descend d'un ton, accords compris."),
    ("Seven-String Sub (Oct Dn chords)", "OCT DN", "Chords", 30, 80, 100, "Octave grave sous les accords : un bas énorme pour la 7 cordes."),
    ("Doom Floor (2 Oct Dn chords)", "2 OCT DN", "Chords", 45, 70, 100, "Deux octaves plus bas, glissement lent : son doom / drone."),
    ("Dive Bomb", "DIVE BOMB", "Classic", 15, 100, 100, "Plongée de trois octaves, glissement court pour une chute sauvage."),
    ("Shallow Chorus (Detune)", "SHALLOW", "Classic", 60, 85, 90, "Doublage léger façon chorus, la pédale dose la voix désaccordée."),
    ("Deep Detune Wash", "DEEP", "Chords", 60, 80, 100, "Large nappe désaccordée pour arpèges en son clair."),
    ("Octave Spread (Oct Dn/Oct Up)", "OCT DN / OCT UP", "Chords", 35, 90, 85, "Harmonie qui va d'une octave en dessous à une octave au-dessus."),
    ("Bass Fifth Bend (5th/7th)", "5TH / 7TH", "Classic", 30, 90, 90, "Harmonie de la quinte à la septième, bluesy et sombre."),
    ("Twin Lead Thirds (3rd/4th)", "3RD / 4TH", "Classic", 25, 100, 90, "Harmonie façon deux guitares, de la tierce à la quarte."),
    ("Pedal Steel Bend (2nd/3rd)", "2ND / 3RD", "Classic", 50, 85, 90, "Bend façon pedal steel, de la seconde à la tierce."),
    ("Whammy Octave Down", "OCT DN", "Classic", 25, 100, 100, "La descente classique : orteil enfoncé = une octave plus bas.")]

# ---------------------------------------------------------------- pages
def cover(c, doc):
    c.saveState()
    c.setFillColor(colors.black); c.rect(0, 0, W_, H_, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, 0, W_, 3 * mm, stroke=0, fill=1)                         # bandeau bas
    c.setFillColor(ACC_L); c.setFont("PopM", 8.5)
    c.drawCentredString(W_ / 2, H_ - 18 * mm - 8.5 * 0.74, "FRANÇAIS")
    # pédale : boîtier + jacks de 82 mm de haut (max 115 mm de large), haut à 84 mm
    pad, padT, cw, ch, iw, ih = COVER_PX
    ph = 82 * mm; pw = ph * cw / ch
    if pw > 115 * mm: pw = 115 * mm; ph = pw * ch / cw
    sc = ph / ch
    x0 = W_ / 2 - (pad + cw / 2) * sc; ytop = H_ - 84 * mm + padT * sc
    c.drawImage(cover_img, x0, ytop - ih * sc, width=iw * sc, height=ih * sc)
    # nom
    name = "WHARRY"; fs = 30; sp = 0.5 * fs
    tw = pdfmetrics.stringWidth(name, "Barlow", fs) + sp * (len(name) - 1)
    t = c.beginText(); t.setFont("Barlow", fs); t.setCharSpace(sp); t.setFillColor(colors.white)
    t.setTextOrigin(W_ / 2 - tw / 2, H_ - 194 * mm - fs * 0.70); t.textOut(name); c.drawText(t)
    # mention courte
    sub = "Pitch shift et harmonies"; fs2 = 10; sp2 = 1.5
    tw2 = pdfmetrics.stringWidth(sub, "Pop", fs2) + sp2 * (len(sub) - 1)
    t = c.beginText(); t.setFont("Pop", fs2); t.setCharSpace(sp2); t.setFillColor(colors.HexColor("#BDC4B8"))
    t.setTextOrigin(W_ / 2 - tw2 / 2, H_ - 208 * mm - fs2 * 0.74); t.textOut(sub); c.drawText(t)
    # logo blanc 38 mm, bas à 22 mm
    lim = Image.open(LOGO_W); lw = 38 * mm; lh = lw * lim.height / lim.width
    c.drawImage(LOGO_W, W_ / 2 - lw / 2, 22 * mm, width=lw, height=lh, mask="auto")
    c.setTitle("Wharry - Notice"); c.setAuthor("NINE HANDS | DSP"); c.setSubject("Notice de la pédale Wharry")
    c.restoreState()

def inner(c, doc):
    c.saveState()
    c.setFillColor(ACC); c.rect(0, H_ - 4, W_, 4, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "NINE HANDS | DSP  ·  Wharry v%s" % VERSION)
    c.drawRightString(W_ - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="Wharry - Notice", author="NINE HANDS | DSP")
fr = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")
doc.addPageTemplates([PageTemplate("cover", frames=[fr], onPage=cover), PageTemplate("inner", frames=[fr], onPage=inner)])
CW = doc.width
S = [NextPageTemplate("inner"), PageBreak()]

S += [P("Présentation", "h1"),
      P("Wharry reproduit le fonctionnement de la DigiTech Whammy 5 : une pédale d'expression qui transpose la guitare en temps réel, "
        "du simple bend d'un demi-ton jusqu'à la plongée de trois octaves, plus des effets de désaccord (Detune) et des harmonies à deux voix (Harmony)."),
      P("Comme sur l'original, l'inverseur <b>CLASSIC / CHORDS</b> choisit entre un moteur très réactif pour les notes seules (le caractère criard du Whammy) "
        "et un moteur pensé pour les accords. Wharry ajoute un moteur Chords <b>Studio HQ</b> plus précis (avec latence, pour le mixage ou le re-amping), "
        "un suivi des notes jusqu'à ~35 Hz (7 cordes en G et plus bas) et des réglages GLIDE, TONE et MIX."),
      P("La pédale se pilote à la souris, par automation dans le DAW, ou avec une vraie pédale d'expression MIDI (M-Vave Chocolate Plus par exemple) : "
        "le plateau bascule à l'écran en même temps que votre pied."),
      P("Installation", "h1"),
      P("<b>Windows</b> : décompressez Wharry-VST3-Windows.zip et copiez <b>Wharry.vst3</b> dans <i>C:\\Program Files\\Common Files\\VST3</i>."),
      P("<b>macOS</b> : décompressez Wharry-VST3-macOS.zip et copiez <b>Wharry.vst3</b> dans <i>~/Library/Audio/Plug-Ins/VST3</i> "
        "(version Audio Unit dans <i>~/Library/Audio/Plug-Ins/Components</i> si elle est fournie). Au premier lancement, autorisez le plugin dans Réglages Système > Confidentialité et sécurité si macOS le demande."),
      P("<b>Reaper</b> : Options > Preferences > Plug-ins > VST > <i>Re-scan</i>. Le plugin apparaît sous le nom <b>Wharry (NINE HANDS | DSP)</b>. "
        "Pour piloter la pédale en MIDI, la piste doit recevoir l'entrée MIDI de votre pédalier (armement et monitoring activés, entrée MIDI choisie).")]

S += [PageBreak(), P("Vue d'ensemble", "h1"), img(annotated, CW * 0.86), Spacer(1, 4),
      table([["N°", "Élément", "Rôle"],
             ["1", "Pédale d'expression", "Pointe levée = pas de transposition, orteil enfoncé = intervalle complet du programme"],
             ["2", "Sélecteur de programmes", "21 programmes : WHAMMY, DETUNE et HARMONY"],
             ["3", "Écran", "Famille et moteur, programme, décalage en direct, course de la pédale ; clic = presets"],
             ["4", "Inverseur CLASSIC / CHORDS", "Moteur notes seules ou accords"],
             ["5", "GLIDE", "Lissage du mouvement de la pédale (0 à 250 ms)"],
             ["6", "TONE", "Filtre de la voix transposée (plus sombre vers la gauche)"],
             ["7", "MIX", "Dosage entre le son direct et le son transposé"],
             ["8", "Barre de presets", "Flèches : programme précédent / suivant ; clic au centre = menu ; affiche aussi les valeurs pendant un réglage"],
             ["9", "Footswitch", "Effet ON / OFF ; l'anneau rouge s'allume quand l'effet est actif"],
             ["10", "IN", "Entrée (tranche droite)"],
             ["11", "OUT", "Sortie (tranche gauche)"]], [11 * mm, 44 * mm, CW - 55 * mm])]

S += [PageBreak(), P("Les commandes", "h1"),
      P("La pédale d'expression", "h2"),
      P("Glissez verticalement sur le plateau (vers le bas = appuyer sur la pointe), utilisez la molette, ou double-cliquez pour passer d'un bout à l'autre. "
        "Le paramètre <b>Expression Pedal</b> est automatisable dans le DAW. Le plateau bascule en perspective avec sa lumière et son ombre : "
        "il suit en temps réel la pédale MIDI et les automations."),
      img(updown, CW * 0.78),
      P("Pointe levée (à gauche) et orteil enfoncé (à droite).", "small"),
      P("Le sélecteur de programmes", "h2"),
      P("Glissez verticalement, utilisez la molette ou cliquez sur l'écran pour choisir parmi les 21 programmes. Les repères autour du bouton regroupent "
        "les familles : <b>WHAMMY</b> (début de course), <b>DETUNE</b> (en haut) et <b>HARMONY</b> (fin de course). Tourner le sélecteur quitte le preset usine en cours."),
      P("L'inverseur CLASSIC / CHORDS", "h2"),
      P("<b>CLASSIC</b> : moteur qui suit la note jouée, très réactif, idéal pour les solos et les cris de la Whammy. "
        "<b>CHORDS</b> : moteur polyphonique pour les accords, sans latence. Un clic sur l'inverseur bascule de l'un à l'autre ; le curseur glisse dans sa fenêtre."),
      P("GLIDE, TONE, MIX", "h2")] + bullets([
        "<b>GLIDE</b> (0 à 250 ms) : lisse les mouvements de la pédale ; court pour des effets francs, long pour des montées douces.",
        "<b>TONE</b> : adoucit la voix transposée (utile sur les octaves hautes).",
        "<b>MIX</b> : à 100 %, seul le son transposé sort (comme la Whammy en mode Whammy) ; plus bas, le son direct est conservé."]) + [
      P("Ces potards se règlent en glissant verticalement (Maj = réglage fin), à la molette, et reviennent à leur valeur par défaut d'un double-clic. "
        "Leur index tourne avec un léger flou de mouvement.", "small"),
      P("Footswitch", "h2"),
      P("Un clic active ou coupe l'effet. Quand l'effet est actif, l'anneau rouge dans la gorge du footswitch est allumé ; son halo se règle avec <b>Glow</b>. "
        "Coupé, l'écran s'assombrit et indique BYPASS."),
      P("Écran et barre de presets", "h2"),
      P("L'écran affiche la famille et le moteur (ex. WHAMMY · CLASSIC), le nom du programme, le décalage en cours (demi-tons, ou cents en Detune), "
        "une jauge de la course de la pédale et le numéro du programme. La barre de presets montre le programme ou le preset usine chargé : "
        "les flèches passent au programme précédent ou suivant, un clic au centre ouvre le menu des presets. Pendant un réglage, la même barre affiche "
        "les valeurs en direct (GLIDE, TONE, MIX, MIDI Learn, calibration), puis la barre de presets revient en fondu.")]

S += [PageBreak(), P("Les 21 programmes", "h1"),
      P("Ordre du sélecteur, identique aux Program Change MIDI 1 à 21. Pour Whammy, la colonne « orteil enfoncé » donne l'intervalle atteint au bout de la course ; "
        "pour Harmony, la voix ajoutée passe du premier intervalle (pointe levée) au second (orteil enfoncé) ; pour Detune, la pédale dose la voix désaccordée."),
      table([["N°", "Famille", "Programme", "Pointe levée", "Orteil enfoncé"]] +
            [[str(i + 1), f, n, u, d] for i, (f, n, u, d) in enumerate(PROGRAMS)],
            [11 * mm, 25 * mm, 45 * mm, 33 * mm, CW - 114 * mm])]

S += [PageBreak(), P("MIDI et pédale d'expression", "h1"),
      P("MIDI Learn", "h2"),
      P("Aucun contrôleur n'est assigné par défaut. Clic droit sur une commande (plateau, sélecteur, potard, inverseur, footswitch) > <b>MIDI Learn</b>, "
        "puis bougez la commande de votre pédalier : la barre d'état confirme l'assignation (ex. « Expression pedal = CC 11 »). "
        "<b>Clear MIDI</b> retire l'assignation. L'inverseur et le footswitch peuvent aussi apprendre une note MIDI. "
        "Les assignations sont enregistrées avec le projet. Le sous-menu <b>MIDI</b> du clic droit principal les regroupe toutes."),
      P("Pédale d'expression MIDI", "h2")] + bullets([
        "<b>Invert pedal direction</b> (coché par défaut) : inverse le sens de la pédale MIDI.",
        "Plage apprise automatiquement : Wharry retient la plage réellement envoyée par la pédale (par ex. 2 à 116) et l'étale sur toute la course.",
        "<b>Calibrate pedal</b> : enfoncez la pédale à fond puis relevez-la ; la calibration se termine seule (ou d'un clic sur l'écran). <b>Reset pedal calibration</b> revient à la plage automatique."]) + [
      P("Program Change (carte Whammy 5)", "h2"),
      table([["Program Change", "Effet"], ["1 à 21", "Programme 1 à 21, CLASSIC, effet actif"], ["22 à 42", "Programme 1 à 21, CLASSIC, bypass"],
             ["43 à 63", "Programme 1 à 21, CHORDS, effet actif"], ["64 à 84", "Programme 1 à 21, CHORDS, bypass"]], [45 * mm, CW - 45 * mm]),
      P("Option du clic droit : <b>Respond to MIDI program change</b> (activée par défaut).", "small"),
      P("Fenêtre et menus", "h1")] + bullets([
        "La fenêtre s'ouvre à la taille « 60 % » et ne peut pas être plus petite ; clic droit > <b>Plugin size</b> : 60 % (default), 75 %, 100 %. "
        "<b>Default size (new windows)</b> choisit la taille d'ouverture de toutes les nouvelles fenêtres.",
        "Clic droit > <b>Glow (halo of the lights)</b> : 25, 50, 75 ou 100 % (par défaut) ; dose seulement le halo de l'anneau du footswitch, commun à toutes les fenêtres.",
        "Clic droit > <b>Chords engine</b> : <b>Live engine</b> (sans latence) ou <b>Studio HQ engine</b> (plus précis sur les accords, latence d'environ 170 ms annoncée au DAW qui la compense à la lecture).",
        "Les menus s'ouvrent à l'endroit du clic ; le clic droit sur une commande commence par son MIDI Learn."])

S += [PageBreak(), P("Presets usine", "h1"),
      P("Menu des presets : clic sur l'écran ou au centre de la barre de presets. Les 14 presets usine NINE HANDS | DSP réglent programme, moteur, GLIDE, TONE et MIX ; "
        "les 21 programmes de la Whammy sont listés en dessous."),
      table([["Preset", "Programme", "Moteur", "Glide", "Tone", "Mix", "Description"]] +
            [[n, p, m, "%d ms" % g, "%d %%" % t, "%d %%" % x, d] for n, p, m, g, t, x, d in FACTORY],
            [36 * mm, 22 * mm, 16 * mm, 14 * mm, 12 * mm, 12 * mm, CW - 112 * mm]),
      P("Conseils d'utilisation", "h1")] + bullets([
        "Placez Wharry en tête de chaîne, avant la distorsion et l'ampli simulé : la transposition suit mieux un signal propre, et la saturation après donne le cri typique.",
        "7 cordes ou accordages graves : le suivi descend jusqu'à ~35 Hz ; en CLASSIC, jouez des notes seules bien attaquées.",
        "Accords : passez en CHORDS ; pour un rendu de studio (re-amping, mixage), choisissez le moteur Studio HQ.",
        "Pour un effet « capodastre » fixe, laissez la pédale enfoncée et automatisez-la ; pour des bends expressifs, réglez GLIDE entre 15 et 40 ms.",
        "Pédale MIDI qui n'atteint pas 0 ou 127 : un aller-retour complet suffit, la plage est apprise automatiquement."])

S += [PageBreak(), P("Caractéristiques techniques", "h1"),
      table([["Caractéristique", "Valeur"],
             ["Formats", "VST3 (Windows 64 bits, macOS universel Intel + Apple Silicon), Audio Unit sur macOS"],
             ["Nom dans le DAW", "Wharry (NINE HANDS | DSP)"],
             ["Programmes", "21 (10 Whammy, 2 Detune, 9 Harmony) + 14 presets usine"],
             ["Moteurs", "CLASSIC (notes seules), CHORDS Live (sans latence), CHORDS Studio HQ (vocodeur de phase, ~170 ms)"],
             ["Plage de transposition", "-36 à +24 demi-tons ; Detune 10 et 25 cents"],
             ["Suivi des notes", "~35 Hz à 1500 Hz"],
             ["GLIDE", "0 à 250 ms (30 ms par défaut)"],
             ["MIDI", "MIDI Learn de toutes les commandes (CC ou note), Program Change carte Whammy 5"],
             ["Fenêtre", "60 % (par défaut et minimum), 75 %, 100 %, redimensionnable"],
             ["Version", VERSION]], [45 * mm, CW - 45 * mm])]

doc.build(S)
print(OUT)
