#pragma once
// Inspo Generator engine: level-string writer, palettes, drawing primitives and
// trigger-based animation helpers. No Geode dependency, so it can be compiled
// and previewed outside the game (see tools/).

#include "GD.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace inspo {

constexpr double PI = 3.14159265358979323846;
using gd::U;

// ---------------------------------------------------------------- basics
struct V { double x = 0, y = 0; };
inline V operator+(V a, V b) { return {a.x + b.x, a.y + b.y}; }
inline V operator-(V a, V b) { return {a.x - b.x, a.y - b.y}; }
inline V operator*(V a, double s) { return {a.x * s, a.y * s}; }
using Pts = std::vector<V>;

Pts rotated(Pts const& pts, double degCCW, V pivot);
Pts translated(Pts const& pts, V by);
Pts mirroredX(Pts const& pts, double axisX);
Pts circlePts(V c, double rx, double ry, int n, double a0 = 0, double a1 = 2 * PI);

std::string fnum(double v);

struct Prop { int key; std::string val; };
inline Prop P(int key, double v) { return {key, fnum(v)}; }
inline Prop P(int key, std::string v) { return {key, std::move(v)}; }

using Groups = std::vector<int>;
inline Groups operator+(Groups a, Groups const& b) { a.insert(a.end(), b.begin(), b.end()); return a; }

class Rng {
public:
    explicit Rng(uint32_t seed) : m_g(seed) {}
    double f(double a, double b) { return std::uniform_real_distribution<double>(a, b)(m_g); }
    int i(int a, int b) { return std::uniform_int_distribution<int>(a, b)(m_g); }
    bool chance(double p) { return f(0, 1) < p; }
    double sign() { return chance(0.5) ? 1.0 : -1.0; }
    template <class T> T const& pick(std::vector<T> const& v) { return v[i(0, (int)v.size() - 1)]; }
    uint32_t next() { return m_g(); }
private:
    std::mt19937 m_g;
};

class GroupAlloc {
public:
    explicit GroupAlloc(int first) : m_next(first) {}
    int get() { int g = m_next++; if (m_next > 9999) m_next = 1; return g; }
    int peek() const { return m_next; }
private:
    int m_next;
};

// ---------------------------------------------------------------- writer
class Writer {
public:
    explicit Writer(int solidId) : m_solid(solidId) {}
    void add(int id, double x, double y, std::vector<Prop> const& props, bool visual = true);
    int solid() const { return m_solid; }
    std::string const& str() const { return m_out; }
    size_t count() const { return m_count; }
    size_t visualCount() const { return m_visual; }
    double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
private:
    int m_solid;
    std::string m_out;
    size_t m_count = 0, m_visual = 0;
};

// ---------------------------------------------------------------- palette
struct Color3 { int r = 0, g = 0, b = 0; };
Color3 hsv(double hDeg, double s, double v);
Color3 mix(Color3 a, Color3 b, double t);

enum Role {
    FILL_DARK, FILL_MID, FILL_LIGHT, EDGE, ACCENT, GLOW, GLOW_SOFT, WINDOW, ENERGY,
    DETAIL, FAR_SIL, MID_SIL, NEAR_SIL, SKY1, SKY2, SKY3, SKY4, CELESTIAL, PARTICLE, FOG,
    PLANET1, PLANET2, PLANET3,
    ROLE_COUNT
};

struct RoleColor { Color3 c; double opacity = 1; bool blend = false; };

struct Palette {
    std::string name;
    std::array<RoleColor, ROLE_COUNT> roles;
    Color3 bg, bgAlt, ground, line;
    int base = 970;
    int ch(Role r) const { return base + (int)r; }
};

// theme: random|neon|sunset|ice|toxic|lava|mono|cosmic|royal|sakura|abyss
// harmony: auto|analogous|complementary|triadic|split
Palette makePalette(std::string const& theme, std::string const& harmony, uint32_t seed, int baseChannel);
std::vector<std::string> const& themeNames();

// ---------------------------------------------------------------- drawing
struct Style {
    int ch = 0;
    int z = 0;
    Groups g;
    Style with(int newCh) const { Style s = *this; s.ch = newCh; return s; }
    Style at(int newZ) const { Style s = *this; s.z = newZ; return s; }
    Style plus(Groups const& extra) const { Style s = *this; s.g = s.g + extra; return s; }
};

class Draw {
public:
    explicit Draw(Writer& w) : m_w(w) {}
    // Axis-aligned or rotated rectangle (rotation is GD-style: clockwise degrees).
    void rect(double cx, double cy, double w, double h, double rotCW, Style const& s);
    void line(V a, V b, double thick, Style const& s);
    void polyline(Pts const& pts, double thick, Style const& s, bool closed = false);
    // Scanline-filled polygon (concave OK). chAt may pick a channel per strip.
    void fill(Pts const& pts, Style const& s, double strip = 6,
              std::function<int(double y)> const& chAt = nullptr);
    void circle(V c, double r, Style const& s, double strip = 5);
    void ring(V c, double r, double thick, Style const& s, int segs = 24,
              double a0 = 0, double a1 = 2 * PI, double ry = -1);
    // Soft glow: stacked square+diamond layers on a blending channel.
    void glow(V c, double size, Style const& s, int layers = 3);
    void diamond(V c, double size, Style const& s, double rotCW = 45);
    void spike(V p, double rotCW, double scale, Style const& s, bool isSmall = false);
    // Tech primitives
    void chevron(V p, double size, double dirDeg, double thick, Style const& s);          // ">" pointing dirDeg (0 = right)
    void dashes(V a, V b, double dash, double gap, double thick, Style const& s,
                std::function<Groups(int idx)> const& groupsFor = nullptr);
    void polyOutline(V c, double r, int sides, double rotDeg, double thick, Style const& s); // regular polygon outline
    void bracket(V corner, double len, double thick, double sx, double sy, Style const& s);  // L shape into (sx, sy)
    void gear(V c, double r, int teeth, Style const& body, Style const& teethStyle, Style const& hub);
    // Raw object with color/group/z.
    void object(int id, V p, double rotCW, double sx, double sy, Style const& s, bool solidHit = false);
    double maxPiece = 10 * U;
    int zLayer = 0; // 0 = object default, else gd::layer value
private:
    Writer& m_w;
};

// ---------------------------------------------------------------- animation
struct Trig { int id; std::vector<Prop> props; };
struct Step { std::vector<Trig> trigs; double wait; };

class Anim {
public:
    Anim(Writer& w, GroupAlloc& ga, V yard, int yardCols = 12);

    int master() const { return m_master; }

    // Builders
    static Trig move(int g, double dx, double dy, double dur, int ease = 0);
    static Trig rotate(int g, int center, double degCW, double dur, int ease = 0);
    static Trig color(int ch, Color3 c, double dur, double opacity = 1, bool blend = false);
    static Trig pulse(int ch, Color3 c, double fadeIn, double hold, double fadeOut);
    static Trig alpha(int g, double opacity, double dur);
    static Trig parallax(int g, double xMod, double dur = 9999);
    static Trig spawn(int g, double delay);

    // Fires once when the master group is spawned.
    void atStart(Trig const& t);
    // Fires when the player passes x.
    void atX(double x, double y, Trig const& t);
    // Repeating sequence, kicked off at start.
    void loop(std::vector<Step> const& steps, double startDelay = 0);

    // Composite motions (all loop forever)
    void bob(int g, double dx, double dy, double period, double phase = 0, int ease = gd::ease::SINE_IN_OUT);
    void spin(int g, int center, double period, bool clockwise = true);
    void swing(int g, int center, double deg, double period, double phase = 0);
    void pulseLoop(int ch, Color3 c, double period, double fadeIn, double fadeOut, double phase = 0);
    void flicker(int g, double lo, double hi, double period, double phase = 0);
    void rise(int g, double dx, double dy, double period, double phase = 0);
    void colorCycle(int ch, Color3 a, Color3 b, double period, double opacity = 1, bool blend = false);
    void entrance(int g, double triggerX, double dx, double dy, double dur, int ease);
    // Lights groups one after another with hard on/off switching (the "circles" chase).
    void sequence(std::vector<int> const& groups, double step, double on = 1.0, double off = 0.1, double fade = 0.0);
    // Cycles a channel through several colors.
    void colorSteps(int ch, std::vector<Color3> const& colors, double step, double opacity = 1, bool blend = false, double fade = 0.3);

    size_t triggerCount() const { return m_placed; }
    double yardTop() const;
private:
    void place(Trig const& t, int ownGroup);
    Writer& m_w;
    GroupAlloc& m_ga;
    V m_yard;
    int m_cols;
    int m_master;
    size_t m_placed = 0;
};

} // namespace inspo
