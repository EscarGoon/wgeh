#pragma once
#include "Generator.hpp"

namespace inspo {

struct Ctx {
    GenSettings const& s;
    Rng& rng;
    Writer& w;
    Draw& d;
    Anim& a;
    GroupAlloc& ga;
    Palette const& pal;

    int ch(Role r) const { return pal.ch(r); }
    Style S(Role r, int z, Groups const& g) const { return Style{pal.ch(r), z, g}; }
    bool anim(int level = 1) const { return s.motion >= level; }
    double strip() const { return s.detail >= 3 ? 4.0 : s.detail == 2 ? 6.0 : 9.0; }

    // A tiny object used as a rotation center; it also belongs to `g` so it
    // follows the structure when the structure moves.
    int center(V p, Groups const& g) {
        int c = ga.get();
        d.rect(p.x, p.y, 1.2, 1.2, 0, Style{pal.ch(FILL_DARK), 1, g + Groups{c}});
        return c;
    }
};

// Structure generators. `root` groups are applied to every object drawn.
void genCitadel(Ctx& c, double cx, double baseY, double H, Groups const& root);
void genCrystals(Ctx& c, double cx, double baseY, double H, Groups const& root);
void genIsland(Ctx& c, double cx, double topY, double W, Groups const& root);
void genMechCore(Ctx& c, double cx, double cy, double R, Groups const& root);
void genPortal(Ctx& c, double cx, double baseY, double H, Groups const& root);
void genOrbital(Ctx& c, double cx, double cy, double R, Groups const& root);
void genTree(Ctx& c, double cx, double baseY, double H, Groups const& root);
void genColonnade(Ctx& c, double cx, double baseY, double W, Groups const& root);

void genReactor(Ctx& c, double cx, double cy, double R, Groups const& root);
void genCyberTower(Ctx& c, double cx, double baseY, double H, Groups const& root);
void genStation(Ctx& c, double cx, double cy, double W, Groups const& root);
void genBlade(Ctx& c, double cx, double baseY, double H, Groups const& root);

void genBackground(Ctx& c, std::string const& style, double x0, double x1, double baseY, double trigX);

} // namespace inspo
