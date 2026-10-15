// Layout decorator. Reads the shape of the selected gameplay blocks and builds
// layered decoration on and around it in one of several styles.

#include "Decorate.hpp"
#include "Greebles.hpp"

#include <algorithm>
#include <map>

namespace inspo {

namespace {

namespace z {
    constexpr int GLOW_OUT = 5, FILL = 10, PANEL = 11, PATTERN = 12, PATTERN2 = 13, SHADE = 14,
                  EDGE = 16, EDGE2 = 17, LINE = 18, TOPDECO = 20, FX = 22;
}

V dir(double a) { return {std::cos(a), std::sin(a)}; }

struct DC {
    DecoSettings const& s;
    Rng& rng;
    Writer& w;
    Draw& d;
    Anim& a;
    GroupAlloc& ga;
    Palette const& pal;
    Layout const& L;

    Style S(Role r, int zz, Groups const& g = {}) const { return Style{pal.ch(r), zz, g}; }
    bool anim(int lvl = 1) const { return s.motion >= lvl; }
    double strip() const { return s.detail >= 3 ? 4.0 : s.detail == 2 ? 6.0 : 9.0; }

    // ---------------------------------------------------------- shape helpers
    void fill(int level, Role r, int zz, Groups const& g = {}) {
        for (auto const& rc : L.rects(level)) d.rect(rc.c().x, rc.c().y, rc.w() + 0.6, rc.h() + 0.6, 0, S(r, zz, g));
    }

    // Line along each edge of the given inset level, sitting just inside it.
    void edgeLines(int level, Role r, double thick, int zz, std::function<bool(Seg const&)> const& want = nullptr,
                   std::function<Groups(Seg const&, double x)> const& groupsFor = nullptr) {
        for (auto const& sg : L.edges(level)) {
            if (want && !want(sg)) continue;
            V n = sg.normal();
            V off = n * (-thick / 2);
            V a0 = sg.a + off, b0 = sg.b + off;
            bool horiz = sg.side == Side::Top || sg.side == Side::Bottom;
            if (groupsFor && sg.length() > 2 * U) {
                // split long edges so animation groups can travel along them
                int n2 = (int)std::ceil(sg.length() / U);
                for (int k = 0; k < n2; k++) {
                    double t0 = (double)k / n2, t1 = (double)(k + 1) / n2;
                    V p = a0 + (b0 - a0) * t0, q = a0 + (b0 - a0) * t1;
                    if (horiz) { p.x -= k == 0 ? thick / 2 : 0; q.x += k == n2 - 1 ? thick / 2 : 0; }
                    else { p.y -= k == 0 ? thick / 2 : 0; q.y += k == n2 - 1 ? thick / 2 : 0; }
                    d.rect((p.x + q.x) / 2, (p.y + q.y) / 2, horiz ? std::fabs(q.x - p.x) + 0.6 : thick,
                           horiz ? thick : std::fabs(q.y - p.y) + 0.6, 0, S(r, zz, groupsFor(sg, (p.x + q.x) / 2)));
                }
                continue;
            }
            Groups g = groupsFor ? groupsFor(sg, (a0.x + b0.x) / 2) : Groups{};
            if (horiz) d.rect((a0.x + b0.x) / 2, a0.y, std::fabs(b0.x - a0.x), thick, 0, S(r, zz, g));
            else d.rect(a0.x, (a0.y + b0.y) / 2, thick, std::fabs(b0.y - a0.y), 0, S(r, zz, g));
        }
    }

    // Additive glow fading outward from the real outline.
    void outerGlow(Role r, double width, int layers, int zz, Groups const& g = {}, std::function<bool(Seg const&)> const& want = nullptr) {
        for (auto const& sg : L.edges(0)) {
            if (want && !want(sg)) continue;
            V n = sg.normal();
            bool horiz = sg.side == Side::Top || sg.side == Side::Bottom;
            for (int i = 1; i <= layers; i++) {
                double wd = width * i / layers;
                V c = (sg.a + sg.b) * 0.5 + n * (wd / 2);
                if (horiz) d.rect(c.x, c.y, sg.length(), wd, 0, S(r, zz, g));
                else d.rect(c.x, c.y, wd, sg.length(), 0, S(r, zz, g));
            }
        }
        for (auto const& cn : L.corners(0)) {
            if (!cn.convex) continue;
            V n{cn.vertical == Side::Left ? -1.0 : 1.0, cn.horizontal == Side::Top ? 1.0 : -1.0};
            for (int i = 1; i <= layers; i++) {
                double wd = width * i / layers;
                d.rect(cn.p.x + n.x * wd / 2, cn.p.y + n.y * wd / 2, wd, wd, 0, S(r, zz, g));
            }
        }
    }

    // Walk along edges of a side placing things roughly every `spacing`.
    void along(Side side, double spacing, double jitter, std::function<void(V p, Seg const&)> const& fn, int level = 0) {
        for (auto const& sg : L.edges(level)) {
            if (sg.side != side) continue;
            double len = sg.length();
            if (len < spacing * 0.6) continue;
            int n = std::max(1, (int)(len / spacing));
            for (int k = 0; k < n; k++) {
                double t = (k + 0.5) / n + rng.f(-jitter, jitter) / std::max(len, 1.0);
                t = std::clamp(t, 0.05, 0.95);
                fn(sg.a + (sg.b - sg.a) * t, sg);
            }
        }
    }

    // Groups that light up one after another along x -> a travelling wave.
    std::vector<int> waveGroups(int n, double period, double lo = 0.35) {
        std::vector<int> gs;
        for (int i = 0; i < n; i++) gs.push_back(ga.get());
        if (anim(1))
            for (int i = 0; i < n; i++) a.flicker(gs[i], lo, 1.0, period, period * i / n);
        return gs;
    }

    void hazards() {
        if (!s.hazardGlow || L.hazards().empty()) return;
        int hg = ga.get();
        for (auto const& h : L.hazards()) {
            d.glow({h.x, h.y - h.h * 0.1}, std::max(h.w, h.h) * 1.5, S(GLOW_SOFT, z::GLOW_OUT, {hg}), 2);
            if (s.detail >= 2) d.diamond({h.x, h.y + h.h * 0.15}, 4, S(ENERGY, z::FX, {hg}));
        }
        if (anim(1)) a.flicker(hg, 0.5, 1, rng.f(1.2, 1.8));
    }

    void crystal(V base, double len, double w, double angCCW, int zz, Groups const& g, bool litLeft) {
        auto T = [&](Pts p) { return translated(rotated(p, angCCW, {0, 0}), base); };
        Pts left{{-w / 2, 0}, {-w / 2, 0.7 * len}, {0, len}, {0, 0}};
        Pts right{{0, 0}, {0, len}, {w / 2, 0.7 * len}, {w / 2, 0}};
        Pts full{{-w / 2, 0}, {-w / 2, 0.7 * len}, {0, len}, {w / 2, 0.7 * len}, {w / 2, 0}};
        double st = std::max(2.5, std::min(strip(), w / 3));
        d.fill(T(left), S(litLeft ? FILL_LIGHT : FILL_MID, zz, g), st);
        d.fill(T(right), S(litLeft ? FILL_MID : FILL_DARK, zz, g), st);
        auto core = T({{0, 0.12 * len}, {0, 0.85 * len}});
        d.line(core[0], core[1], std::max(1.3, w * 0.14), S(ENERGY, zz + 1, g));
        d.polyline(T(full), 1.4, S(EDGE, zz + 2, g), true);
    }

    // Random-walk "circuit trace" through cells at least `minDepth` deep.
    Pts trace(int steps, int minDepth) {
        Pts out;
        int W = L.width(), H = L.height();
        for (int tries = 0; tries < 40; tries++) {
            int i = rng.i(0, W - 1), j = rng.i(0, H - 1);
            if (L.depth(i, j) < minDepth) continue;
            int dirx = rng.chance(0.5) ? 1 : 0, sgn = rng.chance(0.5) ? 1 : -1;
            out.push_back(L.cellCenter(i, j));
            for (int k = 0; k < steps; k++) {
                int run = rng.i(3, 10);
                bool moved = false;
                for (int r = 0; r < run; r++) {
                    int ni = i + (dirx ? sgn : 0), nj = j + (dirx ? 0 : sgn);
                    if (L.depth(ni, nj) < minDepth) break;
                    i = ni; j = nj; moved = true;
                }
                if (moved) out.push_back(L.cellCenter(i, j));
                dirx = 1 - dirx;
                sgn = rng.chance(0.5) ? 1 : -1;
            }
            if (out.size() >= 3) return out;
            out.clear();
        }
        return out;
    }

    double beat() const { return 60.0 / std::clamp(s.bpm, 40.0, 400.0); }

    GCtx gctx() {
        GCtx g{d, rng, pal, a, ga, 2, 2, 0.43, {}, {}, 6, {}};
        g.detail = s.detail;
        g.motion = s.motion;
        g.beat = beat();
        g.fanBudget = 2 + s.detail * 2;
        return g;
    }

    // Splits the inset interior into machined panels and fills them with modules.
    void panels(GCtx& g, Look look, int level, double moduleChance, int zBase, bool conduits = false) {
        double gap = look == Look::Industrial ? 3 : conduits ? 3.5 : 2.5;
        for (auto const& rc : L.rects(level)) {
            Box b{rc.x0, rc.y0, rc.x1, rc.y1};
            if (b.w() < 12 || b.h() < 12) continue;
            double maxW = rng.f(2.5, 4.5) * U, maxH = rng.f(1.5, 2.5) * U;
            for (auto const& p : subdivide(rng, b, 0.6 * U, maxW, 0.5 * U, maxH, gap)) {
                panelPlate(g, p, look, zBase);
                if (conduits) {
                    // glowing conduits running through the seams, pulsing along x
                    int wi = (int)std::floor(p.x0 / (1.5 * U));
                    if (p.y1 + gap < b.y1 - 1) d.line({p.x0 + 1, p.y1 + gap / 2}, {p.x1 - 1, p.y1 + gap / 2}, 1.1, S(ENERGY, zBase, g.waveG(wi)));
                    if (p.x1 + gap < b.x1 - 1) d.line({p.x1 + gap / 2, p.y0 + 1}, {p.x1 + gap / 2, p.y1 - 1}, 1.1, S(ENERGY, zBase, g.waveG(wi + 1)));
                }
                bool big = p.w() > 2.4 * U && p.h() > 1.4 * U;
                if (big && s.detail >= 2 && rng.chance(0.6)) {
                    // nested sub-panels for large plates
                    for (auto const& q : subdivide(rng, p.inset(4), 0.45 * U, 1.4 * U, 0.4 * U, 1.0 * U, 2))
                        if (rng.chance(moduleChance)) placeModule(g, q, look, zBase + 4);
                } else if (rng.chance(moduleChance)) {
                    placeModule(g, p, look, zBase + 4);
                }
            }
        }
    }


    // Angled armor plates wrapping convex corners (breaks the block silhouette).
    void shoulders(Role fillR, int zz, double minWidth = 2 * U) {
        for (auto const& cn : L.corners(0)) {
            if (!cn.convex) continue;
            int reg = cn.region;
            double regH = 99 * U;
            if (reg >= 0 && reg < (int)L.regions().size()) {
                if (L.regions()[reg].bounds.w() < minWidth) continue;
                regH = L.regions()[reg].bounds.h();
            }
            bool thin = regH < 1.6 * U;
            if (thin && cn.horizontal == Side::Bottom) continue; // thin platforms: one end cap from the top corner
            double sx = cn.vertical == Side::Left ? -1 : 1, sy = cn.horizontal == Side::Top ? 1 : -1;
            V p = cn.p;
            auto P2 = [&](double ox, double oy) { return V{p.x + sx * ox * U, p.y + sy * oy * U}; };
            double down = thin ? regH / U + 0.06 : 0.92; // thin: cap wraps the whole end
            Pts sh{P2(-0.95, 0), P2(-0.75, 0.16), P2(0.08, 0.16), P2(0.2, 0.04), P2(0.2, -down + 0.2), P2(0, -down)};
            if (thin) sh.push_back(P2(-0.55, -down));
            d.fill(sh, S(fillR, zz), 3);
            d.polyline(sh, 1.6, S(EDGE, zz + 1), true);
            d.line(P2(-0.65, 0.08), P2(0.06, 0.08), 1.4, S(ENERGY, zz + 2));
            d.line(P2(0.1, -0.08), P2(0.1, -down + 0.3), 1.4, S(ENERGY, zz + 2));
            d.diamond(P2(0.1, 0.08), 3.2, S(ACCENT, zz + 3));
        }
    }

    // Raised armor plates on walls.
    void wallPlates(Role fillR, int zz) {
        for (auto const& sg : L.edges(0)) {
            if ((sg.side != Side::Left && sg.side != Side::Right) || sg.length() < 1.6 * U || !rng.chance(0.65)) continue;
            double sx = sg.side == Side::Left ? -1 : 1;
            double h = std::min(sg.length() * 0.6, 2.2 * U), cy = (sg.a.y + sg.b.y) / 2 + rng.f(-0.15, 0.15) * (sg.length() - h);
            double x0 = sg.a.x - sx * 0.16 * U, x1 = sg.a.x + sx * 0.12 * U;
            Box b{std::min(x0, x1), cy - h / 2, std::max(x0, x1), cy + h / 2};
            Pts pl{{b.x0, b.y0 + 4}, {b.x0 + (sx > 0 ? 0 : 4), b.y0}, {b.x1 - (sx > 0 ? 4 : 0), b.y0}, {b.x1, b.y0 + 4},
                   {b.x1, b.y1 - 4}, {b.x1 - (sx > 0 ? 4 : 0), b.y1}, {b.x0 + (sx > 0 ? 0 : 4), b.y1}, {b.x0, b.y1 - 4}};
            d.fill(pl, S(fillR, zz), 3);
            d.polyline(pl, 1.5, S(EDGE, zz + 1), true);
            for (double y = b.y0 + 6; y < b.y1 - 5; y += 5) d.rect(b.c().x, y, b.w() - 4, 1.2, 0, S(DETAIL, zz + 2));
            d.rect(x1 - sx * 2, cy, 2, h * 0.5, 0, S(ENERGY, zz + 2));
        }
    }

    // Thrusters with flickering exhaust under floating platforms.
    void thrusters(int zz, double flick) {
        int fg = ga.get();
        int n = 0;
        for (auto const& sg : L.edges(0)) {
            if (sg.side != Side::Bottom || sg.a.y < 1.0 * U || sg.length() < 1.5 * U) continue;
            int k = std::max(1, (int)(sg.length() / (2.6 * U)));
            for (int i = 0; i < k && n < 40; i++, n++) {
                double x = sg.a.x + sg.length() * (i + 0.5) / k, y = sg.a.y;
                Pts noz{{x - 0.2 * U, y}, {x + 0.2 * U, y}, {x + 0.3 * U, y - 0.32 * U}, {x - 0.3 * U, y - 0.32 * U}};
                d.fill(noz, S(FILL_MID, zz), 3);
                d.polyline(noz, 1.4, S(EDGE, zz + 1), true);
                d.rect(x, y - 0.12 * U, 0.38 * U, 1.4, 0, S(DETAIL, zz + 1));
                d.rect(x, y - 0.3 * U, 0.5 * U, 2.2, 0, S(ENERGY, zz + 2));
                double hs[3] = {0.9, 0.65, 0.4}, ws[3] = {0.5, 0.34, 0.18};
                for (int j = 0; j < 3; j++) d.rect(x, y - 0.32 * U - hs[j] * U / 2, ws[j] * U, hs[j] * U, 0, S(GLOW_SOFT, zz - 1, {fg}));
                d.rect(x, y - 0.32 * U - 0.2 * U, 1.6, 0.4 * U, 0, S(ENERGY, zz, {fg}));
            }
        }
        if (anim(1)) a.flicker(fg, 0.45, 1, flick);
    }

    // Dark support trusses behind floating platforms, on a deeper layer.
    void supports() {
        int prevLayer = d.zLayer;
        d.zLayer = gd::layer::B2;
        int lg = ga.get();
        for (auto const& reg : L.regions()) {
            Rect rb = reg.bounds;
            if (rb.y0 < 1.5 * U || rb.y0 > 9 * U || rb.w() > 14 * U || rb.w() < 1.5 * U) continue;
            int n = rb.w() > 6 * U ? 2 : 1;
            for (int k = 0; k < n; k++) {
                double x = n == 1 ? rb.c().x : rb.x0 + rb.w() * (k == 0 ? 0.25 : 0.75);
                // find whatever is below
                V o = L.cellMin(0, 0);
                int ci = (int)std::floor((x - o.x) / L.cell());
                double yb = 0;
                for (int j = (int)std::floor((rb.y0 - o.y) / L.cell()) - 2; j >= 0; j--)
                    if (L.occ(ci, j)) { yb = o.y + (j + 1) * L.cell(); break; }
                double y1 = rb.y0, y0 = std::max(yb, rb.y0 - 9 * U);
                if (y1 - y0 < U) continue;
                double hw = 0.28 * U;
                d.rect(x, (y0 + y1) / 2, hw * 2, y1 - y0, 0, S(FAR_SIL, 2));
                d.rect(x - hw, (y0 + y1) / 2, 3, y1 - y0, 0, S(MID_SIL, 3));
                d.rect(x + hw, (y0 + y1) / 2, 3, y1 - y0, 0, S(MID_SIL, 3));
                for (double y = y0; y < y1 - 0.5 * U; y += 0.6 * U) {
                    d.line({x - hw, y}, {x + hw, y + 0.6 * U}, 1.5, S(MID_SIL, 3));
                    d.line({x + hw, y}, {x - hw, y + 0.6 * U}, 1.5, S(MID_SIL, 3));
                }
                for (double y = y0 + 0.8 * U; y < y1; y += 2 * U) d.diamond({x, y}, 3, S(ACCENT, 4, {lg}));
            }
        }
        d.zLayer = prevLayer;
        if (anim(1)) a.flicker(lg, 0.1, 1, beat() * 2);
    }

    // Detail for thin (1-block) parts where interior panels don't fit.
    void thinParts(GCtx& g, Look look, int zz) {
        for (auto const& rc : L.rects(1)) {
            Box b{rc.x0, rc.y0, rc.x1, rc.y1};
            bool thinH = b.h() <= 0.55 * U && b.w() >= 1.2 * U;
            bool thinV = b.w() <= 0.55 * U && b.h() >= 1.2 * U;
            if (!thinH && !thinV) continue;
            if (look == Look::Circles) { mDiamonds(g, b, zz); continue; }
            double pick = rng.f(0, 1);
            if (pick < 0.5) mLightStrip(g, b, zz);
            else if (pick < 0.8) mChevrons(g, b, zz);
            else {
                // segmented light bar with end caps
                V c = b.c();
                if (thinH) {
                    d.rect(c.x, c.y, b.w() - 8, 2.4, 0, S(ENERGY, zz, g.waveG(rng.i(0, 5))));
                    d.diamond({b.x0 + 3, c.y}, 3, S(EDGE, zz + 1));
                    d.diamond({b.x1 - 3, c.y}, 3, S(EDGE, zz + 1));
                } else {
                    d.rect(c.x, c.y, 2.4, b.h() - 8, 0, S(ENERGY, zz, g.waveG(rng.i(0, 5))));
                }
            }
        }
    }

    // Additive glow fading inward from the outline.
    void innerGlow(Role r, double width, int layers, int zz) {
        for (int i = 1; i <= layers; i++) edgeLines(0, r, width * i / layers, zz);
    }

    // Recessed slots along long edges.
    void slots(double every, int zz) {
        for (auto const& sg : L.edges(0)) {
            double len = sg.length();
            if (len < 3 * U) continue;
            int n = std::max(1, (int)(len / every));
            bool horiz = sg.side == Side::Top || sg.side == Side::Bottom;
            V nrm = sg.normal();
            for (int k = 0; k < n; k++) {
                if (!rng.chance(0.6)) continue;
                double t = (k + 0.5) / n;
                V p = sg.a + (sg.b - sg.a) * t - nrm * (0.17 * U);
                double sw = rng.f(0.4, 0.8) * U, sh = 0.2 * U;
                if (horiz) {
                    d.rect(p.x, p.y, sw, sh, 0, S(NEAR_SIL, zz));
                    d.polyline({{p.x - sw / 2, p.y + nrm.y * sh / 2}, {p.x - sw / 2, p.y - nrm.y * sh / 2}, {p.x + sw / 2, p.y - nrm.y * sh / 2}, {p.x + sw / 2, p.y + nrm.y * sh / 2}}, 1.2, S(EDGE, zz + 1));
                    d.rect(p.x, p.y, sw * 0.6, 1.6, 0, S(ENERGY, zz + 2));
                } else {
                    d.rect(p.x, p.y, sh, sw, 0, S(NEAR_SIL, zz));
                    d.polyline({{p.x + nrm.x * sh / 2, p.y - sw / 2}, {p.x - nrm.x * sh / 2, p.y - sw / 2}, {p.x - nrm.x * sh / 2, p.y + sw / 2}, {p.x + nrm.x * sh / 2, p.y + sw / 2}}, 1.2, S(EDGE, zz + 1));
                    d.rect(p.x, p.y, 1.6, sw * 0.6, 0, S(ENERGY, zz + 2));
                }
            }
        }
    }

    // Inward direction of a convex corner.
    static V cornerIn(Corner const& cn) {
        return {cn.vertical == Side::Left ? 1.0 : -1.0, cn.horizontal == Side::Top ? -1.0 : 1.0};
    }

    // Is there solid directly below this hazard (so it points up)?
    bool pointsUp(LayoutObj const& h) const {
        V o = L.cellMin(0, 0);
        int ci = (int)std::floor((h.x - o.x) / L.cell());
        int below = (int)std::floor((h.y - h.h / 2 - 4 - o.y) / L.cell());
        int above = (int)std::floor((h.y + h.h / 2 + 4 - o.y) / L.cell());
        if (L.occ(ci, above) && !L.occ(ci, below)) return false;
        return true;
    }

    // Manhattan trace with 45-degree chamfered corners (futuristic look).
    Pts chamfer(Pts const& p, double cut) const {
        if (p.size() < 3) return p;
        Pts out{p.front()};
        for (size_t i = 1; i + 1 < p.size(); i++) {
            V a = p[i - 1], b = p[i], c2 = p[i + 1];
            V d1 = b - a, d2 = c2 - b;
            double l1 = std::hypot(d1.x, d1.y), l2 = std::hypot(d2.x, d2.y);
            double k = std::min({cut, l1 * 0.45, l2 * 0.45});
            if (l1 < 1 || l2 < 1) { out.push_back(b); continue; }
            out.push_back(b - d1 * (k / l1));
            out.push_back(b + d2 * (k / l2));
        }
        out.push_back(p.back());
        return out;
    }

    // depth thresholds below are in cells of L.cell() (7.5 units by default)
    int interiorCells(int minDepth) const {
        int n = 0;
        for (int j = 0; j < L.height(); j++)
            for (int i = 0; i < L.width(); i++)
                if (L.depth(i, j) >= minDepth && L.occ(i, j)) n++;
        return n;
    }

    // Jagged crack polyline inside the shape.
    Pts crack(int minDepth) {
        Pts out;
        for (int tries = 0; tries < 30; tries++) {
            int i = rng.i(0, L.width() - 1), j = rng.i(0, L.height() - 1);
            if (L.depth(i, j) < minDepth) continue;
            V p = L.cellCenter(i, j);
            double ang = rng.f(0, 2 * PI);
            out.push_back(p);
            int n = rng.i(3, 7);
            for (int k = 0; k < n; k++) {
                ang += rng.f(-0.9, 0.9);
                V q = p + dir(ang) * rng.f(0.3, 0.8) * U;
                int ci = (int)std::floor((q.x - L.cellMin(0, 0).x) / L.cell()), cj = (int)std::floor((q.y - L.cellMin(0, 0).y) / L.cell());
                if (L.depth(ci, cj) < minDepth) break;
                out.push_back(q);
                p = q;
            }
            if (out.size() >= 3) return out;
            out.clear();
        }
        return out;
    }
};

// ======================================================================== styles

void styleNeon(DC& c) {
    auto& d = c.d;
    c.fill(0, FILL_DARK, z::FILL);
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.8 * U, 4, z::GLOW_OUT);
    {
        GCtx g = c.gctx();
        g.wave = c.waveGroups(4, c.beat() * 4, 0.4);
        for (int k = 0; k < 6; k++) g.seq.push_back(c.ga.get());
        c.panels(g, Look::Neon, 3, c.s.detail >= 2 ? 0.55 : 0.3, z::PANEL, true);
        if (c.s.edgeDeco) c.thrusters(z::TOPDECO, c.beat() / 2);
        c.thinParts(g, Look::Neon, z::PATTERN2);
        if (c.anim(1)) c.a.sequence(g.seq, c.beat() / 2, 1.0, 0.3);
    }
    c.innerGlow(GLOW_SOFT, 0.5 * U, 3, z::SHADE);

    auto wave = c.waveGroups(5, c.rng.f(1.6, 2.4));
    double waveLen = c.rng.f(2, 3.5) * U;
    auto gfor = [&](Seg const&, double x) { int k = (((int)std::floor(x / waveLen)) % 5 + 5) % 5; return Groups{wave[k]}; };
    c.edgeLines(0, ACCENT, 3.2, z::LINE, nullptr, gfor);
    c.edgeLines(1, ENERGY, 1.5, z::EDGE);
    if (c.s.detail >= 2) c.edgeLines(3, ENERGY, 1.1, z::EDGE);
    if (c.s.detail >= 3) c.edgeLines(6, GLOW, 1.0, z::EDGE);

    for (auto const& cn : c.L.corners(0)) {
        if (!cn.convex) continue;
        d.diamond(cn.p, 0.28 * U, c.S(EDGE, z::FX));
        if (c.s.glow) d.glow(cn.p, 0.8 * U, c.S(GLOW_SOFT, z::GLOW_OUT), 1);
    }
    // interior dot grid
    if (c.s.detail >= 2) {
        int dg = c.ga.get();
        for (int j = 0; j < c.L.height(); j += 4)
            for (int i = (j / 4) % 2 * 4; i < c.L.width(); i += 8)
                if (c.L.depth(i, j) >= 6) d.diamond(c.L.cellCenter(i, j), 2.4, c.S(ENERGY, z::PATTERN, {dg}));
        if (c.anim(2)) c.a.flicker(dg, 0.2, 0.9, c.rng.f(2.5, 3.5));
    }
    if (c.s.edgeDeco) {
        int hg = c.ga.get();
        c.along(Side::Bottom, c.rng.f(1.2, 2) * U, 0.4 * U, [&](V p, Seg const&) {
            if (!c.rng.chance(0.6)) return;
            double l = c.rng.f(0.3, 1.0) * U;
            d.line(p, p + V{0, -l}, 1.6, c.S(ACCENT, z::TOPDECO, {hg}));
            d.diamond(p + V{0, -l - 3}, 5, c.S(EDGE, z::TOPDECO, {hg}));
        });
        if (c.anim(2)) c.a.flicker(hg, 0.4, 1, c.rng.f(1.5, 2.5));
    }
}

void styleTech(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    c.fill(0, FILL_DARK, z::FILL);
    {
        GCtx g = c.gctx();
        g.wave = c.waveGroups(4, c.beat() * 4, 0.4);
        for (int k = 0; k < 6; k++) g.seq.push_back(c.ga.get());
        if (c.s.edgeDeco) c.supports();
        c.panels(g, Look::Tech, 2, c.s.detail >= 3 ? 0.85 : 0.65, z::PANEL, c.s.detail >= 2);
        if (c.s.edgeDeco) { c.wallPlates(FILL_MID, z::FX - 1); c.shoulders(FILL_LIGHT, z::FX + 2, 3 * U); }
        c.thinParts(g, Look::Tech, z::PATTERN2);
        if (c.anim(1)) c.a.sequence(g.seq, c.beat() / 2, 1.0, 0.35);
    }
    if (false) // seams now come from the panel engine
    for (auto const& rc : c.L.rects(0)) {
        double pw = r.f(2, 3) * U;
        for (double x = rc.x0 + pw; x < rc.x1 - U * 0.8; x += pw)
            d.rect(x, rc.c().y, 1.6, rc.h() - 4, 0, c.S(NEAR_SIL, z::PATTERN));
        if (rc.h() > 3 * U)
            for (double y = rc.y0 + 2 * U; y < rc.y1 - U; y += 2 * U) d.rect(rc.c().x, y, rc.w() - 4, 1.6, 0, c.S(NEAR_SIL, z::PATTERN));
    }
    // bevel
    c.edgeLines(0, FILL_LIGHT, 5, z::SHADE, [](Seg const& s) { return s.side == Side::Top || s.side == Side::Left; });
    c.edgeLines(0, NEAR_SIL, 5, z::SHADE, [](Seg const& s) { return s.side == Side::Bottom || s.side == Side::Right; });
    c.edgeLines(0, EDGE, 1.6, z::LINE, [](Seg const& s) { return s.side == Side::Top; });
    c.edgeLines(2, DETAIL, 1.2, z::EDGE);

    // circuit traces
    int cells = c.interiorCells(4);
    int n = std::min(30, cells / (c.s.detail >= 3 ? 150 : 250) + 1);
    auto wave = c.waveGroups(3, r.f(1.4, 2.2), 0.25);
    for (int k = 0; k < n; k++) {
        Pts t = c.trace(r.i(3, 6), 4);
        if (t.empty()) continue;
        Groups g{wave[k % 3]};
        d.polyline(t, 2.0, c.S(ENERGY, z::PATTERN2, g));
        d.ring(t.front(), 4, 1.4, c.S(ACCENT, z::PATTERN2, g), 8);
        d.diamond(t.back(), 6, c.S(ACCENT, z::PATTERN2, g));
    }
    // chips
    if (c.s.detail >= 2) {
        for (auto const& rc : c.L.rects(5)) {
            if (rc.w() < 2 * U || rc.h() < 0.8 * U || !r.chance(0.5)) continue;
            V p{r.f(rc.x0 + U, rc.x1 - U), rc.c().y};
            d.rect(p.x, p.y, 0.9 * U, 0.55 * U, 0, c.S(NEAR_SIL, z::PATTERN2));
            d.rect(p.x, p.y + 0.2 * U, 0.9 * U, 1.2, 0, c.S(DETAIL, z::PATTERN2 + 1));
            for (int k = -2; k <= 2; k++) {
                d.rect(p.x + k * 0.16 * U, p.y + 0.36 * U, 1.4, 0.18 * U, 0, c.S(DETAIL, z::PATTERN2));
                d.rect(p.x + k * 0.16 * U, p.y - 0.36 * U, 1.4, 0.18 * U, 0, c.S(DETAIL, z::PATTERN2));
            }
            d.diamond(p + V{0.3 * U, 0}, 3, c.S(ACCENT, z::PATTERN2 + 1));
        }
    }
    for (auto const& cn : c.L.corners(0)) {
        if (!cn.convex) continue;
        V in{cn.vertical == Side::Left ? 1.0 : -1.0, cn.horizontal == Side::Top ? -1.0 : 1.0};
        d.diamond(cn.p + in * (0.3 * U), 3.5, c.S(DETAIL, z::LINE));
    }
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.45 * U, 2, z::GLOW_OUT);
    if (c.s.edgeDeco) {
        // sagging cables under platforms
        for (auto const& sg : c.L.edges(0)) {
            if (sg.side != Side::Bottom || sg.length() < 3 * U || !r.chance(0.7)) continue;
            double x0 = r.f(sg.a.x + 0.3 * U, sg.b.x - 2.5 * U), x1 = std::min(sg.b.x - 0.3 * U, x0 + r.f(1.5, 3) * U);
            double sag = r.f(0.3, 0.8) * U;
            Pts cable;
            for (int k = 0; k <= 10; k++) {
                double t = k / 10.0;
                cable.push_back({x0 + (x1 - x0) * t, sg.a.y - 2 - sag * 4 * t * (1 - t)});
            }
            d.polyline(cable, 1.6, c.S(DETAIL, z::TOPDECO));
            V mid = cable[5];
            d.rect(mid.x, mid.y - 3, 4, 4, 0, c.S(FILL_LIGHT, z::TOPDECO));
            d.diamond(mid + V{0, -6}, 3, c.S(ACCENT, z::TOPDECO));
        }
    }
}

void styleCrystal(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    c.fill(0, FILL_DARK, z::FILL);
    // faceted interior: 2-block facets where they fit, 1-block facets elsewhere
    Rect b = c.L.bounds();
    Role tones[3] = {FILL_MID, FILL_LIGHT, FILL_DARK};
    auto inside = [&](double x, double y, double F) {
        for (double yy = y + 1; yy < y + F; yy += c.L.cell())
            for (double xx = x + 1; xx < x + F; xx += c.L.cell()) {
                int ci = (int)std::floor((xx - c.L.cellMin(0, 0).x) / c.L.cell());
                int cj = (int)std::floor((yy - c.L.cellMin(0, 0).y) / c.L.cell());
                if (c.L.depth(ci, cj) < 1) return false;
            }
        return true;
    };
    auto facet = [&](double x, double y, double F) {
        if (r.chance(0.15)) return;
        bool diag = r.chance(0.5);
        Pts t1 = diag ? Pts{{x, y}, {x + F, y}, {x + F, y + F}} : Pts{{x, y}, {x + F, y}, {x, y + F}};
        Pts t2 = diag ? Pts{{x, y}, {x + F, y + F}, {x, y + F}} : Pts{{x + F, y}, {x + F, y + F}, {x, y + F}};
        double st = F > U ? 10 : 7.5;
        d.fill(t1, c.S(tones[r.i(0, 2)], z::PATTERN), st);
        d.fill(t2, c.S(tones[r.i(0, 2)], z::PATTERN), st);
        if (c.s.detail >= 3) d.line(diag ? V{x, y} : V{x + F, y}, diag ? V{x + F, y + F} : V{x, y + F}, 1.2, c.S(DETAIL, z::PATTERN2));
    };
    double F2 = 2 * U;
    double fx0 = std::floor(b.x0 / F2) * F2, fy0 = std::floor(b.y0 / F2) * F2;
    for (double y = fy0; y < b.y1; y += F2)
        for (double x = fx0; x < b.x1; x += F2) {
            if (c.s.detail <= 2 && inside(x, y, F2)) { facet(x, y, F2); continue; }
            for (int q = 0; q < 4; q++) {
                double qx = x + (q % 2) * U, qy = y + (q / 2) * U;
                if (inside(qx, qy, U)) facet(qx, qy, U);
            }
        }
    c.edgeLines(0, EDGE, 2.2, z::LINE);
    c.edgeLines(1, ENERGY, 1.3, z::EDGE);
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.5 * U, 2, z::GLOW_OUT);
    int sp = c.ga.get();
    if (c.s.edgeDeco) {
        c.along(Side::Top, r.f(3, 5) * U, 1.0 * U, [&](V p, Seg const&) {
            if (!r.chance(0.55)) return;
            int k = r.i(2, 4);
            for (int i = 0; i < k; i++) {
                double t = k == 1 ? 0 : (double)i / (k - 1) * 2 - 1;
                double len = r.f(0.5, 1.1) * U * (1 - 0.35 * std::fabs(t));
                c.crystal(p + V{t * 0.25 * U, -2}, len, len * 0.28, -t * 28, z::TOPDECO, {}, t <= 0);
            }
        });
        c.along(Side::Bottom, r.f(1.5, 2.5) * U, 0.6 * U, [&](V p, Seg const&) {
            if (!r.chance(0.6)) return;
            double len = r.f(0.35, 0.9) * U;
            c.crystal(p + V{0, 2}, len, len * 0.3, 180 + r.f(-12, 12), z::TOPDECO, {}, r.chance(0.5));
        });
    }
    for (int k = 0; k < c.interiorCells(2) / 40; k++) {
        int i = r.i(0, c.L.width() - 1), j = r.i(0, c.L.height() - 1);
        if (c.L.depth(i, j) < 2) continue;
        d.diamond(c.L.cellCenter(i, j), r.f(2, 4), c.S(EDGE, z::FX, {sp}));
    }
    if (c.anim(2)) c.a.flicker(sp, 0.05, 1, r.f(1.2, 2));
}

void styleCastle(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    c.fill(0, FILL_MID, z::FILL);
    double bh = 0.5 * U, bw = 1.0 * U;
    for (auto const& rc : c.L.rects(0)) {
        int row = 0;
        for (double y = rc.y0 + bh; y < rc.y1 - 1; y += bh, row++)
            d.rect(rc.c().x, y, rc.w() - 2, 1.8, 0, c.S(NEAR_SIL, z::PATTERN));
        row = 0;
        for (double y = rc.y0; y < rc.y1 - 1; y += bh, row++) {
            double off = (row % 2) ? bw / 2 : 0;
            double rowTop = std::min(rc.y1, y + bh);
            for (double x = std::floor((rc.x0 - off) / bw) * bw + off + bw; x < rc.x1 - 2; x += bw)
                if (x > rc.x0 + 2) d.rect(x, (y + rowTop) / 2, 1.8, rowTop - y, 0, c.S(NEAR_SIL, z::PATTERN));
            for (double x = rc.x0; x < rc.x1 - 2; x += bw) {
                if (!r.chance(0.12)) continue;
                double x1 = std::min(rc.x1, x + bw) - 2;
                d.rect((x + 2 + x1) / 2 + off * 0, (y + rowTop) / 2, x1 - x - 2, rowTop - y - 3, 0,
                       c.S(r.chance(0.7) ? FILL_DARK : FILL_LIGHT, z::PANEL));
            }
        }
    }
    // capstones and shading
    c.edgeLines(0, FILL_LIGHT, 0.32 * U, z::SHADE, [](Seg const& s) { return s.side == Side::Top; });
    c.edgeLines(0, EDGE, 1.6, z::LINE, [](Seg const& s) { return s.side == Side::Top; });
    c.edgeLines(0, NEAR_SIL, 4, z::SHADE, [](Seg const& s) { return s.side == Side::Bottom || s.side == Side::Right; });
    c.edgeLines(0, FILL_LIGHT, 2.5, z::SHADE, [](Seg const& s) { return s.side == Side::Left; });
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.35 * U, 2, z::GLOW_OUT);
    if (!c.s.edgeDeco) return;
    // corbels + banners under platforms
    c.along(Side::Bottom, 1.0 * U, 0, [&](V p, Seg const&) { d.spike(p + V{0, -0.2 * U}, 180, 0.3, c.S(FILL_LIGHT, z::TOPDECO), true); });
    c.along(Side::Bottom, r.f(4, 6) * U, 1.0 * U, [&](V p, Seg const&) {
        if (!r.chance(0.6)) return;
        d.rect(p.x, p.y - 0.75 * U, 0.42 * U, 1.1 * U, 0, c.S(ACCENT, z::TOPDECO));
        d.rect(p.x, p.y - 0.75 * U, 0.12 * U, 0.9 * U, 0, c.S(EDGE, z::TOPDECO + 1));
        d.spike(p + V{0, -1.45 * U}, 180, 0.42, c.S(ACCENT, z::TOPDECO), true);
        d.rect(p.x, p.y - 0.12 * U, 0.6 * U, 0.12 * U, 0, c.S(DETAIL, z::TOPDECO + 1));
    });
    // torches on walls
    int fg = c.ga.get();
    for (auto const& sg : c.L.edges(0)) {
        if ((sg.side != Side::Left && sg.side != Side::Right) || sg.length() < 2.5 * U || !r.chance(0.7)) continue;
        double sideSign = sg.side == Side::Left ? -1 : 1;
        V p{sg.a.x + sideSign * 0.15 * U, (sg.a.y + sg.b.y) / 2 + r.f(-0.3, 0.3) * U};
        d.rect(p.x + sideSign * 0.1 * U, p.y, 0.25 * U, 0.12 * U, 0, c.S(DETAIL, z::TOPDECO));
        d.rect(p.x + sideSign * 0.22 * U, p.y + 0.12 * U, 0.12 * U, 0.3 * U, 0, c.S(FILL_DARK, z::TOPDECO));
        V f = p + V{sideSign * 0.22 * U, 0.38 * U};
        d.diamond(f, 0.22 * U, c.S(ACCENT, z::FX, {fg}));
        d.diamond(f + V{0, 0.04 * U}, 0.12 * U, c.S(ENERGY, z::FX + 1, {fg}));
        d.glow(f, 0.8 * U, c.S(GLOW_SOFT, z::FX - 1, {fg}), 2);
    }
    if (c.anim(2)) c.a.flicker(fg, 0.55, 1, r.f(0.3, 0.45));
}

void styleNature(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    int dark = c.pal.ch(FILL_DARK), mid = c.pal.ch(FILL_MID), sil = c.pal.ch(NEAR_SIL);
    double ph = r.f(0, 6);
    for (auto const& rc : c.L.rects(0)) {
        Pts p{{rc.x0, rc.y0}, {rc.x1, rc.y0}, {rc.x1, rc.y1}, {rc.x0, rc.y1}};
        d.fill(p, c.S(FILL_DARK, z::FILL), 7.5, [&](double y) {
            int band = (int)std::floor(y / (0.45 * U) + 0.7 * std::sin(y * 0.05 + ph));
            int k = ((band % 4) + 4) % 4;
            return k == 1 ? mid : k == 3 ? sil : dark;
        });
    }
    // pebbles
    for (int k = 0; k < c.interiorCells(3) / 40; k++) {
        int i = r.i(0, c.L.width() - 1), j = r.i(0, c.L.height() - 1);
        if (c.L.depth(i, j) < 3) continue;
        d.circle(c.L.cellCenter(i, j), r.f(2.5, 5), c.S(FILL_MID, z::PATTERN), 3);
    }
    c.edgeLines(0, NEAR_SIL, 3, z::SHADE, [](Seg const& s) { return s.side != Side::Top; });
    // grass cap
    c.edgeLines(0, ACCENT, 0.26 * U, z::LINE, [](Seg const& s) { return s.side == Side::Top; });
    c.edgeLines(0, EDGE, 1.4, z::LINE + 1, [](Seg const& s) { return s.side == Side::Top; });
    for (auto const& sg : c.L.edges(0)) {
        if (sg.side != Side::Left && sg.side != Side::Right) continue;
        double s0 = sg.side == Side::Left ? 1 : -1;
        double l = std::min(sg.length(), r.f(0.3, 0.7) * U);
        d.rect(sg.a.x + s0 * 2.5, sg.b.y - l / 2, 5, l, 0, c.S(ACCENT, z::LINE));
    }
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.4 * U, 2, z::GLOW_OUT, {}, [](Seg const& s) { return s.side == Side::Top; });
    if (!c.s.edgeDeco) return;
    c.along(Side::Top, 0.45 * U, 0.2 * U, [&](V p, Seg const&) {
        if (!r.chance(0.65)) return;
        d.spike(p + V{0, 5}, r.f(-14, 14), r.f(0.25, 0.5), c.S(r.chance(0.7) ? ACCENT : DETAIL, z::TOPDECO), true);
        if (c.s.detail >= 2 && r.chance(0.12)) {
            V f = p + V{r.f(-3, 3), r.f(9, 14)};
            d.line(p + V{0, 3}, f, 1.2, c.S(DETAIL, z::TOPDECO));
            for (int k = 0; k < 4; k++) d.diamond(f + dir(k * PI / 2) * 2.5, 3.2, c.S(ENERGY, z::TOPDECO + 1));
            d.diamond(f, 2.2, c.S(EDGE, z::TOPDECO + 2));
        }
    });
    int bg = c.ga.get();
    c.along(Side::Bottom, r.f(0.8, 1.3) * U, 0.4 * U, [&](V p, Seg const&) {
        if (!r.chance(0.55)) return;
        double len = r.f(0.6, 2.2) * U, amp = r.f(2, 5), ph2 = r.f(0, 6);
        Pts vine;
        for (int k = 0; k <= 8; k++) {
            double t = k / 8.0;
            vine.push_back({p.x + std::sin(t * 6 + ph2) * amp * t, p.y - 1 - len * t});
        }
        d.polyline(vine, 1.6, c.S(DETAIL, z::TOPDECO));
        for (int k = 2; k < 8; k += 2) {
            double sgn = k % 4 == 0 ? 1 : -1;
            d.rect(vine[k].x + sgn * 3, vine[k].y, 5, 2.6, sgn * 30, c.S(ACCENT, z::TOPDECO));
        }
        d.diamond(vine.back() + V{0, -2}, 4, c.S(ENERGY, z::FX, {bg}));
    });
    if (c.anim(2)) {
        c.a.flicker(bg, 0.4, 1, r.f(1.6, 2.4));
        Rect b = c.L.bounds();
        for (int k = 0; k < 3; k++) {
            int fg = c.ga.get();
            for (int j = 0; j < 4; j++)
                d.diamond({r.f(b.x0, b.x1), b.y1 + r.f(0.2, 1.5) * U}, 3, c.S(PARTICLE, z::FX, {fg}));
            c.a.rise(fg, r.f(-0.6, 0.6) * U, r.f(1, 2) * U, r.f(3.5, 5), k * 1.3);
        }
    }
}

void styleVoid(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    c.fill(0, FILL_DARK, z::FILL);
    c.edgeLines(0, EDGE, 2.2, z::LINE);
    int maxLevel = c.s.detail + 2;
    std::vector<int> wave;
    for (int lv = 1; lv <= maxLevel; lv++) wave.push_back(c.ga.get());
    for (int lv = 1; lv <= maxLevel; lv++) {
        int g = wave[lv - 1];
        c.edgeLines(lv * 2 - 1, lv % 2 ? ENERGY : DETAIL, 1.1, z::EDGE, nullptr, [&](Seg const&, double) { return Groups{g}; });
    }
    if (c.anim(1))
        for (int lv = 0; lv < maxLevel; lv++) c.a.flicker(wave[lv], 0.15, 1, 2.4, lv * 2.4 / maxLevel);
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.5 * U, 2, z::GLOW_OUT);
    if (!c.s.edgeDeco) return;
    // floating outlined squares drifting around the shapes
    for (auto const& sg : c.L.edges(0)) {
        if (sg.length() < 2 * U || !r.chance(0.5)) continue;
        if (sg.side == Side::Top) continue; // keep the walking surface clean
        int g = c.ga.get();
        V p = sg.a + (sg.b - sg.a) * r.f(0.2, 0.8) + sg.normal() * r.f(0.6, 1.4) * U;
        double sz = r.f(0.25, 0.5) * U, rot = r.f(0, 90);
        Pts sq = rotated({{p.x - sz / 2, p.y - sz / 2}, {p.x + sz / 2, p.y - sz / 2}, {p.x + sz / 2, p.y + sz / 2}, {p.x - sz / 2, p.y + sz / 2}}, rot, p);
        d.polyline(sq, 1.3, c.S(EDGE, z::FX, {g}), true);
        if (c.anim(1)) c.a.bob(g, 0, r.f(0.2, 0.4) * U, r.f(2.5, 4), r.f(0, 2));
    }
}

void styleLava(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    c.fill(0, FILL_DARK, z::FILL);
    c.fill(5, NEAR_SIL, z::PANEL);
    int cg = c.ga.get();
    int n = std::min(60, c.interiorCells(2) / (c.s.detail >= 3 ? 45 : 70) + 1);
    for (int k = 0; k < n; k++) {
        Pts cr = c.crack(2);
        if (cr.empty()) continue;
        d.polyline(cr, 5, c.S(GLOW_SOFT, z::PATTERN, {cg}));
        d.polyline(cr, 1.8, c.S(ENERGY, z::PATTERN2, {cg}));
    }
    c.edgeLines(0, FILL_MID, 0.22 * U, z::SHADE, [](Seg const& s) { return s.side == Side::Top; });
    c.edgeLines(0, ACCENT, 1.8, z::LINE, [](Seg const& s) { return s.side == Side::Top; });
    c.edgeLines(0, ACCENT, 2.4, z::LINE, [](Seg const& s) { return s.side == Side::Bottom; });
    c.edgeLines(0, FILL_MID, 2, z::SHADE, [](Seg const& s) { return s.side == Side::Left || s.side == Side::Right; });
    if (c.s.glow) {
        c.outerGlow(GLOW_SOFT, 0.9 * U, 3, z::GLOW_OUT, {}, [](Seg const& s) { return s.side == Side::Bottom; });
        c.outerGlow(GLOW_SOFT, 0.35 * U, 2, z::GLOW_OUT, {}, [](Seg const& s) { return s.side != Side::Bottom; });
    }
    if (c.anim(1)) c.a.flicker(cg, 0.55, 1, r.f(1.8, 2.6));
    if (!c.s.edgeDeco) return;
    int dg = c.ga.get();
    c.along(Side::Bottom, r.f(1.2, 2) * U, 0.4 * U, [&](V p, Seg const&) {
        if (!r.chance(0.6)) return;
        double l = r.f(0.2, 0.6) * U;
        d.rect(p.x, p.y - l / 2, 3, l, 0, c.S(ACCENT, z::TOPDECO));
        d.diamond(p + V{0, -l - 2}, 5, c.S(ENERGY, z::TOPDECO, {dg}));
        if (c.anim(2)) {
            int fg = c.ga.get();
            d.diamond(p + V{0, -l - 4}, 3.5, c.S(ACCENT, z::FX, {fg}));
            c.a.rise(fg, 0, -r.f(1.5, 3) * U, r.f(1.2, 2), r.f(0, 1.5));
        }
    });
    if (c.anim(2)) {
        Rect b = c.L.bounds();
        for (int k = 0; k < 3; k++) {
            int eg = c.ga.get();
            for (int j = 0; j < 5; j++) d.diamond({r.f(b.x0, b.x1), b.y1 + r.f(0, 0.5) * U}, r.f(2, 3.5), c.S(PARTICLE, z::FX, {eg}));
            c.a.rise(eg, r.f(-0.5, 0.5) * U, r.f(1.5, 3) * U, r.f(2.5, 4), k);
        }
    }
}

void styleIce(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    c.fill(0, FILL_MID, z::FILL);
    c.fill(4, FILL_DARK, z::PANEL);
    for (int k = 0; k < c.interiorCells(2) / 110 + 1; k++) {
        Pts cr = c.crack(2);
        if (!cr.empty()) d.polyline(cr, 1.2, c.S(FILL_LIGHT, z::PATTERN));
    }
    // shine streaks near the top-left of each region
    for (auto const& reg : c.L.regions()) {
        V tl{reg.bounds.x0 + 0.4 * U, reg.bounds.y1 - 0.4 * U};
        for (int k = 0; k < 3; k++) {
            V a0 = tl + V{k * 0.35 * U, 0}, b0 = a0 + V{-0.4 * U, -0.4 * U};
            double ci = std::floor((b0.x - c.L.cellMin(0, 0).x) / c.L.cell()), cj = std::floor((b0.y - c.L.cellMin(0, 0).y) / c.L.cell());
            if (c.L.depth((int)ci, (int)cj) < 1) continue;
            d.line(a0, b0, k == 1 ? 2.4 : 1.4, c.S(EDGE, z::PATTERN2));
        }
    }
    c.edgeLines(0, FILL_LIGHT, 3, z::SHADE);
    c.edgeLines(2, EDGE, 1.2, z::EDGE);
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.5 * U, 2, z::GLOW_OUT);
    // snow caps
    for (auto const& sg : c.L.edges(0)) {
        if (sg.side != Side::Top) continue;
        Pts cap{{sg.a.x - 1, sg.a.y - 3}};
        for (double x = sg.a.x; x <= sg.b.x; x += 0.35 * U) cap.push_back({x, sg.a.y + r.f(0.06, 0.18) * U});
        cap.push_back({sg.b.x + 1, sg.b.y + 0.05 * U});
        cap.push_back({sg.b.x + 1, sg.b.y - 3});
        d.fill(cap, c.S(EDGE, z::LINE), 3);
    }
    if (c.s.edgeDeco) {
        c.along(Side::Bottom, 0.45 * U, 0.15 * U, [&](V p, Seg const&) {
            if (!r.chance(0.6)) return;
            double sx = r.f(0.22, 0.36), sy = r.f(0.35, 0.9);
            d.object(gd::obj::SPIKE, p + V{0, -15 * sy}, 180, sx, sy, c.S(r.chance(0.5) ? FILL_LIGHT : EDGE, z::TOPDECO));
        });
    }
    int sp = c.ga.get();
    for (int k = 0; k < c.interiorCells(1) / 60; k++) {
        int i = r.i(0, c.L.width() - 1), j = r.i(0, c.L.height() - 1);
        if (c.L.depth(i, j) < 1) continue;
        V p = c.L.cellCenter(i, j);
        d.rect(p.x, p.y, 7, 1.2, 0, c.S(EDGE, z::FX, {sp}));
        d.rect(p.x, p.y, 1.2, 7, 0, c.S(EDGE, z::FX, {sp}));
    }
    if (c.anim(2)) c.a.flicker(sp, 0.05, 1, r.f(1.5, 2.5));
}


// Dense futuristic tech (Blade of Justice / Edge of Destiny direction):
// machined panels filled with modules, stacked glowing outlines, inner glow,
// slots, brackets, runway lights, gears, fins, all synced to the beat.
void styleCyber(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    double bt = c.beat();
    c.fill(0, NEAR_SIL, z::FILL);
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.9 * U, 4, z::GLOW_OUT);

    GCtx g = c.gctx();
    g.wave = c.waveGroups(6, bt * 4, 0.45);
    for (int k = 0; k < 8; k++) g.seq.push_back(c.ga.get());

    if (c.s.edgeDeco) c.supports();
    c.panels(g, Look::Cyber, 2, c.s.detail >= 3 ? 0.95 : c.s.detail == 2 ? 0.8 : 0.5, z::PANEL, c.s.detail >= 2);
    c.thinParts(g, Look::Cyber, z::PATTERN2);
    c.innerGlow(GLOW_SOFT, 0.45 * U, 3, z::SHADE);

    double waveLen = r.f(1.5, 2.5) * U;
    auto gfor = [&](Seg const&, double x) { int k = (((int)std::floor(x / waveLen)) % 6 + 6) % 6; return Groups{g.wave[k]}; };
    c.edgeLines(0, EDGE, 2.8, z::LINE, nullptr, gfor);
    c.edgeLines(1, ACCENT, 1.6, z::EDGE2);
    c.edgeLines(2, GLOW, 1.0, z::EDGE);
    if (c.s.detail >= 2) c.slots(r.f(3, 5) * U, z::EDGE2 + 1);
    if (c.s.edgeDeco) {
        c.shoulders(FILL_MID, z::FX + 2);
        c.wallPlates(FILL_DARK, z::FX - 1);
        c.thrusters(z::TOPDECO, bt / 2);
    }

    for (auto const& cn : c.L.corners(0)) {
        if (!cn.convex) { d.rect(cn.p.x, cn.p.y, 3.5, 3.5, 45, c.S(EDGE, z::FX)); continue; }
        V in = DC::cornerIn(cn);
        d.bracket(cn.p + in * 4.5, 0.45 * U, 3.4, in.x, in.y, c.S(ACCENT, z::FX));
        d.bracket(cn.p + in * 9.5, 0.25 * U, 1.4, in.x, in.y, c.S(EDGE, z::FX));
        d.line(cn.p + V{in.x * 0.7 * U, in.y * 0.12 * U}, cn.p + V{in.x * 0.12 * U, in.y * 0.7 * U}, 1.4, c.S(ENERGY, z::FX));
        d.diamond(cn.p, 5.5, c.S(EDGE, z::FX + 1));
        if (c.s.glow) d.glow(cn.p, 0.8 * U, c.S(GLOW_SOFT, z::GLOW_OUT), 2);
    }

    // runway lights just inside top edges
    int li = 0;
    for (auto const& sg : c.L.edges(1)) {
        if (sg.side != Side::Top || sg.length() < 1.2 * U) continue;
        for (double x = sg.a.x + 0.5 * U; x < sg.b.x - 0.3 * U; x += 0.75 * U, li++)
            d.rect(x, sg.a.y - 5, 6, 2, 0, c.S(ENERGY, z::FX, g.seqG(li)));
    }

    if (c.s.edgeDeco) {
        for (auto const& sg : c.L.edges(0)) {
            if ((sg.side != Side::Left && sg.side != Side::Right) || sg.length() < 2.5 * U || !r.chance(0.45)) continue;
            V gp{sg.a.x, (sg.a.y + sg.b.y) / 2};
            double gr = std::min(sg.length() * 0.3, r.f(0.6, 1.0) * U);
            int gg = c.ga.get(), cg = c.ga.get();
            d.rect(gp.x, gp.y, 1.2, 1.2, 0, c.S(FILL_DARK, z::FILL - 2, {cg}));
            d.gear(gp, gr, 12, c.S(FILL_MID, z::FILL - 2, {gg}), c.S(FILL_MID, z::FILL - 2, {gg}), c.S(ACCENT, z::FILL - 1, {gg}));
            if (c.anim(1)) c.a.spin(gg, cg, r.f(4, 7), sg.side == Side::Left);
        }
        c.along(Side::Bottom, r.f(2.5, 4) * U, 0.8 * U, [&](V p, Seg const&) {
            if (!r.chance(0.6)) return;
            double fw = r.f(0.5, 0.9) * U, fh = r.f(0.25, 0.5) * U;
            Pts fin{{p.x - fw / 2, p.y}, {p.x + fw / 2, p.y}, {p.x + fw * 0.3, p.y - fh}, {p.x - fw * 0.3, p.y - fh}};
            d.fill(fin, c.S(FILL_DARK, z::TOPDECO), 4);
            d.polyline(fin, 1.4, c.S(EDGE, z::TOPDECO + 1), true);
            d.rect(p.x, p.y - fh * 0.55, fw * 0.4, 1.6, 0, c.S(ENERGY, z::TOPDECO + 1));
            d.diamond({p.x, p.y - fh - 4}, 4, c.S(ACCENT, z::TOPDECO + 1, g.seqG(0)));
            if (c.s.glow) d.rect(p.x, p.y - fh - 0.5 * U, 3, 0.9 * U, 0, c.S(GLOW_SOFT, z::TOPDECO, g.seqG(0)));
        });
    }

    if (c.anim(1)) {
        c.a.sequence(g.seq, bt / 2, 1.0, 0.3);
        auto const& e = c.pal.roles[EDGE];
        c.a.pulseLoop(c.pal.ch(EDGE), mix(e.c, {255, 255, 255}, 0.8), bt, 0.02, bt * 0.75);
        auto const& gl = c.pal.roles[GLOW];
        c.a.pulseLoop(c.pal.ch(GLOW), mix(gl.c, {255, 255, 255}, 0.5), bt * 2, 0.03, bt * 1.5);
        auto const& gs = c.pal.roles[GLOW_SOFT];
        c.a.pulseLoop(c.pal.ch(GLOW_SOFT), mix(gs.c, {255, 255, 255}, 0.3), bt * 4, 0.05, bt * 3);
    }
}

// Nine Circles: black shapes wrapped in concentric bars and nested triangles
// around spikes that switch on and off in sequence (inward or outward), zigzag
// teeth, nested diamonds, strobing colors on the beat.
void styleNineCircles(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    double bt = c.beat();
    c.fill(0, FILL_DARK, z::FILL);
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.7 * U, 3, z::GLOW_OUT);

    bool inward = r.chance(0.5);
    int bands = 3 + c.s.detail;
    std::vector<int> bandGroups;
    for (int k = 0; k < bands; k++) bandGroups.push_back(c.ga.get());
    for (int k = 0; k < bands; k++) {
        int g = bandGroups[k];
        c.edgeLines(k * 2, k % 2 ? ACCENT : EDGE, k == 0 ? 2.6 : 1.8, z::LINE - (k > 0), nullptr, [&](Seg const&, double) { return Groups{g}; });
    }
    std::vector<int> order = bandGroups;
    if (inward) std::reverse(order.begin(), order.end());

    {
        GCtx g = c.gctx();
        g.seq = bandGroups;
        c.panels(g, Look::Circles, bands * 2, 0.7, z::PANEL);
        c.innerGlow(GLOW_SOFT, 0.4 * U, 2, z::SHADE);
    }
    // nested diamonds inside larger areas
    for (auto const& rc : c.L.rects(6)) {
        if (true) break; // replaced by the panel engine above
        if (rc.w() < 1.2 * U || rc.h() < 1.2 * U) continue;
        double s0 = std::min(rc.w(), rc.h()) * 0.42;
        for (double x = rc.x0 + s0; x <= rc.x1 - s0 + 0.1; x += s0 * 2.2) {
            V p{x, rc.c().y};
            for (int k = 0; k < 3; k++) {
                double rr = s0 * (1 - k * 0.3);
                if (rr < 4) break;
                d.polyOutline(p, rr, 4, 0, 1.4, c.S(k % 2 ? ACCENT : EDGE, z::PATTERN, {bandGroups[(k + 1) % bands]}));
            }
        }
    }

    // the circles effect around spikes: nested triangles lit in sequence
    std::vector<int> triGroups{c.ga.get(), c.ga.get(), c.ga.get(), c.ga.get()};
    for (auto const& h : c.L.hazards()) {
        bool up = c.pointsUp(h);
        double base = std::max(h.w, h.h);
        V center{h.x, h.y + (up ? -base * 0.1 : base * 0.1)};
        for (int k = 0; k < 4; k++) {
            double sc = 0.75 + k * 0.35;
            double rr = base * sc;
            d.polyOutline(center, rr, 3, up ? 90 : -90, 1.8, c.S(k % 2 ? ACCENT : EDGE, z::FX, {triGroups[k]}));
        }
        d.glow(center, base * 1.6, c.S(GLOW_SOFT, z::GLOW_OUT, {triGroups[0]}), 2);
    }

    if (c.s.edgeDeco) {
        // zigzag teeth outlines along tops and bottoms
        auto teeth = [&](Side side) {
            for (auto const& sg : c.L.edges(0)) {
                if (sg.side != side || sg.length() < 1.5 * U) continue;
                double dir = side == Side::Top ? 1 : -1;
                double tw = 0.5 * U, th = 0.28 * U;
                Pts z2;
                for (double x = sg.a.x; x <= sg.b.x + 0.1; x += tw / 2) {
                    int k = (int)std::round((x - sg.a.x) / (tw / 2));
                    z2.push_back({x, sg.a.y + dir * (k % 2 ? th : 2)});
                }
                d.polyline(z2, 1.4, c.S(ACCENT, z::TOPDECO, {bandGroups[0]}));
            }
        };
        teeth(Side::Bottom);
        if (c.s.detail >= 3) teeth(Side::Top);
    }

    if (c.anim(1)) {
        c.a.sequence(order, bt / 2, 1.0, 0.12);
        c.a.sequence(inward ? std::vector<int>{triGroups[3], triGroups[2], triGroups[1], triGroups[0]} : triGroups, bt / 2, 1.0, 0.0);
        // strobe the accent through hues of the palette, one per beat x2
        auto acc = c.pal.roles[ACCENT];
        auto en = c.pal.roles[ENERGY];
        c.a.colorSteps(c.pal.ch(ACCENT), {acc.c, en.c, mix(acc.c, {255, 255, 255}, 0.5), mix(acc.c, en.c, 0.5)}, bt * 2, acc.opacity, acc.blend, 0.08);
        auto const& e = c.pal.roles[EDGE];
        c.a.pulseLoop(c.pal.ch(EDGE), {255, 255, 255}, bt, 0.02, bt * 0.6);
        (void)e;
    }
}


// Heavy industrial sci-fi: riveted plates, hazard stripes, pipes, fans,
// vents, rotating warning beacons, hanging pipework.
void styleIndustrial(DC& c) {
    auto& d = c.d;
    auto& r = c.rng;
    double bt = c.beat();
    c.fill(0, FILL_DARK, z::FILL);
    GCtx g = c.gctx();
    g.wave = c.waveGroups(4, bt * 4, 0.4);
    for (int k = 0; k < 6; k++) g.seq.push_back(c.ga.get());
    if (c.s.edgeDeco) c.supports();
    c.panels(g, Look::Industrial, 2, c.s.detail >= 3 ? 0.9 : 0.75, z::PANEL);
    if (c.s.edgeDeco) { c.wallPlates(FILL_MID, z::FX - 1); c.shoulders(FILL_LIGHT, z::FX + 2); c.thrusters(z::TOPDECO, bt); }
    c.thinParts(g, Look::Industrial, z::PATTERN2);
    // hazard bands just inside top edges
    for (auto const& sg : c.L.edges(1)) {
        if (sg.side != Side::Top || sg.length() < 2 * U || !r.chance(0.6)) continue;
        double x0 = sg.a.x + 2, x1 = sg.b.x - 2, y1 = sg.a.y - 1, y0 = y1 - 0.22 * U;
        d.rect((x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0, 0, c.S(NEAR_SIL, z::PATTERN2));
        for (double x = x0 - 8; x < x1; x += 9) {
            V a{x, y0}, b{x + 8, y1};
            if (b.x > x1) b = a + (b - a) * ((x1 - a.x) / (b.x - a.x));
            if (a.x < x0) { double t = (x0 - a.x) / (b.x - a.x); a = a + (b - a) * t; }
            if (b.x > a.x) d.line(a, b, 3.5, c.S(ACCENT, z::PATTERN2 + 1));
        }
    }
    c.edgeLines(0, FILL_LIGHT, 4, z::SHADE, [](Seg const& s) { return s.side == Side::Top || s.side == Side::Left; });
    c.edgeLines(0, NEAR_SIL, 4, z::SHADE, [](Seg const& s) { return s.side == Side::Bottom || s.side == Side::Right; });
    c.edgeLines(0, EDGE, 1.6, z::LINE);
    c.edgeLines(1, DETAIL, 1.0, z::EDGE);
    if (c.s.glow) c.outerGlow(GLOW_SOFT, 0.4 * U, 2, z::GLOW_OUT);
    // warning beacons on top corners
    int bg = c.ga.get();
    for (auto const& cn : c.L.corners(0)) {
        if (!cn.convex || cn.horizontal != Side::Top) continue;
        if (!r.chance(0.6)) continue;
        V in = DC::cornerIn(cn);
        V p = cn.p + V{in.x * 0.45 * U, 0};
        d.rect(p.x, p.y + 2.5, 0.35 * U, 5, 0, c.S(FILL_LIGHT, z::TOPDECO));
        Pts dome = circlePts(p + V{0, 5}, 0.15 * U, 0.15 * U, 10, 0, PI);
        dome.push_back(p + V{-0.15 * U, 5});
        d.fill(dome, c.S(ACCENT, z::TOPDECO + 1, {bg}), 2);
        d.glow(p + V{0, 7}, 1.0 * U, c.S(GLOW, z::TOPDECO, {bg}), 2);
    }
    if (c.anim(1)) c.a.flicker(bg, 0.15, 1, bt);
    if (c.s.edgeDeco) {
        // pipework hanging under platforms
        for (auto const& sg : c.L.edges(0)) {
            if (sg.side != Side::Bottom || sg.length() < 3 * U || !r.chance(0.7)) continue;
            double y = sg.a.y - r.f(0.25, 0.45) * U;
            double x0 = sg.a.x + 0.3 * U, x1 = sg.b.x - 0.3 * U;
            d.rect((x0 + x1) / 2, y, x1 - x0, 5, 0, c.S(FILL_LIGHT, z::TOPDECO));
            d.rect((x0 + x1) / 2, y + 1.5, x1 - x0, 1.2, 0, c.S(EDGE, z::TOPDECO + 1));
            for (double x = x0 + 0.5 * U; x < x1; x += r.f(1.2, 2) * U) {
                d.rect(x, (y + sg.a.y) / 2, 2, sg.a.y - y, 0, c.S(DETAIL, z::TOPDECO));
                d.rect(x, y, 4, 8, 0, c.S(DETAIL, z::TOPDECO + 1));
            }
            if (r.chance(0.5)) {
                V v{r.f(x0 + U, x1 - U), y};
                d.ring(v, 5, 1.6, c.S(ACCENT, z::TOPDECO + 2), 10);
                d.diamond(v, 3, c.S(ACCENT, z::TOPDECO + 2));
            }
        }
    }
    if (c.anim(1)) c.a.sequence(g.seq, bt / 2, 1.0, 0.35);
}

using StyleFn = void (*)(DC&);
std::map<std::string, StyleFn> const& styleTable() {
    static std::map<std::string, StyleFn> t{
        {"neon", styleNeon}, {"tech", styleTech}, {"crystal", styleCrystal}, {"castle", styleCastle},
        {"nature", styleNature}, {"void", styleVoid}, {"lava", styleLava}, {"ice", styleIce},
        {"cyber", styleCyber}, {"ninecircles", styleNineCircles}, {"industrial", styleIndustrial},
    };
    return t;
}

std::string autoTheme(std::string const& style, Rng& r) {
    if (style == "neon") return r.pick(std::vector<std::string>{"neon", "cosmic", "abyss"});
    if (style == "tech") return r.pick(std::vector<std::string>{"abyss", "neon", "cosmic", "mono"});
    if (style == "crystal") return r.pick(std::vector<std::string>{"cosmic", "sakura", "ice", "royal"});
    if (style == "castle") return r.pick(std::vector<std::string>{"royal", "mono", "sunset"});
    if (style == "nature") return r.pick(std::vector<std::string>{"toxic", "sakura", "sunset"});
    if (style == "void") return r.pick(std::vector<std::string>{"mono", "abyss", "cosmic"});
    if (style == "cyber") return r.pick(std::vector<std::string>{"cyber", "cyber", "voltage", "frost"});
    if (style == "ninecircles") return r.pick(std::vector<std::string>{"crimson", "frost", "voltage", "neon"});
    if (style == "industrial") return r.pick(std::vector<std::string>{"hazard", "hazard", "lava", "mono"});
    if (style == "lava") return "lava";
    if (style == "ice") return "ice";
    return "random";
}

} // namespace

std::vector<std::string> const& decoStyles() {
    static std::vector<std::string> v{"random", "cyber", "ninecircles", "industrial", "neon", "tech", "crystal", "castle", "nature", "void", "lava", "ice"};
    return v;
}

std::string resolveDecoStyle(std::string const& style, uint32_t seed) {
    if (styleTable().count(style)) return style;
    Rng r(seed ^ 0x51ED270Bu);
    auto const& v = decoStyles();
    return v[r.i(1, (int)v.size() - 1)];
}

GenResult decorate(std::vector<LayoutObj> const& objs, DecoSettings const& sIn) {
    DecoSettings s = sIn;
    s.detail = std::clamp(s.detail, 1, 3);
    s.motion = std::clamp(s.motion, 0, 2);
    GenResult res;
    Layout L(objs);
    if (L.empty() && L.hazards().empty()) return res;

    std::string style = resolveDecoStyle(s.style, s.seed);
    Rng rng(s.seed);
    std::string theme = s.theme == "auto" ? autoTheme(style, rng) : s.theme;
    Palette pal = makePalette(theme, s.harmony, s.paletteSeed, s.colorStart);

    Writer w(s.solidId);
    Draw d(w);
    d.zLayer = gd::layer::B1; // behind the player
    GroupAlloc ga(s.groupStart);
    Rect b = L.empty() ? Rect{objs[0].x, objs[0].y, objs[0].x, objs[0].y} : L.bounds();
    Anim a(w, ga, {b.x0, b.y1 + 8 * U});
    DC c{s, rng, w, d, a, ga, pal, L};

    double trigX = s.startAtLevelBegin ? 15.0 : std::max(15.0, b.x0 - 15 * U);
    a.atX(trigX, b.y1 + 6 * U + (double)(s.seed % 6) * U, Anim::spawn(a.master(), 0));
    for (int i = 0; i < ROLE_COUNT; i++) {
        auto const& rc = pal.roles[i];
        a.atStart(Anim::color(pal.ch((Role)i), rc.c, 0, rc.opacity, rc.blend));
    }
    if (s.motion >= 1) {
        auto const& en = pal.roles[ENERGY];
        a.pulseLoop(pal.ch(ENERGY), mix(en.c, {255, 255, 255}, 0.5), rng.f(2.2, 3.2), 0.15, 1.3);
    }

    if (!L.empty()) styleTable().at(style)(c);
    c.hazards();

    res.levelString = w.str();
    res.objects = w.count();
    res.triggers = w.count() - w.visualCount();
    res.firstGroup = s.groupStart;
    res.nextGroup = ga.peek();
    res.palette = pal;
    res.placed = {style};
    res.x0 = b.x0;
    res.x1 = b.x1;
    return res;
}

} // namespace inspo
