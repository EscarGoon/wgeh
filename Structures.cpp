// Structure generators. Everything is built from colour-channel solids, so the
// look comes from layering: dark fill -> lit band -> highlight sliver -> edge
// lines -> emissive details (windows, runes, cores) -> blended glow.

#include "Ctx.hpp"
#include "Greebles.hpp"

#include <algorithm>
#include <functional>

namespace inspo {

namespace z {
    constexpr int BACK = 4, FILL = 10, SHADE = 11, LIT = 12, DETAIL = 13, TRIM = 14,
                  EDGE = 15, WIN = 17, GLOW = 18, TOP = 19, FX = 21;
}

static double deg(double rad) { return rad * 180.0 / PI; }
static V dir(double a) { return {std::cos(a), std::sin(a)}; }

static double beatLen(Ctx& c) { return 60.0 / std::clamp(c.s.bpm, 40.0, 400.0); }

static GCtx gctxOf(Ctx& c, std::vector<int> seq = {}) {
    GCtx g{c.d, c.rng, c.pal, c.a, c.ga, c.s.detail, c.s.motion, beatLen(c), {}, std::move(seq), 2, {}};
    return g;
}

// Fills a structure face with machined panels and modules (root groups applied via Draw style groups is not
// possible here, so faces are drawn into a temporary group list on `root`).
static void facePanels(Ctx& c, GCtx& g, Box b, Look look, Groups const& root, int z) {
    g.base = root;
    if (b.w() < 14 || b.h() < 14) return;
    for (auto const& p : subdivide(c.rng, b, 0.5 * U, 1.6 * U, 0.45 * U, 1.2 * U, 2.5)) {
        panelPlate(g, p, look, z);
        if (c.rng.chance(0.85)) placeModule(g, p, look, z + 4);
    }
}


// ------------------------------------------------------------------ shared
static void pillar(Ctx& c, double x, double y0, double h, double w, Groups const& g) {
    auto& d = c.d;
    d.rect(x, y0 + 0.18 * U, w + 0.4 * U, 0.36 * U, 0, c.S(FILL_LIGHT, z::DETAIL, g));
    d.line({x - (w + 0.4 * U) / 2, y0 + 0.36 * U}, {x + (w + 0.4 * U) / 2, y0 + 0.36 * U}, 1.4, c.S(EDGE, z::EDGE, g));
    double sy = y0 + 0.36 * U, sh = h - 0.72 * U;
    d.rect(x, sy + sh / 2, w, sh, 0, c.S(FILL_DARK, z::FILL, g));
    d.rect(x - w * 0.22, sy + sh / 2, w * 0.36, sh, 0, c.S(FILL_MID, z::SHADE, g));
    d.rect(x - w * 0.44, sy + sh / 2, w * 0.1, sh, 0, c.S(FILL_LIGHT, z::LIT, g));
    if (c.s.detail >= 2) {
        d.line({x + w * 0.12, sy + 2}, {x + w * 0.12, sy + sh - 2}, 1.2, c.S(DETAIL, z::DETAIL, g));
        d.line({x + w * 0.3, sy + 2}, {x + w * 0.3, sy + sh - 2}, 1.2, c.S(NEAR_SIL, z::DETAIL, g));
    }
    d.line({x - w / 2, sy}, {x - w / 2, sy + sh}, 1.5, c.S(EDGE, z::EDGE, g));
    double cy = sy + sh;
    d.rect(x, cy + 0.18 * U, w + 0.45 * U, 0.36 * U, 0, c.S(FILL_LIGHT, z::DETAIL, g));
    d.line({x - (w + 0.45 * U) / 2, cy + 0.36 * U}, {x + (w + 0.45 * U) / 2, cy + 0.36 * U}, 1.4, c.S(EDGE, z::EDGE, g));
    d.spike({x - w / 2 - 0.05 * U, cy - 0.12 * U}, 180, 0.28, c.S(FILL_LIGHT, z::DETAIL, g), true);
    d.spike({x + w / 2 + 0.05 * U, cy - 0.12 * U}, 180, 0.28, c.S(FILL_LIGHT, z::DETAIL, g), true);
}

static void crystal(Ctx& c, V base, double L, double w, double angCCW, int zz, Groups const& g, bool litLeft) {
    auto& d = c.d;
    auto T = [&](Pts p) { return translated(rotated(p, angCCW, {0, 0}), base); };
    Pts left{{-w / 2, 0}, {-w / 2, 0.72 * L}, {0, L}, {0, 0}};
    Pts right{{0, 0}, {0, L}, {w / 2, 0.72 * L}, {w / 2, 0}};
    Pts full{{-w / 2, 0}, {-w / 2, 0.72 * L}, {0, L}, {w / 2, 0.72 * L}, {w / 2, 0}};
    double strip = std::min(c.strip(), std::max(2.5, w / 4));
    d.fill(T(left), c.S(litLeft ? FILL_LIGHT : FILL_MID, zz, g), strip);
    d.fill(T(right), c.S(litLeft ? FILL_MID : FILL_DARK, zz, g), strip);
    auto core = T({{0, 0.1 * L}, {0, 0.86 * L}});
    d.line(core[0], core[1], std::max(1.5, w * 0.14), c.S(ENERGY, zz + 2, g));
    auto hl = T({{-w / 4, 0.15 * L}, {-w / 4, 0.6 * L}});
    d.line(hl[0], hl[1], 1.2, c.S(EDGE, zz + 3, g));
    d.polyline(T(full), 1.6, c.S(EDGE, zz + 4, g), true);
}

static void gear(Ctx& c, V p, double r, int teeth, Groups const& g, int zz) {
    auto& d = c.d;
    for (int i = 0; i < teeth; i++) {
        double a = 2 * PI * i / teeth;
        V q = p + dir(a) * (r * 0.92);
        d.rect(q.x, q.y, r * 0.3, r * 0.24, -deg(a), c.S(FILL_MID, zz, g));
    }
    d.circle(p, r * 0.82, c.S(FILL_MID, zz + 1, g), c.strip());
    d.ring(p, r * 0.82, 1.5, c.S(EDGE, zz + 3, g), std::max(16, teeth * 2));
    d.circle(p, r * 0.56, c.S(FILL_DARK, zz + 2, g), c.strip());
    for (int k = 0; k < 5; k++) {
        double a = 2 * PI * k / 5 + 0.3;
        d.line(p, p + dir(a) * (r * 0.56), r * 0.12, c.S(FILL_MID, zz + 3, g));
    }
    d.ring(p, r * 0.56, 1.2, c.S(DETAIL, zz + 4, g), 18);
    d.circle(p, r * 0.17, c.S(ACCENT, zz + 5, g), 3);
}

// ----------------------------------------------------------------- citadel
void genCitadel(Ctx& c, double cx, double baseY, double H, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    int nT = r.i(3, 4);
    double Hb = H * 0.68, Hs = H * 0.32;
    double w0 = H * r.f(0.40, 0.50);
    std::vector<double> hs(nT);
    double sum = 0;
    for (auto& h : hs) { h = r.f(0.8, 1.25); sum += h; }
    for (auto& h : hs) h = h / sum * Hb;
    std::sort(hs.begin(), hs.end(), std::greater<>());

    Groups wg[3];
    for (auto& g : wg) g = root + Groups{c.ga.get()};

    // foundation
    double fw = w0 + 2.4 * U;
    d.rect(cx, baseY + 0.22 * U, fw, 0.44 * U, 0, c.S(FILL_MID, z::LIT, root));
    d.line({cx - fw / 2, baseY + 0.44 * U}, {cx + fw / 2, baseY + 0.44 * U}, 1.5, c.S(EDGE, z::EDGE, root));
    // buttresses
    for (int side : {-1, 1}) {
        double ex = cx + side * w0 / 2;
        Pts tri{{ex, baseY + 0.44 * U}, {ex + side * 1.1 * U, baseY + 0.44 * U}, {ex, baseY + hs[0] * 0.75}};
        d.fill(tri, c.S(side < 0 ? FILL_MID : FILL_DARK, z::FILL, root), c.strip());
        if (side < 0) d.line(tri[1], tri[2], 1.5, c.S(EDGE, z::EDGE, root));
    }

    double y = baseY + 0.44 * U;
    double wi = w0;
    for (int i = 0; i < nT; i++) {
        wi = w0 * (1 - 0.17 * i);
        double hi = hs[i];
        double L = cx - wi / 2, R = cx + wi / 2, my = y + hi / 2;
        d.rect(cx, my, wi, hi, 0, c.S(FILL_DARK, z::FILL, root));
        d.rect(L + wi * 0.2, my, wi * 0.4, hi, 0, c.S(FILL_MID, z::SHADE, root));
        d.rect(L + wi * 0.04, my, wi * 0.08, hi, 0, c.S(FILL_LIGHT, z::LIT, root));
        d.rect(R - wi * 0.05, my, wi * 0.1, hi, 0, c.S(NEAR_SIL, z::SHADE, root));
        d.line({L, y}, {L, y + hi}, 1.6, c.S(EDGE, z::EDGE, root));

        // windows
        int cols = std::max(1, (int)((wi - 0.8 * U) / (0.85 * U)));
        int rows = std::max(1, (int)((hi - 0.9 * U) / (1.25 * U)));
        double colW = (wi - 0.8 * U) / cols, rowH = (hi - 0.6 * U) / rows;
        for (int ri = 0; ri < rows; ri++) {
            for (int ci = 0; ci < cols; ci++) {
                double wx = L + 0.4 * U + (ci + 0.5) * colW, wy = y + 0.3 * U + (ri + 0.5) * rowH;
                bool lit = r.chance(0.65);
                auto const& g = wg[r.i(0, 2)];
                d.rect(wx, wy, 0.48 * U, 0.8 * U, 0, c.S(NEAR_SIL, z::DETAIL, root));
                Style pane = lit ? c.S(WINDOW, z::WIN, g) : c.S(FILL_DARK, z::TRIM, root);
                d.rect(wx, wy - 0.07 * U, 0.3 * U, 0.52 * U, 0, pane);
                d.diamond({wx, wy + 0.19 * U}, 0.21 * U, pane);
                if (lit && c.s.detail >= 2 && r.chance(0.35)) d.glow({wx, wy}, 0.7 * U, c.S(GLOW_SOFT, z::WIN - 1, g), 1);
            }
        }

        // ledge + corbels
        double lw = wi + 0.7 * U, ly = y + hi;
        d.rect(cx, ly - 0.22 * U, wi, 0.14 * U, 0, c.S(NEAR_SIL, z::LIT, root));
        d.rect(cx, ly, lw, 0.32 * U, 0, c.S(FILL_LIGHT, z::DETAIL, root));
        d.line({cx - lw / 2, ly + 0.16 * U}, {cx + lw / 2, ly + 0.16 * U}, 1.4, c.S(EDGE, z::EDGE, root));
        for (double x = cx - lw / 2 + 0.3 * U; x < cx + lw / 2 - 0.2 * U; x += 0.75 * U)
            d.spike({x, ly - 0.3 * U}, 180, 0.24, c.S(FILL_LIGHT, z::DETAIL, root), true);

        // crenellations
        if (i == nT - 1 || r.chance(0.5)) {
            int n = (int)(lw / (0.55 * U));
            for (int k = 0; k < n; k += 2) {
                double mx = cx - lw / 2 + (k + 0.5) * lw / n;
                d.rect(mx, ly + 0.36 * U, 0.3 * U, 0.42 * U, 0, c.S(FILL_MID, z::LIT, root));
                d.line({mx - 0.15 * U, ly + 0.57 * U}, {mx + 0.15 * U, ly + 0.57 * U}, 1.2, c.S(EDGE, z::EDGE, root));
            }
        }
        // banners
        if (i == 1 && r.chance(0.75)) {
            for (int side : {-1, 1}) {
                double bx = cx + side * (wi / 2 - 0.45 * U);
                d.rect(bx, ly - 0.85 * U, 0.36 * U, 1.2 * U, 0, c.S(ACCENT, z::WIN, root));
                d.rect(bx, ly - 0.85 * U, 0.1 * U, 1.0 * U, 0, c.S(EDGE, z::WIN + 1, root));
                d.spike({bx, ly - 1.62 * U}, 180, 0.36, c.S(ACCENT, z::WIN, root), true);
            }
        }
        y = ly + 0.16 * U;
    }

    // spire
    double b = wi * 0.62, top = y + Hs;
    Pts tri{{cx - b / 2, y}, {cx, top}, {cx + b / 2, y}};
    d.fill(tri, c.S(FILL_DARK, z::FILL, root), c.strip());
    d.fill({{cx - b / 2, y}, {cx, top}, {cx, y}}, c.S(FILL_MID, z::SHADE, root), c.strip());
    d.line(tri[0], tri[1], 1.6, c.S(EDGE, z::EDGE, root));
    d.line({cx, y}, {cx, top}, 1.2, c.S(DETAIL, z::DETAIL, root));
    for (double t : {0.25, 0.5, 0.72}) {
        double yy = y + Hs * t, half = b / 2 * (1 - t);
        d.line({cx - half, yy}, {cx + half, yy}, 1.6, c.S(ACCENT, z::TRIM, root));
    }
    for (int side : {-1, 1}) {
        double tx = cx + side * (wi / 2 - 0.25 * U);
        d.rect(tx, y + 0.45 * U, 0.45 * U, 0.9 * U, 0, c.S(FILL_DARK, z::FILL, root));
        d.line({tx - 0.225 * U, y}, {tx - 0.225 * U, y + 0.9 * U}, 1.3, c.S(EDGE, z::EDGE, root));
        Pts cone{{tx - 0.32 * U, y + 0.9 * U}, {tx, y + 2.0 * U}, {tx + 0.32 * U, y + 0.9 * U}};
        d.fill(cone, c.S(FILL_MID, z::SHADE, root), 3);
        d.line(cone[0], cone[1], 1.3, c.S(EDGE, z::EDGE, root));
        d.diamond({tx, y + 2.1 * U}, 0.14 * U, c.S(ACCENT, z::TOP, root));
    }

    // tip orb + halo
    V tip{cx, top + 0.35 * U};
    d.line({cx, top - 0.1 * U}, {cx, top + 0.9 * U}, 1.4, c.S(EDGE, z::EDGE, root));
    d.circle(tip, 0.2 * U, c.S(ACCENT, z::TOP, root), 3);
    d.glow(tip, 0.7 * U, c.S(GLOW, z::GLOW, root), 2);
    int hg = c.ga.get();
    int ctr = c.center(tip, root);
    for (int k = 0; k < 8; k++) {
        double a = 2 * PI * k / 8;
        d.diamond(tip + dir(a) * (0.75 * U), 0.14 * U, c.S(ENERGY, z::FX, root + Groups{hg}));
    }
    if (c.anim(1)) c.a.spin(hg, ctr, r.f(5, 8), true);
    if (c.anim(2))
        for (auto const& g : wg) c.a.flicker(g.back(), 0.35, 1.0, r.f(1.4, 3.2), r.f(0, 1));
}

// ---------------------------------------------------------------- crystals
void genCrystals(Ctx& c, double cx, double baseY, double H, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    double W = H * 0.95, mh = H * 0.12;

    d.glow({cx, baseY + H * 0.35}, W * 0.45, c.S(GLOW_SOFT, z::BACK, root), 3);

    Pts mound{{cx - W / 2, baseY}};
    int m = 10;
    for (int i = 1; i < m; i++) {
        double t = (double)i / m;
        mound.push_back({cx - W / 2 + W * t, baseY + std::sin(PI * t) * mh * r.f(0.6, 1.15)});
    }
    mound.push_back({cx + W / 2, baseY});
    d.fill(mound, c.S(FILL_DARK, z::FILL + 6, root), c.strip());

    int k = r.i(5, 7);
    std::vector<int> order(k);
    for (int i = 0; i < k; i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return std::fabs(2.0 * a / (k - 1) - 1) > std::fabs(2.0 * b / (k - 1) - 1);
    });
    double spread = r.f(32, 46);
    int zz = z::FILL - 4;
    for (int i : order) {
        double t = 2.0 * i / (k - 1) - 1;
        double ang = -(t * spread + r.f(-5, 5));
        double L = H * (1 - 0.45 * std::fabs(t)) * r.f(0.82, 1.05);
        double w = L * r.f(0.2, 0.27);
        double bx = cx + t * W * 0.28;
        double by = baseY + std::sin(PI * (0.5 + t * 0.28)) * mh * 0.6;
        crystal(c, {bx, by}, L, w, ang, zz, root, t <= 0.15);
        zz += 1;
    }
    d.polyline(mound, 1.4, c.S(DETAIL, z::FILL + 7, root));

    // orbiting shards
    V ctrP{cx, baseY + H * 0.55};
    int sg = c.ga.get();
    int ctr = c.center(ctrP, root);
    int n = r.i(6, 9);
    for (int i = 0; i < n; i++) {
        double a = 2 * PI * i / n + r.f(-0.2, 0.2);
        double rad = H * r.f(0.6, 0.72);
        crystal(c, ctrP + dir(a) * rad, r.f(0.5, 0.8) * U, 0.24 * U, deg(a) - 90, z::FX - 4, root + Groups{sg}, true);
    }
    if (c.anim(1)) c.a.spin(sg, ctr, r.f(14, 20), r.chance(0.5));

    if (c.s.detail >= 2) {
        int sp = c.ga.get(), sp2 = c.ga.get();
        for (int i = 0; i < 10; i++) {
            V p{cx + r.f(-W * 0.45, W * 0.45), baseY + r.f(0.2, 1.0) * H};
            d.diamond(p, r.f(0.08, 0.16) * U, c.S(ENERGY, z::FX, root + Groups{i % 2 ? sp : sp2}));
        }
        if (c.anim(2)) {
            c.a.flicker(sp, 0.1, 1, r.f(1, 1.8), 0);
            c.a.flicker(sp2, 0.1, 1, r.f(1, 1.8), 0.6);
        }
    }
}

// ------------------------------------------------------------------ island
void genIsland(Ctx& c, double cx, double topY, double W, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    int m = 14;
    Pts top;
    for (int i = 0; i <= m; i++) {
        double jitter = (i == 0 || i == m) ? 0 : r.f(-0.16, 0.16) * U;
        top.push_back({cx - W / 2 + W * i / m, topY + jitter});
    }
    double D = W * r.f(0.55, 0.72);
    Pts bottom;
    for (int i = m - 1; i >= 1; i--) {
        double t = (double)i / m;
        double depth = D * std::pow(1 - std::fabs(2 * t - 1), 0.85) + r.f(-0.3, 0.3) * U;
        if (r.chance(0.22)) depth += r.f(0.4, 1.0) * U;
        bottom.push_back({cx - W / 2 + W * t + r.f(-0.15, 0.15) * U, topY - std::max(0.5 * U, depth)});
    }
    Pts poly = top;
    poly.insert(poly.end(), bottom.begin(), bottom.end());

    double ph = r.f(0, 6);
    int dark = c.ch(FILL_DARK), mid = c.ch(FILL_MID), sil = c.ch(NEAR_SIL);
    d.fill(poly, c.S(FILL_DARK, z::FILL, root), c.strip(), [&](double y) {
        int band = (int)std::floor((topY - y) / (0.5 * U) + 0.8 * std::sin(y * 0.06 + ph));
        int k = ((band % 4) + 4) % 4;
        return k == 1 ? mid : k == 3 ? sil : dark;
    });
    // lit left face
    Pts face{{cx - W / 2, topY}, {cx - W * 0.25, topY}, {cx - W * 0.1, topY - D * 0.55}};
    d.fill(face, c.S(FILL_MID, z::SHADE, root), c.strip());
    d.polyline(top, 0.32 * U, c.S(FILL_LIGHT, z::DETAIL, root));
    d.polyline(translated(top, {0, 0.14 * U}), 1.6, c.S(EDGE, z::EDGE, root));
    for (size_t i = 1; i + 1 < top.size(); i++) {
        if (r.chance(0.55))
            d.spike({top[i].x + r.f(-6, 6), top[i].y + 0.25 * U}, r.f(-12, 12), r.f(0.3, 0.55),
                    c.S(r.chance(0.3) ? ACCENT : DETAIL, z::TRIM, root), true);
    }

    // roots with glowing drops
    int dg = c.ga.get();
    for (int k = 0; k < 4 + c.s.detail; k++) {
        V p = bottom[r.i(1, (int)bottom.size() - 2)];
        double l = r.f(0.8, 2.2) * U;
        V a = p + V{r.f(-0.3, 0.3) * U, -l * 0.5}, b = p + V{r.f(-0.4, 0.4) * U, -l};
        d.line(p, a, 1.8, c.S(DETAIL, z::FILL - 1, root));
        d.line(a, b, 1.4, c.S(DETAIL, z::FILL - 1, root));
        d.diamond(b, 0.16 * U, c.S(ENERGY, z::GLOW, root + Groups{dg}));
        if (c.s.detail >= 2) d.glow(b, 0.5 * U, c.S(GLOW_SOFT, z::GLOW - 1, root + Groups{dg}), 1);
    }
    if (c.anim(2)) c.a.flicker(dg, 0.3, 1, r.f(1.5, 2.5));

    // waterfall
    if (c.s.detail >= 2 && r.chance(0.5)) {
        double side = r.sign();
        double wx = cx + side * W * 0.32;
        double y0 = topY + 0.05 * U, y1 = topY - D - 2.5 * U;
        d.line({wx, y0}, {wx, y1}, 0.45 * U, c.S(GLOW_SOFT, z::FILL + 8, root));
        d.line({wx - 0.1 * U, y0}, {wx - 0.1 * U, y1}, 1.6, c.S(ENERGY, z::FILL + 9, root));
        d.line({wx + 0.12 * U, y0}, {wx + 0.12 * U, y1 + 0.6 * U}, 1.2, c.S(ENERGY, z::FILL + 9, root));
        d.glow({wx, y1}, 0.8 * U, c.S(GLOW_SOFT, z::FILL + 8, root), 2);
        if (c.anim(2)) {
            for (int k = 0; k < 3; k++) {
                int fg = c.ga.get();
                for (int j = 0; j < 2; j++)
                    d.rect(wx + r.f(-0.15, 0.15) * U, y0 - j * 0.3 * U, 2, 0.3 * U, 0, c.S(EDGE, z::FX, root + Groups{fg}));
                c.a.rise(fg, 0, y1 - y0, 1.4, k * 0.45);
            }
        }
    }

    // something on top
    double topH = r.f(0.5, 0.7);
    switch (r.i(0, 2)) {
        case 0: genCrystals(c, cx + r.f(-0.12, 0.12) * W, topY - 3, W * 0.42, root); break;
        case 1: genCitadel(c, cx + r.f(-0.1, 0.1) * W, topY - 3, W * topH * 1.1, root); break;
        default: genTree(c, cx + r.f(-0.12, 0.12) * W, topY - 3, W * topH, root); break;
    }
}

// --------------------------------------------------------------- mech core
void genMechCore(Ctx& c, double cx, double cy, double R, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    V P{cx, cy};
    int ctr = c.center(P, root);

    // pistons (behind)
    for (int k = 0; k < 4; k++) {
        double a = PI / 4 + k * PI / 2;
        V u = dir(a);
        V hp = P + u * (R * 1.2);
        d.rect(hp.x, hp.y, R * 0.42, 0.52 * U, -deg(a), c.S(FILL_DARK, z::BACK + 2, root));
        d.line(P + u * (R * 0.99) + V{-u.y, u.x} * (0.26 * U), P + u * (R * 1.41) + V{-u.y, u.x} * (0.26 * U), 1.4,
               c.S(EDGE, z::BACK + 3, root));
        int pg = c.ga.get();
        Groups gg = root + Groups{pg};
        d.line(P + u * (R * 1.1), P + u * (R * 1.62), 0.2 * U, c.S(DETAIL, z::BACK + 1, gg));
        V tipP = P + u * (R * 1.64);
        d.rect(tipP.x, tipP.y, 0.3 * U, 0.75 * U, -deg(a), c.S(FILL_LIGHT, z::BACK + 3, gg));
        d.diamond(tipP + u * (0.2 * U), 0.14 * U, c.S(ACCENT, z::BACK + 4, gg));
        if (c.anim(1)) c.a.bob(pg, u.x * 0.45 * U, u.y * 0.45 * U, 1.4, k * 0.35, gd::ease::IN_OUT);
    }

    d.glow(P, R * 1.1, c.S(GLOW_SOFT, z::BACK, root), 2);
    d.circle(P, R * 0.78, c.S(FILL_DARK, z::BACK + 5, root), c.strip());
    d.ring(P, R * 0.78, 1.6, c.S(EDGE, z::BACK + 6, root), 32);
    {
        GCtx g = gctxOf(c);
        g.base = root;
        mHexPatch(g, Box{cx - R * 0.5, cy - R * 0.5, cx + R * 0.5, cy + R * 0.5}, z::BACK + 6);
    }
    for (int k = 0; k < 8; k++) d.diamond(P + dir(2 * PI * k / 8) * (R * 0.7), 0.12 * U, c.S(DETAIL, z::BACK + 6, root));

    // tick ring
    int ig = c.ga.get();
    for (int k = 0; k < 24; k++) {
        V u = dir(2 * PI * k / 24);
        d.line(P + u * (R * 0.6), P + u * (R * (k % 3 == 0 ? 0.7 : 0.66)), 1.4, c.S(DETAIL, z::BACK + 7, root + Groups{ig}));
    }

    // segmented outer ring
    int og = c.ga.get();
    int segs = r.i(5, 8);
    for (int k = 0; k < segs; k++) {
        double a0 = 2 * PI * k / segs, a1 = a0 + 2 * PI / segs * 0.72;
        d.ring(P, R, 0.32 * U, c.S(FILL_LIGHT, z::FILL - 1, root + Groups{og}), 6, a0, a1);
        d.ring(P, R + 0.18 * U, 1.3, c.S(EDGE, z::FILL, root + Groups{og}), 6, a0, a1);
        d.diamond(P + dir(a0) * R, 0.2 * U, c.S(ACCENT, z::FILL + 1, root + Groups{og}));
    }

    // gears
    double r1 = R * 0.42;
    int g1 = c.ga.get();
    gear(c, P, r1, 14, root + Groups{g1}, z::FILL + 2);
    double p1 = r.f(7, 10);
    if (c.anim(1)) c.a.spin(g1, ctr, p1, true);
    int nSat = r.i(2, 3);
    double a0 = r.f(0, 2 * PI);
    for (int k = 0; k < nSat; k++) {
        double r2 = R * r.f(0.2, 0.25);
        double a = a0 + 2 * PI * k / nSat;
        V q = P + dir(a) * (r1 * 0.95 + r2 * 0.95);
        int g2 = c.ga.get();
        int c2 = c.center(q, root);
        gear(c, q, r2, std::max(7, (int)(14 * r2 / r1)), root + Groups{g2}, z::FILL + 9);
        if (c.anim(1)) c.a.spin(g2, c2, p1 * r2 / r1, false);
    }

    // core
    d.circle(P, R * 0.1, c.S(ENERGY, z::TOP, root), 3);
    d.glow(P, R * 0.32, c.S(GLOW, z::GLOW + 1, root), 3);

    // orbiters
    int orb = c.ga.get();
    for (int k = 0; k < 3; k++) {
        V q = P + dir(2 * PI * k / 3 + 0.5) * (R * 1.18);
        d.diamond(q, 0.22 * U, c.S(ACCENT, z::FX, root + Groups{orb}));
        d.glow(q, 0.5 * U, c.S(GLOW_SOFT, z::FX - 1, root + Groups{orb}), 1);
    }
    if (c.anim(1)) {
        c.a.spin(og, ctr, r.f(14, 20), false);
        c.a.spin(ig, ctr, r.f(9, 12), true);
        c.a.spin(orb, ctr, r.f(4, 6), true);
    }
}

// ------------------------------------------------------------------ portal
void genPortal(Ctx& c, double cx, double baseY, double H, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    double Wp = H * 0.36, pillarH = H * 0.55, pw = 0.7 * U;

    for (int k = 0; k < 2; k++) {
        double sw = 2 * Wp + (2.6 - k * 0.8) * U, sy = baseY + 0.18 * U + k * 0.32 * U;
        d.rect(cx, sy, sw, 0.34 * U, 0, c.S(k ? FILL_LIGHT : FILL_MID, z::LIT, root));
        d.line({cx - sw / 2, sy + 0.17 * U}, {cx + sw / 2, sy + 0.17 * U}, 1.4, c.S(EDGE, z::EDGE, root));
    }
    double y0 = baseY + 0.66 * U;
    int rg = c.ga.get();
    for (int side : {-1, 1}) {
        double px = cx + side * Wp;
        pillar(c, px, y0, pillarH + 0.8 * U, pw, root);
        for (int j = 0; j < 3; j++) {
            V p{px, y0 + 0.9 * U + j * pillarH / 3.2};
            Style st = c.S(ENERGY, z::WIN, root + Groups{rg});
            switch (r.i(0, 3)) {
                case 0: d.diamond(p, 0.2 * U, st); break;
                case 1: d.rect(p.x, p.y, 0.32 * U, 0.08 * U, 0, st); d.rect(p.x, p.y + 0.12 * U, 0.08 * U, 0.2 * U, 0, st); break;
                case 2: d.ring(p, 0.12 * U, 1.3, st, 8); break;
                default: d.diamond(p + V{-0.08 * U, 0}, 0.12 * U, st); d.diamond(p + V{0.08 * U, 0.1 * U}, 0.12 * U, st); break;
            }
        }
        // brazier
        double by = y0 + pillarH + 0.8 * U + 0.36 * U;
        d.rect(px, by + 0.1 * U, 0.55 * U, 0.2 * U, 0, c.S(FILL_LIGHT, z::DETAIL, root));
        int fg = c.ga.get();
        d.diamond({px, by + 0.42 * U}, 0.32 * U, c.S(ACCENT, z::TOP, root + Groups{fg}));
        d.diamond({px, by + 0.5 * U}, 0.18 * U, c.S(ENERGY, z::TOP + 1, root + Groups{fg}));
        d.glow({px, by + 0.4 * U}, 0.8 * U, c.S(GLOW, z::GLOW, root + Groups{fg}), 2);
        if (c.anim(2)) c.a.flicker(fg, 0.55, 1, r.f(0.3, 0.5), side > 0 ? 0.15 : 0);
    }

    double archY = y0 + pillarH + 0.8 * U + 0.36 * U;
    double Ro = Wp + 0.55 * U, Ri = Wp - 0.35 * U;
    Pts outer = circlePts({cx, archY}, Ro, Ro, 22, 0, PI);
    Pts inner = circlePts({cx, archY}, Ri, Ri, 22, 0, PI);
    Pts arch = outer;
    arch.insert(arch.end(), inner.rbegin(), inner.rend());
    d.fill(arch, c.S(FILL_DARK, z::FILL, root), c.strip());
    Pts litHalf = circlePts({cx, archY}, Ro, Ro, 11, PI / 2, PI);
    Pts inHalf = circlePts({cx, archY}, Ri, Ri, 11, PI / 2, PI);
    litHalf.insert(litHalf.end(), inHalf.rbegin(), inHalf.rend());
    d.fill(litHalf, c.S(FILL_MID, z::SHADE, root), c.strip());
    d.polyline(outer, 1.6, c.S(EDGE, z::EDGE, root));
    d.polyline(inner, 1.2, c.S(DETAIL, z::TRIM, root));
    for (double a = PI / 14; a < PI; a += PI / 7)
        d.line({cx + std::cos(a) * Ri, archY + std::sin(a) * Ri}, {cx + std::cos(a) * Ro, archY + std::sin(a) * Ro}, 1.2,
               c.S(NEAR_SIL, z::DETAIL, root));
    Pts key{{cx - 0.32 * U, archY + Ri - 0.05 * U}, {cx - 0.5 * U, archY + Ro + 0.2 * U},
            {cx + 0.5 * U, archY + Ro + 0.2 * U}, {cx + 0.32 * U, archY + Ri - 0.05 * U}};
    d.fill(key, c.S(FILL_LIGHT, z::DETAIL, root), 3);
    d.polyline(key, 1.3, c.S(EDGE, z::EDGE, root), true);
    V gem{cx, archY + (Ri + Ro) / 2 + 0.05 * U};
    d.diamond(gem, 0.3 * U, c.S(ACCENT, z::TOP, root));
    d.glow(gem, 0.8 * U, c.S(GLOW, z::GLOW, root), 2);

    // energy doorway
    double rx = Wp - pw / 2 - 0.15 * U;
    Pts door{{cx - rx, y0}, {cx + rx, y0}};
    for (auto const& p : circlePts({cx, archY}, rx, rx, 16, 0, PI)) door.push_back(p);
    d.fill(door, c.S(GLOW_SOFT, z::BACK, root), c.strip() * 1.5);
    int ig = c.ga.get();
    for (int k = 1; k <= 3; k++) {
        double s = 1.0 - k * 0.2;
        Pts ring;
        double ry0 = y0 + (archY - y0) * (1 - s) * 0.5;
        ring.push_back({cx - rx * s, ry0});
        ring.push_back({cx + rx * s, ry0});
        for (auto const& p : circlePts({cx, archY}, rx * s, rx * s, 14, 0, PI)) ring.push_back(p);
        d.polyline(ring, 1.4, c.S(ENERGY, z::BACK + 1, root + Groups{ig}), true);
    }
    int rr = c.ga.get();
    V rc{cx, (y0 + archY) / 2 + 0.3 * U};
    int ctr = c.center(rc, root);
    for (int k = 0; k < 10; k++) d.diamond(rc + dir(2 * PI * k / 10) * (rx * 0.55), 0.14 * U, c.S(EDGE, z::BACK + 2, root + Groups{rr}));
    if (c.anim(1)) {
        c.a.spin(rr, ctr, r.f(5, 8), true);
        c.a.flicker(ig, 0.3, 1, r.f(1.6, 2.4));
        c.a.flicker(rg, 0.4, 1, r.f(2, 3));
    }
    if (c.anim(2)) {
        for (int k = 0; k < 3; k++) {
            int mg = c.ga.get();
            for (int j = 0; j < 2; j++)
                d.diamond({cx + r.f(-rx * 0.8, rx * 0.8), y0 + r.f(0.2, 1.2) * U}, 0.12 * U, c.S(PARTICLE, z::FX, root + Groups{mg}));
            c.a.rise(mg, r.f(-0.3, 0.3) * U, r.f(2.2, 3.0) * U, r.f(2.2, 3.0), k * 0.8);
        }
    }
}

// ----------------------------------------------------------------- orbital
void genOrbital(Ctx& c, double cx, double cy, double R, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    V P{cx, cy};
    int ctr = c.center(P, root);
    double tilt = r.f(-18, -8);

    d.glow(P, R * 1.5, c.S(GLOW_SOFT, z::BACK - 1, root), 3);
    auto ringArc = [&](double a0, double a1, int zz) {
        Role roles[3] = {DETAIL, FILL_LIGHT, EDGE};
        for (int k = 0; k < 3; k++) {
            double rr = R * (1.5 + k * 0.17);
            Pts pts = rotated(circlePts(P, rr, rr * 0.27, 26, a0, a1), tilt, P);
            d.polyline(pts, k == 1 ? 3.2 : 1.5, c.S(roles[k], zz, root));
        }
    };
    ringArc(0, PI, z::FILL - 2);

    int pat[5] = {c.ch(FILL_MID), c.ch(FILL_LIGHT), c.ch(FILL_MID), c.ch(DETAIL), c.ch(FILL_DARK)};
    double ph = r.f(0, 6);
    d.fill(circlePts(P, R, R, 44), c.S(FILL_MID, z::FILL, root), c.strip(), [&](double y) {
        int b = (int)std::floor((y - (cy - R)) / (R * 0.2) + 0.6 * std::sin(y * 0.05 + ph));
        return pat[((b % 5) + 5) % 5];
    });
    Pts shadow = circlePts(P, R, R, 20, -PI / 2, PI / 2);
    for (int i = 0; i <= 20; i++) {
        double a = PI / 2 - PI * i / 20;
        shadow.push_back({cx + std::cos(a) * R * 0.4, cy + std::sin(a) * R});
    }
    d.fill(shadow, c.S(NEAR_SIL, z::FILL + 1, root), c.strip());
    d.ring(P, R, 2.2, c.S(EDGE, z::FILL + 2, root), 18, PI * 0.58, PI * 1.42);
    d.ring(P, R + 2.5, 3, c.S(GLOW, z::FILL + 2, root), 18, PI * 0.5, PI * 1.5);

    ringArc(PI, 2 * PI, z::FILL + 4);

    for (int m = 0; m < 2; m++) {
        int mg = c.ga.get();
        double mr = R * r.f(0.13, 0.19), dist = R * r.f(2.0, 2.4), a = r.f(0, 2 * PI);
        V q = P + dir(a) * dist;
        Groups g = root + Groups{mg};
        d.glow(q, mr * 2.2, c.S(GLOW_SOFT, z::FILL + 5, g), 1);
        d.circle(q, mr, c.S(FILL_LIGHT, z::FILL + 6, g), 3);
        d.circle(q + V{mr * 0.35, -mr * 0.1}, mr * 0.75, c.S(FILL_DARK, z::FILL + 7, g), 3);
        if (c.anim(1)) c.a.spin(mg, ctr, r.f(8, 15), m == 0);
    }
    int ag = c.ga.get();
    for (int k = 0; k < 8; k++) {
        double a = r.f(0, 2 * PI), rr = R * r.f(1.55, 1.85);
        V q = rotated({{cx + std::cos(a) * rr, cy + std::sin(a) * rr * 0.27}}, tilt, P)[0];
        d.diamond(q, r.f(0.08, 0.16) * U, c.S(DETAIL, std::sin(a) > 0 ? z::FILL - 2 : z::FILL + 4, root + Groups{ag}), r.f(0, 90));
    }
}

// -------------------------------------------------------------------- tree
static void branch(Ctx& c, V p, double ang, double len, double th, int depth, Groups const& g, Pts& tips) {
    auto& r = c.rng;
    V q = p + dir(ang) * len;
    c.d.line(p, q, th, c.S(FILL_DARK, z::FILL, g));
    if (th > 2.5) c.d.line(p + V{-th * 0.22, 0}, q + V{-th * 0.22, 0}, th * 0.35, c.S(FILL_MID, z::SHADE, g));
    if (depth == 0 || len < 0.4 * U) { tips.push_back(q); return; }
    int n = r.chance(0.3) ? 3 : 2;
    double spread = r.f(20, 34) * PI / 180;
    for (int k = 0; k < n; k++) {
        double off = n == 2 ? (k ? spread : -spread) : (k - 1) * spread;
        double a = ang + off + r.f(-0.1, 0.1);
        a += (PI / 2 - a) * 0.18; // upward bias keeps the crown rounded
        branch(c, q, a, len * r.f(0.72, 0.84), th * 0.7, depth - 1, g, tips);
    }
}

void genTree(Ctx& c, double cx, double baseY, double H, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    int tg = c.ga.get();
    Groups g = root + Groups{tg};
    int ctr = c.center({cx, baseY}, root);

    double tw = H * 0.13, th = H * 0.3;
    for (int k = 0; k < 4; k++) {
        double side = k < 2 ? -1 : 1;
        d.line({cx + side * tw * 0.3, baseY + 0.25 * U}, {cx + side * (tw * 0.6 + r.f(0.5, 1.1) * U), baseY}, tw * 0.25,
               c.S(FILL_DARK, z::FILL, g));
    }
    Pts trunk{{cx - tw / 2, baseY}, {cx + tw / 2, baseY}, {cx + tw * 0.3, baseY + th}, {cx - tw * 0.3, baseY + th}};
    d.fill(trunk, c.S(FILL_DARK, z::FILL, g), c.strip());
    d.fill({{cx - tw / 2, baseY}, {cx - tw * 0.1, baseY}, {cx - tw * 0.05, baseY + th}, {cx - tw * 0.3, baseY + th}},
           c.S(FILL_MID, z::SHADE, g), c.strip());
    d.line({cx - tw / 2, baseY}, {cx - tw * 0.3, baseY + th}, 1.5, c.S(EDGE, z::EDGE, g));

    Pts tips;
    int depth = 3 + c.s.detail;
    int n = r.i(2, 3);
    for (int k = 0; k < n; k++) {
        double a = PI / 2 + (k - (n - 1) / 2.0) * r.f(0.3, 0.42);
        branch(c, {cx, baseY + th}, a, H * r.f(0.24, 0.29), tw * 0.55, depth - 1, g, tips);
    }

    int lg[3] = {c.ga.get(), c.ga.get(), c.ga.get()};
    for (auto const& t : tips) {
        if (c.s.detail >= 2) d.circle(t, r.f(0.3, 0.55) * U, c.S(FILL_MID, z::FILL - 1, g), c.strip());
        int k = r.i(3, 4);
        Groups lgg = g + Groups{lg[r.i(0, 2)]};
        for (int j = 0; j < k; j++) {
            V p = t + V{r.f(-0.4, 0.4) * U, r.f(-0.3, 0.4) * U};
            d.diamond(p, r.f(0.16, 0.36) * U, c.S(j % 3 == 2 ? ENERGY : ACCENT, z::WIN + (j % 2), lgg), r.f(30, 60));
        }
        if (c.s.detail >= 2 && r.chance(0.35)) d.glow(t, 0.9 * U, c.S(GLOW_SOFT, z::TRIM, lgg), 1);
    }
    if (c.anim(1)) c.a.swing(tg, ctr, r.f(1.5, 2.5), r.f(4, 6));
    if (c.anim(2)) {
        for (int k = 0; k < 3; k++) c.a.flicker(lg[k], 0.6, 1, r.f(1.6, 3), k * 0.5);
        for (int k = 0; k < 3; k++) {
            int pg = c.ga.get();
            for (int j = 0; j < 3; j++) {
                V p = tips[r.i(0, (int)tips.size() - 1)];
                d.diamond(p, 0.12 * U, c.S(ACCENT, z::FX, root + Groups{pg}), r.f(0, 90));
            }
            c.a.rise(pg, r.f(1, 2) * U, -r.f(2.5, 4) * U, r.f(3.5, 5), k * 1.2);
        }
    }
}

// --------------------------------------------------------------- colonnade
void genColonnade(Ctx& c, double cx, double baseY, double W, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    int n = std::max(3, (int)(W / (2.6 * U))) + 1;
    double sp = W / (n - 1), pw = 0.62 * U;
    double pillarH = r.f(3.2, 4.2) * U;

    for (int k = 0; k < 2; k++) {
        double sw = W + (1.8 - k * 0.7) * U, sy = baseY + 0.17 * U + k * 0.3 * U;
        d.rect(cx, sy, sw, 0.32 * U, 0, c.S(k ? FILL_LIGHT : FILL_MID, z::LIT, root));
        d.line({cx - sw / 2, sy + 0.16 * U}, {cx + sw / 2, sy + 0.16 * U}, 1.4, c.S(EDGE, z::EDGE, root));
    }
    double y0 = baseY + 0.62 * U;
    for (int i = 0; i < n; i++) pillar(c, cx - W / 2 + i * sp, y0, pillarH, pw, root);

    double capTop = y0 + pillarH, beamBot = capTop + sp * 0.5 + 0.2 * U;
    int lanterns = 0;
    for (int i = 0; i + 1 < n; i++) {
        double xa = cx - W / 2 + i * sp + pw / 2 + 0.2 * U, xb = cx - W / 2 + (i + 1) * sp - pw / 2 - 0.2 * U;
        double ar = (xb - xa) / 2, ax = (xa + xb) / 2;
        Pts sp1{{xa, capTop}};
        for (auto const& p : circlePts({ax, capTop}, ar, ar * 0.9, 14, PI, 0)) sp1.push_back(p);
        sp1.push_back({xb, capTop});
        sp1.push_back({xb, beamBot});
        sp1.push_back({xa, beamBot});
        d.fill(sp1, c.S(FILL_DARK, z::FILL - 1, root), c.strip());
        Pts arc = circlePts({ax, capTop}, ar, ar * 0.9, 14, PI, 0);
        d.polyline(arc, 0.26 * U, c.S(FILL_LIGHT, z::DETAIL, root));
        d.polyline(translated(arc, {0, 0.13 * U}), 1.2, c.S(EDGE, z::EDGE, root));
        d.diamond({ax, capTop + ar * 0.9 + 0.12 * U}, 0.2 * U, c.S(ACCENT, z::TRIM, root));

        if (lanterns < 5 && (n <= 6 || i % 2 == 0)) {
            lanterns++;
            V anchor{ax, capTop + ar * 0.9 - 0.15 * U};
            int lg = c.ga.get();
            int ctr = c.center(anchor, root);
            Groups g = root + Groups{lg};
            V body = anchor + V{0, -1.0 * U};
            d.line(anchor, body + V{0, 0.2 * U}, 1.2, c.S(DETAIL, z::DETAIL, g));
            d.glow(body, 0.8 * U, c.S(GLOW_SOFT, z::TRIM, g), 2);
            d.rect(body.x, body.y, 0.3 * U, 0.4 * U, 0, c.S(FILL_LIGHT, z::WIN - 1, g));
            d.rect(body.x, body.y, 0.17 * U, 0.26 * U, 0, c.S(WINDOW, z::WIN, g));
            d.diamond(body + V{0, -0.26 * U}, 0.12 * U, c.S(ACCENT, z::WIN, g));
            d.spike(body + V{0, 0.3 * U}, 0, 0.3, c.S(FILL_LIGHT, z::WIN, g), true);
            if (c.anim(1)) c.a.swing(lg, ctr, r.f(8, 14), r.f(2.2, 2.8), i * 0.3);
        }
    }
    double bw = W + 1.0 * U, by = beamBot + 0.28 * U;
    d.rect(cx, by, bw, 0.56 * U, 0, c.S(FILL_MID, z::LIT, root));
    d.rect(cx, by + 0.12 * U, bw, 0.14 * U, 0, c.S(FILL_LIGHT, z::DETAIL, root));
    d.line({cx - bw / 2, by + 0.28 * U}, {cx + bw / 2, by + 0.28 * U}, 1.5, c.S(EDGE, z::EDGE, root));
    for (double x = cx - bw / 2 + 0.25 * U; x < cx + bw / 2; x += 0.45 * U)
        d.rect(x, by - 0.36 * U, 0.2 * U, 0.16 * U, 0, c.S(DETAIL, z::DETAIL, root));

    double roofY = by + 0.28 * U;
    if (r.chance(0.65)) {
        double ph = std::min(W * 0.18, 2.6 * U);
        Pts ped{{cx - bw / 2 - 0.3 * U, roofY}, {cx, roofY + ph}, {cx + bw / 2 + 0.3 * U, roofY}};
        d.fill(ped, c.S(FILL_DARK, z::FILL, root), c.strip());
        Pts inner{{cx - bw / 2 + 0.6 * U, roofY + 0.2 * U}, {cx, roofY + ph - 0.45 * U}, {cx + bw / 2 - 0.6 * U, roofY + 0.2 * U}};
        d.fill(inner, c.S(FILL_MID, z::SHADE, root), c.strip());
        d.polyline(ped, 1.6, c.S(EDGE, z::EDGE, root));
        V gem{cx, roofY + ph * 0.38};
        d.ring(gem, 0.38 * U, 1.4, c.S(EDGE, z::TRIM, root), 16);
        d.diamond(gem, 0.3 * U, c.S(ACCENT, z::TOP, root));
        d.glow(gem, 0.9 * U, c.S(GLOW, z::GLOW, root), 2);
        for (int side : {-1, 1})
            d.diamond({cx + side * (bw / 2 + 0.3 * U), roofY + 0.3 * U}, 0.3 * U, c.S(FILL_LIGHT, z::TRIM, root));
    } else {
        for (double x = cx - bw / 2 + 0.2 * U; x <= cx + bw / 2 - 0.2 * U; x += 0.4 * U)
            d.rect(x, roofY + 0.3 * U, 0.12 * U, 0.5 * U, 0, c.S(FILL_MID, z::LIT, root));
        d.rect(cx, roofY + 0.6 * U, bw, 0.16 * U, 0, c.S(FILL_LIGHT, z::DETAIL, root));
        d.line({cx - bw / 2, roofY + 0.68 * U}, {cx + bw / 2, roofY + 0.68 * U}, 1.3, c.S(EDGE, z::EDGE, root));
    }
}

} // namespace inspo

// =====================================================================
// Futuristic structures (v4): dense tech detail, beat-synced pulses.
// =====================================================================
namespace inspo {

// --------------------------------------------------------------- reactor
void genReactor(Ctx& c, double cx, double cy, double R, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    double bt = beatLen(c);
    V P{cx, cy};
    int ctr = c.center(P, root);

    // hex housing
    Pts hex;
    for (int i = 0; i < 6; i++) hex.push_back(P + dir(PI / 6 + i * PI / 3) * (R * 1.25));
    d.glow(P, R * 2.4, c.S(GLOW_SOFT, z::BACK - 2, root), 3);
    d.fill(hex, c.S(FILL_DARK, z::BACK, root), c.strip());
    d.polyline(hex, 3, c.S(EDGE, z::FILL + 8, root), true);
    d.polyOutline(P, R * 1.17, 6, 30, 1.5, c.S(ACCENT, z::FILL + 8, root));
    d.polyOutline(P, R * 1.05, 6, 30, 1.0, c.S(DETAIL, z::BACK + 1, root));
    // face lights chasing around the housing
    std::vector<int> faces;
    for (int i = 0; i < 6; i++) faces.push_back(c.ga.get());
    for (int i = 0; i < 6; i++) {
        V a = P + dir(PI / 6 + i * PI / 3) * (R * 1.11), b = P + dir(PI / 6 + (i + 1) * PI / 3) * (R * 1.11);
        d.dashes(a, b, 7, 5, 2, c.S(ENERGY, z::FILL + 9, root + Groups{faces[i]}));
    }
    // pylons + beams
    int beams = c.ga.get();
    for (int i = 0; i < 6; i++) {
        V v = P + dir(PI / 6 + i * PI / 3) * (R * 1.25);
        V tip = P + dir(PI / 6 + i * PI / 3) * (R * 0.95);
        d.line(v, tip, 0.22 * U, c.S(FILL_LIGHT, z::FILL + 6, root));
        d.line(v, tip, 1.2, c.S(EDGE, z::FILL + 7, root));
        d.circle(v, 0.18 * U, c.S(FILL_MID, z::FILL + 9, root), 3);
        d.diamond(v, 0.12 * U, c.S(ENERGY, z::FILL + 10, root));
        d.line(tip, P + dir(PI / 6 + i * PI / 3) * (R * 0.25), 1.6, c.S(ENERGY, z::FILL - 2, root + Groups{beams}));
    }
    // containment rings
    double radii[3] = {0.86, 0.68, 0.5};
    for (int k = 0; k < 3; k++) {
        int g = c.ga.get();
        int segs = 6 + k * 2;
        double rr = R * radii[k];
        for (int sgi = 0; sgi < segs; sgi++) {
            double a0 = 2 * PI * sgi / segs, a1 = a0 + 2 * PI / segs * 0.7;
            d.ring(P, rr, 0.16 * U, c.S(k == 1 ? FILL_LIGHT : FILL_MID, z::FILL + k, root + Groups{g}), 6, a0, a1);
            d.ring(P, rr + 0.1 * U, 1.2, c.S(k == 1 ? ACCENT : EDGE, z::FILL + k + 1, root + Groups{g}), 6, a0, a1);
            d.diamond(P + dir(a1) * rr, 0.1 * U, c.S(ENERGY, z::FILL + k + 2, root + Groups{g}));
        }
        if (c.anim(1)) c.a.spin(g, ctr, r.f(5, 9) * (k + 1) * 0.6, k % 2 == 0);
    }
    // core
    int core = c.ga.get();
    d.glow(P, R * 0.75, c.S(GLOW, z::FILL + 4, root + Groups{core}), 3);
    d.circle(P, R * 0.2, c.S(ENERGY, z::FILL + 5, root), 3);
    d.circle(P, R * 0.1, c.S(CELESTIAL, z::FILL + 6, root), 2.5);
    d.ring(P, R * 0.28, 1.4, c.S(EDGE, z::FILL + 6, root), 20);
    // orbiting sparks
    int orb = c.ga.get();
    for (int k = 0; k < 4; k++) d.diamond(P + dir(k * PI / 2 + 0.4) * (R * 0.38), 0.07 * U, c.S(EDGE, z::FX, root + Groups{orb}));
    if (c.anim(1)) {
        c.a.spin(orb, ctr, bt * 4, true);
        c.a.flicker(beams, 0.15, 1, bt * 2);
        c.a.flicker(core, 0.55, 1, bt, 0);
        c.a.sequence(faces, bt / 2, 1, 0.2);
    }
}

// ------------------------------------------------------------ cyber tower
void genCyberTower(Ctx& c, double cx, double baseY, double H, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    double bt = beatLen(c);
    int nSeg = r.i(4, 6);
    double w0 = H * r.f(0.4, 0.46);
    double bodyH = H * 0.8, spireH = H * 0.2;
    std::vector<double> hs(nSeg);
    double sum = 0;
    for (auto& h : hs) { h = r.f(0.8, 1.3); sum += h; }
    for (auto& h : hs) h = h / sum * bodyH;
    int blink = c.ga.get(), stream = c.ga.get();
    auto bands = std::vector<int>{c.ga.get(), c.ga.get(), c.ga.get()};

    // base
    double bw = w0 + 1.6 * U;
    d.rect(cx, baseY + 0.2 * U, bw, 0.4 * U, 0, c.S(FILL_MID, z::LIT, root));
    d.line({cx - bw / 2, baseY + 0.4 * U}, {cx + bw / 2, baseY + 0.4 * U}, 1.6, c.S(EDGE, z::EDGE, root));
    d.dashes({cx - bw / 2 + 4, baseY + 0.2 * U}, {cx + bw / 2 - 4, baseY + 0.2 * U}, 5, 4, 1.6, c.S(ENERGY, z::EDGE, root + Groups{blink}));

    double y = baseY + 0.4 * U;
    double wTop = w0;
    for (int i = 0; i < nSeg; i++) {
        double wb = w0 * (1 - 0.12 * i), wt = wb * r.f(0.82, 0.95), h = hs[i];
        double ch = std::min(0.3 * U, wt * 0.15);
        Pts seg{{cx - wb / 2, y}, {cx + wb / 2, y}, {cx + wt / 2, y + h - ch}, {cx + wt / 2 - ch, y + h},
                {cx - wt / 2 + ch, y + h}, {cx - wt / 2, y + h - ch}};
        d.fill(seg, c.S(FILL_DARK, z::FILL, root), c.strip());
        // lit left facet
        Pts lit{{cx - wb / 2, y}, {cx - wb * 0.15, y}, {cx - wt * 0.15, y + h}, {cx - wt / 2 + ch, y + h}, {cx - wt / 2, y + h - ch}};
        d.fill(lit, c.S(FILL_MID, z::SHADE, root), c.strip());
        d.polyline(seg, 2.2, c.S(EDGE, z::EDGE, root), true);
        Pts inset;
        V cen{cx, y + h / 2};
        for (auto const& p : seg) inset.push_back(cen + (p - cen) * 0.86);
        d.polyline(inset, 1.2, c.S(ACCENT, z::EDGE - 1, root), true);
        // data stream columns
        for (int col = -1; col <= 1; col += 2) {
            double x = cx + col * (std::min(wb, wt) / 2 - 0.15 * U);
            d.dashes({x, y + 0.3 * U}, {x, y + h - 0.3 * U}, 6, 9, 1.6, c.S(ENERGY, z::TRIM, root + Groups{stream}));
        }
        // machined face panels with modules
        {
            GCtx g = gctxOf(c, bands);
            double ix = std::min(wb, wt) / 2 - 0.3 * U;
            facePanels(c, g, Box{cx - ix, y + 0.3 * U, cx + ix, y + h - 0.35 * U}, Look::Cyber, root, z::SHADE + 1);
        }
        // glowing seam above the segment
        d.rect(cx, y + h, wt - 2 * ch, 2.4, 0, c.S(ENERGY, z::TOP, root));
        if (c.s.detail >= 2) d.glow({cx, y + h}, wt * 0.6, c.S(GLOW_SOFT, z::GLOW - 2, root), 1);
        // fins / antennas
        if (i % 2 == 1) {
            for (int side : {-1, 1}) {
                V a0{cx + side * wb / 2, y + h * 0.2}, a1{cx + side * (wb / 2 + r.f(0.5, 0.9) * U), y + h * 0.7}, a2{cx + side * wt / 2, y + h * 0.75};
                d.fill({a0, a1, a2}, c.S(FILL_MID, z::FILL - 1, root), 4);
                d.polyline({a0, a1, a2}, 1.4, c.S(EDGE, z::EDGE, root));
                d.diamond(a1, 0.1 * U, c.S(ACCENT, z::FX, root + Groups{blink}));
            }
        }
        y += h;
        wTop = wt;
    }
    // spire + beacon + halo rings
    double sb = wTop * 0.35;
    Pts sp{{cx - sb / 2, y}, {cx, y + spireH}, {cx + sb / 2, y}};
    d.fill(sp, c.S(FILL_DARK, z::FILL, root), 4);
    d.polyline(sp, 1.8, c.S(EDGE, z::EDGE, root));
    d.line({cx, y}, {cx, y + spireH}, 1.2, c.S(ENERGY, z::EDGE + 1, root));
    V tip{cx, y + spireH + 0.2 * U};
    int beacon = c.ga.get();
    d.glow(tip, 1.4 * U, c.S(GLOW, z::GLOW, root + Groups{beacon}), 3);
    d.diamond(tip, 0.2 * U, c.S(CELESTIAL, z::TOP, root));
    auto halos = std::vector<int>{c.ga.get(), c.ga.get(), c.ga.get()};
    for (int k = 0; k < 3; k++) {
        V hc{cx, y + spireH * (0.25 + k * 0.25)};
        double rr = wTop * (0.7 - k * 0.15);
        d.polyline(circlePts(hc, rr, rr * 0.2, 32), 1.6, c.S(k % 2 ? ACCENT : EDGE, z::FX, root + Groups{halos[k]}), true);
    }
    // holo panels
    for (int side : {-1, 1}) {
        if (!r.chance(0.7)) continue;
        int hg = c.ga.get();
        V pc{cx + side * (w0 / 2 + r.f(1.0, 1.6) * U), baseY + r.f(0.4, 0.65) * H};
        double pw = r.f(0.9, 1.4) * U, ph = r.f(0.6, 0.9) * U;
        d.rect(pc.x, pc.y, pw, ph, 0, c.S(GLOW_SOFT, z::FX - 2, root + Groups{hg}));
        d.polyline({{pc.x - pw / 2, pc.y - ph / 2}, {pc.x + pw / 2, pc.y - ph / 2}, {pc.x + pw / 2, pc.y + ph / 2}, {pc.x - pw / 2, pc.y + ph / 2}},
                   1.4, c.S(ENERGY, z::FX, root + Groups{hg}), true);
        for (int k = 0; k < 3; k++) {
            double bl = r.f(0.3, 0.8) * pw;
            d.rect(pc.x - pw / 2 + 5 + bl / 2, pc.y + ph / 2 - 7 - k * 6, bl, 2, 0, c.S(EDGE, z::FX, root + Groups{hg}));
        }
        d.line({pc.x - side * pw / 2, pc.y}, {cx + side * w0 * 0.4, pc.y}, 1, c.S(ENERGY, z::FX - 1, root + Groups{hg}));
        if (c.anim(1)) {
            c.a.bob(hg, 0, 0.25 * U, r.f(2.5, 3.5), r.f(0, 1));
            c.a.flicker(hg, 0.55, 1, bt * 2, side > 0 ? bt : 0);
        }
    }
    if (c.anim(1)) {
        c.a.flicker(blink, 0.1, 1, bt);
        c.a.flicker(beacon, 0.35, 1, bt, 0);
        c.a.sequence(bands, bt / 2, 1, 0.3);
        c.a.sequence(halos, bt / 2, 1, 0.15);
        // data streams crawl upward (dash period = 15 units, so the reset is invisible)
        c.a.loop({{{Anim::move(stream, 0, 15, bt, 0)}, bt}, {{Anim::move(stream, 0, -15, 0)}, 0.05}});
    }
}

// ---------------------------------------------------------------- station
void genStation(Ctx& c, double cx, double cy, double W, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    double bt = beatLen(c);
    V P{cx, cy};
    int ctr = c.center(P, root);
    double hubR = W * 0.11, armL = W * 0.38;
    int nav = c.ga.get(), nav2 = c.ga.get();

    // trusses
    for (int side : {-1, 1}) {
        double x0 = cx + side * hubR * 0.9, x1 = cx + side * armL;
        double t = 0.14 * U;
        d.line({x0, cy + t}, {x1, cy + t}, 2, c.S(FILL_LIGHT, z::FILL, root));
        d.line({x0, cy - t}, {x1, cy - t}, 2, c.S(FILL_LIGHT, z::FILL, root));
        int n = (int)(std::fabs(x1 - x0) / (0.28 * U));
        for (int k = 0; k < n; k++) {
            double xa = x0 + (x1 - x0) * k / n, xb = x0 + (x1 - x0) * (k + 1) / n;
            d.line({xa, k % 2 ? cy + t : cy - t}, {xb, k % 2 ? cy - t : cy + t}, 1.1, c.S(DETAIL, z::FILL, root));
        }
        // solar arrays
        for (int vs : {-1, 1}) {
            double pw = armL * 0.42, ph = r.f(0.9, 1.3) * U;
            V pc{cx + side * armL * 0.72, cy + vs * (t + 0.25 * U + ph / 2)};
            d.line({pc.x, cy}, {pc.x, pc.y - vs * ph / 2}, 1.6, c.S(FILL_LIGHT, z::FILL, root));
            d.rect(pc.x, pc.y, pw, ph, 0, c.S(PLANET3, z::FILL + 1, root));
            for (int k = 1; k < 6; k++) d.rect(pc.x - pw / 2 + pw * k / 6, pc.y, 1.1, ph, 0, c.S(DETAIL, z::FILL + 2, root));
            d.rect(pc.x, pc.y, pw, 1.1, 0, c.S(DETAIL, z::FILL + 2, root));
            d.polyline({{pc.x - pw / 2, pc.y - ph / 2}, {pc.x + pw / 2, pc.y - ph / 2}, {pc.x + pw / 2, pc.y + ph / 2}, {pc.x - pw / 2, pc.y + ph / 2}},
                       1.4, c.S(EDGE, z::FILL + 3, root), true);
            d.diamond({pc.x - pw * 0.3, pc.y + vs * ph * 0.3}, 3, c.S(CELESTIAL, z::FILL + 3, root));
        }
        // modules
        double mx = cx + side * armL * 0.35;
        double ml = 0.9 * U, mh = 0.42 * U;
        d.rect(mx, cy, ml, mh, 0, c.S(FILL_MID, z::FILL + 4, root));
        d.circle({mx - ml / 2, cy}, mh / 2, c.S(FILL_MID, z::FILL + 4, root), 3);
        d.circle({mx + ml / 2, cy}, mh / 2, c.S(FILL_MID, z::FILL + 4, root), 3);
        d.rect(mx, cy + mh * 0.38, ml + mh * 0.8, 1.4, 0, c.S(EDGE, z::FILL + 5, root));
        d.dashes({mx - ml / 2, cy}, {mx + ml / 2, cy}, 3, 3, 2.2, c.S(WINDOW, z::FILL + 5, root));
        // nav lights at the ends
        V end{cx + side * armL, cy};
        d.diamond(end, 0.12 * U, c.S(side < 0 ? ACCENT : ENERGY, z::FX, root + Groups{side < 0 ? nav : nav2}));
        d.glow(end, 0.6 * U, c.S(GLOW_SOFT, z::FX - 1, root + Groups{side < 0 ? nav : nav2}), 1);
    }
    // rotating habitat ring with spokes
    int ringG = c.ga.get();
    double ringR = hubR * 2.0;
    for (int k = 0; k < 8; k++) {
        double a0 = 2 * PI * k / 8, a1 = a0 + 2 * PI / 8 * 0.82;
        d.ring(P, ringR, 0.2 * U, c.S(FILL_LIGHT, z::FILL + 6, root + Groups{ringG}), 6, a0, a1);
        d.ring(P, ringR + 0.12 * U, 1.2, c.S(EDGE, z::FILL + 7, root + Groups{ringG}), 6, a0, a1);
        d.dashes(P + dir(a0 + 0.08) * ringR, P + dir(a1 - 0.08) * ringR, 3, 4, 1.6, c.S(WINDOW, z::FILL + 8, root + Groups{ringG}));
        if (k % 2 == 0) d.line(P + dir(a0 + 0.2) * hubR, P + dir(a0 + 0.2) * ringR, 1.4, c.S(DETAIL, z::FILL + 5, root + Groups{ringG}));
    }
    // hub
    d.glow(P, hubR * 2.5, c.S(GLOW_SOFT, z::BACK, root), 2);
    d.circle(P, hubR, c.S(FILL_MID, z::FILL + 9, root), c.strip());
    d.ring(P, hubR, 2, c.S(EDGE, z::FILL + 10, root), 24);
    d.ring(P, hubR * 0.7, 1.2, c.S(ACCENT, z::FILL + 10, root), 20);
    for (int k = 0; k < 10; k++) d.rect(P.x + std::cos(k * PI / 5) * hubR * 0.85, P.y + std::sin(k * PI / 5) * hubR * 0.85, 2.4, 2.4, 0, c.S(WINDOW, z::FILL + 11, root));
    d.circle(P, hubR * 0.3, c.S(ENERGY, z::FILL + 11, root), 2.5);
    // docking beacon beam
    int beam = c.ga.get();
    d.rect(cx, cy - hubR - 1.2 * U, 0.12 * U, 2.4 * U, 0, c.S(GLOW, z::BACK, root + Groups{beam}));
    if (c.anim(1)) {
        c.a.spin(ringG, ctr, r.f(14, 20), true);
        c.a.flicker(nav, 0.1, 1, bt * 2, 0);
        c.a.flicker(nav2, 0.1, 1, bt * 2, bt);
        c.a.flicker(beam, 0.2, 1, bt * 4);
    }
}

// ------------------------------------------------------------------ blade
void genBlade(Ctx& c, double cx, double baseY, double H, Groups const& root) {
    auto& r = c.rng;
    auto& d = c.d;
    double bt = beatLen(c);
    double Lb = H * 0.66, wb = H * 0.11, gw = H * 0.42, gh = H * 0.075, gl = H * 0.16;
    double y0 = baseY + gl + 0.3 * U; // guard top / blade root

    // aura
    int aura = c.ga.get();
    for (int k = 0; k < 3; k++) d.rect(cx, y0 + Lb * 0.5, wb * (2.2 + k * 1.2), Lb * (1.05 + k * 0.08), 0, c.S(GLOW_SOFT, z::BACK, root + Groups{aura}));

    // blade (two-tone bevel)
    Pts left{{cx - wb / 2, y0}, {cx - wb * 0.42, y0 + Lb * 0.82}, {cx, y0 + Lb}, {cx, y0}};
    Pts right{{cx, y0}, {cx, y0 + Lb}, {cx + wb * 0.42, y0 + Lb * 0.82}, {cx + wb / 2, y0}};
    Pts full{{cx - wb / 2, y0}, {cx - wb * 0.42, y0 + Lb * 0.82}, {cx, y0 + Lb}, {cx + wb * 0.42, y0 + Lb * 0.82}, {cx + wb / 2, y0}};
    d.fill(left, c.S(FILL_LIGHT, z::FILL, root), c.strip());
    d.fill(right, c.S(FILL_MID, z::FILL, root), c.strip());
    d.polyline(full, 2.2, c.S(EDGE, z::EDGE, root));
    d.line({cx - wb * 0.25, y0 + 2}, {cx - wb * 0.2, y0 + Lb * 0.8}, 1.2, c.S(EDGE, z::EDGE, root));
    // fuller with runes
    double fy0 = y0 + H * 0.03, fy1 = y0 + Lb * 0.74;
    d.rect(cx, (fy0 + fy1) / 2, wb * 0.22, fy1 - fy0, 0, c.S(NEAR_SIL, z::TRIM, root));
    d.rect(cx, (fy0 + fy1) / 2, wb * 0.08, fy1 - fy0, 0, c.S(ENERGY, z::TRIM + 1, root));
    std::vector<int> runes;
    for (int k = 0; k < 5; k++) runes.push_back(c.ga.get());
    int nr = 10;
    for (int k = 0; k < nr; k++) {
        double ry = fy0 + (fy1 - fy0) * (k + 0.5) / nr;
        Groups g = root + Groups{runes[k % 5]};
        if (k % 2) d.chevron({cx, ry}, wb * 0.35, 90, 1.6, c.S(EDGE, z::WIN, g));
        else d.diamond({cx, ry}, wb * 0.16, c.S(CELESTIAL, z::WIN, g));
        d.glow({cx, ry}, wb * 0.6, c.S(GLOW_SOFT, z::WIN - 1, g), 1);
    }
    // guard
    Pts guardL{{cx - wb * 0.75, y0 - gh}, {cx - gw / 2, y0 + gh * 1.4}, {cx - gw * 0.42, y0 + gh * 1.6}, {cx - wb * 0.75, y0 + gh * 0.4}};
    Pts guardR = mirroredX(guardL, cx);
    Pts center{{cx - wb * 0.9, y0 - gh * 1.3}, {cx + wb * 0.9, y0 - gh * 1.3}, {cx + wb * 0.9, y0 + gh * 0.4}, {cx - wb * 0.9, y0 + gh * 0.4}};
    for (auto const* pg : {&guardL, &guardR, &center}) {
        d.fill(*pg, c.S(FILL_DARK, z::DETAIL, root), 3);
        d.polyline(*pg, 1.8, c.S(EDGE, z::EDGE + 1, root), true);
    }
    d.line(guardL[0] + V{-1, 3}, guardL[1] + V{4, 1}, 1.2, c.S(ACCENT, z::EDGE + 2, root));
    d.line(guardR[0] + V{1, 3}, guardR[1] + V{-4, 1}, 1.2, c.S(ACCENT, z::EDGE + 2, root));
    int gem = c.ga.get();
    V gp{cx, y0 - gh * 0.45};
    d.diamond(gp, wb * 0.55, c.S(ACCENT, z::TOP, root));
    d.diamond(gp, wb * 0.28, c.S(CELESTIAL, z::TOP + 1, root));
    d.glow(gp, wb * 2.2, c.S(GLOW, z::GLOW, root + Groups{gem}), 2);
    for (int side : {-1, 1}) d.diamond(guardL[2] * (side < 0 ? 1.0 : 0.0) + guardR[2] * (side < 0 ? 0.0 : 1.0), wb * 0.2, c.S(ENERGY, z::TOP, root));
    // grip with wrap
    double gy0 = y0 - gh * 1.3 - gl, gy1 = y0 - gh * 1.3;
    d.rect(cx, (gy0 + gy1) / 2, wb * 0.55, gl, 0, c.S(FILL_DARK, z::FILL, root));
    for (double yy = gy0 + 3; yy < gy1 - 2; yy += 5) d.line({cx - wb * 0.27, yy}, {cx + wb * 0.27, yy + 3}, 1.2, c.S(DETAIL, z::TRIM, root));
    d.line({cx - wb * 0.27, gy0}, {cx - wb * 0.27, gy1}, 1.2, c.S(EDGE, z::EDGE, root));
    // pommel
    V pm{cx, gy0 - wb * 0.45};
    d.diamond(pm, wb * 0.75, c.S(FILL_LIGHT, z::DETAIL, root));
    d.diamond(pm, wb * 0.35, c.S(ENERGY, z::TOP, root));
    // drifting sparks
    if (c.anim(2)) {
        for (int k = 0; k < 3; k++) {
            int sg = c.ga.get();
            for (int j = 0; j < 3; j++) d.diamond({cx + r.f(-1.5, 1.5) * wb * 2, y0 + r.f(0, 0.8) * Lb}, 3, c.S(PARTICLE, z::FX, root + Groups{sg}));
            c.a.rise(sg, r.f(-0.3, 0.3) * U, r.f(1.5, 2.5) * U, r.f(2.5, 4), k);
        }
    }
    if (c.anim(1)) {
        c.a.sequence(runes, bt / 2, 1, 0.2);
        c.a.flicker(aura, 0.5, 1, bt * 2);
        c.a.flicker(gem, 0.4, 1, bt);
    }
}

} // namespace inspo
