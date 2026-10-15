#include "Engine.hpp"

#include <algorithm>
#include <cstdio>

namespace inspo {

// ======================================================================= math

Pts rotated(Pts const& pts, double degCCW, V pivot) {
    double a = degCCW * PI / 180.0, c = std::cos(a), s = std::sin(a);
    Pts out;
    out.reserve(pts.size());
    for (auto const& p : pts) {
        V d = p - pivot;
        out.push_back({pivot.x + d.x * c - d.y * s, pivot.y + d.x * s + d.y * c});
    }
    return out;
}

Pts translated(Pts const& pts, V by) {
    Pts out;
    out.reserve(pts.size());
    for (auto const& p : pts) out.push_back(p + by);
    return out;
}

Pts mirroredX(Pts const& pts, double axisX) {
    Pts out;
    out.reserve(pts.size());
    for (auto it = pts.rbegin(); it != pts.rend(); ++it) out.push_back({2 * axisX - it->x, it->y});
    return out;
}

Pts circlePts(V c, double rx, double ry, int n, double a0, double a1) {
    Pts out;
    bool full = std::fabs((a1 - a0) - 2 * PI) < 1e-6;
    int count = full ? n : n + 1;
    for (int i = 0; i < count; i++) {
        double a = a0 + (a1 - a0) * i / n;
        out.push_back({c.x + std::cos(a) * rx, c.y + std::sin(a) * ry});
    }
    return out;
}

std::string fnum(double v) {
    if (std::fabs(v - std::round(v)) < 1e-4) return std::to_string((long long)std::llround(v));
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.3f", v);
    std::string s(buf);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

// ===================================================================== writer

void Writer::add(int id, double x, double y, std::vector<Prop> const& props, bool visual) {
    m_out += "1,";
    m_out += std::to_string(id);
    m_out += ",2,";
    m_out += fnum(x);
    m_out += ",3,";
    m_out += fnum(y);
    for (auto const& p : props) {
        m_out += ',';
        m_out += std::to_string(p.key);
        m_out += ',';
        m_out += p.val;
    }
    m_out += ';';
    m_count++;
    if (visual) {
        m_visual++;
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    }
}

// ==================================================================== palette

Color3 hsv(double h, double s, double v) {
    h = std::fmod(std::fmod(h, 360.0) + 360.0, 360.0);
    s = std::clamp(s, 0.0, 1.0);
    v = std::clamp(v, 0.0, 1.0);
    double c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0, 2.0) - 1)), m = v - c;
    double r = 0, g = 0, b = 0;
    if (h < 60)       { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }
    return {(int)std::lround((r + m) * 255), (int)std::lround((g + m) * 255), (int)std::lround((b + m) * 255)};
}

Color3 mix(Color3 a, Color3 b, double t) {
    return {(int)std::lround(a.r + (b.r - a.r) * t), (int)std::lround(a.g + (b.g - a.g) * t),
            (int)std::lround(a.b + (b.b - a.b) * t)};
}

namespace {
struct ThemeDef {
    std::string name;
    double hueMin, hueMax;  // structure hue range
    double sat;             // overall saturation
    double bgHueOffset;     // background hue relative to structure hue
    double accentOffset;    // accent hue relative (NAN = use harmony)
    double bright;          // brightness multiplier for fills
};

std::vector<ThemeDef> const& defs() {
    static std::vector<ThemeDef> d = {
        {"neon",   250, 300, 0.85, 0,   NAN, 1.0},
        {"sunset", 340, 385, 0.75, -20, 40,  1.1},
        {"ice",    190, 215, 0.55, 15,  -20, 1.25},
        {"toxic",  85,  125, 0.80, 20,  -45, 0.95},
        {"lava",   0,   18,  0.90, 0,   28,  0.9},
        {"mono",   0,   360, 0.0,  0,   0,   1.1},
        {"cosmic", 230, 270, 0.70, 15,  180, 1.0},
        {"royal",  265, 285, 0.60, -10, 140, 1.0},
        {"sakura", 320, 345, 0.45, -90, 15,  1.2},
        {"abyss",  180, 200, 0.80, 25,  10,  0.75},
        {"cyber",  208, 222, 0.85, 8,   0,   0.45},
        {"voltage",270, 285, 0.80, -10, -85, 0.45},
        {"crimson",352, 360, 0.90, 0,   0,   0.40},
        {"frost",  222, 238, 0.75, 15,  45,  0.50},
        {"hazard", 200, 220, 0.18, 0,   -165, 1.15},
    };
    return d;
}
} // namespace

std::vector<std::string> const& themeNames() {
    static std::vector<std::string> names = [] {
        std::vector<std::string> n{"random"};
        for (auto const& d : defs()) n.push_back(d.name);
        return n;
    }();
    return names;
}

Palette makePalette(std::string const& theme, std::string const& harmony, uint32_t seed, int baseChannel) {
    Rng rng(seed ^ 0x9E3779B9u);
    auto const& all = defs();

    ThemeDef def;
    bool found = false;
    for (auto const& d : all) if (d.name == theme) { def = d; found = true; }
    if (!found) {
        if (rng.chance(0.6)) {
            def = all[rng.i(0, (int)all.size() - 1)];
        } else {
            double h = rng.f(0, 360);
            def = {"custom", h, h + 25, rng.f(0.5, 0.9), rng.f(-30, 30), NAN, rng.f(0.9, 1.15)};
        }
    }

    double h = rng.f(def.hueMin, def.hueMax);
    double S = def.sat;
    double B = def.bright;
    double hb = h + def.bgHueOffset;

    double off = def.accentOffset;
    std::string hm = harmony;
    if (hm == "analogous") off = rng.sign() * 35;
    else if (hm == "complementary") off = 180;
    else if (hm == "triadic") off = rng.sign() * 120;
    else if (hm == "split") off = rng.sign() * 150;
    else if (std::isnan(off)) off = rng.pick(std::vector<double>{35, -35, 180, 150, -150, 120});
    double ha = h + off;
    double ha2 = ha + rng.f(-25, 25);
    bool mono = def.name == "mono";
    double aS = mono ? 0.0 : std::max(S, 0.55);

    Palette p;
    p.name = def.name;
    p.base = baseChannel;
    auto set = [&](Role r, Color3 c, double op = 1, bool blend = false) { p.roles[r] = {c, op, blend}; };

    set(FILL_DARK,  hsv(h, S * 0.55, 0.19 * B));
    set(FILL_MID,   hsv(h + 6, S * 0.50, 0.33 * B));
    set(FILL_LIGHT, hsv(h + 12, S * 0.42, 0.52 * B));
    set(EDGE,       hsv(ha, aS * 0.35, 0.97));
    set(ACCENT,     hsv(ha, aS * 0.90, 1.0));
    set(GLOW,       hsv(ha, aS * 0.75, 1.0), 0.22, true);
    set(GLOW_SOFT,  hsv(ha, aS * 0.80, 1.0), 0.07, true);
    set(WINDOW,     hsv(ha + 10, aS * 0.45, 1.0), 0.85, true);
    set(ENERGY,     hsv(ha2, aS * 0.70, 1.0), 0.75, true);
    set(DETAIL,     hsv(h, S * 0.35, 0.62 * B));
    set(FAR_SIL,        mix(hsv(hb, S * 0.50, 0.24), hsv(ha, aS * 0.4, 0.3), 0.15));
    set(MID_SIL,    hsv(hb, S * 0.55, 0.14));
    set(NEAR_SIL,   hsv(hb, S * 0.60, 0.07));
    set(SKY1,       hsv(hb, S * 0.70, 0.06));
    set(SKY2,       hsv(hb + 4, S * 0.68, 0.11));
    set(SKY3,       hsv(hb + 10, S * 0.62, 0.18));
    set(SKY4,       mix(hsv(hb + 18, S * 0.55, 0.30), hsv(ha, aS * 0.6, 0.45), 0.4));
    set(CELESTIAL,  hsv(ha, aS * 0.25, 1.0), 0.9, true);
    set(PARTICLE,   hsv(ha, aS * 0.40, 1.0), 0.7, true);
    set(FOG,        hsv(hb + 10, S * 0.40, 0.6), 0.045, true);
    double hp = ha + rng.f(-40, 40);
    set(PLANET1,    hsv(hp, std::max(0.25, aS * 0.45), 0.62));
    set(PLANET2,    hsv(hp + 25, std::max(0.25, aS * 0.55), 0.42));
    set(PLANET3,    hsv(hp - 20, std::max(0.2, aS * 0.5), 0.26));

    p.bg = hsv(hb, S * 0.7, 0.05);
    p.bgAlt = hsv(hb + 12, S * 0.7, 0.09);
    p.ground = hsv(hb, S * 0.6, 0.05);
    p.line = p.roles[EDGE].c;
    return p;
}

// ====================================================================== draw

void Draw::object(int id, V p, double rotCW, double sx, double sy, Style const& s, bool solidHit) {
    std::vector<Prop> props;
    props.reserve(9);
    double r = std::fmod(rotCW, 360.0);
    if (std::fabs(r) > 0.01) props.push_back(P(gd::k::ROTATION, r));
    if (std::fabs(sx - 1) > 1e-3) props.push_back(P(gd::k::SCALE_X, sx));
    if (std::fabs(sy - 1) > 1e-3) props.push_back(P(gd::k::SCALE_Y, sy));
    if (s.ch) {
        props.push_back(P(gd::k::MAIN_COLOR, s.ch));
        props.push_back(P(gd::k::DETAIL_COLOR, s.ch));
    }
    if (zLayer) props.push_back(P(gd::k::Z_LAYER, zLayer));
    if (s.z) props.push_back(P(gd::k::Z_ORDER, s.z));
    if (!s.g.empty()) {
        std::string gs;
        int n = 0;
        for (int g : s.g) {
            if (n++ >= 10) break;
            if (!gs.empty()) gs += '.';
            gs += std::to_string(g);
        }
        props.push_back(P(gd::k::GROUPS, gs));
    }
    if (!solidHit) props.push_back(P(gd::k::NO_TOUCH, 1));
    m_w.add(id, p.x, p.y, props);
}

void Draw::rect(double cx, double cy, double w, double h, double rotCW, Style const& s) {
    w = std::max(w, 0.6);
    h = std::max(h, 0.6);
    double longSide = std::max(w, h);
    if (longSide > maxPiece) {
        int n = (int)std::ceil(longSide / maxPiece);
        double a = -rotCW * PI / 180.0;
        V axis = w >= h ? V{std::cos(a), std::sin(a)} : V{-std::sin(a), std::cos(a)};
        double piece = longSide / n;
        for (int i = 0; i < n; i++) {
            double o = (i + 0.5) * piece - longSide / 2;
            V c{cx + axis.x * o, cy + axis.y * o};
            if (w >= h) rect(c.x, c.y, piece + 0.6, h, rotCW, s);
            else        rect(c.x, c.y, w, piece + 0.6, rotCW, s);
        }
        return;
    }
    object(m_w.solid(), {cx, cy}, rotCW, w / U, h / U, s);
}

void Draw::line(V a, V b, double thick, Style const& s) {
    V d = b - a;
    double len = std::hypot(d.x, d.y);
    if (len < 0.01) return;
    double rot = -std::atan2(d.y, d.x) * 180.0 / PI;
    rect((a.x + b.x) / 2, (a.y + b.y) / 2, len + thick * 0.35, thick, rot, s);
}

void Draw::polyline(Pts const& pts, double thick, Style const& s, bool closed) {
    for (size_t i = 0; i + 1 < pts.size(); i++) line(pts[i], pts[i + 1], thick, s);
    if (closed && pts.size() > 2) line(pts.back(), pts.front(), thick, s);
}

void Draw::fill(Pts const& pts, Style const& s, double strip, std::function<int(double)> const& chAt) {
    if (pts.size() < 3) return;
    double y0 = 1e18, y1 = -1e18;
    for (auto const& p : pts) { y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
    double H = y1 - y0;
    if (H < 0.5) return;
    int n = std::max(1, (int)std::ceil(H / strip));
    double sh = H / n;
    std::vector<double> xs;
    for (int i = 0; i < n; i++) {
        double yc = y0 + (i + 0.5) * sh;
        xs.clear();
        for (size_t j = 0; j < pts.size(); j++) {
            V a = pts[j], b = pts[(j + 1) % pts.size()];
            if ((a.y <= yc && b.y > yc) || (b.y <= yc && a.y > yc)) {
                xs.push_back(a.x + (yc - a.y) * (b.x - a.x) / (b.y - a.y));
            }
        }
        std::sort(xs.begin(), xs.end());
        Style st = chAt ? s.with(chAt(yc)) : s;
        for (size_t j = 0; j + 1 < xs.size(); j += 2) {
            double xa = xs[j], xb = xs[j + 1];
            if (xb - xa < 0.4) continue;
            rect((xa + xb) / 2, yc, xb - xa + 0.8, sh + 0.8, 0, st);
        }
    }
}

void Draw::circle(V c, double r, Style const& s, double strip) {
    fill(circlePts(c, r, r, std::max(14, (int)(r / 2.5))), s, strip);
}

void Draw::ring(V c, double r, double thick, Style const& s, int segs, double a0, double a1, double ry) {
    if (ry < 0) ry = r;
    auto pts = circlePts(c, r, ry, segs, a0, a1);
    bool full = std::fabs((a1 - a0) - 2 * PI) < 1e-6;
    polyline(pts, thick, s, full);
}

void Draw::glow(V c, double size, Style const& s, int layers) {
    // Three squares at 0/30/60 degrees make a near-round dodecagon; stacking
    // shrinking layers on an additive channel gives a soft falloff.
    for (int i = 0; i < layers; i++) {
        double sz = size * (1.0 - i * 0.28);
        if (sz <= 0.5) break;
        double side = sz * 0.82;
        for (double a : {0.0, 30.0, 60.0}) rect(c.x, c.y, side, side, a, s);
    }
}

void Draw::diamond(V c, double size, Style const& s, double rotCW) {
    rect(c.x, c.y, size, size, rotCW, s);
}

void Draw::spike(V p, double rotCW, double scale, Style const& s, bool isSmall) {
    object(isSmall ? gd::obj::SMALL_SPIKE : gd::obj::SPIKE, p, rotCW, scale, scale, s);
}

void Draw::chevron(V p, double size, double dirDeg, double thick, Style const& s) {
    double a = dirDeg * PI / 180.0;
    V f{std::cos(a), std::sin(a)}, n{-f.y, f.x};
    V tip = p + f * (size * 0.35);
    V b1 = p - f * (size * 0.35) + n * (size * 0.5), b2 = p - f * (size * 0.35) - n * (size * 0.5);
    line(b1, tip, thick, s);
    line(b2, tip, thick, s);
}

void Draw::dashes(V a, V b, double dash, double gap, double thick, Style const& s, std::function<Groups(int)> const& groupsFor) {
    V d = b - a;
    double len = std::hypot(d.x, d.y);
    if (len < 1) return;
    V u = d * (1.0 / len);
    int idx = 0;
    for (double t = gap * 0.5; t + dash <= len; t += dash + gap, idx++) {
        Style st = groupsFor ? s.plus(groupsFor(idx)) : s;
        line(a + u * t, a + u * (t + dash), thick, st);
    }
}

void Draw::polyOutline(V c, double r, int sides, double rotDeg, double thick, Style const& s) {
    Pts pts;
    for (int i = 0; i < sides; i++) {
        double a = rotDeg * PI / 180.0 + 2 * PI * i / sides;
        pts.push_back({c.x + std::cos(a) * r, c.y + std::sin(a) * r});
    }
    polyline(pts, thick, s, true);
}

void Draw::bracket(V corner, double len, double thick, double sx, double sy, Style const& s) {
    rect(corner.x + sx * len / 2, corner.y + sy * thick / 2, len, thick, 0, s);
    rect(corner.x + sx * thick / 2, corner.y + sy * len / 2, thick, len, 0, s);
}

void Draw::gear(V p, double r, int teeth, Style const& body, Style const& teethStyle, Style const& hub) {
    for (int i = 0; i < teeth; i++) {
        double a = 2 * PI * i / teeth;
        V q{p.x + std::cos(a) * r * 0.92, p.y + std::sin(a) * r * 0.92};
        rect(q.x, q.y, r * 0.3, r * 0.24, -a * 180.0 / PI, teethStyle);
    }
    circle(p, r * 0.82, body, std::max(2.5, r / 6));
    ring(p, r * 0.5, std::max(1.2, r * 0.08), hub, 16);
    circle(p, r * 0.16, hub, 2.5);
}

// ====================================================================== anim

Anim::Anim(Writer& w, GroupAlloc& ga, V yard, int yardCols)
    : m_w(w), m_ga(ga), m_yard(yard), m_cols(yardCols) {
    m_master = m_ga.get();
}

double Anim::yardTop() const { return m_yard.y + (double)(m_placed / m_cols + 1) * U; }

Trig Anim::move(int g, double dx, double dy, double dur, int ease) {
    std::vector<Prop> p{P(gd::k::TARGET, g), P(gd::k::MOVE_X, dx), P(gd::k::MOVE_Y, dy), P(gd::k::DURATION, dur)};
    if (ease) p.push_back(P(gd::k::EASING, ease));
    return {gd::obj::MOVE, p};
}

Trig Anim::rotate(int g, int center, double degCW, double dur, int ease) {
    std::vector<Prop> p{P(gd::k::TARGET, g), P(gd::k::CENTER_GROUP, center), P(gd::k::DEGREES, degCW), P(gd::k::DURATION, dur)};
    if (ease) p.push_back(P(gd::k::EASING, ease));
    return {gd::obj::ROTATE, p};
}

Trig Anim::color(int ch, Color3 c, double dur, double opacity, bool blend) {
    std::vector<Prop> p{P(gd::k::TARGET_COLOR, ch), P(gd::k::R, c.r), P(gd::k::G, c.g), P(gd::k::B, c.b),
                        P(gd::k::DURATION, dur), P(gd::k::OPACITY, opacity)};
    if (blend) p.push_back(P(gd::k::BLENDING, 1));
    return {gd::obj::COLOR, p};
}

Trig Anim::pulse(int ch, Color3 c, double fadeIn, double hold, double fadeOut) {
    return {gd::obj::PULSE, {P(gd::k::TARGET, ch), P(gd::k::TARGET_TYPE, 0), P(gd::k::PULSE_HSV, 0),
                             P(gd::k::R, c.r), P(gd::k::G, c.g), P(gd::k::B, c.b),
                             P(gd::k::FADE_IN, fadeIn), P(gd::k::HOLD, hold), P(gd::k::FADE_OUT, fadeOut)}};
}

Trig Anim::alpha(int g, double opacity, double dur) {
    return {gd::obj::ALPHA, {P(gd::k::TARGET, g), P(gd::k::OPACITY, opacity), P(gd::k::DURATION, dur)}};
}

Trig Anim::parallax(int g, double xMod, double dur) {
    return {gd::obj::MOVE, {P(gd::k::TARGET, g), P(gd::k::MOVE_X, 0), P(gd::k::MOVE_Y, 0),
                            P(gd::k::LOCK_PLAYER_X, 1), P(gd::k::MOVE_X_MOD, xMod), P(gd::k::DURATION, dur)}};
}

Trig Anim::spawn(int g, double delay) {
    return {gd::obj::SPAWN, {P(gd::k::TARGET, g), P(gd::k::SPAWN_DELAY, delay)}};
}

void Anim::place(Trig const& t, int own) {
    auto props = t.props;
    props.push_back(P(gd::k::GROUPS, own));
    props.push_back(P(gd::k::SPAWN_TRIG, 1));
    props.push_back(P(gd::k::MULTI_TRIG, 1));
    double x = m_yard.x + (double)(m_placed % m_cols) * U;
    double y = m_yard.y + (double)(m_placed / m_cols) * U;
    m_w.add(t.id, x, y, props, false);
    m_placed++;
}

void Anim::atStart(Trig const& t) { place(t, m_master); }

void Anim::atX(double x, double y, Trig const& t) { m_w.add(t.id, x, y, t.props, false); }

void Anim::loop(std::vector<Step> const& steps, double startDelay) {
    if (steps.empty()) return;
    std::vector<int> gs;
    for (size_t i = 0; i < steps.size(); i++) gs.push_back(m_ga.get());
    for (size_t i = 0; i < steps.size(); i++) {
        for (auto const& t : steps[i].trigs) place(t, gs[i]);
        place(spawn(gs[(i + 1) % gs.size()], std::max(0.05, steps[i].wait)), gs[i]);
    }
    atStart(spawn(gs[0], std::max(0.0, startDelay)));
}

void Anim::bob(int g, double dx, double dy, double period, double phase, int ease) {
    atStart(move(g, -dx / 2, -dy / 2, 0));
    loop({{{move(g, dx, dy, period / 2, ease)}, period / 2},
          {{move(g, -dx, -dy, period / 2, ease)}, period / 2}}, phase);
}

void Anim::spin(int g, int center, double period, bool cw) {
    loop({{{rotate(g, center, cw ? 360 : -360, period)}, period}});
}

void Anim::swing(int g, int center, double deg, double period, double phase) {
    atStart(rotate(g, center, -deg, 0));
    loop({{{rotate(g, center, 2 * deg, period / 2, gd::ease::SINE_IN_OUT)}, period / 2},
          {{rotate(g, center, -2 * deg, period / 2, gd::ease::SINE_IN_OUT)}, period / 2}}, phase);
}

void Anim::pulseLoop(int ch, Color3 c, double period, double fadeIn, double fadeOut, double phase) {
    loop({{{pulse(ch, c, fadeIn, 0, fadeOut)}, period}}, phase);
}

void Anim::flicker(int g, double lo, double hi, double period, double phase) {
    loop({{{alpha(g, lo, period / 2)}, period / 2}, {{alpha(g, hi, period / 2)}, period / 2}}, phase);
}

void Anim::rise(int g, double dx, double dy, double period, double phase) {
    atStart(alpha(g, 0, 0));
    loop({{{move(g, dx, dy, period), alpha(g, 1, period * 0.25)}, period * 0.6},
          {{alpha(g, 0, period * 0.38)}, period * 0.4},
          {{move(g, -dx, -dy, 0)}, 0.05}}, phase);
}

void Anim::colorCycle(int ch, Color3 a, Color3 b, double period, double opacity, bool blend) {
    atStart(color(ch, a, 0, opacity, blend));
    loop({{{color(ch, b, period, opacity, blend)}, period}, {{color(ch, a, period, opacity, blend)}, period}});
}

void Anim::sequence(std::vector<int> const& groups, double step, double on, double off, double fade) {
    if (groups.empty()) return;
    for (int g : groups) atStart(alpha(g, off, 0));
    std::vector<Step> steps;
    size_t n = groups.size();
    for (size_t k = 0; k < n; k++)
        steps.push_back({{alpha(groups[k], on, fade), alpha(groups[(k + n - 1) % n], off, fade)}, step});
    loop(steps);
}

void Anim::colorSteps(int ch, std::vector<Color3> const& colors, double step, double opacity, bool blend, double fade) {
    if (colors.empty()) return;
    std::vector<Step> steps;
    for (auto const& c : colors) steps.push_back({{color(ch, c, std::min(fade, step), opacity, blend)}, step});
    loop(steps);
}

void Anim::entrance(int g, double triggerX, double dx, double dy, double dur, int ease) {
    atStart(move(g, -dx, -dy, 0));
    atX(triggerX, m_yard.y - U, move(g, dx, dy, dur, ease));
}

} // namespace inspo
