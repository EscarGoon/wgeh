#include "DecoratePopup.hpp"

#include "Decorate.hpp"

using namespace geode::prelude;

namespace {
enum { STYLE = 0, THEME = 1, DETAIL = 2, MOTION = 3 };

std::string pretty(std::string s) {
    if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
    return s;
}
} // namespace

DecoratePopup* DecoratePopup::create(DecoChoice const& initial, int selectedCount, Callback cb) {
    auto ret = new DecoratePopup();
    if (ret->init(initial, selectedCount, std::move(cb))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool DecoratePopup::init(DecoChoice const& initial, int selectedCount, Callback cb) {
    if (!Popup::init(370.f, 270.f, "GJ_square01.png")) return false;
    m_cb = std::move(cb);
    this->setTitle("Decorate Selection");

    auto info = CCLabelBMFont::create(fmt::format("{} layout objects selected", selectedCount).c_str(), "chatFont.fnt");
    info->setScale(.7f);
    info->setColor({200, 220, 255});
    m_mainLayer->addChildAtPosition(info, Anchor::Top, ccp(0, -42));

    std::vector<std::string> themes{"auto"};
    for (auto const& t : inspo::themeNames()) themes.push_back(t);

    addCycle(STYLE, "Style", inspo::decoStyles(), initial.style, 62);
    addCycle(THEME, "Colors", themes, initial.theme, 38);
    addCycle(DETAIL, "Detail", {"1", "2", "3"}, std::to_string(initial.detail), 14);
    addCycle(MOTION, "Motion", {"static", "ambient", "full"}, initial.motion, -10);

    m_glow = addToggle("Outer glow", initial.glow, -160, -42);
    m_edge = addToggle("Edge details", initial.edgeDeco, 20, -42);
    m_hazard = addToggle("Spike glow", initial.hazardGlow, -160, -66);
    m_hide = addToggle("Hide layout in game", initial.hideLayout, 20, -66);

    auto diceSpr = ButtonSprite::create("Dice", "goldFont.fnt", "GJ_button_02.png", .8f);
    diceSpr->setScale(.8f);
    auto dice = CCMenuItemSpriteExtra::create(diceSpr, this, menu_selector(DecoratePopup::onDice));
    m_buttonMenu->addChildAtPosition(dice, Anchor::Bottom, ccp(-75, 26));

    auto goSpr = ButtonSprite::create("Decorate", "goldFont.fnt", "GJ_button_01.png", .8f);
    goSpr->setScale(.8f);
    auto go = CCMenuItemSpriteExtra::create(goSpr, this, menu_selector(DecoratePopup::onDecorate));
    m_buttonMenu->addChildAtPosition(go, Anchor::Bottom, ccp(60, 26));
    return true;
}

void DecoratePopup::addCycle(int tag, char const* name, std::vector<std::string> options, std::string const& current, float y) {
    auto& c = m_cycles[tag];
    c.options = std::move(options);
    c.index = 0;
    for (size_t i = 0; i < c.options.size(); i++)
        if (c.options[i] == current) c.index = i;

    auto nameLabel = CCLabelBMFont::create(name, "bigFont.fnt");
    nameLabel->setScale(.42f);
    nameLabel->setAnchorPoint({0.f, .5f});
    m_mainLayer->addChildAtPosition(nameLabel, Anchor::Center, ccp(-160, y));

    auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    prevSpr->setScale(.4f);
    auto prev = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(DecoratePopup::onPrev));
    prev->setTag(tag);
    m_buttonMenu->addChildAtPosition(prev, Anchor::Center, ccp(-20, y));

    auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    nextSpr->setScale(.4f);
    nextSpr->setFlipX(true);
    auto next = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(DecoratePopup::onNext));
    next->setTag(tag);
    m_buttonMenu->addChildAtPosition(next, Anchor::Center, ccp(140, y));

    c.label = CCLabelBMFont::create("", "goldFont.fnt");
    m_mainLayer->addChildAtPosition(c.label, Anchor::Center, ccp(60, y));
    refresh(tag);
}

CCMenuItemToggler* DecoratePopup::addToggle(char const* name, bool on, float x, float y) {
    auto t = CCMenuItemToggler::createWithStandardSprites(this, nullptr, .55f);
    t->toggle(on);
    m_buttonMenu->addChildAtPosition(t, Anchor::Center, ccp(x + 10, y));
    auto label = CCLabelBMFont::create(name, "bigFont.fnt");
    label->setScale(.32f);
    label->setAnchorPoint({0.f, .5f});
    m_mainLayer->addChildAtPosition(label, Anchor::Center, ccp(x + 26, y));
    return t;
}

void DecoratePopup::refresh(int tag) {
    auto& c = m_cycles[tag];
    if (!c.label || c.options.empty()) return;
    c.label->setString(pretty(c.options[c.index]).c_str());
    c.label->setScale(.55f);
}

void DecoratePopup::onPrev(CCObject* sender) {
    int tag = static_cast<CCNode*>(sender)->getTag();
    if (tag < 0 || tag > 3) return;
    auto& c = m_cycles[tag];
    c.index = (c.index + c.options.size() - 1) % c.options.size();
    refresh(tag);
}

void DecoratePopup::onNext(CCObject* sender) {
    int tag = static_cast<CCNode*>(sender)->getTag();
    if (tag < 0 || tag > 3) return;
    auto& c = m_cycles[tag];
    c.index = (c.index + 1) % c.options.size();
    refresh(tag);
}

DecoChoice DecoratePopup::collect() {
    DecoChoice d;
    d.style = m_cycles[STYLE].options[m_cycles[STYLE].index];
    d.theme = m_cycles[THEME].options[m_cycles[THEME].index];
    d.detail = std::atoi(m_cycles[DETAIL].options[m_cycles[DETAIL].index].c_str());
    d.motion = m_cycles[MOTION].options[m_cycles[MOTION].index];
    d.glow = m_glow->isToggled();
    d.edgeDeco = m_edge->isToggled();
    d.hazardGlow = m_hazard->isToggled();
    d.hideLayout = m_hide->isToggled();
    return d;
}

void DecoratePopup::onDecorate(CCObject*) {
    auto choice = collect();
    auto cb = m_cb;
    this->onClose(nullptr);
    if (cb) cb(choice);
}

void DecoratePopup::onDice(CCObject*) {
    auto choice = collect();
    choice.randomize = true;
    auto cb = m_cb;
    this->onClose(nullptr);
    if (cb) cb(choice);
}
