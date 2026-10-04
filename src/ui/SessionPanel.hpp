#pragma once

#include "Common.hpp"

// Panel con las estadisticas de la sesion en la pantalla de info del nivel.
// Tiene un ojo (plegar/desplegar) y un boton de ajustes (mover, tamano...)
class SessionPanel : public CCNode {
protected:
    static constexpr float PANEL_W = 110.f;
    static constexpr float PANEL_H = 190.f;

    int m_levelID = 0;
    GJGameLevel* m_level = nullptr;
    CCNode* m_content = nullptr;
    NineSlice* m_bg = nullptr;
    CCMenu* m_controls = nullptr;
    CCMenuItemSpriteExtra* m_eyeBtn = nullptr;
    CCMenuItemSpriteExtra* m_gearBtn = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCMenu* m_retryMenu = nullptr;
    bool m_transitioning = false;

    static SessionPanel* s_current;

    bool init(GJGameLevel* level);
    void onEnter() override;
    void onExit() override;
    void build();
    void updateStatus();
    void poll(float);

public:
    static SessionPanel* create(GJGameLevel* level);
    static SessionPanel* current() { return s_current; }

    // Aplica posicion, tamano, opacidad, fondo y plegado
    void applyConfig();
};

void showSummaryIfPending();
