#pragma once
// Windows headers define these words as macros; drop them so our names work.
#ifdef IN
#undef IN
#endif
#ifdef OUT
#undef OUT
#endif
#ifdef FAR
#undef FAR
#endif
#ifdef NEAR
#undef NEAR
#endif
#ifdef small
#undef small
#endif
// Geometry Dash level-string constants.
// Positions and move offsets are in GD units: 30 units = 1 block.

namespace inspo::gd {

namespace obj {
    constexpr int BLOCK       = 1;
    constexpr int SPIKE       = 8;
    constexpr int SMALL_SPIKE = 39;
    constexpr int SAW_BIG     = 88;
    constexpr int SAW_MED     = 89;
    constexpr int SAW_SMALL   = 98;
    constexpr int COLOR       = 899;
    constexpr int MOVE        = 901;
    constexpr int PULSE       = 1006;
    constexpr int ALPHA       = 1007;
    constexpr int SPAWN       = 1268;
    constexpr int ROTATE      = 1346;
    // The plain solid square is configurable at runtime (setting "solid-id").
    constexpr int DEFAULT_SOLID = 211;
}

namespace k {
    constexpr int ID            = 1;
    constexpr int X             = 2;
    constexpr int Y             = 3;
    constexpr int FLIP_X        = 4;
    constexpr int FLIP_Y        = 5;
    constexpr int ROTATION      = 6;
    constexpr int R             = 7;
    constexpr int G             = 8;
    constexpr int B             = 9;
    constexpr int DURATION      = 10;
    constexpr int BLENDING      = 17;
    constexpr int MAIN_COLOR    = 21;
    constexpr int DETAIL_COLOR  = 22;
    constexpr int TARGET_COLOR  = 23;
    constexpr int Z_LAYER       = 24;
    constexpr int Z_ORDER       = 25;
    constexpr int MOVE_X        = 28;
    constexpr int MOVE_Y        = 29;
    constexpr int EASING        = 30;
    constexpr int SCALE         = 32;
    constexpr int OPACITY       = 35;
    constexpr int FADE_IN       = 45;
    constexpr int HOLD          = 46;
    constexpr int FADE_OUT      = 47;
    constexpr int PULSE_HSV     = 48;
    constexpr int TARGET        = 51;
    constexpr int TARGET_TYPE   = 52;   // pulse: 0 = channel, 1 = group
    constexpr int GROUPS        = 57;
    constexpr int LOCK_PLAYER_X = 58;
    constexpr int SPAWN_TRIG    = 62;
    constexpr int SPAWN_DELAY   = 63;
    constexpr int DONT_FADE     = 64;
    constexpr int DONT_ENTER    = 67;
    constexpr int DEGREES       = 68;
    constexpr int TIMES_360     = 69;
    constexpr int CENTER_GROUP        = 71;
    constexpr int MULTI_TRIG    = 87;
    constexpr int NO_TOUCH      = 121;
    constexpr int SCALE_X       = 128;
    constexpr int SCALE_Y       = 129;
    constexpr int MOVE_X_MOD    = 143;  // 2.2 move trigger "X Mod" (with lock to player X)
}

namespace ease {
    constexpr int NONE = 0, IN_OUT = 1, EASE_IN = 2, EASE_OUT = 3;
    constexpr int ELASTIC_IN_OUT = 4, ELASTIC_IN = 5, ELASTIC_OUT = 6;
    constexpr int BOUNCE_IN_OUT = 7, BOUNCE_IN = 8, BOUNCE_OUT = 9;
    constexpr int EXP_IN_OUT = 10, EXP_IN = 11, EXP_OUT = 12;
    constexpr int SINE_IN_OUT = 13, SINE_IN = 14, SINE_OUT = 15;
    constexpr int BACK_IN_OUT = 16, BACK_IN = 17, BACK_OUT = 18;
}

namespace layer {
    // GD ZLayer values as stored in the level string
    constexpr int B5 = -5, B4 = -3, B3 = -1, B2 = 1, B1 = 3, T1 = 5, T2 = 7, T3 = 9;
}

namespace ch {
    constexpr int BG = 1000, GROUND = 1001, LINE = 1002, GROUND2 = 1009;
}

constexpr double U = 30.0; // one block

} // namespace inspo::gd
