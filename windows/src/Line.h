#pragma once
#include <string>
#include <vector>
#include <gdiplus.h>

// One hung photo. Mirrors the pegged item model in Line.swift / PeggedView.swift.
struct Peg {
    std::wstring path;      // source file (empty for pure-clipboard items)
    Gdiplus::Bitmap* bitmap = nullptr;
    int          w = 0, h = 0;        // image size px (96 dpi space)
    double       frameW = 0, frameH = 0;
    double       x = 0, y = 0;        // top-left in panel coords
    double       rot = 0;             // resting tilt (radians)
    bool         tiltDone = false;    // tilt is rolled once, like Pegged.tilt
    double       curRot = 0;          // animated sway angle
    double       springV = 0;         // angular velocity for spring integration
    long long    addedMs = 0;
    float        flightFromX = 0, flightFromY = 0;  // capture-flight start point
    bool         flying = false;      // mid capture-flight animation
    long long    flightStart = 0;
};

// WIC decode of PNG/JPEG/GIF/BMP into a GDI+ Bitmap, thumbnailed down to
// maxPixels on the long edge (never up), like makeThumbnail() on macOS.
bool loadPegImage(Peg& peg, const std::wstring& path, double maxPixels);
void freePeg(Peg& peg);

// Rope geometry — port of Layout.x(...) + PeggedView sizing: sagging
// parabola, centred cards, random resting tilt, scroll & keep-on-line modes.
void layoutRope(std::vector<Peg>& pegs, double panelW, int scale,
                int visible, double scroll, bool hasKeep);

// The rope itself — port of Rope: View (shadow/core/highlight quadratic
// strokes extended 20pt past each edge, faded at both ends).
void drawRope(Gdiplus::Graphics& gfx, double width, Gdiplus::REAL alpha);

// Draw one pegged photo: glass frame, clothespin, photo, hover glow, close cross.
// Port of PeggedView.swift drawing. Coordinates: panel space, y down.
void drawPeg(Gdiplus::Graphics& gfx, const Peg& peg, bool hovered, bool pressed, bool showCross);

// Hit rectangles (panel coords), used for mouse routing like the SwiftUI
// PreferenceKey hit-rect system on macOS.
Gdiplus::RectF pegFrameRect(const Peg& peg);          // whole card incl. pin
Gdiplus::RectF pegCrossRect(const Peg& peg);          // corner-cross zone
