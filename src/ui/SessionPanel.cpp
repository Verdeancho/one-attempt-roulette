#include "SessionPanel.hpp"
#include "Hud.hpp"

// ---------------------------------------------------------------------------
// Resumen al terminar la ruleta
// ---------------------------------------------------------------------------

class SummaryPopup : public Popup {
protected:
    HistoryEntry m_entry;

    bool init(HistoryEntry const& entry, bool record) {
        if (!Popup::init(320.f, 240.f)) return false;
        m_entry = entry;
        this->setTitle("Roulette completed!", "goldFont.fnt", 0.8f, 18.f);

        auto st = entry.stats();
        auto icon = categoryIcon(entry.config, 0.9f);
        m_mainLayer->addChildAtPosition(icon, Anchor::BottomLeft, { 60.f, 160.f });

        auto cat = makeLabel(entry.config.categoryName(), "bigFont.fnt", 0.5f, { 0.f, 0.5f });
        cat->limitLabelWidth(190.f, 0.5f, 0.1f);
        m_mainLayer->addChildAtPosition(cat, Anchor::BottomLeft, { 105.f, 172.f });
        auto count = makeLabel(fmt::format("{} levels", entry.config.count), "goldFont.fnt", 0.55f, { 0.f, 0.5f });
        m_mainLayer->addChildAtPosition(count, Anchor::BottomLeft, { 105.f, 150.f });

        auto total = makeLabel(fmt::format("{}%", st.total), "bigFont.fnt", 0.9f);
        total->setColor(record ? ccColor3B { 255, 215, 60 } : ccColor3B { 255, 255, 255 });
        m_mainLayer->addChildAtPosition(total, Anchor::BottomLeft, { 160.f, 112.f });

        if (record) {
            auto rec = makeLabel("NEW RECORD!", "goldFont.fnt", 0.6f);
            rec->runAction(CCRepeatForever::create(CCSequence::create(
                CCScaleTo::create(0.5f, 0.68f), CCScaleTo::create(0.5f, 0.58f), nullptr
            )));
            m_mainLayer->addChildAtPosition(rec, Anchor::BottomLeft, { 160.f, 88.f });
        }

        auto details = makeLabel(
            fmt::format("Mean: {:.2f}%   Std. dev.: {:.2f}   Best: {}%   Worst: {}%", st.mean, st.stddev, st.best, st.worst),
            "chatFont.fnt", 0.6f
        );
        m_mainLayer->addChildAtPosition(details, Anchor::BottomLeft, { 160.f, 66.f });

        auto results = entry.results;
        auto title = entry.config.categoryName();
        auto graph = textButton("Graph", "GJ_button_02.png", 0.7f, [results, title](auto) { openGraphPopup(title, results); });
        auto ok = textButton("OK", "GJ_button_01.png", 0.7f, [this](auto) { this->onClose(nullptr); });
        m_buttonMenu->addChildAtPosition(graph, Anchor::BottomLeft, { 110.f, 28.f });
        m_buttonMenu->addChildAtPosition(ok, Anchor::BottomLeft, { 220.f, 28.f });
        return true;
    }

public:
    static SummaryPopup* create(HistoryEntry const& entry, bool record) {
        auto ret = new SummaryPopup();
        if (ret->init(entry, record)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

void showSummaryIfPending() {
    auto& rm = RouletteManager::get();
    if (!rm.hasFinishedSummary()) return;
    bool record = rm.lastFinishWasRecord();
    if (auto entry = rm.takeFinishedSummary()) {
        if (auto popup = SummaryPopup::create(*entry, record)) popup->show();
    }
}

// ---------------------------------------------------------------------------
// Panel con las estadisticas de la sesion (LevelInfoLayer)
// ---------------------------------------------------------------------------

bool SessionPanel::init(GJGameLevel* level) {
    if (!CCNode::init()) return false;
    m_levelID = level->m_levelID.value();
    m_level = level;

    this->setAnchorPoint({ 0.5f, 0.5f });

    m_content = CCNode::create();
    m_content->setContentSize({ PANEL_W, PANEL_H });
    m_content->setAnchorPoint({ 0.f, 0.f });
    this->addChild(m_content);

    // Controles: ojo (plegar/desplegar) y ajustes (mover, tamano, opacidad...)
    m_controls = CCMenu::create();
    m_controls->ignoreAnchorPointForPosition(false);
    m_controls->setAnchorPoint({ 0.f, 0.f });
    m_controls->setPosition({ 0.f, 0.f });
    this->addChild(m_controls, 5);

    m_eyeBtn = CCMenuItemExt::createSpriteExtra(CCSprite::create("eye_open.png"_spr), [this](auto) {
        auto cfg = OverlayConfig::load(OverlayTarget::Panel);
        cfg.collapsed = !cfg.collapsed;
        cfg.save(OverlayTarget::Panel);
        this->applyConfig();
    });
    m_controls->addChild(m_eyeBtn);

    m_gearBtn = CCMenuItemExt::createSpriteExtra(CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png"), [](auto) {
        openOverlayPopup(OverlayTarget::Panel);
    });
    m_controls->addChild(m_gearBtn);

    this->applyConfig();
    this->schedule(schedule_selector(SessionPanel::poll), 0.2f);
    return true;
}

SessionPanel* SessionPanel::s_current = nullptr;

void SessionPanel::onEnter() {
    CCNode::onEnter();
    s_current = this;
}

void SessionPanel::onExit() {
    if (s_current == this) s_current = nullptr;
    CCNode::onExit();
}

// Aplica opacidad a todos los textos e iconos del panel (el fondo va aparte)
static void applyOpacity(CCNode* node, GLubyte opacity, CCNode* skip) {
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (child == skip) continue;
        if (auto rgba = typeinfo_cast<CCRGBAProtocol*>(child)) rgba->setOpacity(opacity);
        applyOpacity(child, opacity, skip);
    }
}

void SessionPanel::applyConfig() {
    // se reconstruye el contenido para aplicar los colores
    this->build();
    auto cfg = OverlayConfig::load(OverlayTarget::Panel);
    float s = cfg.scale;
    float btn = 16.f * s;
    float gap = 3.f * s;

    for (auto item : { m_eyeBtn, m_gearBtn }) {
        auto spr = static_cast<CCSprite*>(item->getNormalImage());
        fitNode(spr, btn);
        item->setContentSize({ btn, btn });
        spr->setPosition({ btn / 2, btn / 2 });
    }
    static_cast<CCSprite*>(m_eyeBtn->getNormalImage())->setDisplayFrame(
        CCSprite::create(cfg.collapsed ? "eye_closed.png"_spr : "eye_open.png"_spr)->displayFrame()
    );

    CCSize size;
    if (cfg.collapsed) {
        m_content->setVisible(false);
        size = CCSize(btn, btn * 2 + gap);
        m_eyeBtn->setPosition({ btn / 2, size.height - btn / 2 });
        m_gearBtn->setPosition({ btn / 2, btn / 2 });
    } else {
        m_content->setVisible(true);
        m_content->setScale(s);
        float pw = PANEL_W * s;
        float ph = PANEL_H * s;
        size = CCSize(pw + gap + btn, std::max(ph, btn * 2 + gap));
        m_content->setPosition({ 0.f, size.height - ph });
        m_eyeBtn->setPosition({ pw + gap + btn / 2, size.height - btn / 2 });
        m_gearBtn->setPosition({ pw + gap + btn / 2, size.height - btn * 1.5f - gap });

        applyOpacity(m_content, (GLubyte)(cfg.opacity * 255), m_bg);
        if (m_bg) {
            m_bg->setVisible(cfg.background);
            m_bg->setOpacity((GLubyte)(110 * cfg.opacity));
        }
    }

    this->setContentSize(size);
    m_controls->setContentSize(size);
    placeOnScreen(this, cfg);
}

void SessionPanel::build() {
    m_content->removeAllChildren();
    m_status = nullptr;
    m_retryMenu = nullptr;

    auto& rm = RouletteManager::get();
    auto size = m_content->getContentSize();
    float cx = size.width / 2;

    auto bg = makePanel(size, 110);
    bg->setPosition(size / 2);
    m_content->addChild(bg);
    m_bg = bg;

    auto session = rm.session();
    if (!session) {
        // Sesion terminada: solo se muestra un aviso
        auto done = makeLabel("Roulette\ncompleted!", "goldFont.fnt", 0.55f);
        done->setAlignment(kCCTextAlignmentCenter);
        done->setPosition({ cx, size.height / 2 + 20.f });
        m_content->addChild(done);

        auto menu = makeMenu({ size.width, 40.f });
        menu->setPosition({ cx, 40.f });
        auto btn = textButton("Stats", "GJ_button_02.png", 0.6f, [](auto) { openStatsPopup(); });
        btn->setPosition({ cx, 20.f });
        menu->addChild(btn);
        m_content->addChild(menu);
        return;
    }

    auto st = session->stats();
    auto& cfg = session->config;
    auto look = OverlayConfig::load(OverlayTarget::Panel);
    auto textColor = look.color(ColorSlot::Text);
    float y = size.height - 22.f;

    auto icon = categoryIcon(cfg, 0.55f);
    icon->setPosition({ 24.f, y });
    m_content->addChild(icon);

    auto title = makeLabel("Roulette", "bigFont.fnt", 0.38f, { 0.f, 0.5f });
    title->setColor(textColor);
    title->setPosition({ 44.f, y + 7.f });
    m_content->addChild(title);
    auto cat = makeLabel(cfg.categoryName(), "bigFont.fnt", 0.3f, { 0.f, 0.5f });
    cat->limitLabelWidth(62.f, 0.3f, 0.1f);
    cat->setColor(textColor);
    cat->setPosition({ 44.f, y - 8.f });
    m_content->addChild(cat);

    y -= 36.f;
    int index = std::min(st.played + (rm.isAwaitingAdvance() ? 0 : 1), cfg.count);
    auto progress = makeLabel(fmt::format("{}/{}", index, cfg.count), "bigFont.fnt", 0.6f);
    progress->setColor(look.color(ColorSlot::Progress));
    progress->setPosition({ cx, y });
    m_content->addChild(progress);

    auto addLine = [&](std::string const& name, std::string const& value, ccColor3B color = { 255, 255, 255 }) {
        y -= 16.f;
        auto n = makeLabel(name, "bigFont.fnt", 0.3f, { 0.f, 0.5f });
        n->setColor(textColor);
        n->setPosition({ 8.f, y });
        m_content->addChild(n);
        auto v = makeLabel(value, "bigFont.fnt", 0.32f, { 1.f, 0.5f });
        v->setColor(color);
        v->setPosition({ size.width - 8.f, y });
        m_content->addChild(v);
    };

    y -= 4.f;
    addLine("Total", fmt::format("{}%", st.total), look.color(ColorSlot::Total));
    addLine("Mean", fmt::format("{:.2f}%", st.mean), look.color(ColorSlot::Mean));
    addLine("Std. dev.", fmt::format("{:.2f}", st.stddev), look.color(ColorSlot::StdDev));
    if (auto& last = rm.lastResult(); last && !session->results.empty()) {
        addLine("Last", fmt::format("{}%", last->percent), percentColor(last->percent));
    } else {
        addLine("Last", "-");
    }

    // Estado de la busqueda del siguiente nivel
    y -= 18.f;
    m_status = makeLabel("", "chatFont.fnt", 0.5f);
    m_status->setPosition({ cx, y });
    m_content->addChild(m_status);

    m_retryMenu = makeMenu({ size.width, 30.f });
    m_retryMenu->setPosition({ cx, y - 12.f });
    auto retry = textButton("Retry", "GJ_button_06.png", 0.45f, [](auto) { RouletteManager::get().retryNext(); });
    retry->setPosition({ cx, 15.f });
    m_retryMenu->addChild(retry);
    m_retryMenu->setVisible(false);
    m_content->addChild(m_retryMenu);

    // Botones: grafico y menu de la ruleta
    auto menu = makeMenu({ size.width, 30.f });
    menu->setPosition({ cx, 16.f });
    auto graphSpr = CCSprite::createWithSpriteFrameName("GJ_statsBtn_001.png");
    graphSpr->setScale(0.45f);
    auto results = session->results;
    auto name = cfg.categoryName();
    auto graphBtn = CCMenuItemExt::createSpriteExtra(graphSpr, [results, name](auto) {
        openGraphPopup(fmt::format("Current roulette: {}", name), results);
    });
    graphBtn->setPosition({ cx, 15.f });
    menu->addChild(graphBtn);
    m_content->addChild(menu);

    this->updateStatus();
}

void SessionPanel::updateStatus() {
    if (!m_status) return;
    auto& rm = RouletteManager::get();
    bool waiting = rm.isAwaitingAdvance() && rm.lastPlayedID() == m_levelID;
    if (!waiting) {
        m_status->setString("");
        m_retryMenu->setVisible(false);
        return;
    }
    if (rm.nextFailed()) {
        m_status->setString("Search failed");
        m_status->setColor({ 255, 120, 120 });
        m_retryMenu->setVisible(true);
    } else {
        m_status->setString("Finding next level...");
        m_status->setColor({ 160, 220, 255 });
        m_retryMenu->setVisible(false);
    }
}

void SessionPanel::poll(float) {
    auto& rm = RouletteManager::get();
    this->updateStatus();

    if (m_transitioning) return;
    if (rm.isAwaitingAdvance() && rm.lastPlayedID() == m_levelID && rm.isNextReady()) {
        if (auto next = rm.consumeAdvance(m_level)) {
            m_transitioning = true;
            CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, LevelInfoLayer::scene(next, false)));
        }
    }
}

SessionPanel* SessionPanel::create(GJGameLevel* level) {
    auto ret = new SessionPanel();
    if (ret->init(level)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}
