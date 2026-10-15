#pragma once
#include <Geode/Geode.hpp>

#include <functional>
#include <string>
#include <vector>

struct DecoChoice {
    std::string style = "random";
    std::string theme = "auto";
    int detail = 2;
    std::string motion = "full";
    bool glow = true;
    bool edgeDeco = true;
    bool hazardGlow = true;
    bool hideLayout = true;
    bool randomize = false; // set when the player pressed the dice button
};

class DecoratePopup : public geode::Popup {
public:
    using Callback = std::function<void(DecoChoice const&)>;
    static DecoratePopup* create(DecoChoice const& initial, int selectedCount, Callback cb);

protected:
    struct Cycle {
        std::vector<std::string> options;
        size_t index = 0;
        cocos2d::CCLabelBMFont* label = nullptr;
    };

    bool init(DecoChoice const& initial, int selectedCount, Callback cb);
    void addCycle(int tag, char const* name, std::vector<std::string> options, std::string const& current, float y);
    CCMenuItemToggler* addToggle(char const* name, bool on, float x, float y);
    void refresh(int tag);

    void onPrev(cocos2d::CCObject* sender);
    void onNext(cocos2d::CCObject* sender);
    void onDecorate(cocos2d::CCObject*);
    void onDice(cocos2d::CCObject*);
    DecoChoice collect();

    Cycle m_cycles[4];
    CCMenuItemToggler* m_glow = nullptr;
    CCMenuItemToggler* m_edge = nullptr;
    CCMenuItemToggler* m_hazard = nullptr;
    CCMenuItemToggler* m_hide = nullptr;
    Callback m_cb;
};
