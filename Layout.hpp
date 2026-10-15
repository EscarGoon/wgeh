#pragma once
// Layout analysis: turns selected gameplay objects into a cell grid and
// derives the shapes a decorator needs (fill rectangles, merged edges at any
// inset depth, corners, regions).

#include "Engine.hpp"

#include <vector>

namespace inspo {

enum class ObjKind { Solid, Hazard };

struct LayoutObj {
    double x = 0, y = 0, w = 30, h = 30; // center + size, level-string coords (ground y = 0)
    double rot = 0;
    ObjKind kind = ObjKind::Solid;
};

enum class Side { Top, Bottom, Left, Right };

struct Seg {
    V a, b;          // a -> b runs left->right (horizontal) or bottom->top (vertical)
    Side side;
    int region = -1;
    double length() const { return std::hypot(b.x - a.x, b.y - a.y); }
    V normal() const;    // outward normal
};

struct Rect { double x0, y0, x1, y1; int region = -1; double w() const { return x1 - x0; } double h() const { return y1 - y0; } V c() const { return {(x0 + x1) / 2, (y0 + y1) / 2}; } };

struct Corner { V p; Side vertical; Side horizontal; bool convex; int region = -1; };

struct Region { Rect bounds; int cells = 0; };

class Layout {
public:
    explicit Layout(std::vector<LayoutObj> const& objs, double cell = 7.5);

    bool empty() const { return m_regions.empty(); }
    double cell() const { return m_cs; }
    int width() const { return m_w; }
    int height() const { return m_h; }
    bool occ(int i, int j) const;
    int depth(int i, int j) const;           // 0 = empty, 1 = edge cell, 2 = one in...
    int regionAt(int i, int j) const;
    V cellCenter(int i, int j) const { return {m_x0 + (i + 0.5) * m_cs, m_y0 + (j + 0.5) * m_cs}; }
    V cellMin(int i, int j) const { return {m_x0 + i * m_cs, m_y0 + j * m_cs}; }

    // Shapes at inset depth `level` (0 = the real outline, 1 = one cell in, ...)
    std::vector<Rect> rects(int level = 0) const;
    std::vector<Seg> edges(int level = 0) const;
    std::vector<Corner> corners(int level = 0) const;

    std::vector<Region> const& regions() const { return m_regions; }
    std::vector<LayoutObj> const& hazards() const { return m_hazards; }
    Rect bounds() const { return m_bounds; }

private:
    bool in(int i, int j, int level) const { return occ(i, j) && depth(i, j) > level; }
    double m_cs, m_x0 = 0, m_y0 = 0;
    int m_w = 0, m_h = 0;
    std::vector<unsigned char> m_occ;
    std::vector<int> m_depth, m_region;
    std::vector<Region> m_regions;
    std::vector<LayoutObj> m_hazards;
    Rect m_bounds{0, 0, 0, 0};
};

} // namespace inspo
