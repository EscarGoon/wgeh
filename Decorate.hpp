#pragma once
#include "Generator.hpp"
#include "Layout.hpp"

namespace inspo {

struct DecoSettings {
    std::string style = "random";   // random | neon | tech | crystal | castle | nature | void | lava | ice
    std::string theme = "auto";     // auto picks a theme that suits the style
    std::string harmony = "auto";
    int detail = 2;                 // 1..3
    int motion = 2;                 // 0 static, 1 ambient, 2 full
    double bpm = 140;               // beat-synced pulses
    bool glow = true;               // outer glow around platforms
    bool edgeDeco = true;           // hanging / top details (vines, icicles, crystals...)
    bool hazardGlow = true;         // glow behind selected spikes/saws
    bool startAtLevelBegin = true;
    int groupStart = 500;
    int colorStart = 940;
    int solidId = gd::obj::DEFAULT_SOLID;
    uint32_t seed = 1, paletteSeed = 1;
};

std::vector<std::string> const& decoStyles();
std::string resolveDecoStyle(std::string const& style, uint32_t seed);
GenResult decorate(std::vector<LayoutObj> const& objs, DecoSettings const& s);

} // namespace inspo
