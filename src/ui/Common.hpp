#pragma once

#include "../Roulette.hpp"

// Utilidades de interfaz compartidas

// Enlaces de los creditos
inline constexpr char const* HOW_TO_VIDEO_URL = "https://www.youtube.com/watch?v=i_8NfIRx8k8"; // Sr.Guillester
inline constexpr char const* JENRAI_URL = "https://www.youtube.com/@enjoythemomentsbywailai";

inline CCMenu* makeMenu(CCSize size) {
    auto menu = CCMenu::create();
    menu->setContentSize(size);
    menu->ignoreAnchorPointForPosition(false);
    menu->setAnchorPoint({ 0.5f, 0.5f });
    return menu;
}

inline CCNode* makeBox(CCSize size) {
    auto node = CCNode::create();
    node->setContentSize(size);
    node->setAnchorPoint({ 0.5f, 0.5f });
    return node;
}

inline CCLabelBMFont* makeLabel(std::string const& text, char const* font, float scale, CCPoint anchor = { 0.5f, 0.5f }) {
    auto label = CCLabelBMFont::create(text.c_str(), font);
    label->setScale(scale);
    label->setAnchorPoint(anchor);
    return label;
}

inline NineSlice* makePanel(CCSize size, GLubyte opacity = 90) {
    auto bg = NineSlice::create("square02_small.png");
    bg->setContentSize(size);
    bg->setColor({ 0, 0, 0 });
    bg->setOpacity(opacity);
    return bg;
}

inline CCMenuItemSpriteExtra* textButton(
    char const* text, char const* bg, float scale, geode::Function<void(CCMenuItemSpriteExtra*)> cb
) {
    auto spr = ButtonSprite::create(text, "goldFont.fnt", bg, 0.8f);
    spr->setScale(scale);
    return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
}

// Escala un nodo para que quepa en un cuadrado de `size` puntos
inline void fitNode(CCNode* node, float size) {
    auto s = node->getContentSize();
    float m = std::max(s.width, s.height);
    if (m > 0) node->setScale(size / m);
}

// Icono de una ruleta especial
inline CCSprite* specialIcon(int special) {
    auto& info = specialInfo(special);
    CCSprite* spr = nullptr;
    if (info.icon) spr = CCSprite::createWithSpriteFrameName(info.icon);
    if (!spr) spr = CCSprite::create("roulette.png"_spr);
    return spr;
}

// Cara de dificultad con su nombre. En los demons se usa la version "larga" del
// juego, que pone el nombre completo (Easy Demon, Extreme Demon...) en vez de "Demon"
inline GJDifficultySprite* difficultyFace(int diff, float size) {
    int frame = diffInfo(diff).spriteFrame;
    bool demon = frame >= 6;
    auto spr = GJDifficultySprite::create(frame, demon ? GJDifficultyName::Long : GJDifficultyName::Short);
    fitNode(spr, size);
    return spr;
}

// Icono de la categoria: cara de dificultad, icono especial o "TOP x-y"
inline CCNode* categoryIcon(RouletteConfig const& cfg, float scale) {
    if (cfg.mode == RouletteMode::Special) {
        auto spr = specialIcon(cfg.special);
        fitNode(spr, 34.f * scale);
        return spr;
    }
    if (cfg.mode == RouletteMode::Difficulty) {
        return difficultyFace(cfg.difficulty, 44.f * scale);
    }
    auto node = CCNode::create();
    CCSprite* spr = CCSprite::createWithSpriteFrameName("diffIcon_10_btn_001.png");
    if (!spr) spr = GJDifficultySprite::create(10, GJDifficultyName::Short);
    fitNode(spr, 30.f * scale);
    auto topText = cfg.topTo >= 9999 ? fmt::format("#{}+", cfg.topFrom) : fmt::format("#{}-{}", cfg.topFrom, cfg.topTo);
    auto top = CCLabelBMFont::create(topText.c_str(), "bigFont.fnt");
    top->limitLabelWidth(45.f * scale, 0.45f * scale, 0.1f);
    auto size = spr->getScaledContentSize();
    node->setContentSize({ std::max(size.width, top->getScaledContentSize().width), size.height + 10.f * scale });
    node->setAnchorPoint({ 0.5f, 0.5f });
    spr->setPosition({ node->getContentWidth() / 2, node->getContentHeight() - size.height / 2 });
    top->setPosition({ node->getContentWidth() / 2, 4.f * scale });
    node->addChild(spr);
    node->addChild(top);
    return node;
}

// Color segun el porcentaje (rojo -> amarillo -> verde, oro para 100%)
inline ccColor3B percentColor(int percent) {
    if (percent >= 100) return { 255, 215, 60 };
    float t = std::clamp(percent / 99.f, 0.f, 1.f);
    if (t < 0.5f) {
        float k = t / 0.5f;
        return { 255, (GLubyte)(80 + 175 * k), 80 };
    }
    float k = (t - 0.5f) / 0.5f;
    return { (GLubyte)(255 - 175 * k), 255, 80 };
}

// Toggle de seleccion para botones de texto (verde = activo, gris = inactivo)
inline void setButtonSelected(CCMenuItemSpriteExtra* btn, bool selected) {
    if (!btn) return;
    if (auto spr = typeinfo_cast<ButtonSprite*>(btn->getNormalImage())) {
        spr->updateBGImage(selected ? "GJ_button_01.png" : "GJ_button_04.png");
    }
}

// entryID != 0 -> la grafica es de una ruleta del historial (permite candado y borrar)
void openGraphPopup(
    std::string const& title, std::vector<LevelResult> const& results,
    int64_t entryID = 0, std::function<void()> onChanged = nullptr
);
void openLevelListPopup(std::string const& title, std::vector<LevelResult> const& results, bool clickable = false);
void openStatsPopup();
void openSetupPopup();
void openHowToPopup();
// Muestra "Buscando nivel..." y abre la pantalla de info del nivel al encontrarlo
void runWithLoading(std::function<void(RouletteManager::LevelCallback)> action);
void openLevelByID(int levelID);

// Elementos que se pueden mover y ajustar
enum class OverlayTarget : int {
    Hud = 0,    // overlay durante la partida
    Panel = 1,  // panel de la pantalla de info del nivel
    Toasts = 2, // avisos (resultado del nivel, advertencias...)
};
void openOverlayPopup(OverlayTarget target = OverlayTarget::Hud);

// Avisos del mod (se pueden desactivar por tipo y mover en el editor)
enum class ToastKind : int {
    Results = 0,  // resultado del nivel anterior al llegar al siguiente
    Warnings = 1, // "no puedes reiniciar", "practica no permitida"...
    Stats = 2,    // candados, borrados...
    Errors = 3,   // errores al buscar niveles
};
constexpr int TOAST_KIND_COUNT = 4;
// force: se muestra aunque ese tipo este desactivado (boton de prueba)
void showToast(ToastKind kind, std::string const& text, NotificationIcon icon = NotificationIcon::Info, float time = 2.f, bool force = false);
