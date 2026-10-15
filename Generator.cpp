#include "Ctx.hpp"

#include <algorithm>
#include <map>

namespace inspo {

std::vector<std::string> const& structureTypes() {
    static std::vector<std::string> t{"citadel", "crystals", "island", "mechcore", "portal", "orbital", "tree", "colonnade",
                                      "reactor", "cybertower", "station", "blade"};
    return t;
}

namespace {
struct TypeInfo { double slot; bool floating; };
TypeInfo info(std::string const& t, Rng& r) {
    if (t == "citadel") return {r.f(9, 11) * U, false};
    if (t == "crystals") return {r.f(8, 10) * U, false};
    if (t == "island") return {r.f(10, 12) * U, true};
    if (t == "mechcore") return {r.f(10, 11) * U, true};
    if (t == "portal") return {r.f(9, 10) * U, false};
    if (t == "orbital") return {r.f(11, 13) * U, true};
    if (t == "tree") return {r.f(9, 11) * U, false};
    if (t == "reactor") return {r.f(10, 11.5) * U, true};
    if (t == "cybertower") return {r.f(9, 11) * U, false};
    if (t == "station") return {r.f(14, 16) * U, true};
    if (t == "blade") return {r.f(7, 9) * U, true};
    return {r.f(12, 15) * U, false}; // colonnade
}
} // namespace

GenResult generate(GenSettings const& sIn) {
    GenSettings s = sIn;
    s.count = std::clamp(s.count, 0, 8);
    s.detail = std::clamp(s.detail, 1, 3);
    s.motion = std::clamp(s.motion, 0, 2);

    Rng rng(s.seed);
    Palette pal = makePalette(s.theme, s.harmony, s.paletteSeed, s.colorStart);
    std::string bgStyle = resolveBgStyle(s.bgStyle, s.seed);
    if (s.background) adjustPaletteForBackground(pal, bgStyle, s.paletteSeed);
    Writer w(s.solidId);
    Draw d(w);
    GroupAlloc ga(s.groupStart);

    double baseY = std::max(0.0, std::round((s.centerY - 5 * U) / U) * U);

    // choose structures
    auto const& all = structureTypes();
    std::vector<std::string> types;
    bool fixed = std::find(all.begin(), all.end(), s.layout) != all.end();
    static std::vector<std::string> const futuristic{"reactor", "cybertower", "station", "blade", "mechcore", "orbital", "portal"};
    static std::vector<std::string> const classic{"citadel", "crystals", "island", "tree", "colonnade", "portal", "orbital", "mechcore"};
    auto const& pool = s.layout == "futuristic" ? futuristic : s.layout == "classic" ? classic : all;
    for (int i = 0; i < s.count; i++) {
        if (fixed) { types.push_back(s.layout); continue; }
        std::string t;
        do { t = rng.pick(pool); } while (!types.empty() && (t == types.back() || (types.size() > 1 && t == types[types.size() - 2])) && pool.size() > 2);
        types.push_back(t);
    }
    std::vector<TypeInfo> infos;
    double total = 0;
    for (auto const& t : types) { infos.push_back(info(t, rng)); total += infos.back().slot; }
    if (!types.empty()) total += (types.size() - 1) * 1.5 * U;
    else total = std::max(20, s.bgWidthBlocks) * U;

    double x0 = std::max(s.centerX - total / 2, 30.0 * U);
    double x1 = x0 + total;
    double trigX = s.startAtLevelBegin ? 15.0 : std::max(15.0, x0 - 18 * U);

    Anim a(w, ga, {x0, baseY + 26 * U});
    Ctx ctx{s, rng, w, d, a, ga, pal};

    a.atX(trigX, baseY + 15 + (double)(s.seed % 8) * U, Anim::spawn(a.master(), 0));

    // palette
    for (int i = 0; i < ROLE_COUNT; i++) {
        auto const& rc = pal.roles[i];
        a.atStart(Anim::color(pal.ch((Role)i), rc.c, 0, rc.opacity, rc.blend));
    }
    if (ctx.anim(1)) {
        auto const& en = pal.roles[ENERGY];
        a.pulseLoop(pal.ch(ENERGY), mix(en.c, {255, 255, 255}, 0.55), rng.f(2.2, 3.2), 0.15, 1.3);
        auto const& gl = pal.roles[GLOW];
        a.colorCycle(pal.ch(GLOW), gl.c, mix(gl.c, pal.roles[ENERGY].c, 0.6), rng.f(2.5, 4), gl.opacity, true);
    }

    if (s.background) genBackground(ctx, bgStyle, x0, x1, baseY, trigX);

    GenResult res;
    res.firstGroup = s.groupStart;
    double x = x0;
    for (size_t i = 0; i < types.size(); i++) {
        auto const& t = types[i];
        double slot = infos[i].slot, cx = x + slot / 2;
        int rootG = ga.get();
        Groups root{rootG};
        bool floating = infos[i].floating;

        if (t == "citadel") genCitadel(ctx, cx, baseY, rng.f(8, 10) * U, root);
        else if (t == "crystals") {
            if (rng.chance(0.35)) { floating = true; genCrystals(ctx, cx, baseY + rng.f(2, 3) * U, rng.f(4.5, 5.5) * U, root); }
            else genCrystals(ctx, cx, baseY, rng.f(5, 6.5) * U, root);
        }
        else if (t == "island") genIsland(ctx, cx, baseY + rng.f(4.5, 5.5) * U, rng.f(6.5, 8) * U, root);
        else if (t == "mechcore") genMechCore(ctx, cx, baseY + rng.f(4.8, 5.6) * U, rng.f(2.4, 2.9) * U, root);
        else if (t == "portal") genPortal(ctx, cx, baseY, rng.f(7, 8.5) * U, root);
        else if (t == "orbital") genOrbital(ctx, cx, baseY + rng.f(6, 7) * U, rng.f(1.9, 2.4) * U, root);
        else if (t == "tree") genTree(ctx, cx, baseY, rng.f(7, 9) * U, root);
        else if (t == "reactor") genReactor(ctx, cx, baseY + rng.f(5, 5.8) * U, rng.f(2.4, 2.9) * U, root);
        else if (t == "cybertower") genCyberTower(ctx, cx, baseY, rng.f(9, 11) * U, root);
        else if (t == "station") genStation(ctx, cx, baseY + rng.f(6.5, 7.5) * U, rng.f(12, 14) * U, root);
        else if (t == "blade") genBlade(ctx, cx, baseY + rng.f(0.8, 1.6) * U, rng.f(8, 9.5) * U, root);
        else genColonnade(ctx, cx, baseY, rng.f(9, 12) * U, root);

        if (floating && ctx.anim(1)) a.bob(rootG, 0, rng.f(0.4, 0.7) * U, rng.f(3, 4.5), rng.f(0, 1));
        if (s.entrance && ctx.anim(1)) {
            double tx = x - 12 * U;
            if (floating) a.entrance(rootG, tx, 0, -7 * U, rng.f(1.2, 1.6), t == "island" ? gd::ease::BACK_OUT : gd::ease::ELASTIC_OUT);
            else a.entrance(rootG, tx, 0, 9 * U, rng.f(1.0, 1.4), gd::ease::BACK_OUT);
        }
        res.placed.push_back(t);
        x += slot + 1.5 * U;
    }

    if (s.background) res.placed.push_back("bg:" + bgStyle);
    res.levelString = w.str();
    res.objects = w.count();
    res.triggers = w.count() - w.visualCount();
    res.nextGroup = ga.peek();
    res.palette = pal;
    res.x0 = x0;
    res.x1 = x1;
    return res;
}

} // namespace inspo
