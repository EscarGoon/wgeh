#pragma once
// Panel subdivision + a library of detailed tech "greeble" modules used to
// fill decorated blocks and structure surfaces.

#include "Engine.hpp"

#include <vector>

namespace inspo {

struct Box {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double w() const { return x1 - x0; }
    double h() const { return y1 - y0; }
    V c() const { return {(x0 + x1) / 2, (y0 + y1) / 2}; }
    Box inset(double d) const { return {x0 + d, y0 + d, x1 - d, y1 - d}; }
    bool ok() const { return w() > 1 && h() > 1; }
};

enum class Look { Cyber, Tech, Industrial, Neon, Circles };

struct GCtx {
    Draw& d;
    Rng& r;
    Palette const& pal;
    Anim& a;
    GroupAlloc& ga;
    int detail = 2;
    int motion = 2;
    double beat = 0.43;
    std::vector<int> wave;   // chase groups shared by a whole batch (optional)
    std::vector<int> seq;    // sequence groups (lit one after another on the beat)
    int fanBudget = 6;       // fans need their own groups; cap them
    Groups base;             // added to every object (e.g. a structure's root group)

    Style S(Role role, int z, Groups g = {}) const { return Style{pal.ch(role), z, base + g}; }
    bool anim(int l = 1) const { return motion >= l; }
    Groups waveG(int i) const {
        if (wave.empty()) return {};
        int n = (int)wave.size();
        return {wave[((i % n) + n) % n]};
    }
    Groups seqG(int i) const {
        if (seq.empty()) return {};
        int n = (int)seq.size();
        return {seq[((i % n) + n) % n]};
    }
};

// Splits a box into panels separated by `gap`, snapping cuts to `snap`.
std::vector<Box> subdivide(Rng& r, Box b, double minW, double maxW, double minH, double maxH, double gap, double snap = 15);

// Clips segment a-b to the box. Returns false if nothing is left.
bool clipSeg(V& a, V& b, Box const& bx);

// Base plate of a panel: fill, bevels, outline, chamfers, screws.
void panelPlate(GCtx& g, Box const& b, Look look, int z);
// Picks and draws a module that fits the box.
void placeModule(GCtx& g, Box const& b, Look look, int z);

// Individual modules (all draw inside `b`)
void mLightGrid(GCtx& g, Box b, int z);
void mVents(GCtx& g, Box b, int z);
void mPort(GCtx& g, Box b, int z);
void mHazard(GCtx& g, Box b, int z);
void mPowerCell(GCtx& g, Box b, int z);
void mChip(GCtx& g, Box b, int z);
void mFan(GCtx& g, Box b, int z);
void mScreen(GCtx& g, Box b, int z);
void mHexPatch(GCtx& g, Box b, int z);
void mPipes(GCtx& g, Box b, int z);
void mChevrons(GCtx& g, Box b, int z);
void mEqualizer(GCtx& g, Box b, int z);
void mCore(GCtx& g, Box b, int z);
void mTruss(GCtx& g, Box b, int z);
void mDataBars(GCtx& g, Box b, int z);
void mLightStrip(GCtx& g, Box b, int z);
void mDiamonds(GCtx& g, Box b, int z);
void mSevenSeg(GCtx& g, Box b, int z);
void mGauge(GCtx& g, Box b, int z);
void mRadar(GCtx& g, Box b, int z);
void mGrille(GCtx& g, Box b, int z);
void mFins(GCtx& g, Box b, int z);
void mWarning(GCtx& g, Box b, int z);
void mBarcode(GCtx& g, Box b, int z);
void mCapacitors(GCtx& g, Box b, int z);

} // namespace inspo
