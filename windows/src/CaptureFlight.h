// Tendedero for Windows — capture flight + fall animations.
// Port of CaptureFlight.swift: same duration (0.65 s), arc (30 pt),
// easeInOutCubic, chrome fade-in smoothstep(0.35→1), concentric radii
// (frame 0→16, photo inset 4·k), clip geometry (9×26, 12 over the card),
// and the fall (520 pt down, extra tilt (tilt·7+20)·e, 0.55 s cubic ease-in).
#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <functional>

void startFlight(Gdiplus::Bitmap* img, RECT fromScreen, RECT toScreen, double tiltDeg,
                 bool falling, std::function<void()> completion);
int  flightsInProgress();                    // non-falling flights (burst limit)
bool tickFlights();                          // true while anything is animating
void drawActiveFlights(Gdiplus::Graphics& gfx, double offX, double offY);
void reapFinishedFlights();                  // fire completions, delete finished
