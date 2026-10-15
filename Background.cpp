// Backgrounds. Eight archetypes share a common skeleton (gradient sky, layered
// silhouettes, parallax groups, ambient motion) but each has its own set pieces.

#include "Ctx.hpp"

#include <algorithm>
#include <map>

namespace inspo {

namespace {
constexpr int Z_SKY = -100, Z_STAR = -97, Z_CEL = -95, Z_FAR = -90, Z_MID = -80, Z_FOG = -70, Z_NEAR = -60, Z_MOTE = 25;
constexpr double PLAYER_SPEED_1X = 311.58; // units per second at normal speed

double noise1(double x, double const* ph, int oct) {
    double v = 0, amp = 1, f = 1, norm = 0;
    for (int i = 0; i < oct; i++) {
        v += std::sin(x * 0.011 * f + ph[i]) * amp;
        norm += amp;
        amp *= 0.5;
        f *= 2.13;
    }
    return v / norm;
}

V dirv(double a) { return {std::cos(a), std::sin(a)}; }

struct BG {
    Ctx& c;
    double L, R, W, mx, gy, x0, x1;
    int gSky, gCel, gFar, gMid, gFog;

    Rng& r() { return c.rng; }
    Draw& d() { return c.d; }
    Style S(Role role, int z, Groups g) { return c.S(role, z, g); }

    void sky(bool invert = false) {
        Role order[4] = {SKY4, SKY3, SKY2, SKY1};
        if (invert) std::swap(order[0], order[3]), std::swap(order[1], order[2]);
        double ys[5] = {gy - 2 * U, gy + 3.5 * U, gy + 7.5 * U, gy + 12 * U, gy + 24 * U};
        for (int i = 0; i < 4; i++) d().rect(mx, (ys[i] + ys[i + 1]) / 2, W, ys[i + 1] - ys[i] + 1, 0, S(order[i], Z_SKY, {gSky}));
        for (int i = 0; i < 3; i++) {
            double yb = ys[i + 1];
            for (int k = 0; k < 3; k++) {
                double h = 0.22 * U * (1.0 - k * 0.3);
                d().rect(mx, yb + (k * 0.5 + 0.3) * U, W, h, 0, S(order[i], Z_SKY + 1, {gSky}));
                d().rect(mx, yb - (k * 0.5 + 0.3) * U, W, h, 0, S(order[i + 1], Z_SKY + 1, {gSky}));
            }
        }
    }

    void stars(int count, double yMin = 5, double yMax = 22) {
        int tw[3] = {c.ga.get(), c.ga.get(), c.ga.get()};
        for (int i = 0; i < count; i++) {
            V p{r().f(L, R), gy + r().f(yMin, yMax) * U};
            double s = r().chance(0.12) ? r().f(3, 5) : r().f(1.2, 2.6);
            Groups g{gCel};
            if (r().chance(0.4)) g.push_back(tw[r().i(0, 2)]);
            d().diamond(p, s, S(PARTICLE, Z_STAR, g), r().chance(0.5) ? 45 : 0);
            if (s > 3 && c.s.detail >= 2) {
                d().rect(p.x, p.y, s * 3.5, 0.8, 0, S(PARTICLE, Z_STAR, g));
                d().rect(p.x, p.y, 0.8, s * 3.5, 0, S(PARTICLE, Z_STAR, g));
            }
        }
        if (c.anim(2))
            for (int k = 0; k < 3; k++) c.a.flicker(tw[k], 0.15, 1, r().f(1.5, 3), k * 0.7);
    }

    Pts ridge(double base, double amp, double step, double freq, int oct) {
        double ph[4] = {r().f(0, 9), r().f(0, 9), r().f(0, 9), r().f(0, 9)};
        Pts top;
        for (double x = L; x <= R; x += step) top.push_back({x, gy + (base + amp * noise1(x * freq, ph, oct)) * U});
        return top;
    }

    void hills(Pts const& top, Role role, int z, Groups g, Role rim = ROLE_COUNT, double rimW = 1.6) {
        Pts poly{{L, gy - U}};
        poly.insert(poly.end(), top.begin(), top.end());
        poly.push_back({R, gy - U});
        d().fill(poly, S(role, z, g), 8);
        if (rim != ROLE_COUNT) d().polyline(top, rimW, S(rim, z + 1, g));
    }

    void pines(Pts const& top, Role role, int z, Groups g, double chance, double hMin, double hMax) {
        for (size_t i = 1; i < top.size(); i++) {
            if (!r().chance(chance)) continue;
            double th = r().f(hMin, hMax) * U, tw = th * 0.4;
            V b = top[i];
            for (int k = 0; k < 3; k++) {
                double yy = b.y - 3 + k * th * 0.28, ww = tw * (1 - k * 0.25);
                d().fill({{b.x - ww, yy}, {b.x, yy + th * 0.5}, {b.x + ww, yy}}, S(role, z, g), 6);
            }
        }
    }

    void rubble() {
        Pts g{{L, gy - U}};
        for (double x = L; x <= R; x += r().f(0.3, 0.7) * U) g.push_back({x, gy + r().f(0.05, 0.6) * U});
        g.push_back({R, gy - U});
        d().fill(g, S(NEAR_SIL, Z_NEAR, {}), 6);
    }

    void fog(int n) {
        for (int k = 0; k < n; k++) {
            double fw = r().f(0.2, 0.4) * W, fx = r().f(L + fw / 2, R - fw / 2), fy = gy + r().f(0.5, 3.5) * U;
            double fh = r().f(0.8, 1.6) * U;
            for (int j = 0; j < 4; j++)
                d().rect(fx + r().f(-0.05, 0.05) * fw, fy, fw * (1 - j * 0.22), fh * (1 - j * 0.2), 0, S(FOG, Z_FOG, {gFog}));
        }
        if (c.anim(1)) c.a.bob(gFog, 3 * U, 0, r().f(10, 14));
    }

    void motes(double yMax = 8, double rise = 3) {
        if (!c.anim(2)) return;
        int mg[4];
        for (auto& g : mg) g = c.ga.get();
        int n = 10 * c.s.detail + (int)(W / (2 * U));
        for (int i = 0; i < n; i++)
            d().diamond({r().f(x0 - 6 * U, x1 + 6 * U), gy + r().f(0.3, yMax) * U}, r().f(2.2, 4.2), S(PARTICLE, Z_MOTE, {mg[i % 4]}), r().f(0, 90));
        for (int k = 0; k < 4; k++) c.a.rise(mg[k], r().f(-1, 1) * U, r().f(rise * 0.6, rise) * U, r().f(4.5, 7), k * 1.4);
    }

    void celestial(V p, double cr, bool crescent, bool craters) {
        Groups g{gCel};
        d().glow(p, cr * 3.2, S(GLOW_SOFT, Z_CEL - 1, g), 4);
        d().circle(p, cr, S(CELESTIAL, Z_CEL, g), c.strip());
        d().ring(p, cr * 1.35, 1.6, S(CELESTIAL, Z_CEL, g), 36);
        if (crescent) d().circle(p + V{cr * 0.42, cr * 0.18}, cr * 0.92, S(SKY2, Z_CEL + 1, g), c.strip());
        else if (craters)
            for (int k = 0; k < 4; k++) {
                double ang = r().f(0, 2 * PI), dist = r().f(0, 0.6) * cr;
                d().circle(p + V{std::cos(ang) * dist, std::sin(ang) * dist}, r().f(0.12, 0.25) * cr, S(SKY4, Z_CEL + 1, g), 3);
            }
    }

    V skySpot() {
        double side = r().chance(0.5) ? r().f(0.05, 0.3) : r().f(0.7, 0.95);
        return {x0 + (x1 - x0) * side, gy + r().f(11.5, 14) * U};
    }
};

// ---------------------------------------------------------------- archetypes

void bgNight(BG& b) {
    b.sky();
    b.stars(25 * b.c.s.detail + (int)(b.W / U));
    b.celestial(b.skySpot(), b.r().f(1.1, 1.7) * U, b.r().chance(0.45), true);
    if (b.r().chance(0.5)) {
        b.hills(b.ridge(3.6, 2.8, b.r().f(0.5, 1.0) * U, 1, 4), FAR_SIL, Z_FAR, {b.gFar}, SKY4);
    } else {
        double x = b.L;
        double ph[2] = {b.r().f(0, 9), b.r().f(0, 9)};
        while (x < b.R) {
            double bw = b.r().f(1.0, 2.6) * U, env = 0.6 + 0.4 * noise1(x, ph, 2);
            double bh = b.r().f(2, 7.5) * U * env + U;
            b.d().rect(x + bw / 2, b.gy - U + bh / 2, bw, bh + 2 * U, 0, b.S(FAR_SIL, Z_FAR, {b.gFar}));
            b.d().line({x, b.gy + bh - U}, {x + bw, b.gy + bh - U}, 1.3, b.S(SKY4, Z_FAR + 1, {b.gFar}));
            if (b.c.s.detail >= 2)
                for (int k = 0, n = b.r().i(2, 7); k < n; k++)
                    b.d().rect(x + b.r().f(0.2, 0.8) * bw, b.gy + b.r().f(0, bh - 1.4 * U), 2.5, 3.5, 0, b.S(SKY4, Z_FAR + 1, {b.gFar}));
            x += bw + b.r().f(0, 0.25) * U;
        }
    }
    auto mid = b.ridge(1.8, 1.3, 0.5 * U, 1.4, 3);
    b.hills(mid, MID_SIL, Z_MID, {b.gMid}, GLOW_SOFT, 2.2);
    b.pines(mid, MID_SIL, Z_MID, {b.gMid}, 0.3, 0.7, 1.6);
    b.rubble();
    b.fog(3 + b.c.s.detail);
    b.motes();
}

void bgSynthwave(BG& b) {
    auto& d = b.d();
    double hz = b.gy + 3 * b.r().f(0.9, 1.2) * U; // horizon
    b.sky();
    b.stars(15 * b.c.s.detail, 9, 22);
    // striped sun
    V sc{b.x0 + (b.x1 - b.x0) * b.r().f(0.35, 0.65), hz + b.r().f(2.2, 3.2) * U};
    double sr = b.r().f(2.6, 3.6) * U;
    Groups cg{b.gCel};
    d.glow(sc, sr * 3, b.S(GLOW_SOFT, Z_CEL - 1, cg), 4);
    int top = b.c.ch(CELESTIAL), bot = b.c.ch(ACCENT);
    double strip = 4;
    for (double y = sc.y - sr; y < sc.y + sr; y += strip) {
        double t = (y - (sc.y - sr)) / (2 * sr); // 0 bottom .. 1 top
        double yc = y + strip / 2;
        if (yc < hz) continue;
        if (t < 0.5) {
            double period = 6 + 14 * t; // gaps get thinner toward the middle
            double gap = 2.5 + 4 * (0.5 - t);
            if (std::fmod(y - (sc.y - sr), period) < gap) continue;
        }
        double half = std::sqrt(std::max(0.0, sr * sr - (yc - sc.y) * (yc - sc.y)));
        if (half < 1) continue;
        d.rect(sc.x, yc, half * 2, strip + 0.6, 0, Style{t > 0.55 ? top : bot, Z_CEL, cg});
    }
    // wireframe mountains on the horizon
    double x = b.L;
    while (x < b.R) {
        double mw = b.r().f(2, 5) * U, mh = b.r().f(1, 3) * U;
        Pts tri{{x, hz}, {x + mw / 2, hz + mh}, {x + mw, hz}};
        d.fill(tri, b.S(FAR_SIL, Z_FAR, {b.gFar}), 6);
        d.polyline(tri, 1.4, b.S(ENERGY, Z_FAR + 1, {b.gFar}));
        d.line(tri[1], {x + mw * b.r().f(0.3, 0.7), hz}, 1.0, b.S(ENERGY, Z_FAR + 1, {b.gFar}));
        x += mw * b.r().f(0.5, 0.9);
    }
    // perspective grid floor
    Groups fg{b.gSky};
    d.rect(b.mx, (b.gy - U + hz) / 2, b.W, hz - b.gy + U, 0, b.S(NEAR_SIL, Z_MID, fg));
    d.rect(b.mx, hz, b.W, 2.4, 0, b.S(ENERGY, Z_MID + 2, fg));
    double vx = sc.x;
    for (double bx = b.L; bx <= b.R; bx += 1.2 * U) {
        V p{bx, b.gy - U}, q{vx + (bx - vx) * 0.12, hz};
        d.line(p, q, 1.2, b.S(ENERGY, Z_MID + 1, fg));
    }
    int hg = b.c.ga.get();
    for (int k = 0; k < 9; k++) {
        double t = std::pow(k / 9.0, 1.8);
        double y = b.gy - U + (hz - b.gy + U) * t;
        d.rect(b.mx, y, b.W, 1.3 + (1 - t) * 1.2, 0, b.S(ENERGY, Z_MID + 1, Groups{b.gSky, hg}));
    }
    if (b.c.anim(1)) b.c.a.loop({{{Anim::move(hg, 0, -0.35 * U, 0.6)}, 0.6}, {{Anim::move(hg, 0, 0.35 * U, 0)}, 0.05}});
    b.motes(6, 2);
}

void bgNebula(BG& b) {
    auto& d = b.d();
    b.sky();
    b.stars(45 * b.c.s.detail + (int)(b.W / U) * 2, 1, 23);
    Role cloudRoles[3] = {GLOW_SOFT, FOG, ENERGY};
    int clouds = 4 + b.c.s.detail * 2;
    int ng = b.c.ga.get();
    for (int k = 0; k < clouds; k++) {
        V cc{b.r().f(b.L, b.R), b.gy + b.r().f(5, 18) * U};
        double sz = b.r().f(2.5, 5.5) * U;
        Role role = cloudRoles[b.r().i(0, 1)];
        int blobs = b.r().i(5, 10);
        for (int j = 0; j < blobs; j++) {
            V p = cc + V{b.r().f(-1, 1) * sz, b.r().f(-0.45, 0.45) * sz};
            d.glow(p, b.r().f(0.4, 0.9) * sz, b.S(role, Z_STAR - 1, {b.gCel, ng}), 1);
        }
        if (b.c.s.detail >= 2)
            for (int j = 0; j < 6; j++)
                d.diamond(cc + V{b.r().f(-1, 1) * sz, b.r().f(-0.4, 0.4) * sz}, b.r().f(2, 4), b.S(EDGE, Z_STAR, {b.gCel}));
    }
    // spiral galaxy
    V gc = b.skySpot();
    double arms = 2, spin = b.r().sign();
    int gg = b.c.ga.get();
    int ctr = 0;
    {
        int cgid = b.c.ga.get();
        d.rect(gc.x, gc.y, 1.2, 1.2, 0, b.S(FILL_DARK, Z_CEL, {b.gCel, cgid}));
        ctr = cgid;
    }
    d.glow(gc, 1.2 * U, b.S(CELESTIAL, Z_CEL, {b.gCel, gg}), 2);
    for (int a = 0; a < arms; a++)
        for (int i = 0; i < 40; i++) {
            double t = i / 40.0;
            double ang = spin * (t * 3.6 + a * PI);
            double rad = (0.2 + t * 2.2) * U;
            V p = gc + dirv(ang) * rad;
            d.diamond(p + V{b.r().f(-3, 3), b.r().f(-3, 3)}, b.r().f(1.5, 3.5) * (1.2 - t), b.S(PARTICLE, Z_CEL, {b.gCel, gg}));
        }
    if (b.c.anim(1)) {
        b.c.a.spin(gg, ctr, b.r().f(40, 60), spin > 0);
        b.c.a.flicker(ng, 0.7, 1.0, b.r().f(5, 8));
    }
    // distant planet
    V pp{b.r().f(b.x0, b.x1), b.gy + b.r().f(4, 8) * U};
    double pr = b.r().f(0.8, 1.6) * U;
    d.glow(pp, pr * 2, b.S(GLOW_SOFT, Z_CEL, {b.gCel}), 2);
    d.circle(pp, pr, b.S(FILL_MID, Z_CEL + 1, {b.gCel}), b.c.strip());
    d.circle(pp + V{pr * 0.35, -pr * 0.2}, pr * 0.85, b.S(FILL_DARK, Z_CEL + 2, {b.gCel}), b.c.strip());
    d.ring(pp, pr, 1.6, b.S(EDGE, Z_CEL + 3, {b.gCel}), 18, PI * 0.55, PI * 1.25);
    b.motes(10, 2);
}

void bgAurora(BG& b) {
    auto& d = b.d();
    b.sky();
    b.stars(30 * b.c.s.detail + (int)(b.W / U));
    int ribbons = 2 + (b.c.s.detail >= 2);
    for (int k = 0; k < ribbons; k++) {
        int rg = b.c.ga.get();
        double base = b.gy + b.r().f(8, 13) * U, amp = b.r().f(1, 2.5) * U, freq = b.r().f(0.004, 0.009), ph = b.r().f(0, 6);
        double h0 = b.r().f(1.8, 3.5) * U;
        Pts curve;
        for (double x = b.L; x <= b.R; x += 7) {
            double y = base + std::sin(x * freq + ph) * amp + std::sin(x * freq * 2.7 + ph * 2) * amp * 0.3;
            double h = h0 * (0.6 + 0.4 * std::sin(x * freq * 1.9 + ph));
            curve.push_back({x, y});
            d.rect(x, y + h / 2, 7.6, h, 0, b.S(GLOW_SOFT, Z_STAR + 1, {b.gCel, rg}));
            d.rect(x, y + h * 0.25, 7.6, h * 0.5, 0, b.S(GLOW_SOFT, Z_STAR + 1, {b.gCel, rg}));
        }
        d.polyline(curve, 2, b.S(ENERGY, Z_STAR + 2, {b.gCel, rg}));
        if (b.c.anim(1)) {
            b.c.a.bob(rg, b.r().f(1, 2) * U, b.r().f(0.3, 0.6) * U, b.r().f(8, 12), k * 2.0);
            b.c.a.flicker(rg, 0.55, 1, b.r().f(3, 5), k);
        }
    }
    auto farRidge = b.ridge(4, 2.5, 0.5 * U, 1.2, 4);
    b.hills(farRidge, FAR_SIL, Z_FAR, {b.gFar});
    for (size_t i = 1; i + 1 < farRidge.size(); i++)
        if (farRidge[i].y > farRidge[i - 1].y && farRidge[i].y > farRidge[i + 1].y && farRidge[i].y > b.gy + 4.5 * U)
            b.d().fill({{farRidge[i].x - 0.7 * U, farRidge[i].y - 0.6 * U}, farRidge[i], {farRidge[i].x + 0.7 * U, farRidge[i].y - 0.6 * U}}, b.S(EDGE, Z_FAR + 1, {b.gFar}), 4);
    auto mid = b.ridge(1.5, 1.0, 0.5 * U, 1.5, 3);
    b.hills(mid, MID_SIL, Z_MID, {b.gMid});
    b.pines(mid, MID_SIL, Z_MID, {b.gMid}, 0.55, 0.8, 1.8);
    b.rubble();
    b.motes(6, 1.5);
}

void bgAbyss(BG& b) {
    auto& d = b.d();
    b.sky(true);
    // god rays
    int rays = 5 + b.c.s.detail * 2;
    int rg[2] = {b.c.ga.get(), b.c.ga.get()};
    for (int k = 0; k < rays; k++) {
        V top{b.r().f(b.L, b.R), b.gy + 24 * U};
        double ang = b.r().f(-20, -8), len = b.r().f(14, 22) * U, w = b.r().f(0.4, 1.4) * U;
        V mid = top + V{std::sin(ang * PI / 180) * len / 2 * -1, -len / 2};
        d.rect(mid.x, mid.y, w, len, ang, b.S(GLOW_SOFT, Z_STAR, {b.gCel, rg[k % 2]}));
        d.rect(mid.x, mid.y, w * 0.4, len, ang, b.S(GLOW_SOFT, Z_STAR, {b.gCel, rg[k % 2]}));
    }
    if (b.c.anim(1)) for (int k = 0; k < 2; k++) b.c.a.flicker(rg[k], 0.4, 1, b.r().f(4, 7), k * 2);
    auto farRidge = b.ridge(2.5, 2.0, 0.6 * U, 1.3, 4);
    b.hills(farRidge, FAR_SIL, Z_FAR, {b.gFar});
    auto mid = b.ridge(1.2, 0.9, 0.5 * U, 1.6, 3);
    b.hills(mid, MID_SIL, Z_MID, {b.gMid}, GLOW_SOFT, 2);
    // kelp
    int kelps = (int)(b.W / (3 * U));
    for (int k = 0; k < kelps; k++) {
        double kx = b.r().f(b.L, b.R), kh = b.r().f(2, 5) * U, ph = b.r().f(0, 6);
        int kg = b.c.ga.get();
        int ctrg = b.c.ga.get();
        d.rect(kx, b.gy, 1.2, 1.2, 0, b.S(NEAR_SIL, Z_NEAR, {ctrg}));
        Pts stem;
        for (int i = 0; i <= 10; i++) {
            double t = i / 10.0;
            stem.push_back({kx + std::sin(t * 5 + ph) * 0.2 * U * t, b.gy + kh * t});
        }
        d.polyline(stem, 3, b.S(NEAR_SIL, Z_NEAR, {kg}));
        for (int i = 2; i < 10; i += 2) {
            double sgn = i % 4 ? 1 : -1;
            d.rect(stem[i].x + sgn * 5, stem[i].y, 10, 3.5, sgn * -30, b.S(NEAR_SIL, Z_NEAR, {kg}));
        }
        if (b.c.anim(1)) b.c.a.swing(kg, ctrg, b.r().f(4, 8), b.r().f(3, 5), b.r().f(0, 2));
    }
    // bubbles
    if (b.c.anim(2)) {
        for (int k = 0; k < 4; k++) {
            int bg = b.c.ga.get();
            for (int j = 0; j < 5; j++) d.ring({b.r().f(b.x0, b.x1), b.gy + b.r().f(0, 3) * U}, b.r().f(2.5, 5), 1.2, b.S(EDGE, Z_MOTE, {bg}), 8);
            b.c.a.rise(bg, b.r().f(-0.4, 0.4) * U, b.r().f(5, 8) * U, b.r().f(4, 6), k * 1.2);
        }
    }
    b.rubble();
    b.motes(10, 1.5);
}

void bgGeometric(BG& b) {
    auto& d = b.d();
    b.sky();
    b.stars(10 * b.c.s.detail, 6, 22);
    // dot grid farRidge back
    if (b.c.s.detail >= 2)
        for (double y = b.gy + 2 * U; y < b.gy + 20 * U; y += 1.5 * U)
            for (double x = std::floor(b.L / (1.5 * U)) * 1.5 * U; x < b.R; x += 1.5 * U)
                if (b.r().chance(0.5)) d.rect(x, y, 2, 2, 0, b.S(FAR_SIL, Z_SKY + 2, {b.gFar}));
    int shapes = 4 + b.c.s.detail * 2;
    for (int k = 0; k < shapes; k++) {
        V cc{b.r().f(b.L, b.R), b.gy + b.r().f(3, 16) * U};
        double sz = b.r().f(1.2, 4.5) * U;
        int sg = b.c.ga.get(), cgid = b.c.ga.get();
        Groups layer{k % 2 ? b.gFar : b.gMid};
        d.rect(cc.x, cc.y, 1.2, 1.2, 0, b.S(FAR_SIL, Z_FAR, layer + Groups{cgid}));
        Role role = sz > 3 * U ? FAR_SIL : DETAIL;
        int z = sz > 3 * U ? Z_FAR : Z_MID;
        switch (b.r().i(0, 3)) {
            case 0: d.ring(cc, sz, 2.2, b.S(role, z, layer + Groups{sg}), 32); d.ring(cc, sz * 0.8, 1.2, b.S(role, z, layer + Groups{sg}), 32, 0, PI * 1.3); break;
            case 1: {
                Pts sq{{cc.x - sz, cc.y - sz}, {cc.x + sz, cc.y - sz}, {cc.x + sz, cc.y + sz}, {cc.x - sz, cc.y + sz}};
                d.polyline(rotated(sq, b.r().f(0, 90), cc), 2.2, b.S(role, z, layer + Groups{sg}), true);
                break;
            }
            case 2: {
                Pts tri;
                for (int i = 0; i < 3; i++) tri.push_back(cc + dirv(PI / 2 + i * 2 * PI / 3) * sz);
                d.polyline(tri, 2.2, b.S(role, z, layer + Groups{sg}), true);
                d.polyline(rotated(tri, 60, cc), 1.2, b.S(role, z, layer + Groups{sg}), true);
                break;
            }
            default: {
                Pts hex;
                for (int i = 0; i < 6; i++) hex.push_back(cc + dirv(i * PI / 3) * sz);
                d.polyline(hex, 2.2, b.S(role, z, layer + Groups{sg}), true);
                for (int i = 0; i < 6; i += 2) d.line(cc, hex[i], 1.0, b.S(role, z, layer + Groups{sg}));
            }
        }
        if (b.c.anim(1)) b.c.a.spin(sg, cgid, b.r().f(10, 30), b.r().chance(0.5));
    }
    for (int k = 0; k < 8 + 4 * b.c.s.detail; k++) {
        int g = b.c.ga.get();
        V p{b.r().f(b.L, b.R), b.gy + b.r().f(1, 12) * U};
        double sz = b.r().f(0.15, 0.5) * U;
        d.rect(p.x, p.y, sz, sz, b.r().f(0, 90), b.S(k % 3 ? MID_SIL : ACCENT, Z_MID + 2, {b.gMid, g}));
        if (b.c.anim(1)) b.c.a.bob(g, 0, b.r().f(0.3, 0.8) * U, b.r().f(3, 6), b.r().f(0, 3));
    }
    b.fog(2);
}

void bgSunset(BG& b) {
    auto& d = b.d();
    b.sky();
    V sun{b.x0 + (b.x1 - b.x0) * b.r().f(0.3, 0.7), b.gy + b.r().f(3.5, 5.5) * U};
    double sr = b.r().f(1.8, 2.6) * U;
    Groups cg{b.gCel};
    d.glow(sun, sr * 5, b.S(GLOW_SOFT, Z_CEL - 2, cg), 5);
    d.glow(sun, sr * 2.2, b.S(GLOW, Z_CEL - 1, cg), 3);
    d.circle(sun, sr, b.S(CELESTIAL, Z_CEL, cg), b.c.strip());
    for (int k = 0; k < 3; k++) {
        double y = b.gy + b.r().f(7, 12) * U, w = b.r().f(4, 9) * U;
        double x = b.r().f(b.L, b.R);
        for (int j = 0; j < 3; j++) d.rect(x + b.r().f(-0.3, 0.3) * U, y + j * 3, w * (1 - j * 0.25), 3.5, 0, b.S(SKY4, Z_STAR, cg));
    }
    double bases[3] = {3.8, 2.6, 1.4};
    Role roles[3] = {FAR_SIL, MID_SIL, NEAR_SIL};
    int zs[3] = {Z_FAR, Z_MID, Z_NEAR - 1};
    int gs[3] = {b.gFar, b.gMid, -1};
    for (int i = 0; i < 3; i++) {
        auto top = b.ridge(bases[i], 1.2 - i * 0.2, 0.5 * U, 1.0 + i * 0.4, 3);
        Groups g = gs[i] >= 0 ? Groups{gs[i]} : Groups{};
        b.hills(top, roles[i], zs[i], g, i == 0 ? GLOW_SOFT : ROLE_COUNT, 2.2);
        b.pines(top, roles[i], zs[i], g, 0.25 + i * 0.15, 0.6 + i * 0.3, 1.2 + i * 0.5);
    }
    // birds
    if (b.c.anim(1)) {
        int bg = b.c.ga.get();
        for (int k = 0; k < 5; k++) {
            V p{b.x0 + b.r().f(0, 0.5) * (b.x1 - b.x0), b.gy + b.r().f(8, 12) * U};
            double s = b.r().f(4, 7);
            d.line(p + V{-s, s * 0.5}, p, 1.3, b.S(NEAR_SIL, Z_FOG, {bg}));
            d.line(p, p + V{s, s * 0.5}, 1.3, b.S(NEAR_SIL, Z_FOG, {bg}));
        }
        b.c.a.loop({{{Anim::move(bg, 10 * U, U, 12, gd::ease::SINE_IN_OUT)}, 12}, {{Anim::move(bg, -10 * U, -U, 12, gd::ease::SINE_IN_OUT)}, 12}});
    }
    b.fog(2);
    b.motes(5, 1.5);
}

void bgClouds(BG& b) {
    auto& d = b.d();
    b.sky(true);
    V sun = b.skySpot();
    Groups cg{b.gCel};
    int rg = b.c.ga.get(), rc = b.c.ga.get();
    d.rect(sun.x, sun.y, 1.2, 1.2, 0, b.S(CELESTIAL, Z_CEL, Groups{b.gCel, rc}));
    d.glow(sun, 3.5 * U, b.S(GLOW_SOFT, Z_CEL - 1, cg), 4);
    d.circle(sun, 1.3 * U, b.S(CELESTIAL, Z_CEL, cg), b.c.strip());
    for (int k = 0; k < 12; k++) {
        double a = k * PI / 6;
        d.line(sun + dirv(a) * (1.6 * U), sun + dirv(a) * (2.3 * U), 3, b.S(CELESTIAL, Z_CEL, Groups{b.gCel, rg}));
    }
    if (b.c.anim(1)) b.c.a.spin(rg, rc, 20, true);
    auto cloud = [&](V p, double s, Groups g, int z) {
        for (int j = 0; j < 6; j++) {
            V q = p + V{b.r().f(-1, 1) * s, b.r().f(0, 0.5) * s};
            d.circle(q, b.r().f(0.35, 0.6) * s, b.S(FOG, z, g), 6);
        }
        d.rect(p.x, p.y + 0.1 * s, s * 2.2, 0.4 * s, 0, b.S(FOG, z, g));
    };
    for (int k = 0; k < 4 + b.c.s.detail * 2; k++) {
        int g = b.c.ga.get();
        cloud({b.r().f(b.L, b.R), b.gy + b.r().f(7, 16) * U}, b.r().f(0.8, 1.8) * U, {b.gCel, g}, Z_STAR);
        if (b.c.anim(1)) b.c.a.bob(g, b.r().f(2, 4) * U, 0, b.r().f(16, 26), b.r().f(0, 8));
    }
    auto farRidge = b.ridge(3, 1.6, 0.5 * U, 0.9, 3);
    b.hills(farRidge, FAR_SIL, Z_FAR, {b.gFar});
    auto mid = b.ridge(1.6, 0.9, 0.5 * U, 1.4, 3);
    b.hills(mid, MID_SIL, Z_MID, {b.gMid});
    b.pines(mid, MID_SIL, Z_MID, {b.gMid}, 0.2, 0.8, 1.5);
    b.rubble();
}


// ------------------------------------------------------------ space set pieces

double beatOf(BG& b) { return 60.0 / std::clamp(b.c.s.bpm, 40.0, 400.0); }

void planet(BG& b, V p, double R, bool gas, Groups g, int z) {
    auto& d = b.d();
    auto& c = b.c;
    double tilt = b.r().f(-20, -6);
    // atmosphere halo
    d.glow(p, R * 1.6, b.S(GLOW_SOFT, z - 2, g), 3);
    for (int k = 1; k <= 3; k++) d.ring(p, R + k * 3.0, 3.2, b.S(GLOW_SOFT, z - 1, g), 40, PI * 0.45, PI * 1.55);
    auto ringArc = [&](double a0, double a1, int zz) {
        if (!gas) return;
        Role roles[5] = {PLANET3, PLANET1, NEAR_SIL, PLANET2, EDGE};
        double radii[5] = {1.45, 1.55, 1.63, 1.72, 1.9};
        double th[5] = {2.0, 5.0, 1.2, 3.5, 1.2};
        for (int k = 0; k < 5; k++) {
            Pts pts = rotated(circlePts(p, R * radii[k], R * radii[k] * 0.24, 36, a0, a1), tilt, p);
            d.polyline(pts, th[k], b.S(roles[k], zz, g));
        }
    };
    ringArc(0, PI, z);
    if (gas) {
        int pat[7] = {c.ch(PLANET2), c.ch(PLANET1), c.ch(PLANET3), c.ch(PLANET2), c.ch(PLANET1), c.ch(PLANET3), c.ch(PLANET2)};
        double ph = b.r().f(0, 6), fq = b.r().f(0.03, 0.06);
        d.fill(circlePts(p, R, R, 48), b.S(PLANET2, z + 1, g), c.strip(), [&](double y) {
            int k = (int)std::floor((y - (p.y - R)) / (R * 0.16) + 0.7 * std::sin(y * fq + ph));
            return pat[((k % 7) + 7) % 7];
        });
        // storm
        V st = p + V{-R * 0.3, -R * 0.25};
        d.fill(circlePts(st, R * 0.18, R * 0.09, 14), b.S(ACCENT, z + 2, g), 2.5);
        d.fill(circlePts(st, R * 0.1, R * 0.05, 10), b.S(PLANET1, z + 3, g), 2.5);
    } else {
        d.circle(p, R, b.S(PLANET2, z + 1, g), c.strip());
        for (int k = 0; k < 7; k++) {
            double a = b.r().f(0, 2 * PI), dist = b.r().f(0, 0.75) * R, cr = b.r().f(0.06, 0.16) * R;
            V q = p + V{std::cos(a) * dist, std::sin(a) * dist};
            d.circle(q, cr, b.S(PLANET3, z + 2, g), 2.5);
            d.ring(q, cr, 1.2, b.S(PLANET1, z + 3, g), 10, PI * 0.6, PI * 1.4);
        }
    }
    // terminator: soft then hard shadow
    for (int k = 0; k < 2; k++) {
        double inner = k == 0 ? 0.25 : 0.5;
        Pts sh = circlePts(p, R, R, 24, -PI / 2, PI / 2);
        for (int i = 0; i <= 24; i++) {
            double a = PI / 2 - PI * i / 24;
            sh.push_back({p.x + std::cos(a) * R * inner, p.y + std::sin(a) * R});
        }
        d.fill(rotated(sh, -25, p), b.S(k == 0 ? PLANET3 : NEAR_SIL, z + 4 + k, g), c.strip());
    }
    d.ring(p, R, 2.2, b.S(EDGE, z + 6, g), 30, PI * 0.55, PI * 1.35);
    d.ring(p, R + 1.5, 2.5, b.S(GLOW, z + 6, g), 30, PI * 0.5, PI * 1.4);
    ringArc(PI, 2 * PI, z + 7);
}

void blackHole(BG& b, V p, double R, Groups g, int z) {
    auto& d = b.d();
    double tilt = b.r().f(-12, -4);
    d.glow(p, R * 4.2, b.S(GLOW_SOFT, z - 3, g), 4);
    std::vector<int> disk{b.c.ga.get(), b.c.ga.get(), b.c.ga.get()};
    Role roles[3] = {CELESTIAL, ENERGY, ACCENT};
    auto diskHalf = [&](double a0, double a1, int zz) {
        for (int k = 0; k < 14; k++) {
            double rr = R * (1.3 + k * 0.16);
            Pts pts = rotated(circlePts(p, rr, rr * 0.18, 52, a0, a1), tilt, p);
            double th = k < 4 ? 4.5 : k < 9 ? 3 : 1.8;
            d.polyline(pts, th, b.S(roles[k % 3], zz, g + Groups{disk[k % 3]}));
        }
    };
    diskHalf(0, PI, z - 1);
    // gravitationally lensed light wrapping over the top and under the bottom
    for (int k = 0; k < 7; k++) {
        double rr = R * (1.08 + k * 0.09);
        d.ring(p, rr, k < 2 ? 4 : 2.2, b.S(roles[k % 3], z, g + Groups{disk[k % 3]}), 48, PI * 0.03, PI * 0.97);
        if (k < 4) d.ring(p, rr * 0.97, 2, b.S(roles[(k + 1) % 3], z, g + Groups{disk[(k + 1) % 3]}), 40, PI * 1.12, PI * 1.88);
    }
    d.glow(p, R * 2.2, b.S(GLOW, z - 2, g), 3);
    d.circle(p, R, b.S(NEAR_SIL, z + 1, g), b.c.strip());
    d.ring(p, R * 1.03, 2.2, b.S(EDGE, z + 2, g), 40);
    diskHalf(PI, 2 * PI, z + 3);
    if (b.c.anim(1)) b.c.a.sequence({disk[0], disk[1], disk[2]}, beatOf(b), 1.0, 0.55, beatOf(b) * 0.8);
}

void eclipse(BG& b, V p, double R, Groups g, int z) {
    auto& d = b.d();
    d.glow(p, R * 4, b.S(GLOW_SOFT, z - 3, g), 4);
    int r1 = b.c.ga.get(), r2 = b.c.ga.get(), ctr = b.c.ga.get();
    d.rect(p.x, p.y, 1.2, 1.2, 0, b.S(NEAR_SIL, z, g + Groups{ctr}));
    for (int k = 0; k < 40; k++) {
        double a = 2 * PI * k / 40 + b.r().f(-0.03, 0.03);
        double len = R * b.r().f(0.35, 1.3);
        V u{std::cos(a), std::sin(a)};
        d.line(p + u * (R * 1.02), p + u * (R * 1.02 + len), b.r().f(1.6, 4), b.S(k % 3 ? GLOW : CELESTIAL, z - 1, g + Groups{k % 2 ? r1 : r2}));
    }
    d.rect(p.x, p.y, R * 6, 2, 0, b.S(GLOW, z, g + Groups{r1}));
    d.rect(p.x, p.y, 2, R * 3.5, 0, b.S(GLOW, z, g + Groups{r1}));
    d.circle(p, R, b.S(NEAR_SIL, z + 1, g), b.c.strip());
    d.ring(p, R * 1.02, 2.5, b.S(EDGE, z + 2, g), 44);
    d.diamond(p + V{R * 0.7, R * 0.72}, 6, b.S(EDGE, z + 3, g));
    if (b.c.anim(1)) {
        b.c.a.spin(r1, ctr, 40, true);
        b.c.a.spin(r2, ctr, 55, false);
        double bt = beatOf(b);
        b.c.a.flicker(r1, 0.45, 1, bt * 2, 0);
        b.c.a.flicker(r2, 0.45, 1, bt * 2, bt);
    }
}

void shootingStars(BG& b, int n) {
    if (!b.c.anim(2)) return;
    for (int k = 0; k < n; k++) {
        int g = b.c.ga.get();
        V p{b.r().f(b.x0, b.x1 + 8 * U), b.gy + b.r().f(12, 20) * U};
        double len = b.r().f(1.2, 2.2) * U;
        b.d().line(p, p + V{len, len * 0.45}, 1.6, b.S(EDGE, Z_STAR + 3, {b.gCel, g}));
        b.d().diamond(p, 3.5, b.S(EDGE, Z_STAR + 3, {b.gCel, g}));
        b.c.a.atStart(Anim::alpha(g, 0, 0));
        double wait = b.r().f(3, 7);
        b.c.a.loop({{{Anim::alpha(g, 1, 0.05), Anim::move(g, -8 * U, -3.6 * U, 0.9, gd::ease::EASE_IN)}, 0.7},
                    {{Anim::alpha(g, 0, 0.2)}, 0.25},
                    {{Anim::move(g, 8 * U, 3.6 * U, 0)}, wait}}, b.r().f(0, 4));
    }
}

void bgSpaceHero(BG& b, int hero) {
    auto& d = b.d();
    b.sky();
    b.stars(55 * b.c.s.detail + (int)(b.W / U) * 2, 0.5, 23);
    shootingStars(b, 3);
    // nebula: long streaky wisps along a shared flow direction
    int ng = b.c.ga.get();
    for (int k = 0; k < 3 + b.c.s.detail; k++) {
        V cc{b.r().f(b.L, b.R), b.gy + b.r().f(8, 19) * U};
        double sz = b.r().f(3, 6) * U, ang = b.r().f(-35, 35);
        for (int j = 0; j < 12; j++) {
            V q = cc + V{b.r().f(-1, 1) * sz, b.r().f(-0.3, 0.3) * sz};
            double len = b.r().f(0.8, 2.2) * sz, th = b.r().f(0.12, 0.35) * sz;
            Role role = j % 3 == 0 ? FOG : GLOW_SOFT;
            d.rect(q.x, q.y, len, th, ang + b.r().f(-12, 12), b.S(role, Z_STAR - 1, {b.gCel, ng}));
            d.rect(q.x, q.y, len * 0.6, th * 0.45, ang + b.r().f(-8, 8), b.S(role, Z_STAR - 1, {b.gCel, ng}));
        }
        for (int j = 0; j < 8; j++) d.diamond(cc + V{b.r().f(-1, 1) * sz, b.r().f(-0.3, 0.3) * sz}, b.r().f(1.5, 3), b.S(EDGE, Z_STAR, {b.gCel}));
    }
    if (b.c.anim(1)) b.c.a.flicker(ng, 0.75, 1, b.r().f(6, 9));
    // hero
    bool left = b.r().chance(0.5);
    V hp{b.x0 + (b.x1 - b.x0) * (left ? b.r().f(0.1, 0.3) : b.r().f(0.7, 0.9)), b.gy + b.r().f(9, 12) * U};
    if (hero < 0) hero = b.r().i(0, 3);
    if (hero == 0) planet(b, hp, b.r().f(3.4, 4.6) * U, true, {b.gCel}, Z_CEL);
    else if (hero == 1) blackHole(b, hp, b.r().f(1.7, 2.3) * U, {b.gCel}, Z_CEL);
    else if (hero == 2) eclipse(b, hp, b.r().f(1.8, 2.5) * U, {b.gCel}, Z_CEL);
    else {
        planet(b, hp, b.r().f(3.0, 4.0) * U, true, {b.gCel}, Z_CEL);
        planet(b, hp + V{(left ? 1 : -1) * b.r().f(6, 8) * U, -b.r().f(2, 4) * U}, b.r().f(0.9, 1.4) * U, false, {b.gCel}, Z_CEL + 10);
    }
    // smaller worlds at other depths
    int extra = 1 + b.c.s.detail / 2;
    for (int k = 0; k < extra; k++) {
        V q{b.r().f(b.x0, b.x1), b.gy + b.r().f(4, 9) * U};
        if (std::fabs(q.x - hp.x) < 6 * U) q.x += (left ? 1 : -1) * 8 * U;
        planet(b, q, b.r().f(0.7, 1.3) * U, b.r().chance(0.4), {b.gFar}, Z_FAR + 2);
    }
    // asteroid belt
    int ag = b.c.ga.get();
    double ay = b.gy + b.r().f(3, 6) * U;
    for (int k = 0; k < (int)(b.W / U); k++) {
        V q{b.r().f(b.L, b.R), ay + b.r().f(-0.8, 0.8) * U};
        double s0 = b.r().f(2, 7);
        d.rect(q.x, q.y, s0, s0 * b.r().f(0.6, 1), b.r().f(0, 90), b.S(MID_SIL, Z_MID + 2, {b.gMid, ag}));
        if (s0 > 5) d.rect(q.x - 1, q.y + 1, s0 * 0.5, 1.2, 0, b.S(FAR_SIL, Z_MID + 3, {b.gMid, ag}));
    }
    if (b.c.anim(1)) b.c.a.bob(ag, 1.5 * U, 0.2 * U, b.r().f(14, 20));
    b.motes(10, 2);
}

void bgSpace(BG& b) { bgSpaceHero(b, -1); }
void bgPlanets(BG& b) { bgSpaceHero(b, b.r().chance(0.5) ? 0 : 3); }
void bgBlackHole(BG& b) { bgSpaceHero(b, 1); }
void bgEclipse(BG& b) { bgSpaceHero(b, 2); }

void bgCircles(BG& b) {
    auto& d = b.d();
    b.sky();
    double bt = beatOf(b);
    V cc{b.x0 + (b.x1 - b.x0) * b.r().f(0.4, 0.6), b.gy + b.r().f(8, 10) * U};
    Groups base{b.gCel};
    int ctr = b.c.ga.get();
    d.rect(cc.x, cc.y, 1.2, 1.2, 0, b.S(NEAR_SIL, Z_SKY + 2, base + Groups{ctr}));
    // radial burst
    int ray = b.c.ga.get();
    for (int k = 0; k < 32; k++) {
        double a = 2 * PI * k / 32;
        V u{std::cos(a), std::sin(a)};
        d.line(cc + u * (1.5 * U), cc + u * (18 * U), k % 2 ? 2.5 : 6, b.S(GLOW_SOFT, Z_SKY + 3, base + Groups{ray}));
    }
    // concentric rings lit in sequence
    std::vector<int> rings;
    int n = 10 + b.c.s.detail * 2;
    for (int k = 0; k < n; k++) {
        int g = b.c.ga.get();
        rings.push_back(g);
        double rr = (1.4 + k * 1.15) * U;
        d.ring(cc, rr, k % 3 == 0 ? 4 : 2.2, b.S(k % 2 ? ACCENT : EDGE, Z_STAR, base + Groups{g}), std::max(24, (int)(rr / 9)));
    }
    // saw rings
    std::vector<int> saws;
    for (int k = 0; k < 2; k++) {
        int g = b.c.ga.get();
        saws.push_back(g);
        double rr = (k == 0 ? 3.6 : 7.4) * U;
        int teeth = k == 0 ? 24 : 40;
        d.ring(cc, rr, 3, b.S(FAR_SIL, Z_STAR + 1, base + Groups{g}), teeth * 2);
        for (int t = 0; t < teeth; t++) {
            double a = 2 * PI * t / teeth;
            V u{std::cos(a), std::sin(a)}, nrm{-u.y, u.x};
            V b0 = cc + u * rr, tip = cc + u * (rr + 0.45 * U) + nrm * (0.2 * U);
            Pts tri{b0 - nrm * (0.22 * U), tip, b0 + nrm * (0.22 * U)};
            d.polyline(tri, 1.8, b.S(k ? ACCENT : EDGE, Z_STAR + 1, base + Groups{g}), true);
        }
    }
    // big rotating triangles
    int tg = b.c.ga.get();
    for (int k = 0; k < 3; k++) d.polyOutline(cc, (10 + k * 2.5) * U, 3, 90 + k * 40, 2.5, b.S(FAR_SIL, Z_STAR - 1, base + Groups{tg}));
    // corridor diamonds near the ground
    for (double x = b.L; x < b.R; x += 2 * U) d.polyOutline({x, b.gy + 1.2 * U}, 0.9 * U, 4, 0, 2, b.S(MID_SIL, Z_MID, {b.gMid}));
    if (b.c.anim(1)) {
        b.c.a.sequence(rings, bt / 2, 1.0, 0.15);
        b.c.a.spin(saws[0], ctr, 6, true);
        b.c.a.spin(saws[1], ctr, 10, false);
        b.c.a.spin(tg, ctr, 30, true);
        b.c.a.spin(ray, ctr, 45, false);
        auto const& s2 = b.c.pal.roles[SKY2];
        b.c.a.pulseLoop(b.c.ch(SKY2), mix(s2.c, b.c.pal.roles[ACCENT].c, 0.35), bt * 2, 0.02, bt * 1.2);
    }
    b.motes(8, 2);
}

void bgCyber(BG& b) {
    auto& d = b.d();
    b.sky();
    b.stars(10 * b.c.s.detail, 12, 23);
    double bt = beatOf(b);
    // far megastructure skyline
    int blink = b.c.ga.get(), win = b.c.ga.get();
    double x = b.L;
    while (x < b.R) {
        double bw = b.r().f(1.2, 3) * U, bh = b.r().f(3, 11) * U;
        double y0 = b.gy - U;
        d.rect(x + bw / 2, y0 + bh / 2, bw, bh, 0, b.S(FAR_SIL, Z_FAR, {b.gFar}));
        // stepped crown
        d.rect(x + bw / 2, y0 + bh + 0.25 * U, bw * 0.7, 0.5 * U, 0, b.S(FAR_SIL, Z_FAR, {b.gFar}));
        d.line({x + bw / 2, y0 + bh + 0.5 * U}, {x + bw / 2, y0 + bh + b.r().f(0.8, 2) * U}, 1.4, b.S(FAR_SIL, Z_FAR, {b.gFar}));
        d.diamond({x + bw / 2, y0 + bh + 2.1 * U}, 4, b.S(ACCENT, Z_FAR + 2, {b.gFar, blink}));
        d.line({x, y0 + bh}, {x + bw, y0 + bh}, 1.3, b.S(GLOW, Z_FAR + 1, {b.gFar}));
        if (b.c.s.detail >= 2)
            for (double wy = y0 + 0.8 * U; wy < y0 + bh - 0.5 * U; wy += 0.55 * U)
                if (b.r().chance(0.55)) d.dashes({x + 4, wy}, {x + bw - 4, wy}, 4, 3, 1.4, b.S(ENERGY, Z_FAR + 1, {b.gFar, win}));
        x += bw + b.r().f(0.1, 0.5) * U;
    }
    // holographic rings over the city
    for (int k = 0; k < 2; k++) {
        int hg = b.c.ga.get();
        V hc{b.r().f(b.x0, b.x1), b.gy + b.r().f(11, 15) * U};
        double rr = b.r().f(2, 3.5) * U;
        for (int j = 0; j < 3; j++) d.polyline(circlePts(hc, rr * (1 - j * 0.15), rr * 0.25 * (1 - j * 0.15), 40), j ? 1.2 : 2, b.S(j ? ENERGY : EDGE, Z_FAR + 3, {b.gCel, hg}), true);
        d.glow(hc, rr * 0.8, b.S(GLOW_SOFT, Z_FAR + 2, {b.gCel, hg}), 2);
        if (b.c.anim(1)) b.c.a.flicker(hg, 0.35, 1, bt * 4, k * bt * 2);
    }
    // mid layer silhouettes with light bands
    auto mid = b.ridge(1.6, 1.0, 0.5 * U, 1.3, 3);
    b.hills(mid, MID_SIL, Z_MID, {b.gMid}, GLOW, 1.6);
    // hyperspeed streaks
    if (b.c.anim(2)) {
        for (int k = 0; k < 4; k++) {
            int sg = b.c.ga.get();
            for (int j = 0; j < 4; j++) {
                V p{b.r().f(b.x0, b.x1), b.gy + b.r().f(6, 17) * U};
                double len = b.r().f(1, 3) * U;
                d.rect(p.x, p.y, len, 1.5, 0, b.S(GLOW, Z_STAR + 2, {b.gCel, sg}));
            }
            b.c.a.loop({{{Anim::move(sg, -14 * U, 0, 0.8), Anim::alpha(sg, 1, 0.1)}, 0.6},
                        {{Anim::alpha(sg, 0, 0.2)}, 0.2},
                        {{Anim::move(sg, 14 * U, 0, 0)}, 0.05}}, k * 0.35);
        }
    }
    // searchlights
    if (b.c.anim(1)) {
        for (int k = 0; k < 3; k++) {
            int lg = b.c.ga.get(), cgid = b.c.ga.get();
            V base{b.r().f(b.x0, b.x1), b.gy};
            d.rect(base.x, base.y, 1.2, 1.2, 0, b.S(NEAR_SIL, Z_MID, {cgid}));
            double len = 14 * U;
            d.rect(base.x, base.y + len / 2, 0.9 * U, len, 0, b.S(GLOW_SOFT, Z_FAR - 1, {lg}));
            b.c.a.swing(lg, cgid, b.r().f(18, 30), b.r().f(6, 10), k * 1.5);
        }
        b.c.a.flicker(blink, 0.1, 1, bt * 2);
        b.c.a.flicker(win, 0.6, 1, bt * 4);
    }
    b.rubble();
    b.motes(6, 1.5);
}


// Hyperspace tunnel: nested frames rushing outward in sequence, radial light
// streaks, a blazing vanishing point.
void bgWarp(BG& b) {
    auto& d = b.d();
    b.sky();
    double bt = beatOf(b);
    V vp{b.x0 + (b.x1 - b.x0) * b.r().f(0.4, 0.6), b.gy + b.r().f(8.5, 10.5) * U};
    Groups base{b.gCel};
    int ctr = b.c.ga.get();
    d.rect(vp.x, vp.y, 1.2, 1.2, 0, b.S(NEAR_SIL, Z_SKY + 2, base + Groups{ctr}));
    // radial streaks
    std::vector<int> streaks{b.c.ga.get(), b.c.ga.get(), b.c.ga.get(), b.c.ga.get()};
    for (int k = 0; k < 56; k++) {
        double a = 2 * PI * k / 56 + b.r().f(-0.03, 0.03);
        V u{std::cos(a), std::sin(a)};
        double r0 = b.r().f(1.5, 6) * U, r1 = r0 + b.r().f(2, 7) * U;
        d.line(vp + u * r0, vp + u * r1, b.r().f(1.2, 3.2), b.S(k % 5 == 0 ? EDGE : GLOW, Z_STAR, base + Groups{streaks[k % 4]}));
    }
    // nested frames, alternating squares and hexagons, lit outward in sequence
    std::vector<int> frames;
    int n = 12 + b.c.s.detail * 2;
    bool hexes = b.r().chance(0.5);
    for (int k = 0; k < n; k++) {
        int g = b.c.ga.get();
        frames.push_back(g);
        double rr = 0.8 * U * std::pow(1.28, k);
        if (rr > 14 * U) { b.c.ga.peek(); frames.pop_back(); break; }
        int sides = hexes ? 6 : 4;
        double rot = hexes ? 30 : 45;
        d.polyOutline(vp, rr, sides, rot, k % 3 == 0 ? 3.2 : 1.8, b.S(k % 2 ? ACCENT : EDGE, Z_STAR + 1, base + Groups{g}));
        if (k % 3 == 0 && b.c.s.detail >= 2) d.polyOutline(vp, rr * 1.05, sides, rot, 1, b.S(GLOW, Z_STAR + 1, base + Groups{g}));
    }
    // core
    d.glow(vp, 5 * U, b.S(GLOW_SOFT, Z_CEL - 2, base), 5);
    d.glow(vp, 1.6 * U, b.S(GLOW, Z_CEL - 1, base), 3);
    d.circle(vp, 0.35 * U, b.S(CELESTIAL, Z_CEL, base), 3);
    // floor & ceiling perspective rails
    for (int k = -6; k <= 6; k++) {
        double xb = vp.x + k * 3.5 * U;
        d.line({xb, b.gy - U}, vp + V{(xb - vp.x) * 0.08, -0.6 * U}, 1.2, b.S(FAR_SIL, Z_FAR, {b.gFar}));
    }
    if (b.c.anim(1)) {
        b.c.a.sequence(frames, bt / 4, 1.0, 0.12);
        b.c.a.sequence(streaks, bt / 2, 1.0, 0.25);
        b.c.a.spin(frames[0], ctr, 8, true);
    }
    b.motes(10, 3);
}

using BgFn = void (*)(BG&);
std::map<std::string, BgFn> const& bgTable() {
    static std::map<std::string, BgFn> t{{"night", bgNight}, {"synthwave", bgSynthwave}, {"nebula", bgNebula}, {"aurora", bgAurora},
                                         {"abyss", bgAbyss}, {"geometric", bgGeometric}, {"sunset", bgSunset}, {"clouds", bgClouds},
                                         {"space", bgSpace}, {"planets", bgPlanets}, {"blackhole", bgBlackHole}, {"eclipse", bgEclipse},
                                         {"circles", bgCircles}, {"cyber", bgCyber}, {"warp", bgWarp}};
    return t;
}

} // namespace

std::vector<std::string> const& backgroundStyles() {
    static std::vector<std::string> v{"random", "space", "planets", "blackhole", "eclipse", "warp", "circles", "cyber", "night", "synthwave", "nebula", "aurora", "abyss", "geometric", "sunset", "clouds"};
    return v;
}

std::string resolveBgStyle(std::string const& style, uint32_t seed) {
    if (bgTable().count(style)) return style;
    Rng r(seed ^ 0xB6B6B6u);
    auto const& v = backgroundStyles();
    return v[r.i(1, (int)v.size() - 1)];
}

void adjustPaletteForBackground(Palette& p, std::string const& style, uint32_t seed) {
    Rng r(seed ^ 0xC0FFEEu);
    auto set = [&](Role role, Color3 c, double op = 1, bool blend = false) { p.roles[role] = {c, op, blend}; };
    double h = r.f(0, 360);
    if (style == "sunset") {
        double base = r.f(250, 290);
        set(SKY1, hsv(base, 0.6, 0.18));
        set(SKY2, hsv(base + 50, 0.6, 0.38));
        set(SKY3, hsv(r.f(340, 360), 0.65, 0.62));
        set(SKY4, hsv(r.f(20, 35), 0.7, 0.92));
        set(CELESTIAL, hsv(45, 0.35, 1.0), 1, true);
        set(FAR_SIL, hsv(base + 30, 0.45, 0.32));
        set(MID_SIL, hsv(base + 20, 0.5, 0.18));
        set(NEAR_SIL, hsv(base, 0.5, 0.08));
        set(FOG, hsv(20, 0.5, 0.9), 0.06, true);
        p.bg = hsv(base, 0.6, 0.18);
    } else if (style == "clouds") {
        double base = r.f(195, 215);
        set(SKY1, hsv(base, 0.25, 0.95));
        set(SKY2, hsv(base, 0.35, 0.9));
        set(SKY3, hsv(base, 0.45, 0.82));
        set(SKY4, hsv(base, 0.55, 0.72));
        set(CELESTIAL, hsv(50, 0.25, 1.0), 0.9, true);
        set(FOG, hsv(base, 0.05, 1.0), 0.85, false);
        set(FAR_SIL, hsv(base, 0.25, 0.62));
        set(MID_SIL, hsv(base - 60, 0.35, 0.42));
        set(NEAR_SIL, hsv(base - 70, 0.4, 0.22));
        p.bg = hsv(base, 0.55, 0.72);
    } else if (style == "synthwave") {
        set(SKY1, hsv(265, 0.7, 0.07));
        set(SKY2, hsv(285, 0.7, 0.16));
        set(SKY3, hsv(310, 0.7, 0.28));
        set(SKY4, hsv(330, 0.6, 0.42));
        set(CELESTIAL, hsv(48, 0.75, 1.0));
        set(ACCENT, hsv(325, 0.85, 1.0));
        set(ENERGY, hsv(r.chance(0.5) ? 185 : 320, 0.75, 1.0), 0.8, true);
        set(FAR_SIL, hsv(270, 0.6, 0.12));
        set(NEAR_SIL, hsv(265, 0.6, 0.05));
        p.bg = hsv(265, 0.7, 0.07);
    } else if (style == "abyss") {
        double base = r.f(190, 220);
        set(SKY1, hsv(base, 0.8, 0.04));
        set(SKY2, hsv(base, 0.8, 0.09));
        set(SKY3, hsv(base, 0.75, 0.16));
        set(SKY4, hsv(base - 10, 0.7, 0.28));
        set(GLOW_SOFT, hsv(base - 20, 0.4, 1.0), 0.07, true);
        set(FAR_SIL, hsv(base, 0.7, 0.1));
        set(MID_SIL, hsv(base, 0.7, 0.06));
        set(NEAR_SIL, hsv(base, 0.8, 0.03));
        p.bg = hsv(base, 0.8, 0.04);
    } else if (style == "aurora") {
        double ah = r.chance(0.6) ? r.f(130, 170) : r.f(270, 300);
        set(ENERGY, hsv(ah, 0.6, 1.0), 0.8, true);
        set(GLOW_SOFT, hsv(ah + 15, 0.7, 1.0), 0.06, true);
    } else if (style == "space" || style == "planets" || style == "blackhole" || style == "eclipse") {
        double base = r.f(225, 270);
        set(SKY1, hsv(base, 0.7, 0.03));
        set(SKY2, hsv(base, 0.7, 0.06));
        set(SKY3, hsv(base + 15, 0.65, 0.09));
        set(SKY4, mix(hsv(base + 25, 0.6, 0.14), p.roles[ACCENT].c, 0.12));
        set(GLOW_SOFT, p.roles[GLOW_SOFT].c, 0.05, true);
        set(FOG, hsv(base + r.f(40, 100), 0.6, 1.0), 0.03, true);
        p.bg = hsv(base, 0.7, 0.03);
    } else if (style == "warp") {
        double base = r.f(220, 280);
        set(SKY1, hsv(base, 0.8, 0.03));
        set(SKY2, hsv(base, 0.75, 0.05));
        set(SKY3, hsv(base + 10, 0.7, 0.08));
        set(SKY4, mix(hsv(base + 20, 0.6, 0.12), p.roles[ACCENT].c, 0.15));
        set(GLOW_SOFT, p.roles[GLOW_SOFT].c, 0.06, true);
        p.bg = hsv(base, 0.8, 0.03);
    } else if (style == "circles") {
        Color3 acc = p.roles[ACCENT].c;
        set(SKY1, mix({4, 4, 6}, acc, 0.04));
        set(SKY2, mix({6, 6, 8}, acc, 0.08));
        set(SKY3, mix({8, 8, 10}, acc, 0.12));
        set(SKY4, mix({10, 10, 12}, acc, 0.2));
        set(GLOW_SOFT, acc, 0.05, true);
        p.bg = {4, 4, 6};
    } else if (style == "cyber") {
        double base = r.f(215, 250);
        set(SKY1, hsv(base, 0.8, 0.04));
        set(SKY2, hsv(base, 0.75, 0.08));
        set(SKY3, hsv(base + 10, 0.7, 0.13));
        set(SKY4, mix(hsv(base + 20, 0.6, 0.2), p.roles[ACCENT].c, 0.3));
        set(FAR_SIL, hsv(base, 0.6, 0.1));
        set(MID_SIL, hsv(base, 0.6, 0.06));
        set(NEAR_SIL, hsv(base, 0.6, 0.03));
        p.bg = hsv(base, 0.8, 0.04);
    } else if (style == "nebula") {
        set(GLOW_SOFT, hsv(h, 0.6, 1.0), 0.035, true);
        set(FOG, hsv(h + r.f(60, 140), 0.6, 1.0), 0.03, true);
    }
}

void genBackground(Ctx& c, std::string const& style, double x0, double x1, double baseY, double trigX) {
    auto& a = c.a;
    auto const& pal = c.pal;
    if (c.s.setLevelColors) {
        a.colorCycle(gd::ch::BG, pal.bg, mix(pal.bg, pal.roles[SKY2].c, 0.5), c.rng.f(5, 8));
        a.atStart(Anim::color(gd::ch::GROUND, pal.roles[NEAR_SIL].c, 0));
        a.atStart(Anim::color(gd::ch::GROUND2, mix(pal.roles[NEAR_SIL].c, pal.bg, 0.4), 0));
        a.atStart(Anim::color(gd::ch::LINE, pal.line, 0, 1, true));
    }
    bool parallax = c.s.parallax && c.anim(1);
    double pTrig = x0 - 18 * U;
    double L = (parallax ? pTrig - 8 * U : x0 - 12 * U), R = x1 + 14 * U;
    BG b{c, L, R, R - L, (L + R) / 2, baseY, x0, x1,
         c.ga.get(), c.ga.get(), c.ga.get(), c.ga.get(), c.ga.get()};
    auto const& t = bgTable();
    auto it = t.find(style);
    (it != t.end() ? it->second : bgNight)(b);

    if (parallax) {
        double dur = std::max(5.0, (x1 + 15 * U - pTrig) / PLAYER_SPEED_1X);
        double ty = baseY + 18 * U;
        a.atX(pTrig, ty, Anim::parallax(b.gSky, 1.0, dur));
        a.atX(pTrig, ty + U, Anim::parallax(b.gCel, 0.96, dur));
        a.atX(pTrig, ty + 2 * U, Anim::parallax(b.gFar, 0.82, dur));
        a.atX(pTrig, ty + 3 * U, Anim::parallax(b.gMid, 0.6, dur));
    }
    (void)trigX;
}

} // namespace inspo
