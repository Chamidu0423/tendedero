// Tendedero for Windows — CaptureFlight.cpp. See CaptureFlight.h.
#include "Common.h"
#include "CaptureFlight.h"
#include <gdiplus.h>
#include <cmath>
#include <vector>

using namespace Gdiplus;

struct Flight {
    Bitmap* image = nullptr;                  // owned by caller (the peg's thumb)
    double fromX, fromY, fromW, fromH;        // captured area, screen px y-down
    double toX, toY, toW, toH;                // card frame on the line
    double tiltDeg = 0;
    bool falling = false;
    long long startMs = 0;
    double durationS = 0.65;                  // CaptureFlight.duration / fall 0.55
    std::function<void()> completion;
};

static std::vector<Flight*> g_flights;

static double easeInOutCubic(double x) {      // verbatim port of update()'s helper
    return x < 0.5 ? 4 * x * x * x : 1 - pow(-2 * x + 2, 3) / 2;
}
static double smoothstep(double x, double a, double b) {
    double t = max(0.0, min(1.0, (x - a) / (b - a)));
    return t * t * (3 - 2 * t);
}
static void rrect(GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2;
    if (d <= 0.01f) { RectF rr(x, y, w, h); p.AddRectangle(rr); return; }
    p.AddArc(x, y, d, d, 180, 90); p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90); p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

static void paintFlight(Graphics& gfx, const Flight& f, double raw) {
    if (f.falling) {
        // updateFall: e = k³, drop 520 pt, angle = tilt + (tilt·7+20)·e, fade out.
        double e = raw * raw * raw;
        REAL alpha = (REAL)(1 - e);
        if (alpha <= 0) return;
        double cx = f.toX + f.toW / 2;
        double topY = f.toY + 520 * e;
        double angle = f.tiltDeg + (f.tiltDeg * 7 + 20) * e;
        ImageAttributes attr;
        ColorMatrix cm = { 1,0,0,0,0,  0,1,0,0,0,  0,0,1,0,0,
                           0,0,0,alpha,0,  0,0,0,0,1 };
        attr.SetColorMatrix(&cm);
        Matrix save; gfx.GetTransform(&save);
        gfx.TranslateTransform((REAL)cx, (REAL)topY);
        gfx.RotateTransform((REAL)-angle);     // CA sign vs SwiftUI, as in Swift
        RectF dst((REAL)(-f.toW / 2), 0, (REAL)f.toW, (REAL)f.toH);
        GraphicsPath path; rrect(path, dst.X, dst.Y, dst.Width, dst.Height, 16);
        gfx.SetClip(&path);
        if (f.image)
            gfx.DrawImage(f.image, dst, 0, 0, f.image->GetWidth(), f.image->GetHeight(),
                          UnitPixel, &attr);
        gfx.ResetClip();
        gfx.SetTransform(&save);
        return;
    }

    // update(_:): shrink along an arc while the glass chrome grows in.
    double k = easeInOutCubic(raw);
    double chrome = smoothstep(k, 0.35, 1);
    auto lerp = [k](double a, double b) { return a + (b - a) * k; };
    double w = lerp(f.fromW, f.toW), h = lerp(f.fromH, f.toH);
    double midX = lerp(f.fromX + f.fromW / 2, f.toX + f.toW / 2);
    double maxY = lerp(f.fromY + f.fromH, f.toY + f.toH) + sin(3.14159265358979 * k) * 30;
    double inset = 4 * k;
    double radius = lerp(0, 16);

    Matrix save; gfx.GetTransform(&save);
    gfx.TranslateTransform((REAL)midX, (REAL)(maxY - h));   // anchorPoint (0.5, 1)
    gfx.RotateTransform((REAL)(-f.tiltDeg * 3.14159265358979 / 180.0 * k));
    RectF bounds(0, 0, (REAL)w, (REAL)h);

    // container shadow: black .24 radius 10 offset (0,-5) → downward here,
    // faded with the chrome like the CALayer shadow under an animating group.
    {
        GraphicsPath shp; rrect(shp, bounds.X, bounds.Y + 5, bounds.Width, bounds.Height, (float)radius);
        SolidBrush sh(Color((BYTE)(chrome * 0.24 * 255), 0, 0, 0));
        gfx.FillPath(&sh, &shp);
    }
    // glass layer: white .97 at alpha .72 · chrome, cornerRadius = radius
    {
        GraphicsPath gp; rrect(gp, bounds.X, bounds.Y, bounds.Width, bounds.Height, (float)radius);
        SolidBrush gb(Color((BYTE)(chrome * 0.72 * 255), 247, 247, 247));
        gfx.FillPath(&gb, &gp);
    }
    // photo layer: inset 4k, concentric corners radius-4k, aspect-fill clip
    if (f.image) {
        RectF ph((REAL)inset, (REAL)inset, (REAL)(w - 2 * inset), (REAL)(h - 2 * inset));
        GraphicsPath pp; rrect(pp, ph.X, ph.Y, ph.Width, ph.Height, (float)max(0.0, radius - inset));
        gfx.SetClip(&pp);
        gfx.DrawImage(f.image, ph, 0, 0, f.image->GetWidth(), f.image->GetHeight(),
                      UnitPixel, nullptr);
        gfx.ResetClip();
    }
    // edge layer: vertical white .9→.25 gradient stroke 1.5, opacity chrome
    {
        GraphicsPath ep; rrect(ep, bounds.X + 0.75f, bounds.Y + 0.75f,
                        bounds.Width - 1.5f, bounds.Height - 1.5f,
                        (float)max(0.0, radius - 0.75));
        LinearGradientBrush lin(RectF(0, 0, 1, (REAL)h),
                                Color((BYTE)(chrome * 230), 255, 255, 255),
                                Color((BYTE)(chrome * 64), 255, 255, 255),
                                LinearGradientModeVertical);
        Pen pen(&lin, 1.5f);
        gfx.DrawPath(&pen, &ep);
    }
    // clip layer: grips the top edge — 26 tall, 12 over the card, exact stops
    {
        RectF pin((REAL)(w / 2 - 4.5), (REAL)(h - 12), 9, 26);
        GraphicsPath pinPath; rrect(pinPath, pin.X, pin.Y, pin.Width, pin.Height, 3.5f);
        LinearGradientBrush metal(pin, Color(255, 179, 179, 179), Color(255, 158, 158, 158),
                                  LinearGradientModeHorizontal);
        REAL factors[] = { 1.f, 0.72f, 0.9f, 0.55f };
        REAL pos[] = { 0.f, 0.35f, 0.65f, 1.f };
        metal.SetBlend(factors, pos, 4);
        gfx.FillPath(&metal, &pinPath);
        Pen pEdge(Color((BYTE)(chrome * 179), 255, 255, 255), 0.6f);   // border .7/0.6
        gfx.DrawPath(&pEdge, &pinPath);
    }
    gfx.SetTransform(&save);
}

void startFlight(Bitmap* img, RECT fromScreen, RECT toScreen, double tiltDeg,
                 bool falling, std::function<void()> completion) {
    Flight* f = new Flight();
    f->image = img;
    f->fromX = fromScreen.left;  f->fromY = fromScreen.top;
    f->fromW = fromScreen.right - fromScreen.left;
    f->fromH = fromScreen.bottom - fromScreen.top;
    f->toX = toScreen.left;  f->toY = toScreen.top;
    f->toW = toScreen.right - toScreen.left;
    f->toH = toScreen.bottom - toScreen.top;
    f->tiltDeg = tiltDeg;
    f->falling = falling;
    f->durationS = falling ? 0.55 : 0.65;
    f->startMs = nowMs();
    f->completion = std::move(completion);
    g_flights.push_back(f);
}

int flightsInProgress() {
    int n = 0;
    for (auto* f : g_flights) if (!f->falling) n++;
    return n;
}

bool tickFlights() {
    for (auto* f : g_flights)
        if ((nowMs() - f->startMs) < f->durationS * 1000.0) return true;
    return false;
}

void drawActiveFlights(Graphics& gfx, double offX, double offY) {
    if (g_flights.empty()) return;
    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
    gfx.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    Matrix save; gfx.GetTransform(&save);
    gfx.TranslateTransform((REAL)-offX, (REAL)-offY);   // screen → panel coords
    for (auto* f : g_flights) {
        double raw = min(1.0, (nowMs() - f->startMs) / (f->durationS * 1000.0));
        paintFlight(gfx, *f, raw);
    }
    gfx.SetTransform(&save);
}

void reapFinishedFlights() {
    for (auto it = g_flights.begin(); it != g_flights.end();) {
        Flight* f = *it;
        if ((nowMs() - f->startMs) >= f->durationS * 1000.0) {
            auto cb = f->completion;
            delete f;
            it = g_flights.erase(it);
            if (cb) cb();          // completion(): land the card / remove flight
        } else ++it;
    }
}
