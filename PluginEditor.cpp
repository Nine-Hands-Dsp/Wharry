#include "PluginEditor.h"
#include <BinaryData.h>
#include "WigaWindowSize.h"
#include "NineHandsGlow.h"
#include "NineHandsMenu.h"
#include "NHKnob.h"
#include "Layout.h"

using namespace juce;

// Wharry v2 (NINE HANDS | DSP) : refonte de la façade.
// Repère du dessin = image du fond / 2 : 975 x 900 unités (art/background.png = 1950 x 1800 px, pédale plein cadre,
// transparence seulement autour des jacks). Positions générées par tools/make_art.py dans Layout.h.
// Textes, graduations, logo, écran, barre de presets et lumières : dessinés en vectoriel (Poppins + SVG officiel).
namespace
{
    constexpr int kW = lay::kW, kH = lay::kH;
    constexpr float kRefH = 743.0f;                              // hauteur « 100 % » (= Ring Think à 100 %)
    const float kScales[3] = { 495.0f / 743.0f, 0.75f, 1.0f };
    constexpr float kDefaultScale = 495.0f / 743.0f;             // fenêtre par défaut « 60 % » (= taille minimale)
    constexpr int kFrames = 33;                                  // positions précalculées du plateau

    const Colour kAccent (0xffff4a3d);                           // couleur de la pédale (pastilles des menus)
    const Colour kRing (0xffff241e);                             // anneau lumineux du footswitch (rouge Whammy)
    const Colour kInk (0xebffffff);                              // sérigraphie blanche
    const Colour kScrHead (0xdcff785a), kScrName (0xffffebe1), kScrVal (0xe6ffa082), kScrBar (0xebff785a);

    Image loadPng (const void* d, int n) { return ImageFileFormat::loadFrom (d, (size_t) n).convertedToFormat (Image::ARGB); }

    // redimensionne une fois (par le timer, jamais dans paint), puis garde en cache tant que la taille ne change pas
    struct ScaledCache
    {
        Image src, img; float scale = -1.0f;
        void prepare (float s)
        {
            if (std::abs (s - scale) <= 1.0e-4f && img.isValid()) return;
            scale = s;
            const int w = jmax (1, roundToInt ((float) src.getWidth() * s)), h = jmax (1, roundToInt ((float) src.getHeight() * s));
            img = src.rescaled (w, h, Graphics::highResamplingQuality);
        }
        const Image& get() const { return img.isValid() ? img : src; }
    };

    //==========================================================================
    // Plateau : il pivote autour de la charnière (bas du plateau). Vu d'au-dessus par un appareil placé à l'aplomb du
    // centre de l'image : chaque position est une perspective exacte du plateau à plat (orteil enfoncé), inclinée
    // jusqu'à 10° pointe levée. Lumière en haut à gauche à 45° : le plateau qui se lève se détourne de la lumière,
    // un reflet incliné à 45° glisse le long du plateau, l'ombre portée s'allonge vers le bas à droite.
    constexpr double kTheta = 0.17453292519943295;               // 10° en radians
    constexpr double kCamY = 450.0, kCamD = 2500.0;              // centre optique (unités), distance de l'appareil

    struct Homography
    {
        double h[9] {};
        // calcule H tel que H(dst[i]) = src[i] (4 couples de points)
        bool solve (const Point<double> dst[4], const Point<double> src[4])
        {
            double A[8][9] {};
            for (int i = 0; i < 4; ++i)
            {
                const double x = dst[i].x, y = dst[i].y, u = src[i].x, v = src[i].y;
                double r1[9] = { x, y, 1, 0, 0, 0, -u * x, -u * y, u };
                double r2[9] = { 0, 0, 0, x, y, 1, -v * x, -v * y, v };
                std::copy (r1, r1 + 9, A[2 * i]); std::copy (r2, r2 + 9, A[2 * i + 1]);
            }
            for (int c = 0; c < 8; ++c)
            {
                int piv = c;
                for (int r = c + 1; r < 8; ++r) if (std::abs (A[r][c]) > std::abs (A[piv][c])) piv = r;
                if (std::abs (A[piv][c]) < 1e-12) return false;
                for (int k = 0; k < 9; ++k) std::swap (A[c][k], A[piv][k]);
                for (int r = 0; r < 8; ++r)
                    if (r != c)
                    {
                        const double f = A[r][c] / A[c][c];
                        for (int k = c; k < 9; ++k) A[r][k] -= f * A[c][k];
                    }
            }
            for (int i = 0; i < 8; ++i) h[i] = A[i][8] / A[i][i];
            h[8] = 1.0;
            return true;
        }
        Point<double> map (double x, double y) const
        {
            const double w = h[6] * x + h[7] * y + h[8];
            return { (h[0] * x + h[1] * y + h[2]) / w, (h[3] * x + h[4] * y + h[5]) / w };
        }
    };

    class TreadleWarp
    {
    public:
        void setSource (Image flat)
        {
            down = std::move (flat);
            const auto p = lay::plate;
            box = Rectangle<float> (p.getX() - 34.0f, p.getY() - 30.0f, p.getWidth() + 68.0f, p.getHeight() + 62.0f)
                      .getSmallestIntegerContainer().getIntersection ({ 0, 0, kW, kH });
            scale = -1.0f;
        }
        Rectangle<int> designBounds() const { return box; }

        void prepare (float s)
        {
            if (std::abs (s - scale) < 1.0e-4f) return;
            scale = s;
            const auto p = lay::plate;
            src = down.rescaled (jmax (1, roundToInt (p.getWidth() * s)), jmax (1, roundToInt (p.getHeight() * s)), Graphics::highResamplingQuality)
                      .convertedToFormat (Image::ARGB);
            for (size_t i = 0; i < frames.size(); ++i) { if (frames[i].isValid()) oldFrames[i] = frames[i]; frames[i] = {}; }
        }

        bool buildNextMissing (int around)
        {
            if (scale < 0.0f) return false;
            around = jlimit (0, kFrames - 1, around);
            for (int d = 0; d < kFrames; ++d)
                for (int i : { around - d, around + d })
                    if (i >= 0 && i < kFrames && ! frames[(size_t) i].isValid())
                    { frames[(size_t) i] = build ((float) i / (float) (kFrames - 1)); return true; }
            for (auto& f : oldFrames) f = {};
            return false;
        }

        // i : 0 = orteil levé, kFrames - 1 = orteil enfoncé ; nullptr si rien n'est prêt (aucun calcul dans paint)
        const Image* frame (int i) const
        {
            i = jlimit (0, kFrames - 1, i);
            for (auto* set : { &frames, &oldFrames })
                for (int d = 0; d < kFrames; ++d)
                {
                    if (i - d >= 0 && (*set)[(size_t) (i - d)].isValid()) return &(*set)[(size_t) (i - d)];
                    if (i + d < kFrames && (*set)[(size_t) (i + d)].isValid()) return &(*set)[(size_t) (i + d)];
                }
            return nullptr;
        }
        void drawOriginal (Graphics& g) const { g.drawImage (down, lay::plate); }

    private:
        Image build (float t) const
        {
            const double k = scale;
            const int w = jmax (1, roundToInt (box.getWidth() * k)), h = jmax (1, roundToInt (box.getHeight() * k));
            const auto p = lay::plate;
            const double L = p.getX(), R = p.getRight(), T = p.getY(), B = p.getBottom(), CX = p.getCentreX();
            const double lift = 1.0 - (double) jlimit (0.0f, 1.0f, t), th = kTheta * lift;
            auto proj = [&] (double x, double y)
            {
                const double l = B - y, z = l * std::sin (th), Y = B - l * std::cos (th), kk = kCamD / (kCamD - z);
                return Point<double> (((CX + (x - CX) * kk) - box.getX()) * k, ((kCamY + (Y - kCamY) * kk) - box.getY()) * k);
            };
            const Point<double> dst[4] = { proj (L, T), proj (R, T), proj (R, B), proj (L, B) };
            const double sw = src.getWidth(), sh = src.getHeight();
            const Point<double> srcQ[4] = { { 0, 0 }, { sw, 0 }, { sw, sh }, { 0, sh } };
            Homography H;
            if (! H.solve (dst, srcQ)) return {};

            // éclairage diffus : normale (0, sin th, cos th), lumière L = (-1, -1, 1.6) normalisée
            const double lx = -1.0, ly = -1.0, lz = 1.6, ln = std::sqrt (lx * lx + ly * ly + lz * lz);
            const double ndl = std::max (0.0, (ly * std::sin (th) + lz * std::cos (th)) / ln), ndl0 = lz / ln;
            const float dif = (float) ((0.35 + 0.65 * ndl) / (0.35 + 0.65 * ndl0));
            // reflet incliné à 45° (sens antihoraire) qui glisse le long du plateau
            const double topY = dst[0].y, botY = dst[3].y, span = jmax (1.0, botY - topY);
            const double pos = topY + span * (0.295 + 0.35 * lift), xc = 0.5 * (dst[0].x + dst[1].x), bw = 0.10 * span;

            Image plate (Image::ARGB, w, h, true);
            {
                const Image::BitmapData bs (src, Image::BitmapData::readOnly);
                Image::BitmapData bo (plate, Image::BitmapData::writeOnly);
                const int iw = src.getWidth(), ih = src.getHeight();
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                    {
                        const auto q = H.map (x + 0.5, y + 0.5);
                        const double u = q.x - 0.5, v = q.y - 0.5;
                        if (u < -1.0 || v < -1.0 || u > iw || v > ih) continue;
                        PixelARGB px = sample (bs, iw, ih, (float) u, (float) v, dif);
                        const int a = px.getAlpha();
                        if (a > 0)
                        {
                            const double dd = (((double) x - xc) + ((double) y - pos)) * 0.70710678 / bw;
                            const double band = std::exp (-dd * dd);
                            if (band > 0.003)
                            {
                                const double lum = ((double) px.getRed() + px.getGreen() + px.getBlue()) / (3.0 * a);
                                const double add = band * (22.0 + 200.0 * std::pow (lum, 1.5)) * 0.55 * a / 255.0;
                                auto up = [&] (uint8 c) { return (uint8) jmin (a, roundToInt ((double) c + add)); };
                                px.setARGB ((uint8) a, up (px.getRed()), up (px.getGreen()), up (px.getBlue()));
                            }
                        }
                        *(PixelARGB*) bo.getPixelPointer (x, y) = px;
                    }
            }

            // ombre portée : décalage qui grandit avec la hauteur de la pointe (z), projetée vers le bas à droite
            const double zTop = (B - T) * std::sin (th);
            std::vector<float> shd ((size_t) (w * h), 0.0f), tmp ((size_t) (w * h), 0.0f);
            {
                const Image::BitmapData bp (plate, Image::BitmapData::readOnly);
                for (int y = 0; y < h; ++y)
                {
                    const double zy = zTop * jlimit (0.0, 1.0, (botY - (double) y) / span);
                    const int ox = roundToInt ((6.0 + 0.30 * zy) * k), oy = roundToInt ((8.0 + 0.30 * zy) * k);
                    for (int x = 0; x < w; ++x)
                    {
                        const int xs = x - ox, ys = y - oy;
                        if (xs >= 0 && ys >= 0 && xs < w && ys < h)
                            shd[(size_t) (y * w + x)] = (float) ((const PixelARGB*) bp.getPixelPointer (xs, ys))->getAlpha() / 255.0f;
                    }
                }
            }
            const int rad = jmax (1, roundToInt ((4.0 + 6.0 * lift) * k));
            const float norm = 1.0f / (float) (2 * rad + 1);
            for (int pass = 0; pass < 2; ++pass)
            {
                for (int y = 0; y < h; ++y)
                {
                    float acc = 0.0f;
                    for (int x = -rad; x < w + rad; ++x)
                    {
                        if (x + rad < w) acc += shd[(size_t) (y * w + x + rad)];
                        if (x - rad - 1 >= 0) acc -= shd[(size_t) (y * w + x - rad - 1)];
                        if (x >= 0 && x < w) tmp[(size_t) (y * w + x)] = acc * norm;
                    }
                }
                for (int x = 0; x < w; ++x)
                {
                    float acc = 0.0f;
                    for (int y = -rad; y < h + rad; ++y)
                    {
                        if (y + rad < h) acc += tmp[(size_t) ((y + rad) * w + x)];
                        if (y - rad - 1 >= 0) acc -= tmp[(size_t) ((y - rad - 1) * w + x)];
                        if (y >= 0 && y < h) shd[(size_t) (y * w + x)] = acc * norm;
                    }
                }
            }
            Image out (Image::ARGB, w, h, true);
            {
                const Image::BitmapData bp (plate, Image::BitmapData::readOnly);
                Image::BitmapData bo (out, Image::BitmapData::writeOnly);
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                    {
                        const PixelARGB pp = *(const PixelARGB*) bp.getPixelPointer (x, y);
                        const float pa = (float) pp.getAlpha() / 255.0f;
                        const float sa = shd[(size_t) (y * w + x)] * 0.7f * (1.0f - pa);
                        PixelARGB o;   // prémultiplié : l'ombre noire n'ajoute que de l'alpha
                        o.setARGB ((uint8) jlimit (0, 255, roundToInt ((pa + sa) * 255.0f)), pp.getRed(), pp.getGreen(), pp.getBlue());
                        *(PixelARGB*) bo.getPixelPointer (x, y) = o;
                    }
            }
            return out;
        }

        static PixelARGB sample (const Image::BitmapData& bd, int w, int h, float x, float y, float gain)
        {
            const int x0 = (int) std::floor (x), y0 = (int) std::floor (y);
            const float fx = x - (float) x0, fy = y - (float) y0;
            auto px = [&] (int xx, int yy) -> PixelARGB
            {
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) { PixelARGB z; z.setARGB (0, 0, 0, 0); return z; }
                return *(const PixelARGB*) bd.getPixelPointer (xx, yy);
            };
            const PixelARGB a = px (x0, y0), b = px (x0 + 1, y0), c = px (x0, y0 + 1), d = px (x0 + 1, y0 + 1);
            auto mix = [&] (int ca, int cb, int cc, int cd, float k)
            {
                const float t = (float) ca + ((float) cb - (float) ca) * fx, u = (float) cc + ((float) cd - (float) cc) * fx;
                return (uint8) jlimit (0, 255, roundToInt ((t + (u - t) * fy) * k));
            };
            PixelARGB o;
            const uint8 al = mix (a.getAlpha(), b.getAlpha(), c.getAlpha(), d.getAlpha(), 1.0f);
            o.setARGB (al, jmin (al, mix (a.getRed(), b.getRed(), c.getRed(), d.getRed(), gain)),
                       jmin (al, mix (a.getGreen(), b.getGreen(), c.getGreen(), d.getGreen(), gain)),
                       jmin (al, mix (a.getBlue(), b.getBlue(), c.getBlue(), d.getBlue(), gain)));
            return o;
        }

        Image down, src;
        Rectangle<int> box { 0, 0, kW, kH };
        float scale = -1.0f;
        std::array<Image, kFrames> frames, oldFrames;
    };

    //==========================================================================
    // texte centré sur son encre (pas sur l'avance de la police), centre vertical = milieu ascendante / descendante
    void drawInk (Graphics& g, const String& text, const Font& f, float cx, float cy, Colour c)
    {
        GlyphArrangement ga;
        ga.addLineOfText (f, text, 0.0f, 0.0f);
        const auto bb = ga.getBoundingBox (0, -1, false);
        ga.moveRangeOfGlyphs (0, -1, cx - bb.getCentreX(), cy + (f.getAscent() - f.getDescent()) * 0.5f);
        g.setColour (c);
        ga.draw (g);
    }

    // menu de taille de fenêtre (identique à Ring Think) + taille par défaut des nouvelles fenêtres
    constexpr int kNumSizes = 3;
    void addSizeItems (PopupMenu& m, const WharryEditor* ed)
    {
        const char* names[kNumSizes] = { "60 %", "75 %", "100 %" };
        const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
        const int def = WigaWindowSize::defaultIndex (JucePlugin_Name, kNumSizes);
        PopupMenu defSizes;
        for (int i = 0; i < kNumSizes; ++i)
        {
            m.addItem (902 + i, String (names[i]) + (i == def ? "  (default)" : ""), true, cur == i);
            defSizes.addItem (920 + i, names[i], true, i == def);
        }
        m.addSubMenu ("Default size (new windows)", defSizes);
    }

    void handleSizeChoice (int r, WharryEditor* ed)
    {
        if (ed == nullptr) return;
        if (r >= 902 && r < 902 + kNumSizes) ed->setScaleIndex (r - 902);
        else if (r >= 920 && r < 920 + kNumSizes)
        {
            WigaWindowSize::setDefaultIndex (JucePlugin_Name, r - 920);
            ed->setScaleIndex (r - 920);
        }
    }
}

//==============================================================================
class PedalView : public Component, private Timer
{
public:
    explicit PedalView (WharryProcessor& p) : proc (p)
    {
        bgSrc       = loadPng (BinaryData::background_png, BinaryData::background_pngSize);
        slider.src  = loadPng (BinaryData::sw_slider_png, BinaryData::sw_slider_pngSize);
        fsCap.src   = loadPng (BinaryData::fs_cap_png, BinaryData::fs_cap_pngSize);
        treadle.setSource (loadPng (BinaryData::treadle_png, BinaryData::treadle_pngSize));
        tfReg   = Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf, BinaryData::PoppinsRegular_ttfSize);
        tfMed   = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
        tfBold  = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
        tfLight = Typeface::createSystemTypefaceFor (BinaryData::PoppinsLight_ttf, BinaryData::PoppinsLight_ttfSize);
        menuLnf.f = { tfReg, tfMed, tfBold };
        if (auto xml = XmlDocument::parse (String::fromUTF8 (BinaryData::NineHandsLogoWhite_svg, BinaryData::NineHandsLogoWhite_svgSize)))
            logo = Drawable::createFromSVG (*xml);

        // potards : chapeau photo fixe (sa lumière ne tourne pas), index + rainure qui tournent avec flou de mouvement
        nh::Knob::Layers L;
        L.shadow = loadPng (BinaryData::knob_shadow_png, BinaryData::knob_shadow_pngSize);
        L.base   = loadPng (BinaryData::knob_base_png, BinaryData::knob_base_pngSize);
        L.marker = loadPng (BinaryData::knob_marker_png, BinaryData::knob_marker_pngSize);
        nh::Knob::Style selStyle; selStyle.steps = wharry::kNumPrograms; selStyle.dragPixels = 21.0f * 13.0f;
        selStyle.shadowOffset = { 0.02f, 0.02f };
        sel = std::make_unique<nh::Knob> (*proc.apvts.getParameter ("program"), L, selStyle);
        sel->onUserChange = [this] { proc.clearActiveFactory(); };
        sel->onRightClick = [this] (const MouseEvent& e) { showMenu (e.getEventRelativeTo (this).position / s); };
        addAndMakeVisible (*sel);
        const char* ids[3] = { "smooth", "tone", "mix" };
        for (int i = 0; i < 3; ++i)
        {
            small[i] = std::make_unique<nh::Knob> (*proc.apvts.getParameter (ids[i]), L);
            small[i]->onRightClick = [this] (const MouseEvent& e) { showMenu (e.getEventRelativeTo (this).position / s); };
            addAndMakeVisible (*small[i]);
        }
        glow = NineHandsGlow::percent (JucePlugin_Name);
        drawnActive = param ("active");
        ringLevel = drawnActive ? 1.0f : 0.0f;
        tgAnim = param ("chords") ? 1.0f : 0.0f;
        setOpaque (true);
        startTimerHz (30);
    }

    ~PedalView() override { PopupMenu::dismissAllActiveMenus(); }

    void resized() override
    {
        s = (float) getWidth() / (float) kW;
        auto place = [this] (Component& c, Point<float> centre, float d)
        {
            const float sz = d * lay::knobCanvas;
            c.setBounds ((Rectangle<float> (sz, sz).withCentre (centre) * s).toNearestInt());
        };
        place (*sel, lay::selC, lay::selD);
        for (int i = 0; i < 3; ++i) place (*small[i], lay::small[i], lay::smallD);
        resizeSettle = 4;
        repaint();
    }

    // caches à la taille réelle en pixels (fenêtre x échelle Windows / Retina) : appelé par le timer, jamais par paint
    void rebuildCaches()
    {
        if (getWidth() <= 0) return;
        const float ps = s * dispScale;
        slider.prepare (ps * 0.5f);              // sprites rendus à 2 px par unité
        fsCap.prepare (ps * 0.5f);
        treadle.prepare (ps);
        const int w = jmax (1, roundToInt ((float) kW * ps)), h = jmax (1, roundToInt ((float) kH * ps));
        Image bg (Image::ARGB, w, h, true);
        {
            Graphics g (bg);
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (bgSrc, Rectangle<float> ((float) w, (float) h));
            g.addTransform (AffineTransform::scale (ps));
            drawStatic (g);
        }
        bgCache = bg;
        askRepaint (getLocalBounds());
    }

    void paint (Graphics& g) override
    {
        lastPaintMs = Time::getMillisecondCounterHiRes();
        repaintAskedMs = 0.0;
        g.fillAll (Colour (0xff0d0e10));                  // derrière les zones transparentes (autour des jacks)
        if (bgCache.isValid()) g.drawImage (bgCache, getLocalBounds().toFloat());
        else { g.drawImage (bgSrc, getLocalBounds().toFloat()); }
        g.addTransform (AffineTransform::scale (s));

        // ---- plateau (deux positions voisines fondues -> mouvement continu) ----
        const float fpos = jlimit (0.0f, 1.0f, drawnPedal) * (float) (kFrames - 1);
        const int f0 = jlimit (0, kFrames - 1, (int) std::floor (fpos)), f1 = jmin (kFrames - 1, f0 + 1);
        const float fr = fpos - (float) f0;
        const auto tb = treadle.designBounds().toFloat();
        if (auto* im0 = treadle.frame (f0))
        {
            g.drawImage (*im0, tb);
            if (fr > 0.02f && f1 != f0) if (auto* im1 = treadle.frame (f1)) { g.setOpacity (fr); g.drawImage (*im1, tb); g.setOpacity (1.0f); }
        }
        else treadle.drawOriginal (g);

        // ---- inverseur CLASSIC / CHORDS : le curseur glisse dans sa fenêtre ----
        {
            const float x = lay::sliderL + (lay::sliderR - lay::sliderL) * tgAnim;
            g.drawImage (slider.get(), Rectangle<float> (lay::sliderW, lay::sliderH).withCentre ({ x, lay::sliderY }));
        }

        // ---- footswitch : l'écrou reste fixe, le bouton s'enfonce ----
        const Point<float> fc (lay::ringX, lay::ringY);
        if (fsPress > 0.001f)
        {
            const float rr = lay::capR, p = fsPress;
            g.setColour (Colour (0xff0d0c0e).withAlpha (0.92f * p));
            g.fillEllipse (fc.x - rr, fc.y - rr, 2 * rr, 2 * rr);
            const float sc = 1.0f - 0.07f * p, R = rr * sc;
            const Point<float> cc (fc.x + 0.3f * p, fc.y + 1.2f * p);
            g.drawImage (fsCap.get(), Rectangle<float> (cc.x - R, cc.y - R, 2 * R, 2 * R));
            g.setColour (Colours::black.withAlpha (0.26f * p));
            g.fillEllipse (cc.x - R, cc.y - R, 2 * R, 2 * R);
            ColourGradient sh (Colours::black.withAlpha (0.5f * p), cc.x - R * 0.75f, cc.y - R * 0.75f,
                               Colours::transparentBlack, cc.x + R * 0.1f, cc.y + R * 0.1f, false);
            g.setGradientFill (sh);
            Path ring; ring.addEllipse (cc.x - R, cc.y - R, 2 * R, 2 * R);
            ring.addEllipse (cc.x - R + 1.6f * p, cc.y - R + 1.9f * p, 2 * R - 1.4f * p, 2 * R - 1.4f * p);
            ring.setUsingNonZeroWinding (false);
            g.fillPath (ring);
        }
        drawFootRing (g, ringLevel);

        drawScreen (g);
        drawPill (g);
    }

    // ------------------------------------------------------------ souris
    void mouseDown (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (e.mods.isPopupMenu()) { showMenu (p); return; }
        drag = Drag::None;
        if (lay::screen.contains (p))
        {
            if (proc.isCalibrating()) { proc.finishCalibration(); return; }
            showPresetMenu(); return;
        }
        if (lay::pill.contains (p))
        {
            const int cur = currentProgram();
            if (p.x < lay::pill.getX() + 34.0f) { proc.selectWhammyProgram ((cur + wharry::kNumPrograms - 1) % wharry::kNumPrograms); return; }
            if (p.x > lay::pill.getRight() - 34.0f) { proc.selectWhammyProgram ((cur + 1) % wharry::kNumPrograms); return; }
            showPresetMenu(); return;
        }
        if (lay::sw.expanded (10.0f, 14.0f).contains (p)) { proc.togglePedalSwitch ("chords"); return; }
        if (onFootswitch (p)) { pressed = true; proc.togglePedalSwitch ("active"); return; }
        if (lay::plate.contains (p))
        {
            drag = Drag::Pedal; dragStartY = p.y;
            dragStartVal = proc.apvts.getRawParameterValue ("pedal")->load();
            if (auto* par = proc.apvts.getParameter ("pedal")) par->beginChangeGesture();
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (drag == Drag::Pedal)
        {
            // tirer vers le bas = appuyer sur la pointe (orteil enfoncé)
            const float v = jlimit (0.0f, 1.0f, dragStartVal + (p.y - dragStartY) / (lay::plate.getHeight() * 0.7f));
            if (auto* par = proc.apvts.getParameter ("pedal")) par->setValueNotifyingHost (v);
        }
    }

    void mouseUp (const MouseEvent&) override
    {
        if (drag == Drag::Pedal) if (auto* par = proc.apvts.getParameter ("pedal")) par->endChangeGesture();
        drag = Drag::None;
        pressed = false;
    }

    void mouseMove (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        int h = 0;
        if (lay::pill.contains (p)) h = p.x < lay::pill.getX() + 34.0f ? 1 : (p.x > lay::pill.getRight() - 34.0f ? 2 : 0);
        if (h != hoverArrow) { hoverArrow = h; askRepaint (pillArea()); }
    }
    void mouseExit (const MouseEvent&) override { if (hoverArrow != 0) { hoverArrow = 0; askRepaint (pillArea()); } }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (lay::plate.contains (p))              // double-clic sur le plateau : tout en haut / tout en bas
            proc.setParamValue ("pedal", proc.apvts.getRawParameterValue ("pedal")->load() > 0.5f ? 0.0f : 1.0f);
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const auto p = e.position / s;
        if (lay::plate.contains (p))
            proc.setParamValue ("pedal", jlimit (0.0f, 1.0f, proc.apvts.getRawParameterValue ("pedal")->load() - w.deltaY * 0.25f));
        else if (lay::pill.contains (p) || lay::screen.contains (p))
            proc.selectWhammyProgram (jlimit (0, wharry::kNumPrograms - 1, currentProgram() + (w.deltaY > 0 ? -1 : 1)));
    }

    void initCaches() { dispScale = physScale(); approxScale = (float) Component::getApproximateScaleFactorForComponent (this); resizeSettle = 0; rebuildCaches(); }

private:
    enum class Drag { None, Pedal };

    float physScale() const
    {
        if (auto* d = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())) return jmax (1.0f, (float) d->scale);
        return 1.0f;
    }
    Font font (const Typeface::Ptr& tf, float h) const { return Font (FontOptions (tf).withHeight (h)); }
    Rectangle<int> unitsToPx (Rectangle<float> r) const { return (r * s).getSmallestIntegerContainer().expanded (2); }
    Rectangle<int> pillArea() const { return unitsToPx (lay::pill.expanded (3.0f)); }
    Rectangle<int> screenArea() const { return unitsToPx (lay::screen.expanded (2.0f)); }
    Rectangle<int> fsArea() const { const float r = lay::ringR + 22.0f; return unitsToPx ({ lay::ringX - r, lay::ringY - r, 2 * r, 2 * r }); }
    bool onFootswitch (Point<float> p) const { return p.getDistanceFrom ({ lay::ringX, lay::ringY }) < lay::ringR * 1.3f; }

    void askRepaint (Rectangle<int> area)
    {
        if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
        repaint (area);
    }
    int currentProgram() const { return jlimit (0, wharry::kNumPrograms - 1, (int) std::lround (proc.apvts.getRawParameterValue ("program")->load())); }
    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }

    // ---- sérigraphie, graduations et logo (dessinés une fois dans le cache du fond, en vectoriel) ----
    void drawStatic (Graphics& g)
    {
        const Colour tick (0xe1ffffff);
        auto ticks = [&] (Point<float> c, int n, float r0, float r1, float rBig, std::function<bool (int)> big, float wdt)
        {
            g.setColour (tick);
            for (int k = 0; k < n; ++k)
            {
                const float a = degreesToRadians (-135.0f + 270.0f * (float) k / (float) (n - 1));
                const float rb = big (k) ? rBig : r1;
                const Point<float> d (std::sin (a), -std::cos (a));
                g.drawLine (Line<float> (c + d * r0, c + d * rb), wdt);
            }
        };
        ticks (lay::selC, wharry::kNumPrograms, lay::selTickR0, lay::selTickR1, lay::selTickR2,
               [] (int k) { return k == 0 || k == 9 || k == 10 || k == 11 || k == 12 || k == 20; }, 1.6f);
        for (int i = 0; i < 3; ++i)
            ticks (lay::small[i], 11, lay::smallTickR0, lay::smallTickR1, lay::smallTickR1 + 2.0f,
                   [] (int k) { return k == 0 || k == 5 || k == 10; }, 1.6f);

        const auto fLab = font (tfMed, lay::labelSize);
        const char* names[3] = { "GLIDE", "TONE", "MIX" };
        for (int i = 0; i < 3; ++i) drawInk (g, names[i], fLab, lay::small[i].x, lay::labelY, kInk);

        const auto fSel = font (tfMed, lay::selLabelSize);
        drawInk (g, "WHAMMY",  fSel, lay::selC.x - lay::selLabelE, lay::selC.y + lay::selLabelE + lay::selLabelDy, kInk);
        drawInk (g, "HARMONY", fSel, lay::selC.x + lay::selLabelE, lay::selC.y + lay::selLabelE + lay::selLabelDy, kInk);
        drawInk (g, "DETUNE",  fSel, lay::selC.x, lay::selC.y - lay::selLabelTop, kInk);

        {   // CLASSIC / CHORDS sous l'inverseur (la fin du S de CHORDS tombe sur le bord droit de l'écran)
            const auto f = font (tfMed, lay::swLabelSize);
            const float w1 = GlyphArrangement::getStringWidth (f, "CLASSIC");
            const float base = lay::swLabelY + (f.getAscent() - f.getDescent()) * 0.5f;
            g.setColour (kInk);
            GlyphArrangement a; a.addLineOfText (f, "CLASSIC", lay::swLabelX0, base); a.draw (g);
            GlyphArrangement b; b.addLineOfText (f, "CHORDS", lay::swLabelX0 + w1 + 13.0f, base); b.draw (g);
        }

        if (logo != nullptr)
        {
            const float lw = lay::logoW, lh = lw * 236.0f / 2510.0f;
            logo->drawWithin (g, { lay::logoX, lay::logoCY - lh * 0.5f, lw, lh }, RectanglePlacement::centred, 1.0f);
        }

        drawInk (g, "WHARRY", font (tfLight, lay::titleSize).withExtraKerningFactor (lay::titleSpacing / lay::titleSize), lay::titleX, lay::titleY, kInk);
        drawInk (g, "PITCH SHIFT  +  HARMONY  +  DE-TUNE", font (tfReg, lay::subSize), lay::subX, lay::subY, Colour (0xc8ffffff));

        for (int io = 0; io < 2; ++io)           // IN sur la tranche droite, OUT sur la tranche gauche
        {
            Graphics::ScopedSaveState st (g);
            const float x = io == 0 ? lay::inX : lay::outX;
            g.addTransform (AffineTransform::rotation (io == 0 ? MathConstants<float>::halfPi : -MathConstants<float>::halfPi).translated (x, lay::jackY));
            drawInk (g, io == 0 ? "IN" : "OUT", font (tfMed, lay::ioSize), 0.0f, 0.0f, kInk);
        }

        // écran : cadre et vitre (le contenu est dessiné en direct)
        const auto sc = lay::screen;
        g.setColour (Colour (0xff140608)); g.fillRoundedRectangle (sc.expanded (3.0f), 9.0f);
        g.setColour (Colour (0xff080a0e)); g.fillRoundedRectangle (sc, 7.0f);
    }

    // anneau lumineux dans la gorge du footswitch (comme Octopus) ; Glow ne dose que le halo
    void drawFootRing (Graphics& g, float level)
    {
        if (level <= 0.002f) return;
        const float R = lay::ringR, W = lay::ringW, k = R / 66.4f, gl = (float) glow / 100.0f;
        const Point<float> c (lay::ringX, lay::ringY);
        auto ring = [&] (float width, Colour col)
        {
            g.setColour (col);
            g.drawEllipse (c.x - R, c.y - R, 2 * R, 2 * R, width);
        };
        {
            Graphics::ScopedSaveState st (g);
            Path clip; clip.addRectangle (c.x - R - 40, c.y - R - 40, 2 * R + 80, 2 * R + 80);
            const float ri = R - W * 0.6f;
            clip.addEllipse (c.x - ri, c.y - ri, 2 * ri, 2 * ri);
            clip.setUsingNonZeroWinding (false);
            g.reduceClipRegion (clip);                  // le halo reste dans la gorge et sur l'écrou, pas sur le bouton
            for (float wv = 20.0f; wv >= 3.0f; wv -= 1.5f) ring (wv * k, kRing.withAlpha (0.075f * gl * level));
            for (int i = 0; i < 6; ++i)                   // léger reflet de la couleur sur l'écrou
            {
                g.setColour (kRing.withAlpha (0.42f / 6.0f * level * gl));
                const float rr = R + 3.5f;
                g.drawEllipse (c.x - rr, c.y - rr, 2 * rr, 2 * rr, 2.0f + 1.0f * (float) i);
            }
        }
        ring (W, kRing.withAlpha (0.85f * level));
        ring (W * 0.42f, kRing.interpolatedWith (Colours::white, 0.55f).withAlpha (0.9f * level));
    }

    // écran : famille · moteur, programme, décalage en direct, jauge de la pédale, numéro de programme
    void drawScreen (Graphics& g)
    {
        const auto sc = lay::screen;
        const int cur = jlimit (0, wharry::kNumPrograms - 1, drawnProgram);
        const auto& pr = wharry::program (cur);
        const float al = drawnActive ? 1.0f : 0.4f;
        auto row = [&] (int k) { return sc.getY() + sc.getHeight() * ((float) k + 0.5f) / 5.0f; };
        const String fam = pr.type == wharry::Whammy ? "WHAMMY" : (pr.type == wharry::Detune ? "DETUNE" : "HARMONY");
        const String mode = drawnChords ? (param ("studio") ? "CHORDS HQ" : "CHORDS") : "CLASSIC";
        drawInk (g, fam + "  " + String (CharPointer_UTF8 ("\xc2\xb7")) + "  " + mode, font (tfMed, 11.5f), sc.getCentreX(), row (0), kScrHead.withMultipliedAlpha (al));
        {
            auto f = font (tfMed, 23.0f);
            const float maxW = sc.getWidth() - 24.0f, tw = GlyphArrangement::getStringWidth (f, pr.label);
            if (tw > maxW) f = f.withHeight (23.0f * maxW / tw);
            drawInk (g, pr.label, f, sc.getCentreX(), row (1), kScrName.withMultipliedAlpha (al));
        }
        String val;
        if (pr.type == wharry::Detune) val = String (roundToInt (pr.up)) + " cents";
        else val = (drawnSemis > 0.05f ? "+" : "") + String (std::abs (drawnSemis) < 0.05f ? 0.0f : drawnSemis, 1) + " st";
        drawInk (g, val, font (tfLight, 21.0f), sc.getCentreX(), row (2), kScrVal.withMultipliedAlpha (al));
        const float bx0 = sc.getX() + 30.0f, bx1 = sc.getRight() - 30.0f, by = row (3) - 2.5f;
        g.setColour (Colours::white.withAlpha (0.16f * al)); g.fillRoundedRectangle (bx0, by, bx1 - bx0, 5.0f, 2.5f);
        const float tp = jlimit (0.0f, 1.0f, drawnPedal);
        if (tp > 0.005f) { g.setColour (kScrBar.withMultipliedAlpha (al)); g.fillRoundedRectangle (bx0, by, (bx1 - bx0) * tp, 5.0f, 2.5f); }
        String foot = drawnActive ? "PROGRAMME " + String (cur + 1) + " / " + String (wharry::kNumPrograms) : String ("BYPASS");
        drawInk (g, foot, font (tfReg, 10.0f), sc.getCentreX(), row (4), Colours::white.withAlpha (drawnActive ? 0.43f : 0.75f));
    }

    String presetText() const
    {
        const int cur = jlimit (0, wharry::kNumPrograms - 1, drawnProgram), af = proc.getActiveFactory();
        if (af >= 0 && WharryProcessor::factory (af).program == cur) return String (WharryProcessor::factory (af).name).toUpperCase();
        return String (cur + 1) + "  " + String (CharPointer_UTF8 ("\xc2\xb7")) + "  " + wharry::program (cur).label;
    }

    // barre de presets et ligne d'état au même endroit (fondu croisé)
    void drawPill (Graphics& g)
    {
        const auto pb = lay::pill;
        const float h = pb.getHeight();
        g.setColour (Colour (0xff1e080a)); g.fillRoundedRectangle (pb.expanded (1.0f), h * 0.5f + 1.0f);
        g.setColour (Colour (0xff0a0a0c)); g.fillRoundedRectangle (pb, h * 0.5f);
        g.setColour (Colour (0x8c969696)); g.drawRoundedRectangle (pb.reduced (0.5f), h * 0.5f, 1.0f);
        const float sa = statusAlpha;
        const auto f = font (tfMed, 20.7f);
        if (sa < 0.999f)
        {
            const float pa = 1.0f - sa;
            auto ff = f;
            const String t = presetText();
            const float maxW = pb.getWidth() - 2.0f * 34.0f - 8.0f, tw = GlyphArrangement::getStringWidth (ff, t);
            if (tw > maxW) ff = ff.withHorizontalScale (jmax (0.6f, maxW / tw));
            drawInk (g, t, ff, pb.getCentreX(), pb.getCentreY(), kInk.withMultipliedAlpha (pa));
            for (int side = 0; side < 2; ++side)
            {
                const float tx = side == 0 ? pb.getX() + 15.5f : pb.getRight() - 15.5f, ty = pb.getCentreY(), a = 5.5f;
                Path tri;
                if (side == 0) tri.addTriangle (tx - a, ty, tx + a, ty - a, tx + a, ty + a);
                else           tri.addTriangle (tx + a, ty, tx - a, ty - a, tx - a, ty + a);
                g.setColour (Colours::white.withAlpha ((hoverArrow == side + 1 ? 1.0f : 0.6f) * pa));
                g.fillPath (tri);
            }
        }
        if (sa > 0.001f)
        {
            auto ff = f;
            const float maxW = pb.getWidth() - h, tw = GlyphArrangement::getStringWidth (ff, drawnStatus);
            if (tw > maxW) ff = ff.withHeight (20.7f * jmax (0.6f, maxW / tw));
            String t = drawnStatus;
            if (GlyphArrangement::getStringWidth (ff, t) > maxW)
                while (t.length() > 4 && GlyphArrangement::getStringWidth (ff, t + "...") > maxW) t = t.dropLastCharacters (1);
            if (t != drawnStatus) t << "...";
            drawInk (g, t, ff, pb.getCentreX(), pb.getCentreY(), kInk.withMultipliedAlpha (sa));
        }
    }

    // ligne d'état : valeurs en direct, MIDI Learn, calibration, Glow
    String statusText() const
    {
        const int learn = proc.learnTarget.load();
        if (proc.isCalibrating())
            return proc.calibrationSpan() > 0.3f ? "CALIBRATION OK  -  click the screen to finish"
                                                 : "CALIBRATION  -  press the pedal fully down, then up";
        if (learn >= 0) return String ("MIDI LEARN  -  move a control for ") + WharryProcessor::learnDef (learn).name;
        if (Time::getMillisecondCounterHiRes() < msgUntil) return msg;
        auto pc = [this] (const char* id) { return String (roundToInt (proc.apvts.getParameter (id)->getValue() * 100.0f)) + " %"; };
        String st = "GLIDE " + String (roundToInt (proc.apvts.getRawParameterValue ("smooth")->load())) + " ms"
                  + "  " + String (CharPointer_UTF8 ("\xc2\xb7")) + "  TONE " + pc ("tone")
                  + "  " + String (CharPointer_UTF8 ("\xc2\xb7")) + "  MIX " + pc ("mix");
        if (param ("chords") && param ("studio") && proc.getLatency() > 0)
            st << "  " << String (CharPointer_UTF8 ("\xc2\xb7")) << "  " << String (roundToInt (1000.0 * proc.getLatency() / jmax (1.0, proc.getSampleRate()))) << " ms";
        return st;
    }
    void flashMessage (const String& m) { msg = m; msgUntil = Time::getMillisecondCounterHiRes() + 2500.0; statusUntil = msgUntil; }

    void timerCallback() override
    {
        const double now = Time::getMillisecondCounterHiRes();
        auto* ed = findParentComponentOfClass<WharryEditor>();
        if (ed != nullptr) ed->checkRenderer();

        if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
        else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
        {
            lastScaleCheckMs = now;
            const float sc = physScale(), ap = (float) Component::getApproximateScaleFactorForComponent (this);
            if (std::abs (sc - dispScale) > 0.01f || std::abs (ap - approxScale) > 0.01f) { dispScale = sc; approxScale = ap; rebuildCaches(); }
        }
        if (now - lastGlowCheckMs > 3000.0)                // Glow commun à toutes les fenêtres du plugin
        {
            lastGlowCheckMs = now;
            const int gp = NineHandsGlow::percent (JucePlugin_Name);
            if (gp != glow) { glow = gp; askRepaint (fsArea()); }
        }

        // chien de garde : un redessin demandé n'arrive pas -> on relance le moteur de rendu et on redessine tout
        if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
        {
            repaintAskedMs = 0.0;
            if (ed != nullptr) ed->resetRenderer();
            repaint();
        }

        if (proc.isCalibrating() && proc.calibrationSpan() > 0.3f && proc.msSincePedalMove() > 1500u) proc.finishCalibration();
        // les positions du plateau sont précalculées au fil des ticks, jamais dans paint
        if (getWidth() > 0 && resizeSettle == 0)
        {
            const int cur = roundToInt (jlimit (0.0f, 1.0f, proc.getPedalShown()) * (float) (kFrames - 1));
            const double t0 = Time::getMillisecondCounterHiRes();
            bool built = false;
            while (Time::getMillisecondCounterHiRes() - t0 < 6.0 && treadle.buildNextMissing (cur)) built = true;
            if (built) askRepaint (unitsToPx (treadle.designBounds().toFloat()));
        }

        const int learned = proc.lastLearned.exchange (-1);
        if (learned >= 0) flashMessage (String (WharryProcessor::learnDef (learned).name) + " = " + proc.learnAssignText (learned));

        const int prog = currentProgram();
        const bool chords = param ("chords"), active = param ("active");
        const float ped = proc.getPedalShown(), sm = proc.getSemitones();
        bool screen = false, pill = false;
        if (proc.getActiveFactory() != drawnFactory) { drawnFactory = proc.getActiveFactory(); screen = pill = true; }
        if (std::abs (ped - drawnPedal) > 0.002f)
        {
            drawnPedal = ped; screen = true;
            askRepaint (unitsToPx (treadle.designBounds().toFloat()));
        }
        if (std::abs (sm - drawnSemis) > 0.04f) { drawnSemis = sm; screen = true; }
        if (prog != drawnProgram) { drawnProgram = prog; screen = pill = true; }
        if (chords != drawnChords || active != drawnActive)
        {
            if (active != drawnActive && ! pressed) flashTicks = 4;     // commande MIDI / automation : le bouton plonge brièvement
            if (chords != drawnChords && drawnProgram >= 0) flashMessage (chords ? "CHORDS (polyphonic)" : "CLASSIC (single notes)");
            drawnChords = chords; drawnActive = active; screen = true;
        }
        // réglages : la ligne d'état prend la place de la barre de presets ~2,5 s
        for (int i = 0; i < 3; ++i)
        {
            const char* ids[3] = { "smooth", "tone", "mix" };
            const float v = proc.apvts.getParameter (ids[i])->getValue();
            if (std::abs (v - lastSmall[i]) > 0.0005f) { if (lastSmall[i] >= 0.0f) statusUntil = now + 2500.0; lastSmall[i] = v; }
        }
        const bool persistent = proc.isCalibrating() || proc.learnTarget.load() >= 0;
        const String st = statusText();
        if (st != drawnStatus) { drawnStatus = st; pill = true; }
        const float sTarget = (persistent || now < statusUntil) ? 1.0f : 0.0f;
        if (std::abs (statusAlpha - sTarget) > 0.001f)
        {
            statusAlpha = sTarget > statusAlpha ? jmin (sTarget, statusAlpha + 0.28f) : jmax (sTarget, statusAlpha - 0.11f);
            pill = true;
        }

        // animations : curseur de l'inverseur, bouton du footswitch, anneau lumineux
        {
            const float tgT = drawnChords ? 1.0f : 0.0f;
            if (std::abs (tgAnim - tgT) > 0.001f)
            {
                const float d = tgT - tgAnim, st2 = 0.25f;
                tgAnim = std::abs (d) <= st2 ? tgT : tgAnim + (d > 0 ? st2 : -st2);
                askRepaint (unitsToPx (lay::sw.expanded (4.0f)));
            }
            if (flashTicks > 0) --flashTicks;
            const float fsT = (pressed || flashTicks > 0) ? 1.0f : 0.0f;
            if (std::abs (fsPress - fsT) > 0.001f)
            {
                fsPress = fsT > fsPress ? jmin (fsT, fsPress + 0.5f) : jmax (fsT, fsPress - 0.34f);
                askRepaint (fsArea());
            }
            const float rT = drawnActive ? 1.0f : 0.0f;
            if (std::abs (ringLevel - rT) > 0.001f)
            {
                ringLevel = rT > ringLevel ? jmin (rT, ringLevel + 0.45f) : jmax (rT, ringLevel - 0.28f);
                askRepaint (fsArea());
            }
        }
        if (screen) askRepaint (screenArea());
        if (pill) askRepaint (pillArea());
    }

    void showPresetMenu()
    {
        PopupMenu m;
        m.setLookAndFeel (&menuLnf);
        const int cur = currentProgram(), af = proc.getActiveFactory();
        NineHandsMenu::addHeader (m, menuLnf.f, NineHandsMenu::icoPreset, kAccent, "Presets", presetText());
        m.addSectionHeader ("NINE HANDS | DSP factory presets");
        for (int i = 0; i < WharryProcessor::kNumFactory; ++i) m.addItem (101 + i, WharryProcessor::factory (i).name, true, i == af);
        m.addSectionHeader ("Whammy programs");
        for (int i = 0; i < wharry::kNumPrograms; ++i)
            m.addItem (1 + i, String (i + 1) + "  " + wharry::program (i).name, true, i == cur && af < 0);
        Component::SafePointer<PedalView> sp (this);
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [sp] (int res)
        {
            if (sp == nullptr || res <= 0) return;
            if (res >= 101) sp->proc.loadFactory (res - 101);
            else sp->proc.selectWhammyProgram (res - 1);
        });
    }

    // commande sous la souris : index MIDI Learn, -1 = hors commande
    int learnIndexAt (Point<float> p) const
    {
        if (lay::screen.contains (p) || lay::pill.contains (p)) return -1;
        if (p.getDistanceFrom (lay::selC) < lay::selD * 0.55f) return WharryProcessor::lProgram;
        for (int i = 0; i < 3; ++i)
            if (p.getDistanceFrom (lay::small[i]) < lay::smallD * 0.6f) return WharryProcessor::lGlide + i;
        if (lay::sw.expanded (10.0f, 14.0f).contains (p)) return WharryProcessor::lChords;
        if (onFootswitch (p)) return WharryProcessor::lFs;
        if (lay::plate.contains (p)) return WharryProcessor::lPedal;
        return -1;
    }

    String midiSub (int i) const
    {
        const auto a = proc.learnAssignText (i);
        return a == "none" ? String ("MIDI: not assigned") : "MIDI: " + a;
    }

    void addMidiSubmenu (PopupMenu& m)
    {
        PopupMenu midi;
        for (int i = 0; i < WharryProcessor::numLearn; ++i)
            midi.addItem (1000 + i, String ("Learn: ") + WharryProcessor::learnDef (i).name + " (" + proc.learnAssignText (i) + ")",
                          true, proc.learnTarget.load() == i);
        midi.addSeparator();
        if (proc.learnTarget.load() >= 0) midi.addItem (1200, "Cancel MIDI learn");
        midi.addItem (1201, "Clear all MIDI assignments");
        midi.addSeparator();
        midi.addItem (1202, "CLASSIC / CHORDS and footswitch also learn a MIDI note", false);
        m.addSubMenu ("MIDI", midi);
    }

    void handleLearnChoice (int r)
    {
        if (r >= 1000 && r < 1000 + WharryProcessor::numLearn)
            proc.learnTarget.store (proc.learnTarget.load() == r - 1000 ? -1 : r - 1000);
        else if (r >= 1100 && r < 1100 + WharryProcessor::numLearn) proc.clearLearn (r - 1100);
        else if (r == 1200) proc.learnTarget.store (-1);
        else if (r == 1201) proc.resetMidiMap();
    }

    void showMenu (Point<float> p)
    {
        auto* ed = findParentComponentOfClass<WharryEditor>();
        PopupMenu m;
        m.setLookAndFeel (&menuLnf);
        const int learnIdx = learnIndexAt (p);
        if (learnIdx >= 0)
        {
            const String name = WharryProcessor::learnDef (learnIdx).name;
            const int ico = learnIdx == WharryProcessor::lChords ? NineHandsMenu::icoSwitch
                          : learnIdx == WharryProcessor::lFs ? NineHandsMenu::icoLatch
                          : learnIdx == WharryProcessor::lPedal ? NineHandsMenu::icoPedal : NineHandsMenu::icoKnob;
            NineHandsMenu::addHeader (m, menuLnf.f, ico, kAccent, name, midiSub (learnIdx));
            const int lt = proc.learnTarget.load();
            m.addItem (1000 + learnIdx, "MIDI Learn: " + name, true, lt == learnIdx);
            m.addItem (1100 + learnIdx, "Clear MIDI: " + name, proc.learnAssignText (learnIdx) != "none");
            if (lt >= 0) m.addItem (1200, "Cancel MIDI learn");
            m.addSeparator();
        }
        else
            NineHandsMenu::addHeader (m, menuLnf.f, NineHandsMenu::icoPedal, kAccent, "Wharry", "Pitch shift  v" + String (JucePlugin_VersionString));

        const bool onFs = learnIdx == WharryProcessor::lFs, onToggle = learnIdx == WharryProcessor::lChords;
        if (! (onFs || onToggle))
        {
            m.addSectionHeader ("Expression pedal");
            if (learnIdx != WharryProcessor::lPedal)
            {
                m.addItem (31, "MIDI Learn: expression pedal (" + proc.learnAssignText (WharryProcessor::lPedal) + ")");
                m.addItem (32, "Clear MIDI: expression pedal", proc.learnAssignText (WharryProcessor::lPedal) != "none");
            }
            m.addItem (33, "Invert pedal direction", true, param ("invert"));
            m.addItem (35, proc.isCalibrating() ? "Finish pedal calibration" : "Calibrate pedal (" + proc.calibrationDescription() + ")");
            m.addItem (34, "Reset pedal calibration");
            m.addSectionHeader ("Chords engine");
            m.addItem (51, "Live engine (no latency)", true, ! param ("studio"));
            m.addItem (52, "Studio HQ engine (latency, re-amping / mixing)", true, param ("studio"));
            m.addItem (53, "Respond to MIDI program change", true, param ("pc"));
            if (learnIdx < 0) { m.addSeparator(); addMidiSubmenu (m); }
        }
        m.addSectionHeader ("Plugin size");
        addSizeItems (m, ed);
        PopupMenu glowMenu;
        for (int i = 0; i < 4; ++i)
        {
            const int v = 25 * (i + 1);
            glowMenu.addItem (700 + v, "Glow " + String (v) + " %" + (v == 100 ? "  (default)" : ""), true, glow == v);
        }
        m.addSubMenu ("Glow (halo of the lights)", glowMenu);
        m.addSeparator();
        m.addItem (999, "Wharry  v" + String (JucePlugin_VersionString) + "  -  NINE HANDS | DSP", false);

        Component::SafePointer<PedalView> sp (this);
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [sp] (int res)
        {
            if (sp == nullptr || res <= 0) return;
            auto& pr = sp->proc;
            if (res >= 1000) { sp->handleLearnChoice (res); return; }
            if (res >= 725 && res <= 800)
            {
                sp->glow = res - 700;
                NineHandsGlow::setPercent (JucePlugin_Name, sp->glow);
                sp->flashMessage ("Glow  " + String (sp->glow) + " %");
                sp->askRepaint (sp->fsArea());
                return;
            }
            switch (res)
            {
                case 31: pr.startMidiLearn (MidiTarget::Pedal); break;
                case 32: pr.clearMidi (MidiTarget::Pedal); break;
                case 33: pr.togglePedalSwitch ("invert"); break;
                case 34: pr.resetCalibration(); break;
                case 35: if (pr.isCalibrating()) pr.finishCalibration(); else pr.startCalibration(); break;
                case 51: pr.setParamValue ("studio", 0.0f); break;
                case 52: pr.setParamValue ("studio", 1.0f); break;
                case 53: pr.togglePedalSwitch ("pc"); break;
                default: handleSizeChoice (res, sp->findParentComponentOfClass<WharryEditor>()); break;
            }
        });
    }

    WharryProcessor& proc;
    Image bgSrc, bgCache;
    ScaledCache slider, fsCap;
    TreadleWarp treadle;
    std::unique_ptr<Drawable> logo;
    Typeface::Ptr tfReg, tfMed, tfBold, tfLight;
    NineHandsMenu::MenuLnf menuLnf;
    std::unique_ptr<nh::Knob> sel, small[3];
    float s = kDefaultScale * kRefH / (float) kH;

    float dispScale = 1.0f, approxScale = 1.0f;
    int resizeSettle = 0;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastGlowCheckMs = 0.0;

    Drag drag = Drag::None;
    float dragStartY = 0, dragStartVal = 0;
    bool pressed = false;
    int hoverArrow = 0;
    float tgAnim = 0.0f, fsPress = 0.0f, ringLevel = 1.0f, statusAlpha = 0.0f;
    int flashTicks = 0, glow = 100;
    float lastSmall[3] = { -1.0f, -1.0f, -1.0f };

    float drawnPedal = -1.0f, drawnSemis = 0.0f;
    int drawnProgram = -1, drawnFactory = -2;
    bool drawnChords = false, drawnActive = true;
    String drawnStatus, msg;
    double msgUntil = 0.0, statusUntil = 0.0;
};

//==============================================================================
WharryEditor::WharryEditor (WharryProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    view = std::make_unique<PedalView> (proc);
    addAndMakeVisible (*view);
    setResizable (true, true);
    const int minH = (int) std::lround (kRefH * kDefaultScale);          // jamais plus petit que la taille par défaut
    const int maxH = 1120;
    setResizeLimits ((int) std::lround ((double) minH * kW / kH), minH, (int) std::lround ((double) maxH * kW / kH), maxH);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setOpaque (true);
    const int saved = jlimit (-1, 2, proc.uiScaleIdx.load());
    if (saved >= 0) setScaleIndex (saved);
    else            applyScale (WigaWindowSize::defaultIndex (JucePlugin_Name, kNumSizes));
    view->initCaches();
}

WharryEditor::~WharryEditor() = default;

//==============================================================================
// Anti-gel v2 (NINE HANDS | DSP) :
//  1. moteur de rendu logiciel forcé sur Windows (Direct2D peut figer la fenêtre dans certains hôtes),
//     appliqué à chaque nouvelle fenêtre hôte (peer) et revérifié toutes les secondes, jamais en boucle ;
//  2. aucun calcul lourd dans paint() : les caches (fond + sérigraphie, positions du plateau, potards) sont refaits
//     par les timers seulement, après stabilisation de la taille ou quand l'échelle d'affichage change ;
//  3. chien de garde : si un redessin demandé n'est pas arrivé au bout de 1,5 s alors que la fenêtre est
//     visible, le moteur de rendu est réappliqué et toute la fenêtre est redessinée ;
//  4. côté audio : aucun appel à l'hôte ni allocation depuis le thread audio (voir PluginProcessor).
void WharryEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if ((void*) peer != lastPeer) { lastPeer = (void*) peer; repaint(); }
    lastRendererCheckMs = Time::getMillisecondCounterHiRes();
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw)
            peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void WharryEditor::checkRenderer()
{
    if ((void*) getPeer() != lastPeer || Time::getMillisecondCounterHiRes() - lastRendererCheckMs > 1000.0)
        useSoftwareRenderer();
}

void WharryEditor::resetRenderer()
{
    lastPeer = nullptr;
    useSoftwareRenderer();
    repaint();
}

void WharryEditor::paint (Graphics& g) { g.fillAll (Colour (0xff0d0e10)); }
void WharryEditor::parentHierarchyChanged() { useSoftwareRenderer(); }
void WharryEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void WharryEditor::resized()
{
    view->setBounds (getLocalBounds());
}

int WharryEditor::getScaleIndex() const
{
    const float sc = (float) getHeight() / kRefH;
    for (int i = 0; i < kNumSizes; ++i) if (std::abs (sc - kScales[i]) < 0.02f) return i;
    return -1;
}

void WharryEditor::setScaleIndex (int idx)
{
    idx = jlimit (0, kNumSizes - 1, idx);
    proc.uiScaleIdx = idx;
    applyScale (idx);
}

void WharryEditor::applyScale (int idx)
{
    const float sc = kScales[jlimit (0, kNumSizes - 1, idx)];
    const int h = jmax ((int) std::lround (kRefH * kDefaultScale), (int) std::lround (kRefH * sc));
    setSize ((int) std::lround ((double) h * kW / kH), h);
}
