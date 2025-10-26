// GameTimer.h
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

    // Call once per frame
    void tick() {
        const auto now = clock::now();
        const double dtR = std::chrono::duration<double>(now - m_prevReal).count();
        m_prevReal = now;

        // real time (unscaled, never paused)
        m_accumReal += dtR;

        if (m_paused) { m_deltaGame = 0.0; return; }

        // clamp to avoid giant spikes when the app stalls
        const double dtClamped = std::clamp(dtR, 0.0, 0.25);  // max 250ms/frame
        m_deltaGame = dtClamped * m_timeScale;
        m_accumGame += m_deltaGame;
    }

    // Set 0 to pause, <1 for slow-mo, >1 for fast forward
    void set_time_scale(double s) { m_timeScale = std::max(0.0, s); }

    // Seconds
    double total_game_time() const { return m_accumGame; }
    double delta_game_time() const { return m_deltaGame; }
    double total_real_time() const { return m_accumReal; }
    bool   paused() const { return m_paused; }

private:
    clock::time_point m_startReal{};
    clock::time_point m_prevReal{};
    double m_accumGame{ 0.0 };  // scaled, pause-aware
    double m_deltaGame{ 0.0 };  // scaled, pause-aware
    double m_accumReal{ 0.0 };  // unscaled, not pause-aware
    bool   m_paused{ false };
    double m_timeScale{ 1.0 };
};
