// Tendedero for Windows — the line: image loading, rope layout and drawing.
// Ports enum Layout + Rope (LineView.swift), PeggedView geometry/art,
// makeThumbnail() (Line.swift). Every magic number matches the Swift original.
#include "Common.h"
#include "Line.h"
#include <wincodec.h>
#include <random>
#include <cmath>

#pragma comment(lib, "windowscodecs.lib")
using namespace Gdiplus;

// ---------------------------------------------------------------- images ---
// makeThumbnail(): decode + downscale to maxPixels on the long edge, never up.
bool loadPegImage(Peg& peg, const std::wstring& path, double maxPixels) {
    IWICImagingFactory* fac = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&fac)))) return false;
    bool ok = false;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    HBITMAP hbmp = nullptr;
    do {
        if (FAILED(fac->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnLoad, &dec))) break;
        if (FAILED(dec->GetFrame(0, &frame))) break;
        if (FAILED(fac->CreateFormatConverter(&conv))) break;
        if (FAILED(conv->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                                    WICBitmapDitherTypeNone, nullptr, 0.0,
                                    WICBitmapPaletteTypeCustom))) break;
        UINT w = 0, h = 0;
        conv->GetSize(&w, &h);
        double s = min(1.0, maxPixels / (double)max(w, h));
        UINT tw = max(1u, (UINT)(w * s)), th = max(1u, (UINT)(h * s));

        IWICBitmapSource* src = nullptr;
        if (tw != w || th != h) {
            if (FAILED(fac->CreateBitmapScaler(&scaler))) break;
            if (FAILED(scaler->Initialize(conv, tw, th, WICBitmapInterpolationModeFant))) break;
            src = scaler;
        } else {
            src = conv;
        }
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = (LONG)tw;
        bi.bmiHeader.biHeight = -(LONG)th;   // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC dc = GetDC(nullptr);
        hbmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, dc);
        if (!hbmp) break;
        if (FAILED(src->CopyPixels(nullptr, tw * 4, tw * th * 4, (BYTE*)bits))) break;

        delete peg.bitmap;
        peg.bitmap = Bitmap::FromHBITMAP(hbmp, nullptr);
        DeleteObject(hbmp); hbmp = nullptr;
        peg.w = (int)tw; peg.h = (int)th;
        ok = peg.bitmap && peg.bitmap->GetLastStatus() == Ok;
    } while (0);
    if (dec) dec->Release();
    if (frame) frame->Release();
    if (conv) conv->Release();
    if (scaler) scaler->Release();
    if (hbmp) DeleteObject(hbmp);
    fac->Release();
    return ok;
}

void freePeg(Peg& peg) { delete peg.bitmap; peg.bitmap = nullptr; }

// ---------------------------------------------------------------- layout ---
// Port of Layout.x(index:count:width:visible:scroll:) exactly.
static double layoutX(int index, int count, double width, int visible, double scroll,
                      int scale, bool hasKeep) {
    double spacing = layoutSpacing(scale);
    if (hasKeep && count > visible) {
        double newest = width / 2 + (visible - 1) * spacing / 2;
        return newest - (count - 1 - index) * spacing + scroll;
    }
    double gaps = (double)max(count - 1, 0);
    double step = gaps > 0 ? min(spacing, max(0.0, width - 200) / gaps) : spacing;
    return width / 2 - gaps * step / 2 + index * step;
}

// PeggedView.photoSize(for:): photo fits inside the card keeping proportions.
static void photoSizeFor(int imgW, int imgH, int scale, double& pw, double& ph) {
    double maxW = layoutCardWidth(scale) - 14, maxH = layoutPhotoMaxHeight(scale);
    if (imgW <= 0 || imgH <= 0) { pw = maxW; ph = maxH; return; }
    double s = min(maxW / imgW, maxH / imgH);
    pw = imgW * s; ph = imgH * s;
}

void layoutRope(std::vector<Peg>& pegs, double panelW, int scale,
                int visible, double scroll, bool hasKeep) {
    static std::mt19937 rng((unsigned)GetTickCount64());
    std::uniform_real_distribution<double> tilt(-2.5, 2.5);   // Pegged.tilt
    for (size_t i = 0; i < pegs.size(); i++) {
        Peg& p = pegs[i];
        double pw, ph; photoSizeFor(p.w, p.h, scale, pw, ph);
        p.frameW = pw + kFrameInset * 2;                       // PeggedView.cardSize
        p.frameH = ph + kFrameInset * 2;
        double x = layoutX((int)i, (int)pegs.size(), panelW, visible, scroll, scale, hasKeep);
        double ropeY = layoutRopeY(x, panelW);
        // VStack(spacing: -12): pin is 26 tall, card overlaps it by 12, and the
        // whole view sits so the clip grips the rope at pinAbove = 9.5.
        // Card top = rope contact + (26 - 12) - pinAbove + … → simplifies to ropeY + 14.
        p.x = x - p.frameW / 2;
        p.y = ropeY + 14;
        if (!p.tiltDone) { p.rot = tilt(rng) * 3.14159265358979 / 180.0; p.tiltDone = true; }
    }
}

RectF pegFrameRect(const Peg& p) {
    // Whole hanging view: clip above the card (pin top is 14 above card top).
    return RectF((REAL)p.x, (REAL)(p.y - 14), (REAL)p.frameW, (REAL)(p.frameH + 14));
}
RectF pegCrossRect(const Peg& p) {
    // overlay(alignment: .topLeading) cross: 20x20 glass circle, padding 3.
    return RectF((REAL)(p.x + 3), (REAL)(p.y + 3), 20, 20);
}

// --------------------------------------------------------------- drawing ---
static void roundedRect(GraphicsPath& path, float x, float y, float w, float h, float r) {
    float d = r * 2;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}

// Rope: View — quadratic curve from (-20, ropeTop) to (width+20, ropeTop),
// control (width/2, ropeTop + 2*sag). Three strokes: soft shadow, grey core,
// white highlight; faded near both ends. alpha drives the reveal slide.
void drawRope(Graphics& gfx, double width, REAL alpha) {
    REAL top = (REAL)kRopeTop, ctrl = top + (REAL)(2 * layoutSag(width));
    PointF pts[3] = { PointF(-20, top), PointF((REAL)width + 20, top),
                      PointF((REAL)width / 2, ctrl) };
    BYTE a = (BYTE)(alpha * 255);
    {   // shadow: black .22, 1.4 wide, offset y +1.2 (blur approximated by alpha)
        Pen shadow(Color(a, 0, 0, 0), 2.6f);
        Matrix m; m.Translate(0, 1.2f);
        shadow.SetTransform(&m);
        gfx.DrawCurve(&shadow, pts, 3, 0.5f);
    }
    {   // core: white 0.55 gray, 1.2 wide
        Pen core(Color((BYTE)(a * 0.85f), 140, 140, 140), 1.2f);
        gfx.DrawCurve(&core, pts, 3, 0.5f);
    }
    {   // highlight: white .45, 0.4 wide, offset y -0.35
        Pen hi(Color((BYTE)(a * 0.45f), 255, 255, 255), 0.4f);
        Matrix m; m.Translate(0, -0.35f);
        hi.SetTransform(&m);
        gfx.DrawCurve(&hi, pts, 3, 0.5f);
    }
}

void drawPeg(Graphics& gfx, const Peg& peg, bool hovered, bool pressed, bool showCross) {
    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
    gfx.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    gfx.SetPixelOffsetMode(PixelOffsetModeHalf);

    REAL cx = (REAL)(peg.x + peg.frameW / 2);
    REAL cardTop = (REAL)peg.y;
    REAL angleDeg = (REAL)((peg.curRot + peg.rot) * 180.0 / 3.14159265358979);

    // rotationEffect/scaleEffect anchor .top → pivot at the top of the hanging
    // view, i.e. the clothespin's top, 14pt above the card.
    REAL pivotX = cx, pivotY = cardTop - 14;
    gfx.TranslateTransform(pivotX, pivotY);
    gfx.RotateTransform(angleDeg);
    REAL sc = pressed ? 0.95f : (hovered ? 1.035f : 1.0f);
    gfx.ScaleTransform(sc, sc);
    gfx.TranslateTransform(-pivotX, -pivotY);

    RectF frame((REAL)peg.x, cardTop, (REAL)peg.frameW, (REAL)peg.frameH);

    // Drop shadow (.black .18/.26, radius 10/14, y 5/8). No real blur in
    // GDI+, so paint two offset translucent rounds as a soft approximation.
    {
        GraphicsPath sh; roundedRect(sh, frame.X, frame.Y + (hovered ? 8 : 5),
                              frame.Width, frame.Height, (float)kFrameRadius);
        SolidBrush dark(Color(hovered ? 46 : 32, 0, 0, 0));
        gfx.FillPath(&dark, &sh);
        GraphicsPath sh2; roundedRect(sh2, frame.X - 3, frame.Y + (hovered ? 8 : 5) - 3,
                               frame.Width + 6, frame.Height + 6, (float)(kFrameRadius + 3));
        SolidBrush soft(Color(18, 0, 0, 0));
        gfx.FillPath(&soft, &sh2);
    }

    // Glass frame (ultraThinMaterial stand-in): translucent near-white fill…
    GraphicsPath framePath; roundedRect(framePath, frame.X, frame.Y, frame.Width, frame.Height, (float)kFrameRadius);
    SolidBrush glassBg(Color(hovered ? 225 : 205, 250, 250, 251));
    gfx.FillPath(&glassBg, &framePath);
    // …specular edge LinearGradient(white .55 → .12), lineWidth 0.75
    {
        LinearGradientBrush lin(RectF(0, frame.Y, 1, frame.Height),
                                Color((BYTE)(0.55 * 255), 255, 255, 255),
                                Color((BYTE)(0.12 * 255), 255, 255, 255),
                                LinearGradientModeVertical);
        Pen pen(&lin, 0.75f);
        gfx.DrawPath(&pen, &framePath);
    }
    // outer hairline black .10, 0.5
    { Pen pen(Color((BYTE)(0.10 * 255), 0, 0, 0), 0.5f); gfx.DrawPath(&pen, &framePath); }

    // The photo: inset Frame.inset, concentric corners radius-4.
    RectF photo(frame.X + (REAL)kFrameInset, frame.Y + (REAL)kFrameInset,
                frame.Width - 2 * (REAL)kFrameInset, frame.Height - 2 * (REAL)kFrameInset);
    if (peg.bitmap) {
        GraphicsPath clip; roundedRect(clip, photo.X, photo.Y, photo.Width, photo.Height,
                                (float)(kFrameRadius - kFrameInset));
        gfx.SetClip(&clip);
        ImageAttributes attr;
        gfx.DrawImage(peg.bitmap, photo, 0, 0, peg.w, peg.h, UnitPixel, &attr);
        gfx.ResetClip();
        Pen inner(Color((BYTE)(0.18 * 255), 255, 255, 255), 0.5f);  // strokeBorder .18/0.5
        gfx.DrawPath(&inner, &clip);
    }

    // Clothespin: 9×26 pill, brushed-metal horizontal gradient with the exact
    // stops of Clothespin.swift (0.70/0.93/0.82/0.62 white), slot, edge sheen.
    {
        RectF pin(cx - 4.5f, pivotY + 1, 9, 26);
        GraphicsPath pinPath; roundedRect(pinPath, pin.X, pin.Y, pin.Width, pin.Height, 3.5f);
        LinearGradientBrush metal(pin, Color(255, 179, 179, 179), Color(255, 158, 158, 158),
                                  LinearGradientModeHorizontal);
        REAL factors[] = { 1.f, 0.72f, 0.9f, 0.55f };
        REAL positions[] = { 0.f, 0.35f, 0.65f, 1.f };
        metal.SetBlend(factors, positions, 4);   // banded sheen along the pin
        gfx.FillPath(&metal, &pinPath);
        LinearGradientBrush edge(pin, Color(230, 255, 255, 255), Color(46, 0, 0, 0),
                                 LinearGradientModeVertical);
        Pen pEdge(&edge, 0.6f);
        gfx.DrawPath(&pEdge, &pinPath);
        RectF slot(cx - 2.5f, pin.Y + 8.5f, 5, 1.4f);          // grip slot
        GraphicsPath slotPath; roundedRect(slotPath, slot.X, slot.Y, slot.Width, slot.Height, 0.7f);
        SolidBrush slotB(Color(82, 0, 0, 0));                  // black .32
        gfx.FillPath(&slotB, &slotPath);
    }

    // Corner cross: 20px glass circle at top-leading, padding 3, bold glyph.
    if (showCross) {
        RectF cross((REAL)peg.x + 3, cardTop + 3, 20, 20);
        SolidBrush cb(Color(200, 250, 250, 251));
        gfx.FillEllipse(&cb, cross);
        LinearGradientBrush ce(cross, Color(140, 255, 255, 255), Color(30, 255, 255, 255),
                               LinearGradientModeVertical);
        Pen cp(&ce, 0.75f);
        gfx.DrawEllipse(&cp, cross);
        Pen x(Color(235, 30, 30, 32), 1.6f);
        x.SetStartCap(LineCapRound); x.SetEndCap(LineCapRound);
        float m = 6.5f;
        gfx.DrawLine(&x, cross.X + m, cross.Y + m, cross.X + 20 - m, cross.Y + 20 - m);
        gfx.DrawLine(&x, cross.X + 20 - m, cross.Y + m, cross.X + m, cross.Y + 20 - m);
    }

    gfx.ResetTransform();
}
