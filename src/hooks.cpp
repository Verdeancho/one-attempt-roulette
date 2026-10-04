#include "ui/SessionPanel.hpp"
#include "ui/Hud.hpp"

#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>

// Estado global del nivel de ruleta que se esta jugando
static bool s_inRouletteLevel = false;
static bool s_attemptDone = false;

// ---------------------------------------------------------------------------
// Boton en el menu de herramientas (CreatorLayer)
// ---------------------------------------------------------------------------

class $modify(RouletteCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;

        auto winSize = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("roulette-menu"_spr);
        menu->setPosition({ 0.f, 0.f });
        menu->setContentSize(winSize);

        auto icon = CCSprite::create("ruleta.png"_spr);
        auto spr = CircleButtonSprite::create(icon, CircleBaseColor::Green, CircleBaseSize::Medium);
        if (icon) icon->setScale(icon->getScale() * 1.1f);
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [](auto) { openSetupPopup(); });
        btn->setID("roulette-button"_spr);
        btn->setPosition({ 32.f, winSize.height / 2 });
        menu->addChild(btn);

        auto label = CCLabelBMFont::create("Roulette", "bigFont.fnt");
        label->setScale(0.35f);
        label->setPosition({ 32.f, winSize.height / 2 - 30.f });
        menu->addChild(label);

        this->addChild(menu, 10);
        return true;
    }
};

// ---------------------------------------------------------------------------
// Pantalla del nivel (LevelInfoLayer)
// ---------------------------------------------------------------------------

class $modify(RouletteLevelInfoLayer, LevelInfoLayer) {
    struct Fields {
        bool confirmedBack = false;
    };

    bool init(GJGameLevel* level, bool challenge) {
        auto& rm = RouletteManager::get();
        s_inRouletteLevel = false;
        s_attemptDone = false;

        // Si venimos de jugar el nivel anterior y el siguiente ya esta listo,
        // se abre directamente el siguiente
        if (auto next = rm.consumeAdvance(level)) level = next;

        if (!LevelInfoLayer::init(level, challenge)) return false;

        if (rm.isSessionLevel(level)) {
            // el panel se coloca solo segun su configuracion (ojo / ajustes)
            auto panel = SessionPanel::create(level);
            panel->setID("roulette-panel"_spr);
            this->addChild(panel, 20);

            if (auto last = rm.lastResult(); last && rm.isCurrentLevel(level) && !rm.session()->results.empty()) {
                showToast(
                    ToastKind::Results,
                    fmt::format("{}: {}%   |   Total: {}%", last->name, last->percent, rm.session()->stats().total),
                    NotificationIcon::Info, 3.f
                );
            }
        }
        return true;
    }

    void onEnterTransitionDidFinish() {
        LevelInfoLayer::onEnterTransitionDidFinish();
        if (m_level && m_level->m_levelID.value() == RouletteManager::get().lastPlayedID()) {
            showSummaryIfPending();
        }
    }

    bool shouldConfirmBack() {
        auto& rm = RouletteManager::get();
        return !m_fields->confirmedBack && rm.hasSession() && rm.isSessionLevel(m_level);
    }

    void confirmBack(geode::Function<void()> action) {
        createQuickPopup(
            "Leave roulette",
            "The roulette is <cg>saved</c>: you can continue it any time from the <cy>Roulette</c> button in the Create menu.",
            "Cancel", "Leave",
            [this, action = std::move(action)](auto, bool yes) mutable {
                if (!yes) return;
                m_fields->confirmedBack = true;
                RouletteManager::get().cancelFetch();
                action();
            }
        );
    }

    void onBack(CCObject* sender) {
        if (this->shouldConfirmBack()) {
            return this->confirmBack([this] { this->onBack(nullptr); });
        }
        LevelInfoLayer::onBack(sender);
    }

    void keyBackClicked() {
        if (this->shouldConfirmBack()) {
            return this->confirmBack([this] { this->keyBackClicked(); });
        }
        LevelInfoLayer::keyBackClicked();
    }
};

// ---------------------------------------------------------------------------
// Partida: un solo intento por nivel
// ---------------------------------------------------------------------------

class $modify(RoulettePlayLayer, PlayLayer) {
    struct Fields {
        RouletteHud* hud = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        s_inRouletteLevel = RouletteManager::get().isCurrentLevel(level);
        s_attemptDone = false;
        if (s_inRouletteLevel) this->createHud();
        return true;
    }

    void createHud() {
        auto hud = RouletteHud::create();
        this->addChild(hud, 1000);
        m_fields->hud = hud;
    }

    void finishAttempt(int percent) {
        if (!s_inRouletteLevel || s_attemptDone) return;
        s_attemptDone = true;
        percent = std::clamp(percent, 0, 100);
        RouletteManager::get().recordAttempt(m_level, percent);
        if (m_fields->hud) m_fields->hud->setLastPercent(percent);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);

        if (!s_inRouletteLevel || s_attemptDone) return;
        if (object == m_anticheatSpike) return;
        if (!player || !player->m_isDead) return; // noclip o similar
        if (m_isPracticeMode) return;

        this->finishAttempt(this->getCurrentPercentInt());

        float delay = (float)Mod::get()->getSettingValue<double>("exit-delay");
        this->runAction(CCSequence::create(
            CCDelayTime::create(delay),
            CallFuncExt::create([this] { this->onQuit(); }),
            nullptr
        ));
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        if (s_inRouletteLevel && !s_attemptDone && !m_isPracticeMode) {
            this->finishAttempt(100);
        }
    }

    void resetLevel() {
        // Tras el unico intento no se permite reiniciar
        if (s_inRouletteLevel && s_attemptDone) return;
        PlayLayer::resetLevel();
    }

    void delayedResetLevel() {
        if (s_inRouletteLevel && s_attemptDone) return;
        PlayLayer::delayedResetLevel();
    }

    void togglePracticeMode(bool practiceMode) {
        if (s_inRouletteLevel && practiceMode) {
            showToast(ToastKind::Warnings, "Practice mode is not allowed in the roulette", NotificationIcon::Warning);
            return;
        }
        PlayLayer::togglePracticeMode(practiceMode);
    }

    void onQuit() {
        // Salir a mitad de intento cuenta como el intento
        if (s_inRouletteLevel && !s_attemptDone) {
            this->finishAttempt(this->getCurrentPercentInt());
        }
        PlayLayer::onQuit();
        s_inRouletteLevel = false;
    }
};

class $modify(RoulettePauseLayer, PauseLayer) {
    struct Fields {
        bool confirmedQuit = false;
    };

    void onQuit(CCObject* sender) {
        if (s_inRouletteLevel && !s_attemptDone && !m_fields->confirmedQuit && Mod::get()->getSettingValue<bool>("confirm-quit")) {
            int percent = 0;
            if (auto pl = PlayLayer::get()) percent = pl->getCurrentPercentInt();
            createQuickPopup(
                "Quit",
                fmt::format("Quitting <cr>counts as your attempt</c> on this level ({}%). Are you sure?", percent),
                "Cancel", "Leave",
                [this](auto, bool yes) {
                    if (!yes) return;
                    m_fields->confirmedQuit = true;
                    this->onQuit(nullptr);
                }
            );
            return;
        }
        PauseLayer::onQuit(sender);
    }

    void customSetup() {
        PauseLayer::customSetup();
        if (!s_inRouletteLevel) return;

        // Boton para editar el overlay sin salir del nivel
        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("roulette-pause-menu"_spr);
        menu->setPosition({ 0.f, 0.f });
        auto spr = CCSprite::create("eye_open.png"_spr);
        fitNode(spr, 30.f);
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [](auto) { openOverlayPopup(); });
        btn->setPosition({ 30.f, win.height / 2 + 6.f });
        menu->addChild(btn);
        auto label = CCLabelBMFont::create("Overlay", "bigFont.fnt");
        label->setScale(0.3f);
        label->setPosition({ 30.f, win.height / 2 - 16.f });
        menu->addChild(label);
        this->addChild(menu, 10);
    }

    void onRestart(CCObject* sender) {
        if (s_inRouletteLevel) {
            showToast(ToastKind::Warnings, "You only have 1 attempt: restarting is not allowed", NotificationIcon::Warning);
            return;
        }
        PauseLayer::onRestart(sender);
    }

    void onRestartFull(CCObject* sender) {
        if (s_inRouletteLevel) {
            showToast(ToastKind::Warnings, "You only have 1 attempt: restarting is not allowed", NotificationIcon::Warning);
            return;
        }
        PauseLayer::onRestartFull(sender);
    }

    void onPracticeMode(CCObject* sender) {
        if (s_inRouletteLevel) {
            showToast(ToastKind::Warnings, "Practice mode is not allowed in the roulette", NotificationIcon::Warning);
            return;
        }
        PauseLayer::onPracticeMode(sender);
    }
};

class $modify(RouletteEndLevelLayer, EndLevelLayer) {
    void onReplay(CCObject* sender) {
        if (s_inRouletteLevel) return this->onMenu(sender);
        EndLevelLayer::onReplay(sender);
    }

    void onRestartCheckpoint(CCObject* sender) {
        if (s_inRouletteLevel) return this->onMenu(sender);
        EndLevelLayer::onRestartCheckpoint(sender);
    }
};
