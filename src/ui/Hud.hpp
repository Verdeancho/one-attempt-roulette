#pragma once

#include "Common.hpp"

// Colores personalizables (overlay en partida y panel del nivel)
enum class ColorSlot : int {
    Text = 0,     // textos ("Roulette", "Total:", "Mean:"...)
    Progress = 1, // 13/50
    Total = 2,    // suma de porcentajes
    Mean = 3,     // media
    StdDev = 4,   // desviacion tipica
};
constexpr int COLOR_SLOT_COUNT = 5;
char const* colorSlotName(int slot);

// Configuracion de un elemento movible (overlay en partida o panel de la
// pantalla del nivel). Se guarda en los saved values del mod
struct OverlayConfig {
    float x = 0.13f;       // centro, normalizado (0..1)
    float y = 0.86f;
    float scale = 1.f;
    float opacity = 0.85f;
    bool collapsed = false; // overlay: oculto / panel: plegado
    bool background = true;
    bool liveUpdate = true; // overlay: total y media en directo durante el nivel
    bool toasts[TOAST_KIND_COUNT] = { true, true, true, true }; // avisos: que tipos se muestran
    ccColor3B colors[COLOR_SLOT_COUNT] = {};

    ccColor3B color(ColorSlot slot) const { return colors[(int)slot]; }

    static OverlayConfig defaults(OverlayTarget target);
    static OverlayConfig load(OverlayTarget target);
    void save(OverlayTarget target) const;
};

// Posiciona un nodo (anchor 0.5) segun la configuracion, sin salirse de la pantalla
void placeOnScreen(CCNode* node, OverlayConfig const& cfg);

// Overlay durante la partida (solo informacion: mientras juegas no se puede hacer click)
class RouletteHud : public CCNode {
protected:
    NineSlice* m_bg = nullptr;
    CCLabelBMFont* m_title = nullptr;
    CCLabelBMFont* m_progress = nullptr;
    CCLabelBMFont* m_totalName = nullptr;
    CCLabelBMFont* m_totalValue = nullptr;
    CCLabelBMFont* m_meanName = nullptr;
    CCLabelBMFont* m_meanValue = nullptr;
    CCLabelBMFont* m_sdName = nullptr;
    CCLabelBMFont* m_sdValue = nullptr;
    CCLabelBMFont* m_last = nullptr;
    OverlayConfig m_cfg;
    int m_lastPercent = -1; // porcentaje del intento ya terminado (-1 = jugando)
    int m_livePercent = -1; // porcentaje actual mostrado en directo (-1 = no)

    static RouletteHud* s_current;

    bool init() override;
    void onEnter() override;
    void onExit() override;
    void tick(float);

public:
    static RouletteHud* create();
    static RouletteHud* current() { return s_current; }

    void setLastPercent(int percent);
    // Recalcula texto, colores, tamano y posicion segun la configuracion
    void refresh();
};
