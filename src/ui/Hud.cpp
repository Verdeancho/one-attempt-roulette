#include "Hud.hpp"
#include "SessionPanel.hpp"
#include <Geode/ui/ColorPickPopup.hpp>
#include <Geode/ui/OverlayManager.hpp>

// ---------------------------------------------------------------------------
// Configuracion
// ---------------------------------------------------------------------------

static char const* prefixFor(OverlayTarget target) {
    switch (target) {
        case OverlayTarget::Panel: return "panel-";
        case OverlayTarget::Toasts: return "toast-";
        default: return "hud-";
    }
}

static char const* TOAST_KEYS[TOAST_KIND_COUNT] = { "results", "warnings", "stats", "errors" };
static char const* TOAST_NAMES[TOAST_KIND_COUNT] = { "Results", "Warnings", "Statistics", "Errors" };

static char const* COLOR_KEYS[COLOR_SLOT_COUNT] = { "text", "progress", "total", "mean", "stddev" };
static char const* COLOR_NAMES[COLOR_SLOT_COUNT] = { "Text", "Progress", "Total", "Mean", "Std. dev." };

char const* colorSlotName(int slot) {
    return COLOR_NAMES[std::clamp(slot, 0, COLOR_SLOT_COUNT - 1)];
}

OverlayConfig OverlayConfig::defaults(OverlayTarget target) {
    OverlayConfig c;
    c.colors[(int)ColorSlot::Text] = { 255, 255, 255 };
    c.colors[(int)ColorSlot::Progress] = { 130, 210, 255 };
    c.colors[(int)ColorSlot::Total] = { 255, 225, 90 };
    c.colors[(int)ColorSlot::Mean] = { 140, 255, 150 };
    c.colors[(int)ColorSlot::StdDev] = { 210, 175, 255 };
    if (target == OverlayTarget::Panel) {
        c.x = 0.125f;
        c.y = 0.47f;
        c.opacity = 1.f;
        c.colors[(int)ColorSlot::Text] = { 255, 205, 80 };
    }
    if (target == OverlayTarget::Toasts) {
        // por defecto un poco mas abajo que los avisos normales de Geode
        c.x = 0.5f;
        c.y = 0.13f;
        c.opacity = 1.f;
    }
    return c;
}

static int colorToInt(ccColor3B c) {
    return (c.r << 16) | (c.g << 8) | c.b;
}

static ccColor3B intToColor(int v) {
    return { (GLubyte)((v >> 16) & 0xFF), (GLubyte)((v >> 8) & 0xFF), (GLubyte)(v & 0xFF) };
}

OverlayConfig OverlayConfig::load(OverlayTarget target) {
    auto mod = Mod::get();
    auto p = std::string(prefixFor(target));
    auto c = defaults(target);
    c.x = (float)mod->getSavedValue<double>(p + "x", c.x);
    c.y = (float)mod->getSavedValue<double>(p + "y", c.y);
    c.scale = (float)mod->getSavedValue<double>(p + "scale", c.scale);
    c.opacity = (float)mod->getSavedValue<double>(p + "opacity", c.opacity);
    c.collapsed = mod->getSavedValue<bool>(p + "collapsed", c.collapsed);
    c.background = mod->getSavedValue<bool>(p + "background", c.background);
    c.liveUpdate = mod->getSavedValue<bool>(p + "live", c.liveUpdate);
    for (int i = 0; i < TOAST_KIND_COUNT; i++) {
        c.toasts[i] = mod->getSavedValue<bool>(p + "on-" + TOAST_KEYS[i], c.toasts[i]);
    }
    for (int i = 0; i < COLOR_SLOT_COUNT; i++) {
        c.colors[i] = intToColor(mod->getSavedValue<int>(p + "color-" + COLOR_KEYS[i], colorToInt(c.colors[i])));
    }
    c.x = std::clamp(c.x, 0.f, 1.f);
    c.y = std::clamp(c.y, 0.f, 1.f);
    c.scale = std::clamp(c.scale, 0.5f, 2.5f);
    c.opacity = std::clamp(c.opacity, 0.2f, 1.f);
    return c;
}

void OverlayConfig::save(OverlayTarget target) const {
    auto mod = Mod::get();
    auto p = std::string(prefixFor(target));
    mod->setSavedValue<double>(p + "x", x);
    mod->setSavedValue<double>(p + "y", y);
    mod->setSavedValue<double>(p + "scale", scale);
    mod->setSavedValue<double>(p + "opacity", opacity);
    mod->setSavedValue<bool>(p + "collapsed", collapsed);
    mod->setSavedValue<bool>(p + "background", background);
    mod->setSavedValue<bool>(p + "live", liveUpdate);
    for (int i = 0; i < TOAST_KIND_COUNT; i++) {
        mod->setSavedValue<bool>(p + "on-" + TOAST_KEYS[i], toasts[i]);
    }
    for (int i = 0; i < COLOR_SLOT_COUNT; i++) {
        mod->setSavedValue<int>(p + "color-" + COLOR_KEYS[i], colorToInt(colors[i]));
    }
}

void placeOnScreen(CCNode* node, OverlayConfig const& cfg) {
    auto size = node->getScaledContentSize();
    auto win = CCDirector::get()->getWinSize();
    float px = std::clamp(cfg.x * win.width, size.width / 2, win.width - size.width / 2);
    float py = std::clamp(cfg.y * win.height, size.height / 2, win.height - size.height / 2);
    node->setPosition({ px, py });
}

// ---------------------------------------------------------------------------
// Avisos (como los de Geode, pero se pueden mover y desactivar por tipo)
// ---------------------------------------------------------------------------

static Ref<CCNodeRGBA> s_toast;

static CCSprite* toastIcon(NotificationIcon icon) {
    char const* frame = nullptr;
    switch (icon) {
        case NotificationIcon::Success: frame = "GJ_completesIcon_001.png"; break;
        case NotificationIcon::Warning: frame = "geode.loader/info-alert.png"; break;
        case NotificationIcon::Error: frame = "GJ_deleteIcon_001.png"; break;
        case NotificationIcon::Info: frame = "GJ_infoIcon_001.png"; break;
        default: return nullptr;
    }
    auto spr = CCSprite::createWithSpriteFrameName(frame);
    if (!spr && icon == NotificationIcon::Warning) spr = CCSprite::createWithSpriteFrameName("exMark_001.png");
    return spr;
}

void showToast(ToastKind kind, std::string const& text, NotificationIcon icon, float time, bool force) {
    auto cfg = OverlayConfig::load(OverlayTarget::Toasts);
    if (!force && !cfg.toasts[(int)kind]) return;

    // solo un aviso a la vez: el nuevo sustituye al anterior
    if (s_toast) {
        s_toast->stopAllActions();
        s_toast->removeFromParent();
        s_toast = nullptr;
    }

    float s = cfg.scale;
    float pad = 6.f * s;
    float gap = 5.f * s;

    auto toast = CCNodeRGBA::create();
    toast->setCascadeOpacityEnabled(true);
    toast->setAnchorPoint({ 0.5f, 0.5f });

    auto bg = NineSlice::create("square02_small.png");
    bg->setColor({ 0, 0, 0 });
    bg->setOpacity((GLubyte)(150 * cfg.opacity));
    toast->addChild(bg);

    auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    label->limitLabelWidth(330.f * s, 0.45f * s, 0.1f);
    label->setAnchorPoint({ 0.f, 0.5f });
    toast->addChild(label);

    auto spr = toastIcon(icon);
    float iconSize = 17.f * s;
    if (spr) {
        fitNode(spr, iconSize);
        toast->addChild(spr);
    }

    auto labelSize = label->getScaledContentSize();
    float contentW = labelSize.width + (spr ? iconSize + gap : 0.f);
    auto size = CCSize(contentW + pad * 2, std::max(labelSize.height, spr ? iconSize : 0.f) + pad * 2);
    toast->setContentSize(size);
    bg->setContentSize(size);
    bg->setPosition(size / 2);
    float x = pad;
    if (spr) {
        spr->setPosition({ x + iconSize / 2, size.height / 2 });
        x += iconSize + gap;
    }
    label->setPosition({ x, size.height / 2 });

    placeOnScreen(toast, cfg);
    toast->setOpacity(0);
    toast->setScale(0.7f);
    OverlayManager::get()->addChild(toast, 1000);
    s_toast = toast;

    toast->runAction(CCSequence::create(
        CCSpawn::create(CCFadeTo::create(0.2f, (GLubyte)(255 * cfg.opacity)), CCEaseBackOut::create(CCScaleTo::create(0.25f, 1.f)), nullptr),
        CCDelayTime::create(time),
        CCFadeOut::create(0.4f),
        CallFuncExt::create([toast] {
            toast->removeFromParent();
            if (s_toast.data() == toast) s_toast = nullptr;
        }),
        nullptr
    ));
}

// ---------------------------------------------------------------------------
// Overlay en partida
// ---------------------------------------------------------------------------

RouletteHud* RouletteHud::s_current = nullptr;

RouletteHud* RouletteHud::create() {
    auto ret = new RouletteHud();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RouletteHud::init() {
    if (!CCNode::init()) return false;
    this->setAnchorPoint({ 0.5f, 0.5f });
    this->setID("roulette-hud"_spr);

    m_bg = makePanel({ 10.f, 10.f }, 110);
    this->addChild(m_bg);

    for (auto label : { &m_title, &m_progress, &m_totalName, &m_totalValue, &m_meanName, &m_meanValue, &m_sdName, &m_sdValue, &m_last }) {
        *label = CCLabelBMFont::create("", "bigFont.fnt");
        (*label)->setAnchorPoint({ 0.f, 0.5f });
        this->addChild(*label);
    }

    this->refresh();
    this->schedule(schedule_selector(RouletteHud::tick), 0.1f);
    return true;
}

void RouletteHud::onEnter() {
    CCNode::onEnter();
    s_current = this;
}

void RouletteHud::onExit() {
    if (s_current == this) s_current = nullptr;
    CCNode::onExit();
}

void RouletteHud::setLastPercent(int percent) {
    m_lastPercent = percent;
    m_livePercent = -1;
    this->refresh();
}

// Con "en directo" activado, el total y la media incluyen el porcentaje que
// llevas en el intento actual
void RouletteHud::tick(float) {
    int live = -1;
    if (m_cfg.liveUpdate && m_lastPercent < 0 && RouletteManager::get().session()) {
        if (auto pl = PlayLayer::get()) live = std::clamp(pl->getCurrentPercentInt(), 0, 100);
    }
    if (live != m_livePercent) {
        m_livePercent = live;
        this->refresh();
    }
}

void RouletteHud::refresh() {
    m_cfg = OverlayConfig::load(OverlayTarget::Hud);
    auto& cfg = m_cfg;
    this->setVisible(!cfg.collapsed);

    // --- valores ---
    auto& rm = RouletteManager::get();
    std::string category = "Easy Demon";
    int index = 1, count = 25;
    SessionStats st;
    if (auto s = rm.session()) {
        auto results = s->results;
        if (m_livePercent >= 0) {
            LevelResult cur;
            cur.percent = m_livePercent;
            results.push_back(cur);
        }
        st = computeStats(results);
        category = s->config.categoryName();
        count = s->config.count;
        index = std::min((int)s->results.size() + (m_lastPercent >= 0 ? 0 : 1), count);
    } else if (!rm.history().empty()) {
        // la sesion acaba de terminar con este nivel
        auto& e = rm.history().back();
        st = e.stats();
        category = e.config.categoryName();
        count = e.config.count;
        index = st.played;
    }

    // --- textos y colores ---
    float s = cfg.scale;
    float fontScale = 0.32f * s;
    auto opacity = (GLubyte)(cfg.opacity * 255);
    auto setLabel = [&](CCLabelBMFont* label, std::string const& text, ccColor3B color) {
        label->setString(text.c_str());
        label->setScale(fontScale);
        label->setColor(color);
        label->setOpacity(opacity);
    };
    auto textColor = cfg.color(ColorSlot::Text);
    setLabel(m_title, fmt::format("Roulette {}", category), textColor);
    setLabel(m_progress, fmt::format("{}/{}", index, count), cfg.color(ColorSlot::Progress));
    setLabel(m_totalName, "Total:", textColor);
    setLabel(m_totalValue, fmt::format("{}%", st.total), cfg.color(ColorSlot::Total));
    setLabel(m_meanName, "Mean:", textColor);
    setLabel(m_meanValue, fmt::format("{:.1f}%", st.mean), cfg.color(ColorSlot::Mean));
    setLabel(m_sdName, "SD:", textColor);
    setLabel(m_sdValue, fmt::format("{:.1f}", st.stddev), cfg.color(ColorSlot::StdDev));
    setLabel(m_last, fmt::format("+{}%", std::max(m_lastPercent, 0)), percentColor(m_lastPercent));
    m_last->setVisible(m_lastPercent >= 0);

    // --- disposicion: lineas de etiquetas una detras de otra ---
    float pad = 4.f * s;
    float space = 6.f * s;
    float lineH = m_title->getScaledContentSize().height;
    std::vector<std::vector<CCLabelBMFont*>> lines = {
        { m_title, m_progress },
        { m_totalName, m_totalValue, m_meanName, m_meanValue, m_sdName, m_sdValue },
    };
    if (m_lastPercent >= 0) lines.push_back({ m_last });

    float width = 0.f;
    for (auto& line : lines) {
        float w = 0.f;
        for (size_t i = 0; i < line.size(); i++) {
            w += line[i]->getScaledContentSize().width;
            if (i + 1 < line.size()) w += (i % 2 == 0) ? space * 0.5f : space;
        }
        width = std::max(width, w);
    }
    auto size = CCSize(width + pad * 2, lines.size() * lineH + pad * 2);

    float y = size.height - pad - lineH / 2;
    for (auto& line : lines) {
        float x = pad;
        for (size_t i = 0; i < line.size(); i++) {
            line[i]->setPosition({ x, y });
            x += line[i]->getScaledContentSize().width;
            if (i + 1 < line.size()) x += (i % 2 == 0) ? space * 0.5f : space;
        }
        y -= lineH;
    }

    m_bg->setVisible(cfg.background);
    m_bg->setContentSize(size);
    m_bg->setOpacity((GLubyte)(110 * cfg.opacity));
    m_bg->setPosition(size / 2);

    this->setContentSize(size);
    placeOnScreen(this, cfg);
}

// ---------------------------------------------------------------------------
// Editor (overlay en partida o panel del nivel) con vista previa arrastrable
// ---------------------------------------------------------------------------

class OverlayPopup : public Popup {
protected:
    static constexpr float W = 360.f;
    static constexpr float H = 280.f;
    static constexpr float PREVIEW_W = 170.f;

    OverlayTarget m_target = OverlayTarget::Hud;
    OverlayConfig m_cfg;
    CCNode* m_preview = nullptr;
    CCSize m_previewSize;
    NineSlice* m_rect = nullptr;
    CCLabelBMFont* m_scaleLabel = nullptr;
    CCLabelBMFont* m_opacityLabel = nullptr;
    CCMenuItemSpriteExtra* m_colorButtons[COLOR_SLOT_COUNT] = {};
    bool m_dragging = false;

    bool init(OverlayTarget target) {
        if (!Popup::init(W, H)) return false;
        m_target = target;
        bool panel = target == OverlayTarget::Panel;
        bool toasts = target == OverlayTarget::Toasts;
        this->setTitle("Position & look", "goldFont.fnt", 0.6f, 16.f);
        m_cfg = OverlayConfig::load(target);

        // Pestanas: que elemento se edita
        char const* tabNames[] = { "In-game overlay", "Level panel", "Notifications" };
        float tabX[] = { 75.f, 180.f, 285.f };
        for (int i = 0; i < 3; i++) {
            auto tab = textButton(tabNames[i], "GJ_button_04.png", 0.4f, [this, i](auto) { this->switchTo((OverlayTarget)i); });
            setButtonSelected(tab, i == (int)target);
            m_buttonMenu->addChildAtPosition(tab, Anchor::BottomLeft, { tabX[i], 241.f });
        }

        // Vista previa de la pantalla
        auto win = CCDirector::get()->getWinSize();
        m_previewSize = CCSize(PREVIEW_W, PREVIEW_W * win.height / win.width);

        m_preview = CCNode::create();
        m_preview->setContentSize(m_previewSize);
        m_preview->setAnchorPoint({ 0.5f, 0.5f });
        m_mainLayer->addChildAtPosition(m_preview, Anchor::BottomLeft, { W / 2, 180.f });

        auto screen = makePanel(m_previewSize, 170);
        screen->setPosition(m_previewSize / 2);
        m_preview->addChild(screen);

        auto screenLabel = makeLabel(panel ? "Level screen" : toasts ? "Screen" : "Gameplay", "bigFont.fnt", 0.3f);
        screenLabel->setOpacity(60);
        screenLabel->setPosition(m_previewSize / 2);
        m_preview->addChild(screenLabel);

        m_rect = NineSlice::create("square02_small.png");
        m_rect->setColor(panel ? ccColor3B { 120, 255, 120 } : toasts ? ccColor3B { 255, 170, 60 } : ccColor3B { 80, 200, 255 });
        m_preview->addChild(m_rect);

        auto hint = makeLabel(
            panel ? "Drag the green box to move the panel"
                : toasts ? "Drag the orange box to move the notifications"
                : "Drag the blue box to move the overlay",
            "chatFont.fnt", 0.52f
        );
        hint->setOpacity(200);
        m_mainLayer->addChildAtPosition(hint, Anchor::BottomLeft, { W / 2, 121.f });

        // Tamano
        float y = 101.f;
        auto sl = makeLabel("Size", "goldFont.fnt", 0.45f);
        m_mainLayer->addChildAtPosition(sl, Anchor::BottomLeft, { 50.f, y });
        m_buttonMenu->addChildAtPosition(textButton("-", "GJ_button_04.png", 0.42f, [this](auto) { this->changeScale(-0.1f); }), Anchor::BottomLeft, { 90.f, y });
        m_scaleLabel = makeLabel("", "bigFont.fnt", 0.35f);
        m_mainLayer->addChildAtPosition(m_scaleLabel, Anchor::BottomLeft, { 119.f, y });
        m_buttonMenu->addChildAtPosition(textButton("+", "GJ_button_04.png", 0.42f, [this](auto) { this->changeScale(0.1f); }), Anchor::BottomLeft, { 148.f, y });

        // Opacidad
        auto ol = makeLabel("Opacity", "goldFont.fnt", 0.45f);
        m_mainLayer->addChildAtPosition(ol, Anchor::BottomLeft, { 212.f, y });
        m_buttonMenu->addChildAtPosition(textButton("-", "GJ_button_04.png", 0.42f, [this](auto) { this->changeOpacity(-0.1f); }), Anchor::BottomLeft, { 256.f, y });
        m_opacityLabel = makeLabel("", "bigFont.fnt", 0.35f);
        m_mainLayer->addChildAtPosition(m_opacityLabel, Anchor::BottomLeft, { 288.f, y });
        m_buttonMenu->addChildAtPosition(textButton("+", "GJ_button_04.png", 0.42f, [this](auto) { this->changeOpacity(0.1f); }), Anchor::BottomLeft, { 320.f, y });

        y = 77.f;
        if (toasts) return this->initToasts(y);

        // Colores: un boton por cada tipo de texto (el texto del boton va en su color)
        auto cl = makeLabel("Colors", "goldFont.fnt", 0.45f);
        m_mainLayer->addChildAtPosition(cl, Anchor::BottomLeft, { 34.f, y });
        float x = 64.f;
        for (int i = 0; i < COLOR_SLOT_COUNT; i++) {
            auto btn = textButton(colorSlotName(i), "GJ_button_04.png", 0.37f, [this, i](auto) { this->pickColor(i); });
            float w = btn->getScaledContentSize().width;
            m_buttonMenu->addChildAtPosition(btn, Anchor::BottomLeft, { x + w / 2, y });
            x += w + 3.f;
            m_colorButtons[i] = btn;
        }

        // Toggles
        y = 53.f;
        auto addToggle = [&](char const* text, bool* value, float tx) {
            auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.5f, [this, value](auto) {
                *value = !*value;
                this->apply();
            });
            toggle->toggle(*value);
            m_buttonMenu->addChildAtPosition(toggle, Anchor::BottomLeft, { tx, y });
            auto label = makeLabel(text, "bigFont.fnt", 0.31f, { 0.f, 0.5f });
            m_mainLayer->addChildAtPosition(label, Anchor::BottomLeft, { tx + 13.f, y });
        };
        addToggle("Background", &m_cfg.background, 26.f);
        addToggle(panel ? "Collapsed" : "Hidden", &m_cfg.collapsed, 126.f);
        if (!panel) addToggle("Live total/mean", &m_cfg.liveUpdate, 218.f);

        auto reset = textButton("Reset", "GJ_button_06.png", 0.48f, [this](auto) {
            m_cfg = OverlayConfig::defaults(m_target);
            this->apply();
            this->switchTo(m_target);
        });
        m_buttonMenu->addChildAtPosition(reset, Anchor::BottomLeft, { W / 2, 24.f });

        this->apply();
        return true;
    }

    // Pestana "Notifications": que tipos de aviso se muestran + boton de prueba
    bool initToasts(float y) {
        auto showLabel = makeLabel("Show", "goldFont.fnt", 0.45f);
        m_mainLayer->addChildAtPosition(showLabel, Anchor::BottomLeft, { 28.f, y });
        float x = 56.f;
        for (int i = 0; i < TOAST_KIND_COUNT; i++) {
            auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.48f, [this, i](auto) {
                m_cfg.toasts[i] = !m_cfg.toasts[i];
                this->apply();
            });
            toggle->toggle(m_cfg.toasts[i]);
            m_buttonMenu->addChildAtPosition(toggle, Anchor::BottomLeft, { x, y });
            auto label = makeLabel(TOAST_NAMES[i], "bigFont.fnt", 0.29f, { 0.f, 0.5f });
            m_mainLayer->addChildAtPosition(label, Anchor::BottomLeft, { x + 12.f, y });
            x += 22.f + label->getScaledContentSize().width;
        }

        auto test = textButton("Test", "GJ_button_02.png", 0.48f, [](auto) {
            showToast(ToastKind::Results, "Bloodbath: 47%   |   Total: 523%", NotificationIcon::Info, 2.f, true);
        });
        m_buttonMenu->addChildAtPosition(test, Anchor::BottomLeft, { W / 2 - 45.f, 51.f });

        auto reset = textButton("Reset", "GJ_button_06.png", 0.48f, [this](auto) {
            m_cfg = OverlayConfig::defaults(m_target);
            this->apply();
            this->switchTo(m_target);
        });
        m_buttonMenu->addChildAtPosition(reset, Anchor::BottomLeft, { W / 2 + 45.f, 51.f });

        auto note = makeLabel("Results = info about the previous level when the next one loads", "chatFont.fnt", 0.45f);
        note->setOpacity(160);
        m_mainLayer->addChildAtPosition(note, Anchor::BottomLeft, { W / 2, 24.f });

        this->apply();
        return true;
    }

    void switchTo(OverlayTarget target) {
        this->onClose(nullptr);
        openOverlayPopup(target);
    }

    void pickColor(int slot) {
        auto popup = ColorPickPopup::create(m_cfg.colors[slot]);
        popup->setCallback([this, slot](ccColor4B const& c) {
            m_cfg.colors[slot] = { c.r, c.g, c.b };
            this->apply();
        });
        popup->show();
    }

    void changeScale(float d) {
        m_cfg.scale = std::clamp(std::round((m_cfg.scale + d) * 10.f) / 10.f, 0.5f, 2.5f);
        this->apply();
    }

    void changeOpacity(float d) {
        m_cfg.opacity = std::clamp(std::round((m_cfg.opacity + d) * 10.f) / 10.f, 0.2f, 1.f);
        this->apply();
    }

    // Tamano real en pantalla del elemento que se edita
    CCSize targetSize() {
        float s = m_cfg.scale;
        if (m_target == OverlayTarget::Panel) {
            if (auto panel = SessionPanel::current()) return panel->getScaledContentSize();
            return m_cfg.collapsed ? CCSize(16.f * s, 34.f * s) : CCSize(128.f * s, 190.f * s);
        }
        if (m_target == OverlayTarget::Toasts) return CCSize(230.f * s, 28.f * s);
        if (auto hud = RouletteHud::current()) return hud->getScaledContentSize();
        return CCSize(190.f * s, 30.f * s);
    }

    // Guarda la configuracion, refresca el elemento real y la vista previa
    void apply() {
        m_cfg.save(m_target);
        if (m_target == OverlayTarget::Panel) {
            if (auto panel = SessionPanel::current()) panel->applyConfig();
        } else {
            if (auto hud = RouletteHud::current()) hud->refresh();
        }

        m_scaleLabel->setString(fmt::format("{:.1f}", m_cfg.scale).c_str());
        m_opacityLabel->setString(fmt::format("{}%", (int)std::round(m_cfg.opacity * 100)).c_str());
        for (int i = 0; i < COLOR_SLOT_COUNT; i++) {
            if (!m_colorButtons[i]) continue;
            if (auto spr = typeinfo_cast<ButtonSprite*>(m_colorButtons[i]->getNormalImage())) {
                if (spr->m_label) spr->m_label->setColor(m_cfg.colors[i]);
            }
        }

        auto win = CCDirector::get()->getWinSize();
        float k = m_previewSize.width / win.width;
        auto size = this->targetSize() * k;
        size.width = std::max(size.width, 6.f);
        size.height = std::max(size.height, 6.f);
        m_rect->setContentSize(size);
        float px = std::clamp(m_cfg.x * m_previewSize.width, size.width / 2, m_previewSize.width - size.width / 2);
        float py = std::clamp(m_cfg.y * m_previewSize.height, size.height / 2, m_previewSize.height - size.height / 2);
        m_rect->setPosition({ px, py });
        bool hidden = m_cfg.collapsed && m_target == OverlayTarget::Hud;
        m_rect->setOpacity(hidden ? 60 : (GLubyte)(120 + 100 * m_cfg.opacity));
    }

    void dragTo(CCTouch* touch) {
        auto p = m_preview->convertToNodeSpace(touch->getLocation());
        m_cfg.x = std::clamp(p.x / m_previewSize.width, 0.f, 1.f);
        m_cfg.y = std::clamp(p.y / m_previewSize.height, 0.f, 1.f);
        this->apply();
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        auto p = m_preview->convertToNodeSpace(touch->getLocation());
        if (p.x >= 0 && p.y >= 0 && p.x <= m_previewSize.width && p.y <= m_previewSize.height) {
            m_dragging = true;
            this->dragTo(touch);
            return true;
        }
        return Popup::ccTouchBegan(touch, event);
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (m_dragging) return this->dragTo(touch);
        Popup::ccTouchMoved(touch, event);
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (m_dragging) {
            m_dragging = false;
            return;
        }
        Popup::ccTouchEnded(touch, event);
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
        if (m_dragging) {
            m_dragging = false;
            return;
        }
        Popup::ccTouchCancelled(touch, event);
    }

public:
    static OverlayPopup* create(OverlayTarget target) {
        auto ret = new OverlayPopup();
        if (ret->init(target)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

void openOverlayPopup(OverlayTarget target) {
    if (auto popup = OverlayPopup::create(target)) popup->show();
}
