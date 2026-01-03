
#pragma once
#include <chrono>
#include <algorithm>

class GameTimer {
public:
    using clock = std::chrono::steady_clock;

    void reset() {
        m_startReal = clock::now();
        m_prevReal = m_startReal;
        m_accumGame = 0.0;
        m_deltaGame = 0.0;
        m_accumReal = 0.0;
        m_paused = false;
        m_timeScale = 1.0;
    }

    void start() { if (m_paused) { m_paused = false; m_prevReal = clock::now(); } }
    void stop() { if (!m_paused) { tick(); m_paused = true; } }

    
    void tick() {
        const auto now = clock::now();
        const double dtR = std::chrono::duration<double>(now - m_prevReal).count();
        m_prevReal = now;

        
        m_accumReal += dtR;

        if (m_paused) { m_deltaGame = 0.0; return; }

        
        const double dtClamped = std::clamp(dtR, 0.0, 0.25);  
        m_deltaGame = dtClamped * m_timeScale;
        m_accumGame += m_deltaGame;
    }

    
    void set_time_scale(double s) { m_timeScale = std::max(0.0, s); }

    
    double total_game_time() const { return m_accumGame; }
    double delta_game_time() const { return m_deltaGame; }
    double total_real_time() const { return m_accumReal; }
    bool   paused() const { return m_paused; }

private:
    clock::time_point m_startReal{};
    clock::time_point m_prevReal{};
    double m_accumGame{ 0.0 };  
    double m_deltaGame{ 0.0 };  
    double m_accumReal{ 0.0 };  
    bool   m_paused{ false };
    double m_timeScale{ 1.0 };
};
