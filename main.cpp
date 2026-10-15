// Inspo Generator v4 - Geode mod entry point.
// Editor toolbar:
//   Decorate - decorate the selected layout (opens the style window)
//   Inspire  - generate structures + background around the camera
//   BG Dice  - roll a random background around the camera
//   Reroll   - new shapes for the last batch, same colors
//   Recolor  - same shapes, new palette
//   < > ^ v  - move the last batch
//   Delete   - remove the last batch (and un-hide its layout)
//   ...      - settings

#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/GeodeUI.hpp>

#include "Decorate.hpp"
#include "Generator.hpp"
#include "DecoratePopup.hpp"

#include <functional>
#include <random>

using namespace geode::prelude;

namespace {

constexpr float GROUND_Y = 90.f; // editor object-layer y of the ground line

template <class T>
T setting(char const* key) { return Mod::get()->getSettingValue<T>(key); }

uint32_t randomSeed() {
    static std::random_device rd;
    return rd() & 0x7fffffff;
}

uint32_t seedFromSetting() {
    auto text = setting<std::string>("seed");
    if (text.empty()) return randomSeed();
    char* end = nullptr;
    unsigned long v = std::strtoul(text.c_str(), &end, 10);
    if (end && *end == '\0') return (uint32_t)v;
    return (uint32_t)(std::hash<std::string>{}(text) & 0x7fffffff);
}

int motionFrom(std::string const& m) {
    if (m == "static") return 0;
    if (m == "ambient") return 1;
    return 2;
}

int highestGroupInLevel(LevelEditorLayer* lel) {
    int maxG = 0;
    if (!lel || !lel->m_objects) return 0;
    for (auto obj : CCArrayExt<GameObject*>(lel->m_objects)) {
        if (!obj || !obj->m_groups) continue;
        for (int i = 0; i < obj->m_groupCount && i < 10; i++) maxG = std::max(maxG, (int)(*obj->m_groups)[i]);
    }
    return maxG;
}

// Writes the palette into the level's starting colors so the editor shows the
// real colors right away. Build with -DINSPO_EDITOR_COLORS=OFF to skip.
void applyEditorColors(LevelEditorLayer* lel, inspo::GenResult const& r, bool levelColors) {
#if INSPO_EDITOR_COLORS
    if (!lel || !lel->m_levelSettings || !lel->m_levelSettings->m_effectManager) return;
    auto em = lel->m_levelSettings->m_effectManager;
    auto set = [&](int ch, inspo::Color3 c, float op, bool blend) {
        auto action = em->getColorAction(ch);
        if (!action) return;
        action->m_fromColor = ccc3(c.r, c.g, c.b);
        action->m_color = ccc3(c.r, c.g, c.b);
        action->m_fromOpacity = op;
        action->m_currentOpacity = op;
        action->m_blending = blend;
    };
    for (int i = 0; i < inspo::ROLE_COUNT; i++) {
        auto const& rc = r.palette.roles[i];
        set(r.palette.ch((inspo::Role)i), rc.c, (float)rc.opacity, rc.blend);
    }
    if (levelColors) {
        set(inspo::gd::ch::BG, r.palette.bg, 1.f, false);
        set(inspo::gd::ch::GROUND, r.palette.roles[inspo::NEAR_SIL].c, 1.f, false);
        set(inspo::gd::ch::LINE, r.palette.line, 1.f, true);
    }
    lel->updateLevelColors();
#else
    (void)lel; (void)r; (void)levelColors;
#endif
}

enum class BatchKind { None, Scene, Deco };

} // namespace

class $modify(InspoEditorUI, EditorUI) {
    struct Fields {
        Ref<CCArray> batch;
        BatchKind kind = BatchKind::None;
        inspo::GenSettings scene;
        inspo::DecoSettings deco;
        std::vector<inspo::LayoutObj> layout;
        std::vector<Ref<GameObject>> hidden;
        CCMenu* menu = nullptr;
    };

    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;
        this->buildToolbar();
        return true;
    }

    void showUI(bool show) {
        EditorUI::showUI(show);
        if (m_fields->menu) m_fields->menu->setVisible(show);
    }

    // ------------------------------------------------------------ toolbar
    CCMenuItemSpriteExtra* makeButton(char const* text, SEL_MenuHandler sel, char const* bg = "GJ_button_04.png") {
        auto spr = ButtonSprite::create(text, "bigFont.fnt", bg, .8f);
        spr->setScale(.42f);
        return CCMenuItemSpriteExtra::create(spr, this, sel);
    }

    void buildToolbar() {
        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("inspo-toolbar"_spr);

        std::vector<CCMenuItemSpriteExtra*> items = {
            makeButton("Decorate", menu_selector(InspoEditorUI::onDecorate), "GJ_button_01.png"),
            makeButton("Inspire", menu_selector(InspoEditorUI::onInspire), "GJ_button_01.png"),
            makeButton("BG Dice", menu_selector(InspoEditorUI::onBgDice), "GJ_button_02.png"),
            makeButton("Reroll", menu_selector(InspoEditorUI::onReroll)),
            makeButton("Recolor", menu_selector(InspoEditorUI::onRecolor)),
            makeButton("<", menu_selector(InspoEditorUI::onMoveLeft)),
            makeButton(">", menu_selector(InspoEditorUI::onMoveRight)),
            makeButton("^", menu_selector(InspoEditorUI::onMoveUp)),
            makeButton("v", menu_selector(InspoEditorUI::onMoveDown)),
            makeButton("Delete", menu_selector(InspoEditorUI::onDeleteBatch), "GJ_button_06.png"),
            makeButton("...", menu_selector(InspoEditorUI::onSettings), "GJ_button_05.png"),
        };
        float total = 0, gap = 3.f;
        for (auto it : items) total += it->getScaledContentSize().width + gap;
        total -= gap;
        float x = -total / 2;
        for (auto it : items) {
            float w = it->getScaledContentSize().width;
            it->setPosition({x + w / 2, 0});
            menu->addChild(it);
            x += w + gap;
        }
        float maxW = win.width - 20.f;
        if (total > maxW) menu->setScale(maxW / total);
        menu->setPosition({win.width / 2, win.height - 52.f});
        this->addChild(menu, 100);
        m_fields->menu = menu;
    }

    // ------------------------------------------------------------ batch helpers
    std::vector<GameObject*> liveBatch() {
        std::vector<GameObject*> out;
        if (!m_fields->batch) return out;
        for (auto obj : CCArrayExt<GameObject*>(m_fields->batch.data()))
            if (obj && obj->getParent()) out.push_back(obj);
        return out;
    }

    bool deleteBatch() {
        auto objs = liveBatch();
        if (objs.empty()) return false;
        auto arr = CCArray::create();
        for (auto o : objs) arr->addObject(o);
        this->deselectAll();
        this->selectObjects(arr, true);
        this->onDeleteSelected(nullptr);
        m_fields->batch = Ref<CCArray>();
        return true;
    }

    void unhideLayout() {
        for (auto& o : m_fields->hidden)
            if (auto p = o.data()) p->m_isHide = false;
        m_fields->hidden.clear();
    }

    std::pair<double, double> cameraCenter() {
        auto win = CCDirector::get()->getWinSize();
        auto c = m_editorLayer->m_objectLayer->convertToNodeSpace({win.width / 2, win.height / 2});
        return {c.x, c.y - GROUND_Y};
    }

    int freeGroupStart() { return std::max((int)setting<int64_t>("group-start"), highestGroupInLevel(m_editorLayer) + 1); }

    inspo::GenSettings sceneSettings() {
        inspo::GenSettings s;
        s.theme = setting<std::string>("theme");
        s.harmony = setting<std::string>("harmony");
        s.layout = setting<std::string>("layout");
        s.count = (int)setting<int64_t>("structures");
        s.detail = (int)setting<int64_t>("detail");
        s.motion = motionFrom(setting<std::string>("motion"));
        s.bpm = setting<double>("bpm");
        s.background = setting<bool>("background");
        s.bgStyle = setting<std::string>("bg-style");
        s.bgWidthBlocks = (int)setting<int64_t>("bg-width");
        s.parallax = setting<bool>("parallax");
        s.entrance = setting<bool>("entrance");
        s.setLevelColors = setting<bool>("level-colors");
        s.startAtLevelBegin = setting<bool>("start-at-level-begin");
        s.colorStart = (int)setting<int64_t>("color-start");
        s.solidId = (int)setting<int64_t>("solid-id");
        s.groupStart = freeGroupStart();
        auto [cx, cy] = cameraCenter();
        s.centerX = cx;
        s.centerY = cy;
        return s;
    }

    bool place(inspo::GenResult const& res, uint32_t seed, bool levelColors) {
        if (res.levelString.empty()) {
            Notification::create("Nothing to generate", NotificationIcon::Error)->show();
            return false;
        }
        auto objs = m_editorLayer->createObjectsFromString(res.levelString, true, true);
        if (!objs || objs->count() == 0) {
            Notification::create("Generation failed", NotificationIcon::Error)->show();
            return false;
        }
        m_fields->batch = objs;
        if (setting<bool>("editor-preview-colors")) applyEditorColors(m_editorLayer, res, levelColors);
        this->deselectAll();
        this->selectObjects(objs, true);
        this->updateButtons();
        std::string names;
        for (auto const& p : res.placed) names += (names.empty() ? "" : ", ") + p;
        Notification::create(fmt::format("{} | {} | {} objs | seed {}", names, res.palette.name, res.objects, seed),
                             NotificationIcon::Success, 3.f)->show();
        return true;
    }

    void runScene(inspo::GenSettings const& s) {
        if (s.groupStart > 9500) return (void)Notification::create("Out of free group IDs", NotificationIcon::Error)->show();
        auto res = inspo::generate(s);
        if (place(res, s.seed, s.setLevelColors && s.background)) {
            m_fields->kind = BatchKind::Scene;
            m_fields->scene = s;
        }
    }

    void runDeco(inspo::DecoSettings const& s) {
        if (s.groupStart > 9500) return (void)Notification::create("Out of free group IDs", NotificationIcon::Error)->show();
        auto res = inspo::decorate(m_fields->layout, s);
        if (place(res, s.seed, false)) {
            m_fields->kind = BatchKind::Deco;
            m_fields->deco = s;
        }
    }

    // ------------------------------------------------------------ decorate
    void onDecorate(CCObject*) {
        std::vector<GameObject*> sel;
        if (m_selectedObjects)
            for (auto o : CCArrayExt<GameObject*>(m_selectedObjects)) sel.push_back(o);
        if (sel.empty() && m_selectedObject) sel.push_back(m_selectedObject);

        std::vector<inspo::LayoutObj> layout;
        std::vector<Ref<GameObject>> solids;
        for (auto o : sel) {
            if (!o || o->m_isTrigger || o->m_isNoTouch) continue;
            bool solid = o->m_objectType == GameObjectType::Solid || o->m_objectType == GameObjectType::Slope;
            bool hazard = o->m_objectType == GameObjectType::Hazard;
            if (!solid && !hazard) continue;
            auto rc = o->getObjectRect();
            if (rc.size.width < 1 || rc.size.height < 1) continue;
            inspo::LayoutObj lo;
            lo.x = rc.getMidX();
            lo.y = rc.getMidY() - GROUND_Y;
            lo.w = rc.size.width;
            lo.h = rc.size.height;
            lo.kind = solid ? inspo::ObjKind::Solid : inspo::ObjKind::Hazard;
            layout.push_back(lo);
            if (solid) solids.push_back(o);
        }
        if (layout.empty()) {
            FLAlertLayer::create("Decorate",
                "Select part of your <cy>layout</c> first: swipe-select the blocks (and spikes) you want decorated, then press <cg>Decorate</c>.",
                "OK")->show();
            return;
        }

        DecoChoice initial;
        auto mod = Mod::get();
        initial.style = mod->getSavedValue<std::string>("deco-style", "random");
        initial.theme = mod->getSavedValue<std::string>("deco-theme", "auto");
        initial.detail = mod->getSavedValue<int>("deco-detail", 2);
        initial.motion = mod->getSavedValue<std::string>("deco-motion", "full");
        initial.glow = mod->getSavedValue<bool>("deco-glow", true);
        initial.edgeDeco = mod->getSavedValue<bool>("deco-edge", true);
        initial.hazardGlow = mod->getSavedValue<bool>("deco-hazard", true);
        initial.hideLayout = mod->getSavedValue<bool>("deco-hide", true);

        int count = (int)layout.size();
        auto popup = DecoratePopup::create(initial, count, [this, layout, solids](DecoChoice const& choice) {
            auto mod = Mod::get();
            if (!choice.randomize) {
                mod->setSavedValue<std::string>("deco-style", choice.style);
                mod->setSavedValue<std::string>("deco-theme", choice.theme);
            }
            mod->setSavedValue<int>("deco-detail", choice.detail);
            mod->setSavedValue<std::string>("deco-motion", choice.motion);
            mod->setSavedValue<bool>("deco-glow", choice.glow);
            mod->setSavedValue<bool>("deco-edge", choice.edgeDeco);
            mod->setSavedValue<bool>("deco-hazard", choice.hazardGlow);
            mod->setSavedValue<bool>("deco-hide", choice.hideLayout);

            inspo::DecoSettings s;
            s.style = choice.randomize ? "random" : choice.style;
            s.theme = choice.randomize ? "random" : choice.theme;
            s.harmony = setting<std::string>("harmony");
            s.detail = choice.detail;
            s.motion = motionFrom(choice.motion);
            s.bpm = setting<double>("bpm");
            s.glow = choice.glow;
            s.edgeDeco = choice.edgeDeco;
            s.hazardGlow = choice.hazardGlow;
            s.startAtLevelBegin = setting<bool>("start-at-level-begin");
            s.colorStart = (int)setting<int64_t>("deco-color-start");
            s.solidId = (int)setting<int64_t>("solid-id");
            s.groupStart = freeGroupStart();
            s.seed = seedFromSetting();
            s.paletteSeed = s.seed;

            m_fields->layout = layout;
            m_fields->batch = Ref<CCArray>(); // keep the previous batch; this is a new one
            m_fields->hidden.clear();
            if (choice.hideLayout)
                for (auto const& o : solids)
                    if (auto p = o.data()) { p->m_isHide = true; m_fields->hidden.push_back(o); }
            runDeco(s);
        });
        if (popup) popup->show();
    }

    // ------------------------------------------------------------ scenes
    void onInspire(CCObject*) {
        auto s = sceneSettings();
        s.seed = seedFromSetting();
        s.paletteSeed = s.seed;
        runScene(s);
    }

    void onBgDice(CCObject*) {
        auto s = sceneSettings();
        s.count = 0;
        s.background = true;
        s.seed = randomSeed();
        s.paletteSeed = s.seed;
        runScene(s);
    }

    void onReroll(CCObject*) {
        if (m_fields->kind == BatchKind::None) return onInspire(nullptr);
        deleteBatch();
        if (m_fields->kind == BatchKind::Deco) {
            auto s = m_fields->deco;
            s.seed = randomSeed();
            runDeco(s);
        } else {
            auto s = m_fields->scene;
            s.seed = randomSeed();
            runScene(s);
        }
    }

    void onRecolor(CCObject*) {
        if (m_fields->kind == BatchKind::None) return onInspire(nullptr);
        deleteBatch();
        bool keep = setting<bool>("recolor-keeps-theme");
        if (m_fields->kind == BatchKind::Deco) {
            auto s = m_fields->deco;
            s.paletteSeed = randomSeed();
            if (!keep) s.theme = "random";
            runDeco(s);
        } else {
            auto s = m_fields->scene;
            s.paletteSeed = randomSeed();
            if (!keep) s.theme = "random";
            runScene(s);
        }
    }

    void moveBatch(float dx, float dy) {
        auto objs = liveBatch();
        if (objs.empty()) return (void)Notification::create("No batch to move", NotificationIcon::Info)->show();
        float step = (float)setting<int64_t>("nudge-blocks") * 30.f;
        for (auto o : objs) this->moveObject(o, {dx * step, dy * step});
        m_fields->scene.centerX += dx * step;
        m_fields->scene.centerY += dy * step;
        for (auto& lo : m_fields->layout) { lo.x += dx * step; lo.y += dy * step; }
        this->updateButtons();
    }

    void onMoveLeft(CCObject*)  { moveBatch(-1, 0); }
    void onMoveRight(CCObject*) { moveBatch(1, 0); }
    void onMoveUp(CCObject*)    { moveBatch(0, 1); }
    void onMoveDown(CCObject*)  { moveBatch(0, -1); }

    void onDeleteBatch(CCObject*) {
        if (!deleteBatch()) return (void)Notification::create("No batch to delete", NotificationIcon::Info)->show();
        if (m_fields->kind == BatchKind::Deco) unhideLayout();
        m_fields->kind = BatchKind::None;
    }

    void onSettings(CCObject*) { openSettingsPopup(Mod::get(), false); }
};
