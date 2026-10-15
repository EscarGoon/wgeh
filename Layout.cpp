#include "Layout.hpp"

#include <algorithm>
#include <deque>

namespace inspo {

V Seg::normal() const {
    switch (side) {
        case Side::Top: return {0, 1};
        case Side::Bottom: return {0, -1};
        case Side::Left: return {-1, 0};
        default: return {1, 0};
    }
}

Layout::Layout(std::vector<LayoutObj> const& objs, double cell) : m_cs(cell) {
    std::vector<Rect> solids;
    double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
    for (auto const& o : objs) {
        if (o.kind == ObjKind::Hazard) { m_hazards.push_back(o); continue; }
        double w = o.w, h = o.h;
        double r = std::fmod(std::fabs(o.rot), 180.0);
        if (std::fabs(r - 90) < 1) std::swap(w, h);
        else if (r > 1 && r < 179) { double s = std::max(w, h) * 0.85; w = h = s; }
        Rect rc{o.x - w / 2, o.y - h / 2, o.x + w / 2, o.y + h / 2};
        if (rc.w() < 2 || rc.h() < 2) continue;
        solids.push_back(rc);
        minX = std::min(minX, rc.x0); minY = std::min(minY, rc.y0);
        maxX = std::max(maxX, rc.x1); maxY = std::max(maxY, rc.y1);
    }
    if (solids.empty()) return;
    m_bounds = {minX, minY, maxX, maxY};

    auto setup = [&] {
        m_x0 = std::floor(minX / m_cs) * m_cs - 2 * m_cs;
        m_y0 = std::floor(minY / m_cs) * m_cs - 2 * m_cs;
        m_w = (int)std::ceil((maxX - m_x0) / m_cs) + 2;
        m_h = (int)std::ceil((maxY - m_y0) / m_cs) + 2;
    };
    setup();
    if ((double)m_w * m_h > 3.0e6) { m_cs = 15; setup(); }
    if ((double)m_w * m_h > 3.0e6) { m_cs = 30; setup(); }

    m_occ.assign((size_t)m_w * m_h, 0);
    for (auto const& rc : solids) {
        int i0 = (int)std::ceil((rc.x0 - m_x0) / m_cs - 0.5 - 1e-6);
        int i1 = (int)std::ceil((rc.x1 - m_x0) / m_cs - 0.5 - 1e-6) - 1;
        int j0 = (int)std::ceil((rc.y0 - m_y0) / m_cs - 0.5 - 1e-6);
        int j1 = (int)std::ceil((rc.y1 - m_y0) / m_cs - 0.5 - 1e-6) - 1;
        for (int j = std::max(0, j0); j <= std::min(m_h - 1, j1); j++)
            for (int i = std::max(0, i0); i <= std::min(m_w - 1, i1); i++)
                m_occ[(size_t)j * m_w + i] = 1;
    }

    // Chebyshev distance to the nearest empty cell
    m_depth.assign((size_t)m_w * m_h, -1);
    std::deque<std::pair<int, int>> q;
    for (int j = 0; j < m_h; j++)
        for (int i = 0; i < m_w; i++)
            if (!m_occ[(size_t)j * m_w + i]) { m_depth[(size_t)j * m_w + i] = 0; q.push_back({i, j}); }
    while (!q.empty()) {
        auto [i, j] = q.front();
        q.pop_front();
        int d = m_depth[(size_t)j * m_w + i];
        for (int dj = -1; dj <= 1; dj++)
            for (int di = -1; di <= 1; di++) {
                int ni = i + di, nj = j + dj;
                if (ni < 0 || nj < 0 || ni >= m_w || nj >= m_h) continue;
                auto& nd = m_depth[(size_t)nj * m_w + ni];
                if (nd < 0) { nd = d + 1; q.push_back({ni, nj}); }
            }
    }

    // regions (4-connected)
    m_region.assign((size_t)m_w * m_h, -1);
    for (int j = 0; j < m_h; j++)
        for (int i = 0; i < m_w; i++) {
            if (!occ(i, j) || m_region[(size_t)j * m_w + i] >= 0) continue;
            int id = (int)m_regions.size();
            Region reg{{1e18, 1e18, -1e18, -1e18}, 0};
            std::vector<std::pair<int, int>> st{{i, j}};
            m_region[(size_t)j * m_w + i] = id;
            while (!st.empty()) {
                auto [ci, cj] = st.back();
                st.pop_back();
                reg.cells++;
                V mn = cellMin(ci, cj);
                reg.bounds.x0 = std::min(reg.bounds.x0, mn.x);
                reg.bounds.y0 = std::min(reg.bounds.y0, mn.y);
                reg.bounds.x1 = std::max(reg.bounds.x1, mn.x + m_cs);
                reg.bounds.y1 = std::max(reg.bounds.y1, mn.y + m_cs);
                int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& n : nb) {
                    int ni = ci + n[0], nj = cj + n[1];
                    if (!occ(ni, nj)) continue;
                    auto& r = m_region[(size_t)nj * m_w + ni];
                    if (r < 0) { r = id; st.push_back({ni, nj}); }
                }
            }
            reg.bounds.region = id;
            m_regions.push_back(reg);
        }
}

bool Layout::occ(int i, int j) const {
    if (i < 0 || j < 0 || i >= m_w || j >= m_h) return false;
    return m_occ[(size_t)j * m_w + i] != 0;
}

int Layout::depth(int i, int j) const {
    if (i < 0 || j < 0 || i >= m_w || j >= m_h) return 0;
    return m_depth[(size_t)j * m_w + i];
}

int Layout::regionAt(int i, int j) const {
    if (i < 0 || j < 0 || i >= m_w || j >= m_h) return -1;
    return m_region[(size_t)j * m_w + i];
}

std::vector<Rect> Layout::rects(int level) const {
    std::vector<Rect> out;
    struct Open { int i0, i1, j0; bool used; };
    std::vector<Open> open;
    for (int j = 0; j <= m_h; j++) {
        std::vector<std::pair<int, int>> runs;
        if (j < m_h) {
            int i = 0;
            while (i < m_w) {
                if (!in(i, j, level)) { i++; continue; }
                int s = i;
                while (i < m_w && in(i, j, level) && regionAt(i, j) == regionAt(s, j)) i++;
                runs.push_back({s, i - 1});
            }
        }
        for (auto& o : open) o.used = false;
        std::vector<Open> next;
        for (auto const& r : runs) {
            bool ext = false;
            for (auto& o : open)
                if (!o.used && o.i0 == r.first && o.i1 == r.second) { o.used = true; next.push_back(o); ext = true; break; }
            if (!ext) next.push_back({r.first, r.second, j, true});
        }
        for (auto const& o : open) {
            if (o.used) continue;
            V a = cellMin(o.i0, o.j0);
            out.push_back({a.x, a.y, m_x0 + (o.i1 + 1) * m_cs, m_y0 + j * m_cs, regionAt(o.i0, o.j0)});
        }
        open = next;
    }
    return out;
}

std::vector<Seg> Layout::edges(int level) const {
    std::vector<Seg> out;
    // horizontal edges
    for (int j = 0; j < m_h; j++) {
        for (int pass = 0; pass < 2; pass++) {
            int dj = pass == 0 ? 1 : -1;
            int i = 0;
            while (i < m_w) {
                if (!(in(i, j, level) && !in(i, j + dj, level))) { i++; continue; }
                int s = i;
                while (i < m_w && in(i, j, level) && !in(i, j + dj, level)) i++;
                double y = m_y0 + (pass == 0 ? j + 1 : j) * m_cs;
                out.push_back({{m_x0 + s * m_cs, y}, {m_x0 + i * m_cs, y}, pass == 0 ? Side::Top : Side::Bottom, regionAt(s, j)});
            }
        }
    }
    // vertical edges
    for (int i = 0; i < m_w; i++) {
        for (int pass = 0; pass < 2; pass++) {
            int di = pass == 0 ? -1 : 1;
            int j = 0;
            while (j < m_h) {
                if (!(in(i, j, level) && !in(i + di, j, level))) { j++; continue; }
                int s = j;
                while (j < m_h && in(i, j, level) && !in(i + di, j, level)) j++;
                double x = m_x0 + (pass == 0 ? i : i + 1) * m_cs;
                out.push_back({{x, m_y0 + s * m_cs}, {x, m_y0 + j * m_cs}, pass == 0 ? Side::Left : Side::Right, regionAt(i, s)});
            }
        }
    }
    return out;
}

std::vector<Corner> Layout::corners(int level) const {
    std::vector<Corner> out;
    for (int j = 0; j < m_h; j++)
        for (int i = 0; i < m_w; i++) {
            if (!in(i, j, level)) continue;
            V mn = cellMin(i, j);
            bool L = in(i - 1, j, level), R = in(i + 1, j, level), Up = in(i, j + 1, level), Dn = in(i, j - 1, level);
            int reg = regionAt(i, j);
            if (!L && !Up) out.push_back({{mn.x, mn.y + m_cs}, Side::Left, Side::Top, true, reg});
            if (!R && !Up) out.push_back({{mn.x + m_cs, mn.y + m_cs}, Side::Right, Side::Top, true, reg});
            if (!L && !Dn) out.push_back({{mn.x, mn.y}, Side::Left, Side::Bottom, true, reg});
            if (!R && !Dn) out.push_back({{mn.x + m_cs, mn.y}, Side::Right, Side::Bottom, true, reg});
            // concave: this cell's top is open but the left neighbour continues upward
            if (!Up && L && in(i - 1, j + 1, level)) out.push_back({{mn.x, mn.y + m_cs}, Side::Left, Side::Top, false, reg});
            if (!Up && R && in(i + 1, j + 1, level)) out.push_back({{mn.x + m_cs, mn.y + m_cs}, Side::Right, Side::Top, false, reg});
            if (!Dn && L && in(i - 1, j - 1, level)) out.push_back({{mn.x, mn.y}, Side::Left, Side::Bottom, false, reg});
            if (!Dn && R && in(i + 1, j - 1, level)) out.push_back({{mn.x + m_cs, mn.y}, Side::Right, Side::Bottom, false, reg});
        }
    return out;
}

} // namespace inspo
