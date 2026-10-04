#include "Common.hpp"

// Crea una lista con scroll a partir de filas ya construidas
static ScrollLayer* makeList(CCSize size, std::vector<CCNode*> const& rows, float rowHeight) {
    auto scroll = ScrollLayer::create(size);
    float total = std::max(size.height, rows.size() * rowHeight);
    scroll->m_contentLayer->setContentSize({ size.width, total });
    for (size_t i = 0; i < rows.size(); i++) {
        auto row = rows[i];
        row->setAnchorPoint({ 0.5f, 0.5f });
        row->setPosition({ size.width / 2, total - (i + 0.5f) * rowHeight });
        scroll->m_contentLayer->addChild(row);
    }
    scroll->scrollToTop();
    return scroll;
}

static CCNode* makeRow(CCSize size, GLubyte opacity = 70) {
    auto row = CCNode::create();
    row->setContentSize(size);
    auto bg = makePanel(size, opacity);
    bg->setPosition(size / 2);
    row->addChild(bg);
    return row;
}

static CCMenuItemSpriteExtra* trashButton(float scale, geode::Function<void(CCMenuItemSpriteExtra*)> cb) {
    auto spr = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
    spr->setScale(scale);
    return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
}

// ---------------------------------------------------------------------------
// Borrado en bloque
// ---------------------------------------------------------------------------

class BulkDeletePopup : public Popup {
protected:
    std::string m_key;
    KeepOptions m_keep;
    std::function<void()> m_onDone;
    CCLabelBMFont* m_countLabel = nullptr;

    bool init(std::string const& key, std::string const& name, std::function<void()> onDone) {
        if (!Popup::init(330.f, 250.f)) return false;
        m_key = key;
        m_onDone = std::move(onDone);
        this->setTitle("Delete history", "goldFont.fnt", 0.7f, 18.f);

        auto scope = makeLabel(key.empty() ? "All roulettes" : fmt::format("Roulettes of: {}", name), "bigFont.fnt", 0.4f);
        scope->limitLabelWidth(290.f, 0.4f, 0.1f);
        m_mainLayer->addChildAtPosition(scope, Anchor::BottomLeft, { 165.f, 205.f });

        auto addOption = [&](char const* text, bool* value, float y) {
            auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.65f, [this, value](auto) {
                *value = !*value;
                this->updateCount();
            });
            toggle->toggle(*value);
            m_buttonMenu->addChildAtPosition(toggle, Anchor::BottomLeft, { 35.f, y });
            auto label = makeLabel(text, "bigFont.fnt", 0.36f, { 0.f, 0.5f });
            m_mainLayer->addChildAtPosition(label, Anchor::BottomLeft, { 55.f, y });
        };
        addOption("Keep the best total record", &m_keep.best, 172.f);
        addOption("Keep the worst total record", &m_keep.worst, 145.f);
        addOption("Keep the most 100% record", &m_keep.mostCompletions, 118.f);

        auto note = makeLabel(
            "Records are per category and number of levels.\nLocked roulettes are never deleted here.",
            "chatFont.fnt", 0.55f
        );
        note->setAlignment(kCCTextAlignmentCenter);
        note->setOpacity(190);
        m_mainLayer->addChildAtPosition(note, Anchor::BottomLeft, { 165.f, 85.f });

        m_countLabel = makeLabel("", "goldFont.fnt", 0.5f);
        m_mainLayer->addChildAtPosition(m_countLabel, Anchor::BottomLeft, { 165.f, 58.f });

        auto del = textButton("Delete", "GJ_button_06.png", 0.75f, [this](auto) { this->onDelete(); });
        m_buttonMenu->addChildAtPosition(del, Anchor::BottomLeft, { 165.f, 26.f });

        this->updateCount();
        return true;
    }

    void updateCount() {
        int n = RouletteManager::get().countBulkDelete(m_key, m_keep);
        m_countLabel->setString(fmt::format("{} roulettes will be deleted", n).c_str());
    }

    void onDelete() {
        int n = RouletteManager::get().countBulkDelete(m_key, m_keep);
        if (n == 0) {
            showToast(ToastKind::Stats, "There are no roulettes to delete", NotificationIcon::Info);
            return;
        }
        createQuickPopup(
            "Are you sure?",
            fmt::format("<cr>{} roulettes</c> will be deleted from the history. <cr>This cannot be undone.</c>", n),
            "Cancel", "Delete",
            [this](auto, bool yes) {
                if (!yes) return;
                int deleted = RouletteManager::get().bulkDelete(m_key, m_keep);
                showToast(ToastKind::Stats, fmt::format("{} roulettes deleted", deleted), NotificationIcon::Success);
                auto cb = m_onDone;
                this->onClose(nullptr);
                if (cb) cb();
            }
        );
    }

public:
    static BulkDeletePopup* create(std::string const& key, std::string const& name, std::function<void()> onDone) {
        auto ret = new BulkDeletePopup();
        if (ret->init(key, name, std::move(onDone))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Historial de una categoria
// ---------------------------------------------------------------------------

class HistoryPopup : public Popup {
protected:
    std::string m_key;
    std::string m_name;
    std::function<void()> m_onChanged;
    CCNode* m_listNode = nullptr;
    // en las dificultades: pestana "Normal" (0) y "Popular" (1) con las categorias por minimo de descargas
    bool m_hasPopularTab = false;
    int m_tab = 0;
    CCMenuItemSpriteExtra* m_tabButtons[2] = {};

    bool init(std::string const& key, std::string const& name, std::function<void()> onChanged) {
        if (!Popup::init(440.f, 280.f)) return false;
        m_key = key;
        m_name = name;
        m_onChanged = std::move(onChanged);
        this->setTitle(fmt::format("History: {}", name), "goldFont.fnt", 0.6f, 18.f);
        if (m_title) m_title->limitLabelWidth(300.f, 0.6f, 0.2f);

        auto trash = trashButton(0.6f, [this](auto) {
            if (auto p = BulkDeletePopup::create(m_key, m_name, [this] { this->changed(); })) p->show();
        });
        m_buttonMenu->addChildAtPosition(trash, Anchor::TopRight, { -22.f, -20.f });

        m_hasPopularTab = key.starts_with("diff:") && key.find("|P") == std::string::npos;
        if (m_hasPopularTab) {
            // si solo hay ruletas populares, se abre directamente esa pestana
            bool hasNormal = false;
            for (auto& e : RouletteManager::get().history()) {
                if (e.config.categoryKey() == key) hasNormal = true;
            }
            m_tab = hasNormal ? 0 : 1;

            char const* names[] = { "Normal", "Popular" };
            for (int i = 0; i < 2; i++) {
                m_tabButtons[i] = textButton(names[i], "GJ_button_04.png", 0.5f, [this, i](auto) {
                    m_tab = i;
                    this->changed();
                });
                m_buttonMenu->addChildAtPosition(m_tabButtons[i], Anchor::BottomLeft, { 175.f + i * 90.f, 240.f });
            }
        }

        m_listNode = CCNode::create();
        m_listNode->setContentSize({ 440.f, 280.f });
        m_mainLayer->addChildAtPosition(m_listNode, Anchor::BottomLeft, { 0.f, 0.f });

        this->buildList();
        return true;
    }

    // Pestana "Popular": una fila por cada minimo de descargas jugado en esta dificultad
    void buildPopularList() {
        struct Group {
            RouletteConfig config;
            int sessions = 0;
            int completed = 0;
            int bestTotal = -1;
            int bestCount = 0;
            int worstTotal = INT_MAX;
            int worstCount = 0;
            int mostCompletions = -1;
        };
        std::map<std::string, Group> groups;
        for (auto& e : RouletteManager::get().history()) {
            if (e.config.mode != RouletteMode::Difficulty || !e.config.popular) continue;
            auto base = e.config;
            base.popular = false;
            if (base.categoryKey() != m_key) continue;
            auto& g = groups[e.config.categoryKey()];
            g.config = e.config;
            g.sessions++;
            if (!e.completed) continue;
            auto st = e.stats();
            g.completed++;
            if (st.total > g.bestTotal) { g.bestTotal = st.total; g.bestCount = e.config.count; }
            if (st.total < g.worstTotal) { g.worstTotal = st.total; g.worstCount = e.config.count; }
            g.mostCompletions = std::max(g.mostCompletions, st.completions);
        }

        if (groups.empty()) {
            auto empty = makeLabel(
                "No popular roulettes yet.\nCheck \"Force popular levels\" in the roulette menu to play one.",
                "bigFont.fnt", 0.32f
            );
            empty->setAlignment(kCCTextAlignmentCenter);
            m_listNode->addChildAtPosition(empty, Anchor::BottomLeft, { 220.f, 130.f }, false);
            return;
        }

        std::vector<Group const*> order;
        for (auto& [key, g] : groups) order.push_back(&g);
        std::sort(order.begin(), order.end(), [](auto a, auto b) { return a->config.minDownloads < b->config.minDownloads; });

        CCSize listSize = { 400.f, 182.f };
        float const cols[] = { 12.f, 175.f, 238.f, 292.f, 336.f, 378.f };
        char const* headers[] = { "Min. downloads", "Played", "Best", "Worst", "100%" };
        for (int i = 0; i < 5; i++) {
            auto h = makeLabel(headers[i], "goldFont.fnt", 0.42f, { i == 0 ? 0.f : 0.5f, 0.5f });
            m_listNode->addChildAtPosition(h, Anchor::BottomLeft, { 20.f + cols[i], 218.f }, false);
        }

        std::vector<CCNode*> rows;
        for (auto g : order) {
            auto row = makeRow({ listSize.width, 32.f });

            auto name = makeLabel(fmt::format("{}+ downloads", formatCount(g->config.minDownloads)), "bigFont.fnt", 0.38f, { 0.f, 0.5f });
            name->limitLabelWidth(150.f, 0.38f, 0.1f);
            name->setPosition({ cols[0], 16.f });
            row->addChild(name);

            auto sessions = makeLabel(fmt::format("{} ({} comp.)", g->sessions, g->completed), "chatFont.fnt", 0.5f);
            sessions->setPosition({ cols[1], 16.f });
            row->addChild(sessions);

            auto addRecord = [&](int value, int count, float x, bool gold) {
                if (value < 0 || value == INT_MAX) {
                    auto dash = makeLabel("-", "chatFont.fnt", 0.55f);
                    dash->setPosition({ x, 16.f });
                    row->addChild(dash);
                    return;
                }
                auto v = makeLabel(fmt::format("{}%", value), gold ? "goldFont.fnt" : "bigFont.fnt", gold ? 0.42f : 0.32f);
                if (!gold) v->setColor({ 255, 130, 130 });
                v->setPosition({ x, 20.f });
                row->addChild(v);
                auto c = makeLabel(fmt::format("{} lvls", count), "chatFont.fnt", 0.42f);
                c->setOpacity(170);
                c->setPosition({ x, 7.f });
                row->addChild(c);
            };
            addRecord(g->bestTotal, g->bestCount, cols[2], true);
            addRecord(g->worstTotal, g->worstCount, cols[3], false);

            auto comp = makeLabel(g->mostCompletions >= 0 ? fmt::format("{}", g->mostCompletions) : "-", g->mostCompletions > 0 ? "goldFont.fnt" : "chatFont.fnt", 0.5f);
            comp->setPosition({ cols[4], 16.f });
            row->addChild(comp);

            auto menu = makeMenu({ 40.f, 32.f });
            menu->setPosition({ cols[5], 16.f });
            auto spr = ButtonSprite::create("View", "bigFont.fnt", "GJ_button_02.png", 0.6f);
            spr->setScale(0.55f);
            auto key = g->config.categoryKey();
            auto catName = g->config.categoryName();
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, key, catName](auto) {
                if (auto p = HistoryPopup::create(key, catName, [this] { this->changed(); })) p->show();
            });
            btn->setPosition({ 20.f, 16.f });
            menu->addChild(btn);
            row->addChild(menu);

            rows.push_back(row);
        }

        auto list = makeList(listSize, rows, 35.f);
        m_listNode->addChildAtPosition(list, Anchor::BottomLeft, { 20.f, 25.f }, false);
        handleTouchPriority(this);
    }

    void changed() {
        // se difiere: puede llamarse desde un boton de la propia lista
        Ref<HistoryPopup> self = this;
        queueInMainThread([self] {
            self->buildList();
            if (self->m_onChanged) self->m_onChanged();
        });
    }

    void buildList() {
        m_listNode->removeAllChildren();
        auto& rm = RouletteManager::get();
        for (int i = 0; i < 2; i++) setButtonSelected(m_tabButtons[i], i == m_tab);
        if (m_hasPopularTab && m_tab == 1) return this->buildPopularList();

        std::vector<HistoryEntry const*> entries;
        for (auto& e : rm.history()) {
            if (e.config.categoryKey() == m_key) entries.push_back(&e);
        }
        std::sort(entries.begin(), entries.end(), [](auto a, auto b) { return a->startedAt > b->startedAt; });
        auto records = rm.computeRecords();

        CCSize listSize = { 400.f, 182.f };
        float const cols[] = { 10.f, 128.f, 175.f, 222.f, 268.f, 310.f, 345.f, 378.f };

        char const* headers[] = { "Date", "Lvls", "Total", "Mean", "SD", "100%" };
        for (int i = 0; i < 6; i++) {
            auto h = makeLabel(headers[i], "goldFont.fnt", 0.42f, { i == 0 ? 0.f : 0.5f, 0.5f });
            m_listNode->addChildAtPosition(h, Anchor::BottomLeft, { 20.f + cols[i], 218.f }, false);
        }

        if (entries.empty()) {
            auto empty = makeLabel("No roulettes recorded", "bigFont.fnt", 0.4f);
            m_listNode->addChildAtPosition(empty, Anchor::BottomLeft, { 220.f, 130.f }, false);
            return;
        }

        std::vector<CCNode*> rows;
        for (auto e : entries) {
            auto st = e->stats();
            auto row = makeRow({ listSize.width, 26.f });
            GroupRecords rec;
            if (auto it = records.find(fmt::format("{}#{}", e->config.categoryKey(), e->config.count)); it != records.end()) {
                rec = it->second;
            }
            bool isBest = e->completed && rec.bestID == e->id;
            bool isWorst = e->completed && rec.worstID == e->id && rec.worstID != rec.bestID;
            bool isMost = e->completed && rec.mostCompletionsID == e->id && st.completions > 0;

            auto date = makeLabel(formatDate(e->startedAt), "chatFont.fnt", 0.55f, { 0.f, 0.5f });
            date->setPosition({ cols[0], 13.f });
            row->addChild(date);

            auto played = makeLabel(
                e->completed ? fmt::format("{}", e->config.count) : fmt::format("{}/{}", st.played, e->config.count),
                "chatFont.fnt", 0.55f
            );
            if (!e->completed) played->setColor({ 255, 140, 140 });
            played->setPosition({ cols[1], 13.f });
            row->addChild(played);

            auto total = makeLabel(fmt::format("{}%", st.total), isBest ? "goldFont.fnt" : "chatFont.fnt", isBest ? 0.5f : 0.6f);
            if (isWorst) total->setColor({ 255, 110, 110 });
            total->setPosition({ cols[2], 13.f });
            row->addChild(total);

            auto mean = makeLabel(fmt::format("{:.1f}%", st.mean), "chatFont.fnt", 0.55f);
            mean->setPosition({ cols[3], 13.f });
            row->addChild(mean);

            auto sd = makeLabel(fmt::format("{:.1f}", st.stddev), "chatFont.fnt", 0.55f);
            sd->setPosition({ cols[4], 13.f });
            row->addChild(sd);

            auto comp = makeLabel(fmt::format("{}", st.completions), isMost ? "goldFont.fnt" : "chatFont.fnt", isMost ? 0.5f : 0.55f);
            comp->setPosition({ cols[5], 13.f });
            row->addChild(comp);

            auto menu = makeMenu({ 80.f, 26.f });
            menu->setPosition({ cols[6] + 17.f, 13.f });

            int64_t id = e->id;
            auto lockSpr = CCSprite::createWithSpriteFrameName(e->locked ? "GJ_lock_001.png" : "GJ_lock_open_001.png");
            fitNode(lockSpr, 15.f);
            lockSpr->setColor(e->locked ? ccColor3B { 255, 220, 80 } : ccColor3B { 150, 150, 150 });
            auto lockBtn = CCMenuItemExt::createSpriteExtra(lockSpr, [this, id](auto) {
                auto entry = RouletteManager::get().findEntry(id);
                if (!entry) return;
                RouletteManager::get().setLocked(id, !entry->locked);
                this->changed();
            });
            lockBtn->setPosition({ 23.f, 13.f });
            menu->addChild(lockBtn);

            auto results = e->results;
            auto title = fmt::format("{} - {}", e->config.categoryName(), formatDate(e->startedAt));
            auto graphSpr = CCSprite::createWithSpriteFrameName("GJ_statsBtn_001.png");
            graphSpr->setScale(0.42f);
            auto graphBtn = CCMenuItemExt::createSpriteExtra(graphSpr, [this, results, title, id](auto) {
                openGraphPopup(title, results, id, [this] { this->changed(); });
            });
            graphBtn->setPosition({ 56.f, 13.f });
            menu->addChild(graphBtn);
            row->addChild(menu);

            rows.push_back(row);
        }

        auto list = makeList(listSize, rows, 29.f);
        m_listNode->addChildAtPosition(list, Anchor::BottomLeft, { 20.f, 25.f }, false);

        auto hint = makeLabel("Gold = record (best total / most 100%)   Red = worst total   Lock = never deleted", "chatFont.fnt", 0.45f);
        hint->setOpacity(150);
        m_listNode->addChildAtPosition(hint, Anchor::BottomLeft, { 220.f, 13.f }, false);

        handleTouchPriority(this);
    }

public:
    static HistoryPopup* create(std::string const& key, std::string const& name, std::function<void()> onChanged) {
        auto ret = new HistoryPopup();
        if (ret->init(key, name, std::move(onChanged))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Estadisticas generales
// ---------------------------------------------------------------------------

class StatsPopup : public Popup {
protected:
    RouletteMode m_tab = RouletteMode::Difficulty;
    CCNode* m_content = nullptr;
    CCMenuItemSpriteExtra* m_tabs[3] = {};

    bool init() override {
        if (!Popup::init(440.f, 290.f)) return false;
        this->setTitle("Statistics", "goldFont.fnt", 0.75f, 18.f);

        char const* names[] = { "Difficulties", "Demonlist", "Specials" };
        for (int i = 0; i < 3; i++) {
            m_tabs[i] = textButton(names[i], "GJ_button_04.png", 0.6f, [this, i](auto) { this->setTab((RouletteMode)i); });
            m_buttonMenu->addChildAtPosition(m_tabs[i], Anchor::BottomLeft, { 110.f + i * 110.f, 245.f });
        }

        auto trash = trashButton(0.6f, [this](auto) {
            if (auto p = BulkDeletePopup::create("", "", [this] { this->buildList(); })) p->show();
        });
        m_buttonMenu->addChildAtPosition(trash, Anchor::TopRight, { -22.f, -20.f });

        m_content = CCNode::create();
        m_content->setContentSize({ 440.f, 290.f });
        m_mainLayer->addChildAtPosition(m_content, Anchor::BottomLeft, { 0.f, 0.f });

        this->setTab(RouletteMode::Difficulty);
        return true;
    }

    void setTab(RouletteMode tab) {
        m_tab = tab;
        for (int i = 0; i < 3; i++) setButtonSelected(m_tabs[i], i == (int)tab);
        this->buildList();
    }

    struct CategorySummary {
        RouletteConfig config;
        int sessions = 0;
        int completed = 0;
        int bestTotal = -1;
        int bestTotalCount = 0;
        int worstTotal = INT_MAX;
        int worstTotalCount = 0;
        int mostCompletions = -1;
        int64_t lastPlayed = 0;
        bool hasPopular = false; // tiene ruletas "popular" (se ven dentro de su historial)
    };

    void buildList() {
        m_content->removeAllChildren();

        std::map<std::string, CategorySummary> summaries;
        std::vector<std::string> order;

        auto addCategory = [&](RouletteConfig const& cfg) {
            auto key = cfg.categoryKey();
            if (!summaries.contains(key)) {
                summaries[key].config = cfg;
                order.push_back(key);
            }
        };

        // Filas fijas (version clasica de cada dificultad / especial)
        if (m_tab == RouletteMode::Difficulty) {
            for (int d = 1; d <= DIFF_COUNT; d++) {
                RouletteConfig cfg;
                cfg.mode = RouletteMode::Difficulty;
                cfg.difficulty = d;
                addCategory(cfg);
            }
        } else if (m_tab == RouletteMode::Special) {
            for (int s = 0; s < SPECIAL_COUNT; s++) {
                RouletteConfig cfg;
                cfg.mode = RouletteMode::Special;
                cfg.special = s;
                addCategory(cfg);
            }
        }

        for (auto& e : RouletteManager::get().history()) {
            if (e.config.mode != m_tab) continue;
            if (e.config.mode == RouletteMode::Difficulty && e.config.popular) {
                // las populares no ocupan filas: van en la pestana "Popular" de su dificultad
                auto base = e.config;
                base.popular = false;
                addCategory(base);
                summaries[base.categoryKey()].hasPopular = true;
                continue;
            }
            addCategory(e.config);
            auto& s = summaries[e.config.categoryKey()];
            auto st = e.stats();
            s.sessions++;
            s.lastPlayed = std::max(s.lastPlayed, e.startedAt);
            if (e.completed) {
                s.completed++;
                if (st.total > s.bestTotal) {
                    s.bestTotal = st.total;
                    s.bestTotalCount = e.config.count;
                }
                if (st.total < s.worstTotal) {
                    s.worstTotal = st.total;
                    s.worstTotalCount = e.config.count;
                }
                s.mostCompletions = std::max(s.mostCompletions, st.completions);
            }
        }

        // Orden: por dificultad/especial/rango y despues por tipo (clasico, plat., ambos)
        std::stable_sort(order.begin(), order.end(), [&](auto const& a, auto const& b) {
            auto& ca = summaries[a].config;
            auto& cb = summaries[b].config;
            auto ka = std::tuple(ca.difficulty, ca.special, ca.topFrom, ca.topTo, (int)ca.effectiveLength());
            auto kb = std::tuple(cb.difficulty, cb.special, cb.topFrom, cb.topTo, (int)cb.effectiveLength());
            return ka < kb;
        });

        if (order.empty()) {
            auto empty = makeLabel("You have not played any Demonlist roulette yet", "bigFont.fnt", 0.35f);
            m_content->addChildAtPosition(empty, Anchor::BottomLeft, { 220.f, 130.f }, false);
            return;
        }

        CCSize listSize = { 400.f, 185.f };
        float const cols[] = { 20.f, 44.f, 180.f, 238.f, 292.f, 336.f, 378.f };

        char const* headers[] = { "", "Category", "Played", "Best", "Worst", "100%" };
        for (int i = 1; i < 6; i++) {
            auto h = makeLabel(headers[i], "goldFont.fnt", 0.42f, { i == 1 ? 0.f : 0.5f, 0.5f });
            m_content->addChildAtPosition(h, Anchor::BottomLeft, { 20.f + cols[i], 214.f }, false);
        }

        std::vector<CCNode*> rows;
        for (auto& key : order) {
            auto& s = summaries[key];
            auto row = makeRow({ listSize.width, 32.f }, s.sessions ? 80 : 40);

            auto icon = categoryIcon(s.config, 0.45f);
            icon->setPosition({ cols[0], 16.f });
            row->addChild(icon);

            auto name = makeLabel(s.config.categoryName(), "bigFont.fnt", 0.36f, { 0.f, 0.5f });
            name->limitLabelWidth(125.f, 0.36f, 0.1f);
            name->setPosition({ cols[1], s.lastPlayed ? 20.f : 16.f });
            row->addChild(name);

            if (s.lastPlayed) {
                auto last = makeLabel(fmt::format("Last: {}", formatDate(s.lastPlayed)), "chatFont.fnt", 0.45f, { 0.f, 0.5f });
                last->setOpacity(170);
                last->setPosition({ cols[1], 8.f });
                row->addChild(last);
            }

            auto sessions = makeLabel(s.sessions ? fmt::format("{} ({} comp.)", s.sessions, s.completed) : "-", "chatFont.fnt", 0.5f);
            sessions->setPosition({ cols[2], 16.f });
            row->addChild(sessions);

            auto addRecord = [&](int value, int count, float x, bool gold) {
                if (value < 0 || value == INT_MAX) {
                    auto dash = makeLabel("-", "chatFont.fnt", 0.55f);
                    dash->setPosition({ x, 16.f });
                    row->addChild(dash);
                    return;
                }
                auto v = makeLabel(fmt::format("{}%", value), gold ? "goldFont.fnt" : "bigFont.fnt", gold ? 0.42f : 0.32f);
                if (!gold) v->setColor({ 255, 130, 130 });
                v->setPosition({ x, 20.f });
                row->addChild(v);
                auto c = makeLabel(fmt::format("{} lvls", count), "chatFont.fnt", 0.42f);
                c->setOpacity(170);
                c->setPosition({ x, 7.f });
                row->addChild(c);
            };
            addRecord(s.bestTotal, s.bestTotalCount, cols[3], true);
            addRecord(s.worstTotal, s.worstTotalCount, cols[4], false);

            auto comp = makeLabel(s.mostCompletions >= 0 ? fmt::format("{}", s.mostCompletions) : "-", s.mostCompletions > 0 ? "goldFont.fnt" : "chatFont.fnt", 0.5f);
            comp->setPosition({ cols[5], 16.f });
            row->addChild(comp);

            if (s.sessions || s.hasPopular) {
                auto menu = makeMenu({ 40.f, 32.f });
                menu->setPosition({ cols[6], 16.f });
                auto spr = ButtonSprite::create("View", "bigFont.fnt", "GJ_button_02.png", 0.6f);
                spr->setScale(0.55f);
                auto catName = s.config.categoryName();
                auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, key, catName](auto) {
                    if (auto p = HistoryPopup::create(key, catName, [this] { this->buildList(); })) p->show();
                });
                btn->setPosition({ 20.f, 16.f });
                menu->addChild(btn);
                row->addChild(menu);
            }

            rows.push_back(row);
        }

        auto list = makeList(listSize, rows, 35.f);
        m_content->addChildAtPosition(list, Anchor::BottomLeft, { 20.f, 18.f }, false);
        handleTouchPriority(this);
    }

public:
    static StatsPopup* create() {
        auto ret = new StatsPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

void openStatsPopup() {
    if (auto popup = StatsPopup::create()) popup->show();
}
