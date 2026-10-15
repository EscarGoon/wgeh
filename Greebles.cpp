#include "Greebles.hpp"

#include <algorithm>
#include <functional>

namespace inspo {

namespace {
V dirOf(double a) { return {std::cos(a), std::sin(a)}; }

Pts boxPts(Box const& b) { return {{b.x0, b.y0}, {b.x1, b.y0}, {b.x1, b.y1}, {b.x0, b.y1}}; }
} // namespace

// ------------------------------------------------------------------ helpers

std::vector<Box> subdivide(Rng& r, Box b, double minW, double maxW, double minH, double maxH, double gap, double snap) {
    std::vector<Box> out;
    std::function<void(Box, int)> rec = [&](Box bx, int depth) {
        bool canX = bx.w() >= 2 * minW + gap, canY = bx.h() >= 2 * minH + gap;
        bool mustX = bx.w() > maxW, mustY = bx.h() > maxH;
        bool split = (mustX && canX) || (mustY && canY) || ((canX || canY) && depth < 6 && r.chance(0.25));
        if (!split || depth > 12) { out.push_back(bx); return; }
        bool alongX;
        if (canX && canY) alongX = (mustX && !mustY) ? true : (mustY && !mustX) ? false : bx.w() / maxW >= bx.h() / maxH;
        else alongX = canX;
        double len = alongX ? bx.w() : bx.h(), mn = alongX ? minW : minH;
        double lo = mn, hi = len - mn - gap;
        double cut = r.f(lo, hi);
        double base = alongX ? bx.x0 : bx.y0;
        double snapped = std::round((base + cut) / snap) * snap - base;
        if (snapped >= lo && snapped <= hi) cut = snapped;
        if (alongX) {
            rec({bx.x0, bx.y0, bx.x0 + cut, bx.y1}, depth + 1);
            rec({bx.x0 + cut + gap, bx.y0, bx.x1, bx.y1}, depth + 1);
        } else {
            rec({bx.x0, bx.y0, bx.x1, bx.y0 + cut}, depth + 1);
            rec({bx.x0, bx.y0 + cut + gap, bx.x1, bx.y1}, depth + 1);
        }
    };
    rec(b, 0);
    return out;
}

bool clipSeg(V& a, V& b, Box const& bx) {
    double t0 = 0, t1 = 1, dx = b.x - a.x, dy = b.y - a.y;
    double p[4] = {-dx, dx, -dy, dy}, q[4] = {a.x - bx.x0, bx.x1 - a.x, a.y - bx.y0, bx.y1 - a.y};
    for (int i = 0; i < 4; i++) {
        if (std::fabs(p[i]) < 1e-9) { if (q[i] < 0) return false; continue; }
        double t = q[i] / p[i];
        if (p[i] < 0) t0 = std::max(t0, t); else t1 = std::min(t1, t);
        if (t0 > t1) return false;
    }
    V na{a.x + dx * t0, a.y + dy * t0}, nb{a.x + dx * t1, a.y + dy * t1};
    a = na; b = nb;
    return true;
}

static void hatch(GCtx& g, Box b, double spacing, double thick, Role role, int z, Groups const& gr = {}) {
    double span = b.w() + b.h();
    for (double k = -b.h(); k < b.w() + spacing; k += spacing) {
        V a{b.x0 + k, b.y0}, e{b.x0 + k + span, b.y0 + span};
        if (clipSeg(a, e, b)) g.d.line(a, e, thick, g.S(role, z, gr));
    }
}

// ------------------------------------------------------------------- plates

void panelPlate(GCtx& g, Box const& b, Look look, int z) {
    auto& d = g.d;
    if (!b.ok()) return;
    V c = b.c();
    Role fillRole = look == Look::Tech || look == Look::Industrial ? FILL_MID : look == Look::Neon ? NEAR_SIL : FILL_DARK;
    if (look == Look::Cyber && g.r.chance(0.3)) fillRole = NEAR_SIL;
    d.rect(c.x, c.y, b.w(), b.h(), 0, g.S(fillRole, z));
    double bv = look == Look::Tech || look == Look::Industrial ? 3 : 2;
    if (look != Look::Neon && look != Look::Circles) {
        Role lit = look == Look::Cyber ? FILL_MID : FILL_LIGHT;
        d.rect(c.x, b.y1 - bv / 2, b.w(), bv, 0, g.S(lit, z + 1));
        d.rect(b.x0 + bv / 2, c.y, bv, b.h(), 0, g.S(lit, z + 1));
        d.rect(c.x, b.y0 + bv / 2, b.w(), bv, 0, g.S(NEAR_SIL, z + 1));
        d.rect(b.x1 - bv / 2, c.y, bv, b.h(), 0, g.S(NEAR_SIL, z + 1));
    }
    if (look == Look::Neon) d.polyline(boxPts(b.inset(0.6)), 1.0, g.S(ENERGY, z + 1), true);
    if (look == Look::Circles) d.polyline(boxPts(b.inset(0.6)), 1.0, g.S(ACCENT, z + 1), true);
    if (look == Look::Cyber && b.w() > 14 && b.h() > 14) {
        double ch = std::min(6.0, std::min(b.w(), b.h()) * 0.2);
        d.line({b.x0 + 1, b.y1 - ch}, {b.x0 + ch, b.y1 - 1}, 1.2, g.S(EDGE, z + 2));
        d.line({b.x1 - ch, b.y0 + 1}, {b.x1 - 1, b.y0 + ch}, 1.2, g.S(EDGE, z + 2));
        if (b.w() > 40 && b.h() > 24) {
            d.bracket({b.x0 + 3, b.y0 + 3}, 6, 1.4, 1, 1, g.S(ACCENT, z + 2));
            d.bracket({b.x1 - 3, b.y1 - 3}, 6, 1.4, -1, -1, g.S(ACCENT, z + 2));
        }
    }
    // screws / rivets
    if (g.detail >= 2 && b.w() > 22 && b.h() > 18 && look != Look::Neon && look != Look::Circles) {
        double s = 2.2, in = 4.5;
        Role rr = look == Look::Cyber ? DETAIL : FILL_LIGHT;
        for (V p : {V{b.x0 + in, b.y0 + in}, V{b.x1 - in, b.y0 + in}, V{b.x0 + in, b.y1 - in}, V{b.x1 - in, b.y1 - in}})
            d.rect(p.x, p.y, s, s, 45, g.S(rr, z + 2));
        if (look == Look::Industrial && g.detail >= 3)
            for (double x = b.x0 + 12; x < b.x1 - 10; x += 9) {
                d.rect(x, b.y1 - in, s, s, 0, g.S(FILL_LIGHT, z + 2));
                d.rect(x, b.y0 + in, s, s, 0, g.S(FILL_LIGHT, z + 2));
            }
    }
}

// ------------------------------------------------------------------ modules

void mLightGrid(GCtx& g, Box b, int z) {
    b = b.inset(5);
    if (!b.ok()) return;
    double st = 6;
    int cols = std::max(1, (int)(b.w() / st)), rows = std::max(1, (int)(b.h() / st));
    double ox = b.x0 + (b.w() - (cols - 1) * st) / 2, oy = b.y0 + (b.h() - (rows - 1) * st) / 2;
    for (int j = 0; j < rows; j++)
        for (int i = 0; i < cols; i++) {
            bool lit = g.r.chance(0.6);
            Role role = lit ? (g.r.chance(0.8) ? ENERGY : ACCENT) : NEAR_SIL;
            g.d.rect(ox + i * st, oy + j * st, 3, 3, 0, g.S(role, z, lit ? g.seqG(g.r.i(0, 7)) : Groups{}));
        }
}

void mVents(GCtx& g, Box b, int z) {
    b = b.inset(4);
    if (!b.ok()) return;
    V c = b.c();
    g.d.rect(c.x, c.y, b.w(), b.h(), 0, g.S(NEAR_SIL, z));
    g.d.rect(c.x, c.y, b.w() * 0.85, b.h() * 0.7, 0, g.S(GLOW_SOFT, z + 1));
    bool horiz = b.w() >= b.h();
    double st = 5;
    if (horiz)
        for (double y = b.y0 + 3; y < b.y1 - 2; y += st) g.d.rect(c.x, y, b.w() - 4, 2, 0, g.S(FILL_LIGHT, z + 2));
    else
        for (double x = b.x0 + 3; x < b.x1 - 2; x += st) g.d.rect(x, c.y, 2, b.h() - 4, 0, g.S(FILL_LIGHT, z + 2));
    g.d.polyline(boxPts(b), 1.1, g.S(DETAIL, z + 3), true);
}

void mPort(GCtx& g, Box b, int z) {
    double r = std::min(b.w(), b.h()) * 0.38;
    if (r < 6) return;
    V c = b.c();
    auto& d = g.d;
    d.circle(c, r, g.S(NEAR_SIL, z), 3);
    d.ring(c, r, 1.6, g.S(EDGE, z + 1), 20);
    d.ring(c, r * 0.72, r * 0.16, g.S(FILL_MID, z + 1), 18);
    d.ring(c, r * 0.5, 1.1, g.S(ACCENT, z + 2), 16);
    d.circle(c, r * 0.28, g.S(ENERGY, z + 2, g.seqG(g.r.i(0, 7))), 2.5);
    if (g.detail >= 2) d.glow(c, r * 1.1, g.S(GLOW_SOFT, z + 1), 1);
    for (int k = 0; k < 4; k++) d.rect(c.x + std::cos(k * PI / 2 + PI / 4) * r * 0.86, c.y + std::sin(k * PI / 2 + PI / 4) * r * 0.86, 2.2, 2.2, 45, g.S(DETAIL, z + 3));
}

void mHazard(GCtx& g, Box b, int z) {
    b = b.inset(4);
    if (!b.ok()) return;
    V c = b.c();
    g.d.rect(c.x, c.y, b.w(), b.h(), 0, g.S(NEAR_SIL, z));
    hatch(g, b, 9, 4, ACCENT, z + 1);
    g.d.polyline(boxPts(b), 1.4, g.S(EDGE, z + 2), true);
}

void mPowerCell(GCtx& g, Box b, int z) {
    b = b.inset(4);
    if (!b.ok()) return;
    bool vert = b.h() > b.w();
    auto& d = g.d;
    d.polyline(boxPts(b), 1.4, g.S(EDGE, z + 1), true);
    V c = b.c();
    if (vert) d.rect(c.x, b.y1 + 2, b.w() * 0.4, 2.5, 0, g.S(EDGE, z + 1));
    else d.rect(b.x1 + 2, c.y, 2.5, b.h() * 0.4, 0, g.S(EDGE, z + 1));
    int n = std::max(3, (int)((vert ? b.h() : b.w()) / 7));
    double len = vert ? b.h() : b.w();
    double seg = (len - 4) / n;
    int full = g.r.i(n / 2, n);
    for (int k = 0; k < n; k++) {
        double t = 2 + seg * (k + 0.5);
        Role role = k < full ? (k >= full - 1 ? ACCENT : ENERGY) : NEAR_SIL;
        Groups gr = k < full ? g.seqG(k) : Groups{};
        if (vert) d.rect(c.x, b.y0 + t, b.w() - 4, seg - 1.6, 0, g.S(role, z + 2, gr));
        else d.rect(b.x0 + t, c.y, seg - 1.6, b.h() - 4, 0, g.S(role, z + 2, gr));
    }
}

void mChip(GCtx& g, Box b, int z) {
    b = b.inset(6);
    if (!b.ok() || b.w() < 10 || b.h() < 8) return;
    auto& d = g.d;
    V c = b.c();
    d.rect(c.x, c.y, b.w(), b.h(), 0, g.S(NEAR_SIL, z));
    d.polyline(boxPts(b), 1.1, g.S(DETAIL, z + 1), true);
    for (double x = b.x0 + 3; x < b.x1 - 1; x += 4) {
        d.rect(x, b.y1 + 2, 1.4, 4, 0, g.S(DETAIL, z));
        d.rect(x, b.y0 - 2, 1.4, 4, 0, g.S(DETAIL, z));
    }
    d.rect(c.x, c.y + b.h() * 0.18, b.w() * 0.6, 1.6, 0, g.S(EDGE, z + 1));
    d.rect(c.x - b.w() * 0.1, c.y - b.h() * 0.12, b.w() * 0.4, 1.2, 0, g.S(DETAIL, z + 1));
    d.rect(b.x1 - 4, b.y0 + 4, 2.6, 2.6, 0, g.S(ACCENT, z + 2, g.seqG(g.r.i(0, 7))));
}

void mFan(GCtx& g, Box b, int z) {
    double r = std::min(b.w(), b.h()) * 0.4;
    if (r < 8) return;
    auto& d = g.d;
    V c = b.c();
    d.circle(c, r, g.S(NEAR_SIL, z), 3);
    d.glow(c, r * 1.4, g.S(GLOW_SOFT, z + 1), 1);
    d.ring(c, r, 1.6, g.S(EDGE, z + 3), 22);
    d.ring(c, r * 1.12, 1.0, g.S(DETAIL, z + 3), 22);
    if (g.fanBudget <= 0) {
        for (int k = 0; k < 4; k++) d.rect(c.x + std::cos(k * PI / 2) * r * 0.45, c.y + std::sin(k * PI / 2) * r * 0.45, r * 0.75, r * 0.22, -k * 90 - 20, g.S(FILL_MID, z + 2));
        return;
    }
    g.fanBudget--;
    int fg = g.ga.get(), cg = g.ga.get();
    d.rect(c.x, c.y, 1.2, 1.2, 0, g.S(NEAR_SIL, z + 2, {cg}));
    int blades = g.r.i(3, 5);
    for (int k = 0; k < blades; k++) {
        double a = 2 * PI * k / blades;
        V p = c + dirOf(a) * (r * 0.45);
        d.rect(p.x, p.y, r * 0.78, r * 0.24, -a * 180 / PI - 20, g.S(FILL_MID, z + 2, {fg}));
    }
    d.circle(c, r * 0.18, g.S(ACCENT, z + 4), 2.5);
    if (g.anim(1)) g.a.spin(fg, cg, g.r.f(0.8, 1.6), g.r.chance(0.5));
}

void mScreen(GCtx& g, Box b, int z) {
    b = b.inset(4);
    if (!b.ok() || b.w() < 16 || b.h() < 12) return;
    auto& d = g.d;
    V c = b.c();
    d.rect(c.x, c.y, b.w(), b.h(), 0, g.S(NEAR_SIL, z));
    d.rect(c.x, c.y, b.w() - 2, b.h() - 2, 0, g.S(GLOW_SOFT, z + 1));
    for (double y = b.y0 + 2; y < b.y1 - 1; y += 3) d.rect(c.x, y, b.w() - 3, 0.8, 0, g.S(DETAIL, z + 2));
    int lines = std::max(1, (int)((b.h() - 6) / 5));
    for (int k = 0; k < lines && k < 6; k++) {
        double y = b.y1 - 5 - k * 5;
        double l = g.r.f(0.25, 0.85) * (b.w() - 8);
        d.rect(b.x0 + 4 + l / 2, y, l, 2, 0, g.S(k == 0 ? EDGE : ENERGY, z + 3, g.seqG(k)));
    }
    d.polyline(boxPts(b), 1.4, g.S(EDGE, z + 4), true);
    // moving scan bar
    if (g.anim(2)) {
        int sg = g.ga.get();
        d.rect(c.x, b.y1 - 2, b.w() - 3, 1.6, 0, g.S(ENERGY, z + 4, {sg}));
        double travel = b.h() - 4;
        g.a.loop({{{Anim::move(sg, 0, -travel, g.beat * 4)}, g.beat * 4}, {{Anim::move(sg, 0, travel, 0)}, 0.05}}, g.r.f(0, g.beat * 4));
    }
}

void mHexPatch(GCtx& g, Box b, int z) {
    b = b.inset(5);
    if (!b.ok()) return;
    double r = 4.5, dx = r * 1.732, dy = r * 1.5;
    int row = 0;
    for (double y = b.y0 + r; y <= b.y1 - r; y += dy, row++)
        for (double x = b.x0 + r + (row % 2) * dx / 2; x <= b.x1 - r; x += dx) {
            bool lit = g.r.chance(0.25);
            g.d.polyOutline({x, y}, r, 6, 30, 1.0, g.S(lit ? ENERGY : DETAIL, z, lit ? g.seqG(g.r.i(0, 7)) : Groups{}));
            if (lit && g.detail >= 3) g.d.diamond({x, y}, 2, g.S(EDGE, z + 1));
        }
}

void mPipes(GCtx& g, Box b, int z) {
    b = b.inset(3);
    if (!b.ok()) return;
    auto& d = g.d;
    bool horiz = b.w() >= b.h();
    int n = std::max(1, std::min(3, (int)((horiz ? b.h() : b.w()) / 7)));
    for (int k = 0; k < n; k++) {
        double t = (k + 0.5) / n;
        if (horiz) {
            double y = b.y0 + b.h() * t;
            d.rect(b.c().x, y, b.w(), 4, 0, g.S(FILL_LIGHT, z));
            d.rect(b.c().x, y + 1.2, b.w(), 1, 0, g.S(EDGE, z + 1));
            for (double x = b.x0 + 8; x < b.x1 - 4; x += g.r.f(14, 24)) d.rect(x, y, 3, 6, 0, g.S(DETAIL, z + 2));
        } else {
            double x = b.x0 + b.w() * t;
            d.rect(x, b.c().y, 4, b.h(), 0, g.S(FILL_LIGHT, z));
            d.rect(x - 1.2, b.c().y, 1, b.h(), 0, g.S(EDGE, z + 1));
            for (double y = b.y0 + 8; y < b.y1 - 4; y += g.r.f(14, 24)) d.rect(x, y, 6, 3, 0, g.S(DETAIL, z + 2));
        }
    }
    if (g.detail >= 2 && b.w() > 30 && b.h() > 12 && g.r.chance(0.5)) {
        V v{b.x0 + b.w() * g.r.f(0.3, 0.7), b.c().y};
        d.ring(v, 5, 1.6, g.S(ACCENT, z + 3), 10);
        d.line(v + V{-4, 0}, v + V{4, 0}, 1.2, g.S(ACCENT, z + 3));
        d.line(v + V{0, -4}, v + V{0, 4}, 1.2, g.S(ACCENT, z + 3));
    }
}

void mChevrons(GCtx& g, Box b, int z) {
    b = b.inset(4);
    if (!b.ok()) return;
    bool horiz = b.w() >= b.h();
    double s = std::min(horiz ? b.h() : b.w(), 14.0);
    double dirDeg = horiz ? (g.r.chance(0.5) ? 0 : 180) : (g.r.chance(0.5) ? 90 : 270);
    double len = horiz ? b.w() : b.h();
    int n = std::max(1, (int)(len / (s * 0.8)));
    for (int k = 0; k < n; k++) {
        double t = (k + 0.5) / n;
        V p = horiz ? V{b.x0 + b.w() * t, b.c().y} : V{b.c().x, b.y0 + b.h() * t};
        int idx = (dirDeg == 0 || dirDeg == 90) ? k : n - k;
        g.d.chevron(p, s, dirDeg, 1.8, g.S(ACCENT, z, g.waveG(idx)));
    }
}

void mEqualizer(GCtx& g, Box b, int z) {
    b = b.inset(4);
    if (!b.ok() || b.h() < 10) return;
    double bw = 3.5, gap = 2;
    int n = std::max(2, (int)(b.w() / (bw + gap)));
    double ox = b.x0 + (b.w() - n * (bw + gap) + gap) / 2 + bw / 2;
    for (int k = 0; k < n; k++) {
        double h = g.r.f(0.25, 1.0) * b.h();
        int steps = std::max(1, (int)(h / 4));
        for (int s = 0; s < steps; s++)
            g.d.rect(ox + k * (bw + gap), b.y0 + 2 + s * 4, bw, 2.8, 0, g.S(s == steps - 1 ? EDGE : ENERGY, z, g.seqG(s + k % 2)));
    }
}

void mCore(GCtx& g, Box b, int z) {
    double r = std::min(b.w(), b.h()) * 0.35;
    if (r < 6) return;
    auto& d = g.d;
    V c = b.c();
    d.glow(c, r * 2.2, g.S(GLOW, z, g.seqG(0)), 2);
    d.circle(c, r * 0.45, g.S(ENERGY, z + 1), 2.5);
    d.circle(c, r * 0.22, g.S(EDGE, z + 2), 2);
    d.ring(c, r, 1.4, g.S(EDGE, z + 2), 20);
    d.ring(c, r * 0.75, 1.0, g.S(ACCENT, z + 2), 16, 0, PI * 1.4);
    for (int k = 0; k < 4; k++) {
        V u = dirOf(k * PI / 2);
        d.line(c + u * (r * 1.05), c + u * (r * 1.35), 1.4, g.S(EDGE, z + 2));
    }
}

void mTruss(GCtx& g, Box b, int z) {
    b = b.inset(3);
    if (!b.ok()) return;
    auto& d = g.d;
    d.polyline(boxPts(b), 2, g.S(FILL_LIGHT, z), true);
    int n = std::max(1, (int)std::round(b.w() / std::max(12.0, b.h())));
    for (int k = 0; k < n; k++) {
        double x0 = b.x0 + b.w() * k / n, x1 = b.x0 + b.w() * (k + 1) / n;
        d.line({x0, b.y0}, {x1, b.y1}, 1.6, g.S(DETAIL, z));
        d.line({x0, b.y1}, {x1, b.y0}, 1.6, g.S(DETAIL, z));
        if (k) d.rect(x0, b.c().y, 1.6, b.h(), 0, g.S(FILL_LIGHT, z));
    }
}

void mDataBars(GCtx& g, Box b, int z) {
    b = b.inset(5);
    if (!b.ok()) return;
    int n = std::max(1, std::min(8, (int)(b.h() / 5)));
    for (int k = 0; k < n; k++) {
        double l = g.r.f(0.2, 1.0) * b.w();
        g.d.rect(b.x0 + l / 2, b.y1 - 2 - k * 5, l, 2.4, 0, g.S(k % 3 == 0 ? EDGE : k % 2 ? DETAIL : ENERGY, z, g.seqG(k)));
    }
}

void mLightStrip(GCtx& g, Box b, int z) {
    bool horiz = b.w() >= b.h();
    V c = b.c();
    if (horiz) g.d.dashes({b.x0 + 3, c.y}, {b.x1 - 3, c.y}, 8, 5, std::min(2.4, b.h() * 0.4), g.S(ENERGY, z), [&](int i) { return g.waveG(i); });
    else g.d.dashes({c.x, b.y0 + 3}, {c.x, b.y1 - 3}, 8, 5, std::min(2.4, b.w() * 0.4), g.S(ENERGY, z), [&](int i) { return g.waveG(i); });
}

void mDiamonds(GCtx& g, Box b, int z) {
    double s = std::min(b.w(), b.h()) * 0.42;
    if (s < 5) return;
    for (double x = b.x0 + s * 1.1; x <= b.x1 - s * 1.1 + 0.1; x += s * 2.3)
        for (int k = 0; k < 3; k++) {
            double rr = s * (1 - k * 0.3);
            if (rr < 3) break;
            g.d.polyOutline({x, b.c().y}, rr, 4, 0, 1.3, g.S(k % 2 ? ACCENT : EDGE, z, g.seqG(k)));
        }
}


// ------------------------------------------------------------ v6 modules

void mSevenSeg(GCtx& g, Box b, int z) {
    b = b.inset(5);
    if (!b.ok() || b.h() < 12 || b.w() < 10) return;
    auto& d = g.d;
    double dh = std::min(b.h(), 16.0), dw = dh * 0.55, sp = dw + 3;
    int n = std::max(1, std::min(5, (int)((b.w() + 3) / sp)));
    double ox = b.c().x - (n * sp - 3) / 2 + dw / 2, cy = b.c().y;
    // segment bit masks a..g for digits 0-9
    static int const mask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    d.rect(b.c().x, cy, n * sp + 4, dh + 6, 0, g.S(NEAR_SIL, z));
    double t = std::max(1.2, dh * 0.11), hl = dw - t, vl = dh / 2 - t;
    for (int k = 0; k < n; k++) {
        double x = ox + k * sp;
        int m = mask[g.r.i(0, 9)];
        V seg[7] = {{x, cy + dh / 2 - t / 2}, {x + dw / 2 - t / 2, cy + dh / 4}, {x + dw / 2 - t / 2, cy - dh / 4},
                    {x, cy - dh / 2 + t / 2}, {x - dw / 2 + t / 2, cy - dh / 4}, {x - dw / 2 + t / 2, cy + dh / 4}, {x, cy}};
        for (int sgi = 0; sgi < 7; sgi++) {
            bool on = (m >> sgi) & 1;
            bool horiz = sgi == 0 || sgi == 3 || sgi == 6;
            Style st = g.S(on ? ENERGY : FILL_DARK, z + 1, on ? g.seqG(k) : Groups{});
            if (horiz) d.rect(seg[sgi].x, seg[sgi].y, hl, t, 0, st);
            else d.rect(seg[sgi].x, seg[sgi].y, t, vl, 0, st);
        }
    }
    d.polyline({{b.c().x - n * sp / 2 - 2, cy - dh / 2 - 3}, {b.c().x + n * sp / 2 + 2, cy - dh / 2 - 3}, {b.c().x + n * sp / 2 + 2, cy + dh / 2 + 3}, {b.c().x - n * sp / 2 - 2, cy + dh / 2 + 3}}, 1.1, g.S(DETAIL, z + 2), true);
}

void mGauge(GCtx& g, Box b, int z) {
    double r = std::min(b.w(), b.h()) * 0.4;
    if (r < 8) return;
    auto& d = g.d;
    V c = b.c();
    d.circle(c, r, g.S(NEAR_SIL, z), 3);
    d.ring(c, r, 1.6, g.S(EDGE, z + 1), 20);
    for (int k = 0; k <= 8; k++) {
        double a = PI * (1.25 - 1.5 * k / 8);
        V u = dirOf(a);
        d.line(c + u * (r * 0.72), c + u * (r * (k % 4 == 0 ? 0.92 : 0.85)), 1.1, g.S(k >= 6 ? ACCENT : DETAIL, z + 1));
    }
    d.ring(c, r * 0.8, 1.4, g.S(ACCENT, z + 1), 10, -PI * 0.25, PI * 0.1);
    Groups ng;
    int cg = -1;
    if (g.fanBudget > 0 && g.anim(1)) { g.fanBudget--; ng = {g.ga.get()}; cg = g.ga.get(); d.rect(c.x, c.y, 1.2, 1.2, 0, g.S(NEAR_SIL, z + 2, {cg})); }
    d.line(c, c + dirOf(PI * 0.55) * (r * 0.78), 1.6, g.S(EDGE, z + 2, ng));
    d.circle(c, r * 0.12, g.S(ACCENT, z + 3), 2);
    if (cg >= 0) g.a.swing(ng[0], cg, g.r.f(25, 50), g.beat * 2, g.r.f(0, g.beat));
}

void mRadar(GCtx& g, Box b, int z) {
    double r = std::min(b.w(), b.h()) * 0.42;
    if (r < 9) return;
    auto& d = g.d;
    V c = b.c();
    d.circle(c, r, g.S(NEAR_SIL, z), 3);
    for (double k : {1.0, 0.66, 0.33}) d.ring(c, r * k, k == 1.0 ? 1.6 : 1.0, g.S(k == 1.0 ? EDGE : DETAIL, z + 1), 22);
    d.line(c - V{r, 0}, c + V{r, 0}, 0.9, g.S(DETAIL, z + 1));
    d.line(c - V{0, r}, c + V{0, r}, 0.9, g.S(DETAIL, z + 1));
    for (int k = 0; k < 3; k++) d.diamond(c + dirOf(g.r.f(0, 2 * PI)) * (r * g.r.f(0.3, 0.85)), 3, g.S(ACCENT, z + 2, g.seqG(k)));
    Groups sg;
    int cg = -1;
    if (g.fanBudget > 0 && g.anim(1)) { g.fanBudget--; sg = {g.ga.get()}; cg = g.ga.get(); d.rect(c.x, c.y, 1.2, 1.2, 0, g.S(NEAR_SIL, z + 2, {cg})); }
    d.line(c, c + V{r * 0.95, 0}, 1.8, g.S(ENERGY, z + 3, sg));
    d.line(c, c + dirOf(-0.25) * (r * 0.9), 1.2, g.S(GLOW, z + 2, sg));
    d.line(c, c + dirOf(-0.5) * (r * 0.85), 1.2, g.S(GLOW_SOFT, z + 2, sg));
    if (cg >= 0) g.a.spin(sg[0], cg, g.beat * 4, true);
}

void mGrille(GCtx& g, Box b, int z) {
    b = b.inset(5);
    if (!b.ok()) return;
    V c = b.c();
    double rr = std::min(b.w(), b.h()) / 2;
    g.d.ring(c, rr, 1.4, g.S(DETAIL, z), 18);
    for (double y = c.y - rr + 3; y <= c.y + rr - 3; y += 4)
        for (double x = c.x - rr + 3; x <= c.x + rr - 3; x += 4)
            if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) < (rr - 3) * (rr - 3)) g.d.rect(x, y, 1.8, 1.8, 0, g.S(FILL_MID, z + 1));
}

void mFins(GCtx& g, Box b, int z) {
    b = b.inset(4);
    if (!b.ok()) return;
    V c = b.c();
    g.d.rect(c.x, c.y, b.w(), b.h(), 0, g.S(NEAR_SIL, z));
    for (double x = b.x0 - b.h(); x < b.x1; x += 5) {
        V a{x, b.y0}, e{x + b.h(), b.y1};
        if (clipSeg(a, e, b)) g.d.line(a, e, 2.4, g.S(FILL_LIGHT, z + 1));
    }
    g.d.polyline(boxPts(b), 1.2, g.S(EDGE, z + 2), true);
}

void mWarning(GCtx& g, Box b, int z) {
    double s = std::min(b.w(), b.h()) * 0.42;
    if (s < 7) return;
    V c = b.c();
    auto& d = g.d;
    Pts tri{{c.x - s, c.y - s * 0.8}, {c.x + s, c.y - s * 0.8}, {c.x, c.y + s * 0.95}};
    d.fill(tri, g.S(NEAR_SIL, z), 3);
    d.polyline(tri, 2, g.S(ACCENT, z + 1, g.seqG(0)), true);
    d.rect(c.x, c.y + s * 0.05, 2, s * 0.6, 0, g.S(EDGE, z + 2));
    d.rect(c.x, c.y - s * 0.52, 2.2, 2.2, 0, g.S(EDGE, z + 2));
}

void mBarcode(GCtx& g, Box b, int z) {
    b = b.inset(5);
    if (!b.ok()) return;
    double x = b.x0;
    while (x < b.x1 - 1) {
        double w = g.r.f(0.8, 2.6);
        if (g.r.chance(0.7)) g.d.rect(x + w / 2, b.c().y + 1.5, w, b.h() - 5, 0, g.S(DETAIL, z));
        x += w + g.r.f(0.8, 2);
    }
    g.d.rect(b.c().x, b.y0 + 1.5, b.w() * 0.6, 1.4, 0, g.S(EDGE, z));
}

void mCapacitors(GCtx& g, Box b, int z) {
    b = b.inset(4);
    if (!b.ok() || b.h() < 10) return;
    double cw = 6, gap = 2.5;
    int n = std::max(1, (int)(b.w() / (cw + gap)));
    double ox = b.c().x - (n * (cw + gap) - gap) / 2 + cw / 2;
    for (int k = 0; k < n; k++) {
        double h = g.r.f(0.6, 1.0) * (b.h() - 2);
        double x = ox + k * (cw + gap), y0 = b.y0 + 1;
        g.d.rect(x, y0 + h / 2, cw, h, 0, g.S(FILL_MID, z));
        g.d.rect(x - cw * 0.2, y0 + h / 2, cw * 0.25, h - 2, 0, g.S(DETAIL, z + 1));
        g.d.polyline({{x - cw / 2, y0}, {x + cw / 2, y0}, {x + cw / 2, y0 + h}, {x - cw / 2, y0 + h}}, 1.0, g.S(EDGE, z + 2), true);
        g.d.rect(x, y0 + h + 1, cw * 0.6, 2, 0, g.S(EDGE, z + 2));
        g.d.rect(x, y0 + h * 0.4, cw, 2.2, 0, g.S(ACCENT, z + 2, g.seqG(k)));
    }
}

// ------------------------------------------------------------ module picker

void placeModule(GCtx& g, Box const& b, Look look, int z) {
    double w = b.w(), h = b.h(), mn = std::min(w, h), ar = w / std::max(1.0, h);
    auto& r = g.r;
    if (mn < 9) { if (std::max(w, h) > 20) mLightStrip(g, b, z); return; }
    if (look == Look::Circles) { if (r.chance(0.7)) mDiamonds(g, b, z); else mChevrons(g, b, z); return; }
    if (look == Look::Neon) {
        int k = r.i(0, 3);
        if (k == 0) mLightStrip(g, b, z);
        else if (k == 1 && mn >= 14) mCore(g, b, z);
        else if (k == 2) mHexPatch(g, b, z);
        else mChevrons(g, b, z);
        return;
    }
    // weighted choice by shape
    struct Opt { void (*fn)(GCtx&, Box, int); double wgt; };
    std::vector<Opt> opts;
    bool square = ar > 0.6 && ar < 1.7;
    bool wide = ar >= 1.7, tall = ar <= 0.6;
    bool ind = look == Look::Industrial;
    if (square && mn >= 18) { opts.push_back({mPort, 2}); opts.push_back({mFan, ind ? 3.0 : 2.0}); opts.push_back({mCore, look == Look::Cyber ? 2 : 0.5}); opts.push_back({mChip, 1.5}); }
    if (mn >= 14 && w >= 24) opts.push_back({mScreen, look == Look::Cyber ? 2.5 : 1.5});
    if (wide) { opts.push_back({mChevrons, 1.5}); opts.push_back({mPipes, ind ? 3.0 : 1.0}); opts.push_back({mLightStrip, 1.5}); opts.push_back({mEqualizer, h >= 14 ? 1.5 : 0}); }
    if (tall) { opts.push_back({mPowerCell, 2.5}); opts.push_back({mPipes, ind ? 2 : 0.8}); opts.push_back({mLightStrip, 1}); }
    opts.push_back({mVents, ind ? 2.5 : 1.2});
    opts.push_back({mLightGrid, look == Look::Cyber ? 1.5 : 1});
    opts.push_back({mHexPatch, look == Look::Cyber ? 1.2 : 0.4});
    opts.push_back({mDataBars, look == Look::Cyber ? 1.2 : 0.6});
    opts.push_back({mHazard, ind ? 3 : look == Look::Tech ? 0.6 : 0.2});
    opts.push_back({mTruss, ind ? 1.5 : 0.4});
    if (!square && mn >= 12) opts.push_back({mPowerCell, 0.8});
    bool cyb = look == Look::Cyber;
    if (wide && h >= 14) opts.push_back({mSevenSeg, cyb ? 2.0 : 1.0});
    if (square && mn >= 18) { opts.push_back({mGauge, ind ? 2.0 : 1.2}); opts.push_back({mRadar, cyb ? 1.8 : 0.8}); opts.push_back({mGrille, 0.8}); opts.push_back({mWarning, ind ? 2.0 : 0.6}); }
    if (wide) { opts.push_back({mFins, ind ? 2.0 : 1.0}); opts.push_back({mBarcode, cyb ? 0.8 : 0.4}); opts.push_back({mCapacitors, h >= 12 ? (ind ? 1.8 : 1.0) : 0.0}); }
    if (tall) opts.push_back({mFins, 0.8});
    double total = 0;
    for (auto& o : opts) total += o.wgt;
    double pick = r.f(0, total);
    for (auto& o : opts) {
        if ((pick -= o.wgt) <= 0) { o.fn(g, b, z); return; }
    }
    opts.back().fn(g, b, z);
}

} // namespace inspo
