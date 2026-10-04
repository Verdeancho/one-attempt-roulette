#include "Common.hpp"
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/LoadingSpinner.hpp>

// ---------------------------------------------------------------------------
// Popup de carga mientras se busca un nivel
// ---------------------------------------------------------------------------

class LoadingPopup : public Popup {
protected:
    bool m_cancelled = false;

    bool init(std::string const& text) {
        if (!Popup::init(240.f, 130.f)) return false;
        this->setTitle("Roulette", "goldFont.fnt", 0.7f, 18.f);

        auto spinner = LoadingSpinner::create(36.f);
        m_mainLayer->addChildAtPosition(spinner, Anchor::BottomLeft, { 120.f, 68.f });

        auto label = makeLabel(text, "bigFont.fnt", 0.4f);
        m_mainLayer->addChildAtPosition(label, Anchor::BottomLeft, { 120.f, 30.f });
        return true;
    }

    void onClose(CCObject* sender) override {
        m_cancelled = true;
        RouletteManager::get().cancelFetch();
        Popup::onClose(sender);
    }

public:
    bool cancelled() const { return m_cancelled; }

    void finish() {
        m_cancelled = true;
        Popup::onClose(nullptr);
    }

    static LoadingPopup* create(std::string const& text) {
        auto ret = new LoadingPopup();
        if (ret->init(text)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// Muestra un popup de carga y, cuando el nivel esta listo, abre su LevelInfoLayer
void openLevelByID(int levelID) {
    runWithLoading([levelID](RouletteManager::LevelCallback cb) {
        RouletteManager::get().fetchLevel(levelID, std::move(cb));
    });
}

void runWithLoading(std::function<void(RouletteManager::LevelCallback)> action) {
    Ref<LoadingPopup> loading = LoadingPopup::create("Finding a level...");
    loading->show();

    action([loading](GJGameLevel* level, std::string const& err) {
        if (loading->cancelled()) return;
        loading->finish();
        if (!level) {
            showToast(ToastKind::Errors, err.empty() ? "Could not get the level" : err, NotificationIcon::Error, 4.f);
            return;
        }
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, LevelInfoLayer::scene(level, false)));
    });
}

// ---------------------------------------------------------------------------
// Popup principal de configuracion
// ---------------------------------------------------------------------------

class SetupPopup : public Popup {
protected:
    static constexpr float W = 400.f;
    static constexpr float H = 290.f;

    RouletteConfig m_config;
    CCNode* m_modeNode = nullptr;
    CCMenu* m_modeMenu = nullptr;
    CCMenuItemSpriteExtra* m_tabs[3] = {};
    std::vector<CCMenuItemSpriteExtra*> m_choiceButtons; // dificultades o especiales
    CCLabelBMFont* m_choiceName = nullptr;
    TextInput* m_fromInput = nullptr;
    TextInput* m_toInput = nullptr;
    TextInput* m_countInput = nullptr;
    TextInput* m_minDownloadsInput = nullptr;
    CCLabelBMFont* m_popularLabel = nullptr;
    CCMenuItemSpriteExtra* m_lengthButtons[3] = {};
    CCLabelBMFont* m_lengthLabel = nullptr;
    CCLabelBMFont* m_lengthNote = nullptr;

    bool init() override {
        if (!Popup::init(W, H)) return false;
        this->setTitle("One Attempt Roulette", "goldFont.fnt", 0.55f, 17.f);
        if (m_title) m_title->limitLabelWidth(178.f, 0.55f, 0.3f);
        m_config = RouletteManager::get().lastConfig();

        // Botones de la esquina: estadisticas, ajustes y overlay
        auto statsSpr = CCSprite::createWithSpriteFrameName("GJ_statsBtn_001.png");
        statsSpr->setScale(0.48f);
        auto statsBtn = CCMenuItemExt::createSpriteExtra(statsSpr, [](auto) { openStatsPopup(); });
        m_buttonMenu->addChildAtPosition(statsBtn, Anchor::TopRight, { -19.f, -19.f });

        auto optSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        optSpr->setScale(0.4f);
        auto optBtn = CCMenuItemExt::createSpriteExtra(optSpr, [](auto) { openSettingsPopup(Mod::get()); });
        m_buttonMenu->addChildAtPosition(optBtn, Anchor::TopRight, { -45.f, -19.f });

        auto eyeSpr = CCSprite::create("eye_open.png"_spr);
        fitNode(eyeSpr, 18.f);
        auto eyeBtn = CCMenuItemExt::createSpriteExtra(eyeSpr, [](auto) { openOverlayPopup(); });
        m_buttonMenu->addChildAtPosition(eyeBtn, Anchor::TopRight, { -69.f, -19.f });

        // Como se juega
        auto helpSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
        helpSpr->setScale(0.6f);
        auto helpBtn = CCMenuItemExt::createSpriteExtra(helpSpr, [](auto) { openHowToPopup(); });
        m_buttonMenu->addChildAtPosition(helpBtn, Anchor::TopRight, { -92.f, -19.f });

        // Video de como se juega (Sr.Guillester), con su icono al lado
        auto openVideo = [](auto) {
            createQuickPopup(
                "How to play video",
                "Open <cy>Sr.Guillester</c>'s video about the mod in your browser?",
                "Cancel", "Open",
                [](auto, bool yes) { if (yes) web::openLinkInBrowser(HOW_TO_VIDEO_URL); }
            );
        };
        auto videoSpr = CCSprite::create("play_video.png"_spr);
        fitNode(videoSpr, 20.f);
        auto videoBtn = CCMenuItemExt::createSpriteExtra(videoSpr, openVideo);
        m_buttonMenu->addChildAtPosition(videoBtn, Anchor::TopLeft, { 50.f, -19.f });
        if (auto guilleSpr = CCSprite::create("guillester.png"_spr)) {
            fitNode(guilleSpr, 18.f);
            auto guilleBtn = CCMenuItemExt::createSpriteExtra(guilleSpr, openVideo);
            m_buttonMenu->addChildAtPosition(guilleBtn, Anchor::TopLeft, { 73.f, -19.f });
        }

        // Pestanas de modo
        char const* tabNames[] = { "Difficulty", "Demonlist", "Specials" };
        for (int i = 0; i < 3; i++) {
            m_tabs[i] = textButton(tabNames[i], "GJ_button_04.png", 0.55f, [this, i](auto) { this->setMode((RouletteMode)i); });
            m_buttonMenu->addChildAtPosition(m_tabs[i], Anchor::BottomLeft, { 80.f + i * 120.f, 240.f });
        }

        // Contenedores del contenido de cada modo
        m_modeNode = CCNode::create();
        m_modeNode->setContentSize({ W, H });
        m_mainLayer->addChildAtPosition(m_modeNode, Anchor::BottomLeft, { 0.f, 0.f });

        m_modeMenu = makeMenu({ W, H });
        m_mainLayer->addChildAtPosition(m_modeMenu, Anchor::Center);

        auto sep = makePanel({ W - 40.f, 1.5f }, 120);
        m_mainLayer->addChildAtPosition(sep, Anchor::BottomLeft, { W / 2, 84.f });

        this->buildLengthRow();
        this->buildCountRow();
        this->buildBottom();
        this->setMode(m_config.mode);

        return true;
    }

    // --- clasico / plataformas / ambos ---
    void buildLengthRow() {
        float y = 99.f;
        m_lengthLabel = makeLabel("Type:", "goldFont.fnt", 0.48f);
        m_mainLayer->addChildAtPosition(m_lengthLabel, Anchor::BottomLeft, { 88.f, y });

        char const* names[] = { "Classic", "Platformer", "Both" };
        float xs[] = { 148.f, 228.f, 304.f };
        for (int i = 0; i < 3; i++) {
            m_lengthButtons[i] = textButton(names[i], "GJ_button_04.png", 0.42f, [this, i](auto) {
                m_config.length = (LengthFilter)i;
                this->updateLength();
            });
            m_buttonMenu->addChildAtPosition(m_lengthButtons[i], Anchor::BottomLeft, { xs[i], y });
        }

        m_lengthNote = makeLabel("The Pointercrate Demonlist only has classic levels", "chatFont.fnt", 0.52f);
        m_lengthNote->setOpacity(170);
        m_mainLayer->addChildAtPosition(m_lengthNote, Anchor::BottomLeft, { W / 2, y });
    }

    void updateLength() {
        bool demonlist = m_config.isListMode();
        m_lengthNote->setString(m_config.mode == RouletteMode::Demonlist
            ? "The Pointercrate Demonlist only has classic levels"
            : "The Challenge List only has classic levels");
        m_lengthLabel->setVisible(!demonlist);
        m_lengthNote->setVisible(demonlist);
        for (int i = 0; i < 3; i++) {
            m_lengthButtons[i]->setVisible(!demonlist);
            setButtonSelected(m_lengthButtons[i], i == (int)m_config.length);
        }
    }

    // --- numero de niveles ---
    void buildCountRow() {
        float y = 64.f;
        auto label = makeLabel("Levels:", "goldFont.fnt", 0.5f);
        m_mainLayer->addChildAtPosition(label, Anchor::BottomLeft, { 62.f, y });

        auto minus = textButton("-", "GJ_button_04.png", 0.5f, [this](auto) { this->changeCount(-1); });
        m_buttonMenu->addChildAtPosition(minus, Anchor::BottomLeft, { 108.f, y });

        m_countInput = TextInput::create(56.f, "25");
        m_countInput->setScale(0.85f);
        m_countInput->setCommonFilter(CommonFilter::Uint);
        m_countInput->setMaxCharCount(4);
        m_countInput->setString(std::to_string(m_config.count));
        m_mainLayer->addChildAtPosition(m_countInput, Anchor::BottomLeft, { 146.f, y });

        auto plus = textButton("+", "GJ_button_04.png", 0.5f, [this](auto) { this->changeCount(1); });
        m_buttonMenu->addChildAtPosition(plus, Anchor::BottomLeft, { 184.f, y });

        int presets[] = { 10, 25, 50, 100 };
        for (int i = 0; i < 4; i++) {
            int n = presets[i];
            auto btn = textButton(std::to_string(n).c_str(), "GJ_button_05.png", 0.42f, [this, n](auto) {
                m_countInput->setString(std::to_string(n));
            });
            m_buttonMenu->addChildAtPosition(btn, Anchor::BottomLeft, { 236.f + i * 37.f, y });
        }
    }

    void changeCount(int delta) {
        int n = numFromString<int>(std::string(m_countInput->getString())).unwrapOr(m_config.count);
        n = std::clamp(n + delta, 1, 9999);
        m_countInput->setString(std::to_string(n));
    }

    // --- botones inferiores ---
    void buildBottom() {
        auto& rm = RouletteManager::get();
        float y = 21.f;

        if (auto s = rm.session()) {
            auto st = s->stats();
            auto info = makeLabel(
                fmt::format("Roulette in progress: {}  |  {}/{}  |  Total {}%", s->config.categoryName(), st.played, s->config.count, st.total),
                "chatFont.fnt", 0.6f
            );
            info->limitLabelWidth(W - 30.f, 0.6f, 0.2f);
            info->setColor({ 140, 255, 140 });
            m_mainLayer->addChildAtPosition(info, Anchor::BottomLeft, { W / 2, 41.f });

            auto cont = textButton("Continue", "GJ_button_01.png", 0.62f, [this](auto) { this->onContinue(); });
            auto start = textButton("New", "GJ_button_02.png", 0.62f, [this](auto) { this->onStart(); });
            auto abandon = textButton("Abandon", "GJ_button_06.png", 0.62f, [this](auto) { this->onAbandon(); });
            m_buttonMenu->addChildAtPosition(cont, Anchor::BottomLeft, { 105.f, y });
            m_buttonMenu->addChildAtPosition(start, Anchor::BottomLeft, { 200.f, y });
            m_buttonMenu->addChildAtPosition(abandon, Anchor::BottomLeft, { 295.f, y });
        } else {
            auto start = textButton("Start", "GJ_button_01.png", 0.75f, [this](auto) { this->onStart(); });
            m_buttonMenu->addChildAtPosition(start, Anchor::BottomLeft, { W / 2, y + 3.f });
        }
    }

    // --- modo ---
    void setMode(RouletteMode mode) {
        this->readInputs();
        m_config.mode = mode;
        for (int i = 0; i < 3; i++) setButtonSelected(m_tabs[i], i == (int)mode);

        m_modeNode->removeAllChildren();
        m_modeMenu->removeAllChildren();
        m_choiceButtons.clear();
        m_choiceName = nullptr;
        m_fromInput = nullptr;
        m_toInput = nullptr;
        m_minDownloadsInput = nullptr;
        m_popularLabel = nullptr;

        switch (mode) {
            case RouletteMode::Difficulty: this->buildDifficulty(); break;
            case RouletteMode::Demonlist: this->buildDemonlist(); break;
            case RouletteMode::Special: this->buildSpecials(); break;
        }
        this->updateLength();
        handleTouchPriority(this);
    }

    void highlightChoice(int index) {
        for (int i = 0; i < (int)m_choiceButtons.size(); i++) {
            auto btn = m_choiceButtons[i];
            bool sel = i == index;
            if (auto spr = typeinfo_cast<CCSprite*>(btn->getNormalImage())) {
                spr->setOpacity(sel ? 255 : 110);
                spr->setColor(sel ? ccColor3B { 255, 255, 255 } : ccColor3B { 170, 170, 170 });
            }
            float base = btn->getTag() / 100.f;
            float scale = sel ? base * 1.15f : base * 0.9f;
            btn->setScale(scale);
            btn->m_baseScale = scale;
        }
    }

    void buildDifficulty() {
        for (int d = 1; d <= DIFF_COUNT; d++) {
            int row = d <= 5 ? 0 : 1;
            int col = (d - 1) % 5;
            // en los demons la cara lleva el nombre completo (Easy Demon...), no solo "Demon"
            auto spr = difficultyFace(d, d <= 5 ? 36.f : 40.f);
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, d](auto) { this->selectDifficulty(d); });
            btn->setTag(87); // escala base * 100
            btn->setPosition({ W / 2 + (col - 2) * 62.f, row == 0 ? 207.f : 166.f });
            m_modeMenu->addChild(btn);
            m_choiceButtons.push_back(btn);
        }
        m_choiceName = makeLabel("", "bigFont.fnt", 0.42f);
        m_choiceName->setPosition({ W / 2, 137.f });
        m_modeNode->addChild(m_choiceName);

        // Solo niveles populares: ordena por descargas y exige un minimo
        float py = 118.f;
        auto popular = CCMenuItemExt::createTogglerWithStandardSprites(0.48f, [this](auto) {
            m_config.popular = !m_config.popular;
            this->updatePopular();
        });
        popular->toggle(m_config.popular);
        popular->setPosition({ 96.f, py });
        m_modeMenu->addChild(popular);
        m_popularLabel = makeLabel("", "bigFont.fnt", 0.27f, { 0.f, 0.5f });
        m_popularLabel->setPosition({ 108.f, py });
        m_modeNode->addChild(m_popularLabel);

        m_minDownloadsInput = TextInput::create(110.f, "100000");
        m_minDownloadsInput->setCommonFilter(CommonFilter::Uint);
        m_minDownloadsInput->setMaxCharCount(9);
        m_minDownloadsInput->setString(std::to_string(m_config.minDownloads));
        m_minDownloadsInput->setScale(0.6f);
        m_minDownloadsInput->setPosition({ 318.f, py });
        m_modeNode->addChild(m_minDownloadsInput);
        this->updatePopular();

        this->selectDifficulty(m_config.difficulty);
    }

    // Muestra el minimo de descargas solo con la casilla marcada (por defecto 100000)
    void updatePopular() {
        if (!m_popularLabel || !m_minDownloadsInput) return;
        bool on = m_config.popular;
        m_popularLabel->setString(on ? "Popular, min. downloads:" : "Force popular levels");
        m_minDownloadsInput->setVisible(on);
        if (on && numFromString<int>(std::string(m_minDownloadsInput->getString())).unwrapOr(0) <= 0) {
            m_minDownloadsInput->setString("100000");
        }
    }

    void selectDifficulty(int d) {
        m_config.difficulty = d;
        this->highlightChoice(d - 1);
        if (m_choiceName) {
            m_choiceName->setString(diffInfo(d).name);
            m_choiceName->limitLabelWidth(180.f, 0.42f, 0.1f);
        }
    }

    void buildSpecials() {
        // fila 1: recientes, aleatorio, amigos (y la Challenge List, que se anade al final
        // para que el indice del boton coincida con el tipo de especial)
        auto rowX = [](int s) { return s == (int)SpecialType::ChallengeList ? 320.f : 80.f + s * 80.f; };
        for (int s = 0; s < 3; s++) {
            auto spr = specialIcon(s);
            fitNode(spr, 28.f);
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, s](auto) { this->selectSpecial(s); });
            btn->setTag(100);
            btn->setPosition({ rowX(s), 208.f });
            m_modeMenu->addChild(btn);
            m_choiceButtons.push_back(btn);

            auto label = makeLabel(s == 1 ? "Random" : specialInfo(s).name, "bigFont.fnt", 0.3f);
            label->setPosition({ rowX(s), 189.f });
            m_modeNode->addChild(label);
        }
        // fila 2: challenges de cada modo de juego
        for (int s = 3; s < (int)SpecialType::ChallengeList; s++) {
            auto spr = specialIcon(s);
            fitNode(spr, 28.f);
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, s](auto) { this->selectSpecial(s); });
            btn->setTag(100);
            btn->setPosition({ W / 2 + (s - 3 - 3.5f) * 40.f, 160.f });
            m_modeMenu->addChild(btn);
            m_choiceButtons.push_back(btn);
        }
        // Challenge List (fila 1, a la derecha)
        {
            int s = (int)SpecialType::ChallengeList;
            auto spr = specialIcon(s);
            fitNode(spr, 28.f);
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, s](auto) { this->selectSpecial(s); });
            btn->setTag(100);
            btn->setPosition({ rowX(s), 208.f });
            m_modeMenu->addChild(btn);
            m_choiceButtons.push_back(btn);
            auto label = makeLabel("Challenge List", "bigFont.fnt", 0.3f);
            label->setPosition({ rowX(s), 189.f });
            m_modeNode->addChild(label);
        }
        m_choiceName = makeLabel("", "bigFont.fnt", 0.4f);
        m_choiceName->setPosition({ W / 2, 128.f });
        m_modeNode->addChild(m_choiceName);
        this->selectSpecial(m_config.special);
    }

    void selectSpecial(int s) {
        m_config.special = s;
        this->highlightChoice(s);
        if (!m_choiceName) return;
        std::string desc = specialInfo(s).name;
        switch ((SpecialType)s) {
            case SpecialType::Recent: desc = "Recent: one of the ~100 latest uploads"; break;
            case SpecialType::RandomAll: desc = "Random: any level from all of GD"; break;
            case SpecialType::Friends: desc = "Levels made by your friends"; break;
            case SpecialType::ChallengeList: desc = "The Challenge List (challengelist.gd): the hardest challenges"; break;
            default: break;
        }
        m_choiceName->setString(desc.c_str());
        m_choiceName->limitLabelWidth(W - 40.f, 0.4f, 0.1f);
        if (m_lengthNote) this->updateLength();
    }

    void buildDemonlist() {
        auto fromLabel = makeLabel("From top", "goldFont.fnt", 0.48f);
        fromLabel->setPosition({ 130.f, 216.f });
        m_modeNode->addChild(fromLabel);
        auto toLabel = makeLabel("To top", "goldFont.fnt", 0.48f);
        toLabel->setPosition({ 270.f, 216.f });
        m_modeNode->addChild(toLabel);

        m_fromInput = TextInput::create(90.f, "From");
        m_fromInput->setCommonFilter(CommonFilter::Uint);
        m_fromInput->setMaxCharCount(4);
        m_fromInput->setString(std::to_string(m_config.topFrom));
        m_fromInput->setScale(0.85f);
        m_fromInput->setPosition({ 130.f, 194.f });
        m_modeNode->addChild(m_fromInput);

        auto dash = makeLabel("-", "bigFont.fnt", 0.6f);
        dash->setPosition({ 200.f, 194.f });
        m_modeNode->addChild(dash);

        m_toInput = TextInput::create(90.f, "To");
        m_toInput->setCommonFilter(CommonFilter::Uint);
        m_toInput->setMaxCharCount(4);
        m_toInput->setString(std::to_string(m_config.topTo));
        m_toInput->setScale(0.85f);
        m_toInput->setPosition({ 270.f, 194.f });
        m_modeNode->addChild(m_toInput);

        struct Preset { char const* name; int from; int to; };
        Preset presets[] = {
            { "Main List", 1, 75 }, { "Extended", 76, 150 }, { "Legacy", 151, 9999 },
            { "Top 10", 1, 10 }, { "Top 50", 1, 50 }, { "Top 100", 1, 100 },
        };
        float x = 30.f;
        for (int i = 0; i < 6; i++) {
            auto p = presets[i];
            auto btn = textButton(p.name, i < 3 ? "GJ_button_02.png" : "GJ_button_05.png", 0.4f, [this, p](auto) {
                m_fromInput->setString(std::to_string(p.from));
                m_toInput->setString(std::to_string(p.to));
            });
            float w = btn->getScaledContentSize().width;
            btn->setPosition({ x + w / 2, 162.f });
            x += w + 6.f;
            m_modeMenu->addChild(btn);
        }
        // centrar la fila de presets
        float rowWidth = x - 6.f - 30.f;
        float offset = (W - rowWidth) / 2 - 30.f;
        for (auto child : CCArrayExt<CCNode*>(m_modeMenu->getChildren())) {
            child->setPositionX(child->getPositionX() + offset);
        }

        auto hint = makeLabel("Type any range (e.g. 100 - 500). Main 1-75, Extended 76-150, Legacy 151+", "chatFont.fnt", 0.5f);
        hint->limitLabelWidth(W - 30.f, 0.5f, 0.1f);
        hint->setOpacity(180);
        hint->setPosition({ W / 2, 133.f });
        m_modeNode->addChild(hint);
    }

    void readInputs() {
        if (m_minDownloadsInput) {
            int n = numFromString<int>(std::string(m_minDownloadsInput->getString())).unwrapOr(0);
            m_config.minDownloads = n > 0 ? n : 100000;
        }
        if (m_fromInput && m_toInput) {
            m_config.topFrom = numFromString<int>(std::string(m_fromInput->getString())).unwrapOr(m_config.topFrom);
            m_config.topTo = numFromString<int>(std::string(m_toInput->getString())).unwrapOr(m_config.topTo);
        }
        if (m_countInput) {
            m_config.count = numFromString<int>(std::string(m_countInput->getString())).unwrapOr(m_config.count);
        }
    }

    // --- acciones ---
    void onStart() {
        this->readInputs();
        auto cfg = m_config;
        cfg.count = std::clamp(cfg.count, 1, 9999);
        if (cfg.mode == RouletteMode::Demonlist) {
            if (cfg.topFrom < 1) cfg.topFrom = 1;
            if (cfg.topTo < cfg.topFrom) std::swap(cfg.topFrom, cfg.topTo);
            if (cfg.topFrom < 1 || cfg.topTo < 1) {
                showToast(ToastKind::Errors, "Invalid top range", NotificationIcon::Error);
                return;
            }
        }

        auto begin = [this, cfg] {
            this->onClose(nullptr);
            runWithLoading([cfg](RouletteManager::LevelCallback cb) {
                RouletteManager::get().startSession(cfg, std::move(cb));
            });
        };

        if (RouletteManager::get().hasSession()) {
            createQuickPopup(
                "New roulette",
                "You already have a roulette in progress. If you start another one, the current one will be saved in the history as <cr>abandoned</c>.",
                "Cancel", "Start",
                [begin](auto, bool yes) { if (yes) begin(); }
            );
        } else {
            begin();
        }
    }

    void onContinue() {
        this->onClose(nullptr);
        runWithLoading([](RouletteManager::LevelCallback cb) {
            RouletteManager::get().continueSession(std::move(cb));
        });
    }

    void onAbandon() {
        createQuickPopup(
            "Abandon",
            "The current roulette will be saved in the history as <cr>abandoned</c>. Are you sure?",
            "Cancel", "Abandon",
            [this](auto, bool yes) {
                if (!yes) return;
                RouletteManager::get().abandonSession();
                this->onClose(nullptr);
                openSetupPopup();
            }
        );
    }

public:
    static SetupPopup* create() {
        auto ret = new SetupPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

void openSetupPopup() {
    if (auto popup = SetupPopup::create()) popup->show();
}
