# Wharry v2 (NINE HANDS | DSP) : fabrique les images du plugin a partir des pieces photo de design/src.
# Repere de travail = image 1950 x 1800 px ; repere du plugin = image / 2 (975 x 900 unites).
# Aucune piece n'est agrandie : toutes les sources sont seulement reduites.
# Sorties (dossier art/) : background.png, treadle.png, knob_base.png, knob_marker.png, knob_shadow.png,
#                          sw_slider.png, fs_cap.png ; et Layout.h (positions en unites du plugin).
# Les textes, graduations, logo, ecran, barre de presets et lumieres sont dessines en vectoriel par l'editeur.
from PIL import Image, ImageDraw, ImageFilter, ImageChops, ImageFont
import numpy as np, math, os, json, cv2

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'design', 'src')
OUT = os.path.join(ROOT, 'art')
FONTS = os.path.join(ROOT, 'fonts')
os.makedirs(OUT, exist_ok=True)
def src(n): return Image.open(os.path.join(SRC, n)).convert('RGBA')
def F(n, sz): return ImageFont.truetype(os.path.join(FONTS, 'Poppins-' + n + '.ttf'), sz)

H = 1800
JL = 205                                   # longueur des jacks le long de la tranche
P = round(JL * 109 / 515) - 4              # saillie des jacks
house = src('boitier.png').crop((131, 124, 3966, 3813))
s = H / house.height
BW = round(house.width * s)
W = BW + 2 * P + 1                         # 1950 : largeur paire (repere du plugin entier)
house = house.resize((BW, H), Image.LANCZOS)
hx = lambda v: (v - 131) * s
hyy = lambda v: (v - 124) * s
def X(v): return P + hx(v)
def Y(v): return hyy(v)

# 2e trait grave efface (la barre de presets le remplace)
hy = round(hyy(2570)); a0 = round(hx(2060)); a1 = round(hx(3840))
band = house.crop((a0, hy - 14, a1, hy - 6))
for k in range(0, 16, 8): house.paste(band, (a0, hy - 6 + k))
# 1er trait efface aussi : il est redessine, centre sur la zone de droite
l1 = round(hyy(1897)); e0 = round(hx(1800)); e1 = round(hx(3900))
band = house.crop((e0, l1 - 14, e1, l1 - 6))
for k in range(0, 16, 8): house.paste(band, (e0, l1 - 6 + k))

# cuvette agrandie de 10 % (plateau + 10 %), reprise dans la source pleine resolution, decalee de 25 px vers la gauche
m = 10; HO = src('boitier.png'); mo = m / s
cav = HO.crop((round(331 - mo), round(394 - mo), round(1970 + mo), round(3618 + mo)))
ow = round((1970 - 331) * s); NW = round(ow * 1.10); NT, NB = 50, 1745; NH = NB - NT
cav = cav.resize((NW + 2 * m, NH + 2 * m), Image.LANCZOS)
fm = np.ones((cav.height, cav.width))
for k in range(m):
    v = k / m; fm[k, :] = np.minimum(fm[k, :], v); fm[-1 - k, :] = np.minimum(fm[-1 - k, :], v)
    fm[:, k] = np.minimum(fm[:, k], v); fm[:, -1 - k] = np.minimum(fm[:, -1 - k], v)
ca = np.array(cav).astype(float); ca[..., 3] *= fm; cav = Image.fromarray(ca.astype('uint8'))
DX = -25
house.alpha_composite(cav, (round(hx(331) - m) + DX, NT - m))
CAV = (P + hx(331) + DX, NT, P + hx(331) + NW + DX, NB)

img = Image.new('RGBA', (W, H), (0, 0, 0, 0)); img.alpha_composite(house, (P, 0))

def shadow(sprite, off, blur, op):
    a = sprite.split()[3].point(lambda v: int(v * op))
    sh = Image.new('RGBA', (sprite.width + 4 * blur, sprite.height + 4 * blur), (0, 0, 0, 0))
    mk = Image.new('L', sh.size, 0); mk.paste(a, (2 * blur, 2 * blur)); mk = mk.filter(ImageFilter.GaussianBlur(blur))
    sh.putalpha(mk); return sh
def put(sprite, cx, cy, sh=None):
    x = round(cx - sprite.width / 2); y = round(cy - sprite.height / 2)
    if sh:
        o, b, op = sh; S = shadow(sprite, o, b, op); img.alpha_composite(S, (x - 2 * b + o[0], y - 2 * b + o[1]))
    img.alpha_composite(sprite, (x, y))

# ---- plateau (a plat = orteil enfonce), plaque de caoutchouc remplacee, bords du cadre reduits de 40 %
cx0, cy0, cx1, cy1 = CAV
tr = src('plateau.png')
rb = src('caoutchouc.png')
RX0, RY0, RX1, RY1 = 188, 244, 1580, 2958; rw, rh = RX1 - RX0, RY1 - RY0
cy = 1601
rb = rb.crop((800 - rw // 2, cy - rh // 2, 800 - rw // 2 + rw, cy - rh // 2 + rh))
ra = np.array(rb).astype(float); ra[..., 3] = 255
yy, xx = np.mgrid[0:rh, 0:rw]; dd = np.minimum.reduce([xx, yy, rw - 1 - xx, rh - 1 - yy]).astype(float)
ra[..., :3] *= (0.55 + 0.45 * np.clip(dd / 18, 0, 1))[..., None]
rb = Image.fromarray(np.clip(ra, 0, 255).astype('uint8'))
mk = Image.new('L', (rw, rh), 0); ImageDraw.Draw(mk).rounded_rectangle((0, 0, rw - 1, rh - 1), 38, fill=255); rb.putalpha(mk)
tr.alpha_composite(rb, (RX0, RY0)); tr = tr.crop(tr.getbbox())
tw = round(cx1 - cx0 - 28); th = round(tw * tr.height / tr.width); tr = tr.resize((tw, th), Image.LANCZOS)
TX, TY = round((cx0 + cx1) / 2 - tw / 2), round(cy0 + 14)
tr.save(os.path.join(OUT, 'treadle.png'))

# ---- jacks (IN a droite, OUT a gauche, jamais de miroir)
jk = src('jack_photo.png').crop((45, 79, 560, 188))
jk = jk.resize((JL, round(JL * jk.height / jk.width)), Image.LANCZOS)
g = np.linspace(1.08, 0.86, jk.height)[:, None]
ja = np.array(jk).astype(float); ja[..., :3] *= g[..., None]; jk = Image.fromarray(np.clip(ja, 0, 255).astype('uint8'))
JY = 910
jr = jk.rotate(-90, expand=True); img.alpha_composite(jr, (P + BW - 4, round(JY - jr.height / 2)))
jl = jk.rotate(90, expand=True);  img.alpha_composite(jl, (P - jl.width + 4, round(JY - jl.height / 2)))

# ---- vis : tete seule, meme retrait depuis les deux bords dans les 4 coins de la facade
sc = src('vis.png')
mk = Image.new('L', (sc.width * 4, sc.height * 4), 0)
ImageDraw.Draw(mk).ellipse(((137 - 85) * 4, (135 - 85) * 4, (137 + 85) * 4, (135 + 85) * 4), fill=255)
sc.putalpha(mk.resize(sc.size, Image.LANCZOS)); sc = sc.crop((137 - 87, 135 - 87, 137 + 87, 135 + 87))
fx0, fx1, fy0, fy1 = X(183), X(3912), Y(178), Y(3756)
SI, SD = 27, 30
for i, (x, y) in enumerate([(fx0 + SI, fy0 + SI), (fx1 - SI, fy0 + SI), (fx0 + SI, fy1 - SI), (fx1 - SI, fy1 - SI)]):
    v = sc.rotate(17 + i * 31, resample=Image.BICUBIC).resize((SD, SD), Image.LANCZOS); put(v, x, y, ((3, 3), 3, 0.6))

# ---- zones et repartition (memes regles que l'apercu valide)
L1 = Y(1897)
ZL, ZR = CAV[2] + 4, X(3912)
_w = X(3850) - (CAV[2] - DX + 40)
px0 = (ZL + ZR - _w) / 2; px1 = px0 + _w; pc = (px0 + px1) / 2
_lbb = F('Medium', 30).getbbox('GLIDE', anchor='mm')[3]
_wt = F('Light', 86).getbbox('WHARRY', anchor='mm')[1]; _sb = F('Regular', 26).getbbox('PITCH SHIFT', anchor='mm')[3]
FSW = 189; FSH = round(FSW * 721 / 816); BHP = round(76 * 720 / 800)
_hK = 102 + 110 + _lbb; _hT = (72 + _sb) - _wt
Z0, Z1 = L1 + 2, Y(3756); _g = (Z1 - Z0 - (_hK + BHP + FSH + _hT)) / 5
KC = Z0 + _g + 102; L2C = Z0 + 2 * _g + _hK + BHP / 2; FC = Z0 + 3 * _g + _hK + BHP + FSH / 2; WC = Z0 + 4 * _g + _hK + BHP + FSH - _wt
_ZT = 135 + 62 + F('Medium', 24).getbbox('CLASSIC', anchor='lm')[3]; _ZC = (_ZT + L1) / 2
SCR_H = 500; SY0 = _ZC - SCR_H / 2
_dt = F('Medium', 25).getbbox('DETUNE', anchor='mm'); _wb = F('Medium', 25).getbbox('WHAMMY', anchor='mm')
_top = -144 - 24 + _dt[1]; _bot = 144 * math.sqrt(0.5) + 30 + _wb[3]; SCY = _ZC - (_top + _bot) / 2
SC = (px0 + 150, SCY)
SMALL = [(pc + (i - 1) * 285, KC) for i in range(3)]

# trait grave redessine (filet sombre + filet clair)
d = ImageDraw.Draw(img)
d.line([(px0, L1 - 1), (px1, L1 - 1)], fill=(60, 0, 8, 200), width=2)
d.line([(px0, L1 + 1), (px1, L1 + 1)], fill=(235, 200, 200, 210), width=2)

# ---- footswitch (ecrou photo + bouton), ombre a 45 deg vers le bas a droite
fsrc = src('footswitch_photo.png').crop((218, 258, 1034, 979))
fsk = FSW / 816
fs = fsrc.resize((FSW, FSH), Image.LANCZOS); put(fs, pc, FC, ((9, 9), 8, 0.6))
FSX0, FSY0 = round(pc - FSW / 2), round(FC - FSH / 2)
RING = (FSX0 + (626 - 218) * fsk, FSY0 + (616 - 258) * fsk, 293 * fsk, 13 * fsk)
# bouton seul (pour l'appui) : disque du chapeau dans la photo
capR = 281
cap = src('footswitch_photo.png')
mk = Image.new('L', (cap.width * 4, cap.height * 4), 0)
ImageDraw.Draw(mk).ellipse(((626 - capR) * 4, (616 - capR) * 4, (626 + capR) * 4, (616 + capR) * 4), fill=255)
cap.putalpha(ImageChops.multiply(cap.split()[3], mk.resize(cap.size, Image.LANCZOS)))
cap = cap.crop((626 - capR - 2, 616 - capR - 2, 626 + capR + 2, 616 + capR + 2))
CAPPX = round(cap.width * fsk * 2)          # rendu a 2x la taille de la fenetre a 100 % (net en HiDPI)
cap.resize((CAPPX, CAPPX), Image.LANCZOS).save(os.path.join(OUT, 'fs_cap.png'))
CAPR = (capR + 2) * fsk

# ---- inverseur : fenetre vide (assemblee des deux photos) + curseur seul eclairci de 10 %
L_ = np.array(src('inverseur_gauche.png')).astype(float)   # curseur a gauche
R_ = np.array(src('inverseur_droite.png')).astype(float)   # curseur a droite
win = R_.copy()
# moitie gauche vide = photo curseur-a-droite, moitie droite vide = photo curseur-a-gauche
win[:, :650] = R_[:, :650]; win[:, 650:] = L_[:, 650:]
win = Image.fromarray(np.clip(win, 0, 255).astype('uint8')).crop((347, 557, 899, 741))
slider = L_.copy(); slider[..., :3] *= 1.10
slider = Image.fromarray(np.clip(slider, 0, 255).astype('uint8')).crop((352, 557, 632, 741))
SWW = 150; swk = SWW / win.width
win = win.resize((SWW, round(win.height * swk)), Image.LANCZOS)
mk = Image.new('L', win.size, 0); ImageDraw.Draw(mk).rounded_rectangle((0, 0, win.width - 1, win.height - 1), round(12 * swk), fill=255)
win.putalpha(ImageChops.multiply(win.split()[3], mk))
SLW = round(slider.width * swk * 2); SLH = round(slider.height * swk * 2)
slider.resize((SLW, SLH), Image.LANCZOS).save(os.path.join(OUT, 'sw_slider.png'))
_fo = F('Medium', 24); _tw = _fo.getlength('CLASSIC') + 26 + _fo.getlength('CHORDS'); _bbc = _fo.getbbox('CHORDS')
swx = (px1 - 10 + 6) - _tw / 2 - (_bbc[2] - _fo.getlength('CHORDS')); swy = 135
d.rounded_rectangle((round(swx - 78), round(swy - win.height / 2) - 3, round(swx + 78), round(swy + win.height / 2) + 3), 13, fill=(25, 4, 6, 255))
img.alpha_composite(win, (round(swx - 75), round(swy - win.height / 2)))
SWC = round(swy - win.height / 2) - 3 + (win.height + 6) / 2
# course du curseur dans la fenetre (centre du curseur a gauche / a droite)
SL_L = swx - 75 + (492 - 347) * swk; SL_R = swx - 75 + (780 - 347) * swk

# ---- potards : base fixe (index efface) + index seul + ombre (calques pour nh::Knob)
kn = src('potard.png')
KCX, KCY, KR = 438, 420, 346
mk = Image.new('L', (kn.width * 4, kn.height * 4), 0)
ImageDraw.Draw(mk).ellipse(((KCX - KR) * 4, (KCY - KR) * 4, (KCX + KR) * 4, (KCY + KR) * 4), fill=255)
kn.putalpha(ImageChops.multiply(kn.split()[3], mk.resize(kn.size, Image.LANCZOS)))
ka = np.array(kn)
L = ka[..., :3].mean(2)
mark = np.zeros(L.shape, bool); mark[80:285, 424:456] = L[80:285, 424:456] > 120
# zone de l'index et de sa rainure : remplacee par la matiere du chapeau prise au meme rayon, +/- 22 deg (brossage conique)
zone = np.zeros(L.shape, np.float32); zone[70:300, 414:464] = 1.0
zone = cv2.GaussianBlur(zone, (0, 0), 3.0)
ys, xs = np.mgrid[0:L.shape[0], 0:L.shape[1]].astype(np.float32)
dx, dy = xs - KCX, ys - KCY
base = ka[..., :3].astype(np.float32).copy()
# interpolation angulaire : de chaque cote de la zone, au meme rayon, puis fondu selon l'angle
kr_ = np.sqrt(dx * dx + dy * dy) + 1e-3
kth_ = np.arctan2(dx, -dy)                                   # 0 = vers le haut
phi = np.arcsin(np.clip(30.0 / kr_, 0, 1)) + 0.02
def at(angle):
    mx = (KCX + kr_ * np.sin(angle)).astype(np.float32); my = (KCY - kr_ * np.cos(angle)).astype(np.float32)
    return cv2.remap(ka[..., :3].astype(np.float32), mx, my, cv2.INTER_LINEAR)
left, right = at(-phi), at(phi)
kw_ = np.clip((kth_ + phi) / (2 * phi), 0, 1)[..., None]
acc = left * (1 - kw_) + right * kw_
base = base * (1 - zone[..., None]) + acc * zone[..., None]
basei = np.dstack([base, ka[..., 3]])
marki = ka.copy(); soft = cv2.GaussianBlur(mark.astype(np.float32), (0, 0), 1.0)
wa = np.clip(soft, 0, 1) * (L > 60) * (ka[..., 3] / 255)
groove = np.clip(cv2.GaussianBlur(cv2.dilate(mark.astype(np.uint8), np.ones((9, 9), np.uint8)).astype(np.float32), (0, 0), 2.0), 0, 1) * 0.55
ga = groove * (1 - wa)                                      # rainure sombre autour de l'index (tourne avec lui)
outa = wa + ga
marki[..., :3] = np.where(outa[..., None] > 0, (ka[..., :3] * wa[..., None]) / np.maximum(outa, 1e-4)[..., None], 0).astype(np.uint8)
marki[..., 3] = (np.clip(outa, 0, 1) * 255).astype(np.uint8)
CAN = 880                                    # toile carree : potard de 700 px au centre, marge pour l'ombre
def canvas(arr):
    im = Image.fromarray(arr.astype('uint8'), 'RGBA').crop((KCX - 350, KCY - 350, KCX + 350, KCY + 350))
    c = Image.new('RGBA', (CAN, CAN), (0, 0, 0, 0)); c.alpha_composite(im, ((CAN - 700) // 2, (CAN - 700) // 2)); return c
canvas(basei).resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, 'knob_base.png'))
canvas(marki).resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, 'knob_marker.png'))
shm = Image.new('L', (CAN, CAN), 0); ImageDraw.Draw(shm).ellipse((CAN / 2 - 346, CAN / 2 - 346, CAN / 2 + 346, CAN / 2 + 346), fill=165)
shm = shm.filter(ImageFilter.GaussianBlur(26)); shi = Image.new('RGBA', (CAN, CAN), (0, 0, 0, 255)); shi.putalpha(shm)
shi.resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, 'knob_shadow.png'))
KS = CAN / 700                               # taille du composant / diametre du potard

img.save(os.path.join(OUT, 'background.png'))

# ---- Layout.h (unites du plugin = pixels de l'image / 2)
u = lambda v: v / 2
L = {
    'kW': W / 2, 'kH': H / 2,
    'plate': (u(TX), u(TY), u(tw), u(th)),
    'cavity': (u(CAV[0]), u(CAV[1]), u(CAV[2] - CAV[0]), u(CAV[3] - CAV[1])),
    'selC': (u(SC[0]), u(SC[1])), 'selD': u(230), 'selTickR0': u(128), 'selTickR1': u(138), 'selTickR2': u(144),
    'small': [(u(x), u(y)) for x, y in SMALL], 'smallD': u(150), 'smallTickR0': u(84), 'smallTickR1': u(98),
    'knobCanvas': KS,
    'labelY': u(KC + 110), 'labelSize': u(30),
    'selLabelE': u(144 * math.sqrt(0.5)), 'selLabelDy': u(30), 'selLabelTop': u(144 + 24), 'selLabelSize': u(25),
    'screen': (u(px0 + 352), u(SY0), u(px1 - 10 - (px0 + 352)), u(SCR_H)),
    'pill': (u(pc - 360), u(L2C - BHP / 2), u(720), u(BHP)),
    'fsRect': (u(FSX0), u(FSY0), u(FSW), u(FSH)),
    'ring': (u(RING[0]), u(RING[1]), u(RING[2]), u(RING[3])), 'capR': u(CAPR),
    'sw': (u(swx - 78), u(swy - win.height / 2 - 3), u(156), u(win.height + 6)),
    'slider': (u(SL_L), u(SL_R), u(swy), u(SLW / 2), u(SLH / 2)),
    'swLabelY': u(swy + 62), 'swLabelX0': u(swx - _tw / 2), 'swLabelSize': u(24),
    'logo': (u(px0), u(SWC), u(400)),
    'title': (u(pc), u(WC), u(86), u(22)), 'subtitle': (u(pc), u(WC + 72), u(26)),
    'jackY': u(JY), 'inX': u(fx1 - 20), 'outX': u(fx0 + 20), 'ioSize': u(30),
}
with open(os.path.join(ROOT, 'Layout.h'), 'w') as f:
    f.write('#pragma once\n// genere par tools/make_art.py : positions dans le repere du plugin (image / 2)\n#include <juce_graphics/juce_graphics.h>\nnamespace lay\n{\n')
    def val(v):
        if isinstance(v, (tuple, list)) and len(v) == 2 and not isinstance(v[0], (tuple, list)): return 'juce::Point<float> (%.2ff, %.2ff)' % v
        if isinstance(v, (tuple, list)) and len(v) == 4 and not isinstance(v[0], (tuple, list)): return 'juce::Rectangle<float> (%.2ff, %.2ff, %.2ff, %.2ff)' % v
        return '%.3ff' % v
    for k, v in L.items():
        if k in ('kW', 'kH'): f.write('    constexpr int %s = %d;\n' % (k, round(v)))
        elif k == 'small': f.write('    const juce::Point<float> small[3] = { %s };\n' % ', '.join('{ %.2ff, %.2ff }' % p for p in v))
        elif k == 'slider': f.write('    constexpr float sliderL = %.2ff, sliderR = %.2ff, sliderY = %.2ff, sliderW = %.2ff, sliderH = %.2ff;\n' % v)
        elif k == 'logo': f.write('    constexpr float logoX = %.2ff, logoCY = %.2ff, logoW = %.2ff;\n' % v)
        elif k == 'title': f.write('    constexpr float titleX = %.2ff, titleY = %.2ff, titleSize = %.2ff, titleSpacing = %.2ff;\n' % v)
        elif k == 'subtitle': f.write('    constexpr float subX = %.2ff, subY = %.2ff, subSize = %.2ff;\n' % v)
        elif k == 'ring': f.write('    constexpr float ringX = %.2ff, ringY = %.2ff, ringR = %.2ff, ringW = %.2ff;\n' % v)
        elif isinstance(v, (tuple, list)) and len(v) == 2: f.write('    const juce::Point<float> %s %s;\n' % (k, val(v)[len('juce::Point<float> '):]))
        elif isinstance(v, (tuple, list)): f.write('    const juce::Rectangle<float> %s %s;\n' % (k, val(v)[len('juce::Rectangle<float> '):]))
        else: f.write('    constexpr float %s = %s;\n' % (k, val(v)))
    f.write('}\n')
print('image', img.size, 'plateau', tw, th, 'ok')
