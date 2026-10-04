#include "Common.hpp"

// ---------------------------------------------------------------------------
// Lista de niveles jugados en una ruleta
// ---------------------------------------------------------------------------

class LevelListPopup : public Popup {
protected:
    bool init(std::string const& title, std::vector<LevelResult> const& results, bool clickable) {
        if (!Popup::init(380.f, 270.f)) return false;
        this->setTitle(title, "goldFont.fnt", 0.6f, 18.f);

        auto stats = computeStats(results);
        auto info = makeLabel(
            fmt::format("{} levels   Total: {}%   Mean: {:.2f}%   100%: {}", stats.played, stats.total, stats.mean, stats.completions),
            "chatFont.fnt", 0.6f
        );
        m_mainLayer->addChildAtPosition(info, Anchor::BottomLeft, { 190.f, 233.f });

        CCSize listSize = { 340.f, 192.f };
        float rowH = 26.f;
        auto scroll = ScrollLayer::create(listSize);
        float total = std::max(listSize.height, results.size() * rowH);
        scroll->m_contentLayer->setContentSize({ listSize.width, total });

        // cada fila es un boton: al pulsarla se ofrece abrir el nivel
        auto menu = CCMenu::create();
        menu->ignoreAnchorPointForPosition(false);
        menu->setAnchorPoint({ 0.f, 0.f });
        menu->setPosition({ 0.f, 0.f });
        menu->setContentSize({ listSize.width, total });
        scroll->m_contentLayer->addChild(menu);

        for (size_t i = 0; i < results.size(); i++) {
            auto& r = results[i];
            auto row = CCNode::create();
            row->setContentSize({ listSize.width, rowH - 3.f });

            auto bg = makePanel(row->getContentSize(), i % 2 ? 50 : 80);
            bg->setPosition(row->getContentSize() / 2);
            row->addChild(bg);

            float cy = row->getContentHeight() / 2;
            auto index = makeLabel(fmt::format("{}", i + 1), "goldFont.fnt", 0.45f);
            index->setPosition({ 16.f, cy });
            row->addChild(index);

            auto name = makeLabel(r.name.empty() ? fmt::format("ID {}", r.levelID) : r.name, "bigFont.fnt", 0.36f, { 0.f, 0.5f });
            name->limitLabelWidth(180.f, 0.36f, 0.1f);
            name->setPosition({ 34.f, cy + 4.f });
            row->addChild(name);

            auto sub = r.position > 0
                ? fmt::format("#{}  -  by {}", r.position, r.creator)
                : fmt::format("by {}  -  ID {}", r.creator, r.levelID);
            auto creator = makeLabel(sub, "chatFont.fnt", 0.45f, { 0.f, 0.5f });
            creator->limitLabelWidth(200.f, 0.45f, 0.1f);
            creator->setOpacity(170);
            creator->setPosition({ 34.f, cy - 6.f });
            row->addChild(creator);

            auto pct = makeLabel(fmt::format("{}%", r.percent), r.percent >= 100 ? "goldFont.fnt" : "bigFont.fnt", r.percent >= 100 ? 0.6f : 0.45f, { 1.f, 0.5f });
            if (r.percent < 100) pct->setColor(percentColor(r.percent));
            pct->setPosition({ listSize.width - 10.f, cy });
            row->addChild(pct);

            auto levelID = r.levelID;
            auto levelName = r.name.empty() ? fmt::format("ID {}", r.levelID) : r.name;
            auto item = CCMenuItemExt::createSpriteExtra(row, [levelID, levelName](auto) {
                if (levelID <= 0) return;
                createQuickPopup(
                    "Go to level",
                    fmt::format("Do you want to go to <cy>{}</c>?", levelName),
                    "No", "Yes",
                    [levelID](auto, bool yes) {
                        if (yes) openLevelByID(levelID);
                    }
                );
            });
            item->m_scaleMultiplier = 1.02f;
            item->setEnabled(clickable);
            item->setPosition({ listSize.width / 2, total - (i + 0.5f) * rowH });
            menu->addChild(item);
        }
        scroll->scrollToTop();
        m_mainLayer->addChildAtPosition(scroll, Anchor::BottomLeft, { 20.f, 26.f });

        if (clickable && !results.empty()) {
            auto hint = makeLabel("Click a level to open it", "chatFont.fnt", 0.5f);
            hint->setOpacity(160);
            m_mainLayer->addChildAtPosition(hint, Anchor::BottomLeft, { 190.f, 13.f });
        }

        if (results.empty()) {
            auto empty = makeLabel("No levels played yet", "bigFont.fnt", 0.4f);
            m_mainLayer->addChildAtPosition(empty, Anchor::BottomLeft, { 190.f, 120.f });
        }
        handleTouchPriority(this);
        return true;
    }

public:
    static LevelListPopup* create(std::string const& title, std::vector<LevelResult> const& results, bool clickable) {
        auto ret = new LevelListPopup();
        if (ret->init(title, results, clickable)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

void openLevelListPopup(std::string const& title, std::vector<LevelResult> const& results, bool clickable) {
    if (auto popup = LevelListPopup::create(title, results, clickable)) popup->show();
}

// ---------------------------------------------------------------------------
// Diagrama de lineas con los porcentajes de una sesion
// ---------------------------------------------------------------------------

class GraphPopup : public Popup {
protected:
    std::string m_titleText;
    std::vector<LevelResult> m_results;
    int64_t m_entryID = 0;
    std::function<void()> m_onChanged;
    CCMenuItemSpriteExtra* m_lockBtn = nullptr;

    bool init(std::string const& title, std::vector<LevelResult> const& results, int64_t entryID, std::function<void()> onChanged) {
        if (!Popup::init(440.f, 285.f)) return false;
        m_titleText = title;
        m_results = results;
        m_entryID = entryID;
        m_onChanged = std::move(onChanged);
        this->setTitle(title, "goldFont.fnt", 0.6f, 18.f);
        if (m_title) m_title->limitLabelWidth(270.f, 0.6f, 0.2f);

        auto const W = 440.f;
        auto stats = computeStats(m_results);

        auto info = makeLabel(
            fmt::format("Levels: {}   Total: {}%   Mean: {:.2f}%   Std. dev.: {:.2f}   100%: {}", stats.played, stats.total, stats.mean, stats.stddev, stats.completions),
            "chatFont.fnt", 0.6f
        );
        m_mainLayer->addChildAtPosition(info, Anchor::BottomLeft, { W / 2, 248.f });

        // Lista de niveles
        auto listBtn = textButton("View levels", "GJ_button_02.png", 0.5f, [this](auto) {
            openLevelListPopup(m_titleText, m_results, m_entryID != 0);
        });
        m_buttonMenu->addChildAtPosition(listBtn, Anchor::BottomLeft, { 55.f, 16.f });

        // Candado y papelera (solo ruletas del historial)
        if (auto entry = RouletteManager::get().findEntry(m_entryID)) {
            m_lockBtn = CCMenuItemExt::createSpriteExtra(CCSprite::createWithSpriteFrameName("GJ_lock_001.png"), [this](auto) {
                auto e = RouletteManager::get().findEntry(m_entryID);
                if (!e) return;
                RouletteManager::get().setLocked(m_entryID, !e->locked);
                this->updateLock();
                showToast(ToastKind::Stats, !e->locked ? "Lock removed" : "Roulette locked", NotificationIcon::Info, 1.5f);
                if (m_onChanged) m_onChanged();
            });
            m_buttonMenu->addChildAtPosition(m_lockBtn, Anchor::TopRight, { -54.f, -20.f });
            this->updateLock();

            auto trashSpr = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
            trashSpr->setScale(0.6f);
            auto trash = CCMenuItemExt::createSpriteExtra(trashSpr, [this](auto) { this->onDelete(); });
            m_buttonMenu->addChildAtPosition(trash, Anchor::TopRight, { -22.f, -20.f });
            (void)entry;
        }

        this->drawGraph();
        return true;
    }

    void updateLock() {
        auto e = RouletteManager::get().findEntry(m_entryID);
        if (!e || !m_lockBtn) return;
        auto spr = static_cast<CCSprite*>(m_lockBtn->getNormalImage());
        auto frame = CCSpriteFrameCache::get()->spriteFrameByName(e->locked ? "GJ_lock_001.png" : "GJ_lock_open_001.png");
        if (frame) spr->setDisplayFrame(frame);
        fitNode(spr, 22.f);
        spr->setPosition(m_lockBtn->getContentSize() / 2);
        spr->setColor(e->locked ? ccColor3B { 255, 220, 80 } : ccColor3B { 200, 200, 200 });
    }

    void onDelete() {
        auto e = RouletteManager::get().findEntry(m_entryID);
        if (!e) return;
        createQuickPopup(
            "Delete roulette",
            e->locked
                ? "This roulette is <cy>locked</c>. Are you sure you want to delete it? <cr>This cannot be undone.</c>"
                : "Are you sure you want to delete this roulette from the history? <cr>This cannot be undone.</c>",
            "Cancel", "Delete",
            [this](auto, bool yes) {
                if (!yes) return;
                RouletteManager::get().deleteEntry(m_entryID);
                showToast(ToastKind::Stats, "Roulette deleted", NotificationIcon::Success, 1.5f);
                auto cb = m_onChanged;
                this->onClose(nullptr);
                if (cb) cb();
            }
        );
    }

    void drawGraph() {
        auto const W = 440.f;
        auto stats = computeStats(m_results);

        // Area del grafico
        CCRect area = { 55.f, 45.f, 360.f, 180.f };

        auto bg = makePanel({ area.size.width + 16.f, area.size.height + 16.f }, 70);
        m_mainLayer->addChildAtPosition(bg, Anchor::BottomLeft, { area.getMidX(), area.getMidY() });

        auto draw = CCDrawNode::create();
        draw->setBlendFunc({ GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA });
        draw->setContentSize({ W, 285.f });
        draw->setAnchorPoint({ 0.f, 0.f });
        m_mainLayer->addChildAtPosition(draw, Anchor::BottomLeft, { 0.f, 0.f });

        auto toPoint = [&](int index, int percent) {
            float x = m_results.size() <= 1
                ? area.getMidX()
                : area.getMinX() + area.size.width * index / float(m_results.size() - 1);
            float y = area.getMinY() + area.size.height * percent / 100.f;
            return CCPoint { x, y };
        };

        // Rejilla horizontal cada 25%
        for (int p = 0; p <= 100; p += 25) {
            float y = area.getMinY() + area.size.height * p / 100.f;
            draw->drawSegment({ area.getMinX(), y }, { area.getMaxX(), y }, 0.4f, { 1.f, 1.f, 1.f, p == 0 ? 0.6f : 0.18f });
            auto label = makeLabel(fmt::format("{}%", p), "chatFont.fnt", 0.5f, { 1.f, 0.5f });
            m_mainLayer->addChildAtPosition(label, Anchor::BottomLeft, { area.getMinX() - 6.f, y });
        }

        if (m_results.empty()) {
            auto empty = makeLabel("No levels played yet", "bigFont.fnt", 0.4f);
            m_mainLayer->addChildAtPosition(empty, Anchor::BottomLeft, { area.getMidX(), area.getMidY() });
            return;
        }

        // Linea de la media (discontinua)
        {
            float y = area.getMinY() + area.size.height * stats.mean / 100.f;
            for (float x = area.getMinX(); x < area.getMaxX(); x += 8.f) {
                draw->drawSegment({ x, y }, { std::min(x + 4.f, area.getMaxX()), y }, 0.6f, { 0.4f, 0.8f, 1.f, 0.8f });
            }
            auto meanLabel = makeLabel(fmt::format("mean {:.1f}%", stats.mean), "chatFont.fnt", 0.45f, { 1.f, 0.f });
            meanLabel->setColor({ 120, 200, 255 });
            m_mainLayer->addChildAtPosition(meanLabel, Anchor::BottomLeft, { area.getMaxX(), y + 2.f });
        }

        // Linea de porcentajes
        for (size_t i = 1; i < m_results.size(); i++) {
            draw->drawSegment(
                toPoint(i - 1, m_results[i - 1].percent), toPoint(i, m_results[i].percent),
                1.1f, { 1.f, 1.f, 1.f, 0.85f }
            );
        }
        float dot = m_results.size() > 60 ? 1.6f : 2.6f;
        for (size_t i = 0; i < m_results.size(); i++) {
            auto c = percentColor(m_results[i].percent);
            draw->drawDot(toPoint(i, m_results[i].percent), dot, { c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f });
        }

        // Etiquetas del eje X
        size_t step = std::max<size_t>(1, (m_results.size() + 9) / 10);
        for (size_t i = 0; i < m_results.size(); i += step) {
            auto label = makeLabel(std::to_string(i + 1), "chatFont.fnt", 0.5f, { 0.5f, 1.f });
            m_mainLayer->addChildAtPosition(label, Anchor::BottomLeft, { toPoint(i, 0).x, area.getMinY() - 4.f });
        }
        auto axis = makeLabel("Level", "chatFont.fnt", 0.45f);
        axis->setOpacity(160);
        m_mainLayer->addChildAtPosition(axis, Anchor::BottomLeft, { area.getMidX(), 18.f });
    }

public:
    static GraphPopup* create(std::string const& title, std::vector<LevelResult> const& results, int64_t entryID, std::function<void()> onChanged) {
        auto ret = new GraphPopup();
        if (ret->init(title, results, entryID, std::move(onChanged))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

void openGraphPopup(std::string const& title, std::vector<LevelResult> const& results, int64_t entryID, std::function<void()> onChanged) {
    if (auto popup = GraphPopup::create(title, results, entryID, std::move(onChanged))) popup->show();
}
