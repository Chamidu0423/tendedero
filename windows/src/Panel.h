// Tendedero for Windows — the line panel: a topmost, click-through-except-on-
// photos strip at the top of the screen (port of LinePanel.swift + the reveal
// state machine of AppDelegate.swift + LineView rendering).
#pragma once
#include "Line.h"
#include <vector>

struct PanelState {
    std::vector<Peg> pegs;
    double scroll = 0;             // Layout: scroll towards older photos
    bool   revealed = false;       // slid down into view?
    double revealAnim = 0;         // 0..1 slide progress (spring / easeIn)
    int    hoverPeg = -1;
    int    pressPeg = -1;
    long long inBandSince = 0;     // cursor dwell timing, like AppDelegate
    long long outBandSince = 0;
    bool   pinned = false;         // menu open / dialog: line stays down
    long long nextGustMs = 0;      // scheduleGust()
    std::vector<long long> fallRemoveAt;  // drop(id): remove card 0.6 s after it falls
};

extern PanelState S;

void panelCreate(HINSTANCE hi);
void panelDestroy();
void panelRelayout();                       // size/position to the work area
void hangPath(const std::wstring& path, bool flyingFromCaptureArea, RECT captureRect);
void panelDrop(int i, bool quietly);        // take one photo off (fall animation)
void panelDiscard(int i);                   // cross: trash inbox files, else hide
void panelCopy(int i);
void panelOpen(int i);
void panelTrash(int i);
void panelMarkup(int i);
void panelQuickLook(int i);
void panelRevealInExplorer(int i);
void panelSaveToDesktop(int i);
void panelTakeDownSide(int i, bool toTheLeft);
void panelClearAll();
void panelScroll(double delta);
int  panelLiveCount();
bool panelIsInInbox(int i);
std::vector<int> panelNeighbours(int i, bool toTheLeft);
void panelReloadThumbnails();
void panelPrune();                          // vanished files fall off by themselves
void panelShowHint(bool on);                // empty-line hint bubble
bool panelPointInPegs(POINT screenPt);      // for the ignoresMouseEvents flip
void panelTick();                           // 30 Hz: reveal machine, gusts, repaint
HWND panelHwnd();                           // for tray positioning & click-through
void panelRefresh();                        // schedule a redraw
int  panelHitTest(POINT screenPt, bool* onCross); // -1 none; sets pressed/hover ids
RECT panelPegScreenRect(int i);             // card rect in screen coords (flights)
void panelSetCursorShape(int hit);          // arrow vs pointing hand like .onHover
extern RECT g_captureArea;                  // last known capture flash region
