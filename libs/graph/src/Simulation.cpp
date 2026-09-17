#include "Simulation.h"
#include <cmath>
#include <algorithm>

/*
方法	精度	稳定性	计算量	适用场景
显式欧拉	一阶	一般	最低	快速原型、简单布局
半隐式欧拉	一阶	较好	低	大多数力导向图
Verlet	二阶	较好	低	粒子/节点系统
Velocity Verlet	二阶	较好	低	需要速度信息的模拟
RK4	四阶	好	高	高精度、时间步较大
隐式欧拉	一阶	最好	高	易发散、刚性系统

*/



// =====================================================================
// Construction
// =====================================================================
Simulation::Simulation(PhysicsState* state)
    : m_state(state)
{}

// =====================================================================
// Force management
// =====================================================================
void Simulation::addForce(const std::string& name, std::unique_ptr<Force> force)
{
    force->initialize(m_state);
    m_forces[name] = std::move(force);
}

void Simulation::removeForce(const std::string& name)
{
    m_forces.erase(name);
}

Force* Simulation::getForce(const std::string& name) const
{
    auto it = m_forces.find(name);
    return (it != m_forces.end()) ? it->second.get() : nullptr;
}

// =====================================================================
// Tick — single simulation step
// =====================================================================
void Simulation::tick()
{
    bool anyDragging = false;
    if (!m_state->dragging.empty()) {
        for (uint8_t d : m_state->dragging) {
            if (d) { anyDragging = true; break; }
        }
    }
    if (anyDragging) {
        m_alpha = 1.0f;
        m_active = true;
    }
    if (m_alpha <= m_alphaMin) {
        m_active = false;
        return;
    }

    if (!m_state->dragPos.empty()) {
        const int N = m_state->nNodes;
        float* pos = m_state->pos.data();
        float* vel = m_state->vel.data();
        const float* dpos = m_state->dragPos.data();
        const auto& drag = m_state->dragging;
        for (int i = 0; i < N; ++i) {
            if (drag[i]) {
                pos[2 * i]     = dpos[2 * i];
                pos[2 * i + 1] = dpos[2 * i + 1];
                vel[2 * i]     = 0.0f;
                vel[2 * i + 1] = 0.0f;
            }
        }
    }

    // Apply each force (modifies vel)
    for (auto& [name, force] : m_forces) {
        force->apply(m_alpha);
    }

    // Integrate velocities -> positions
    float totalSpeed = integrate();

    // Cooling
    ++m_tickCount;
    if (!anyDragging && m_firstStarted && m_tickCount >= m_cooldownDelay) {
        m_alpha *= (1.0f - m_alphaDecay);
    }

    // Early stop when almost settled
    float avgSpeed = totalSpeed / std::max(1, m_state->nNodes);
    if (!anyDragging && avgSpeed < 0.01f && m_alpha < 0.01f) {
        m_active = false;
    }
}

// =====================================================================
// Integrate — velocity decay, max displacement clamp, position update
// =====================================================================
float Simulation::integrate()
{
    const int   N       = m_state->nNodes;
    float*      pos     = m_state->pos.data();
    float*      vel     = m_state->vel.data();
    const auto& drag    = m_state->dragging;
    const float decay   = m_velocityDecay;
    const float dt      = m_dt;
    const float maxDisp = m_maxDisp;


    // Velocity decay (all nodes)
    for (int i = 0; i < 2 * N; ++i) {
        vel[i] *= decay;
    }

    float totalSpeed = 0.0f;

    for (int i = 0; i < N; ++i) {
        float vxi = vel[2 * i];
        float vyi = vel[2 * i + 1];

        totalSpeed += std::fabs(vxi) + std::fabs(vyi);

        // Skip dragged nodes
        if (drag[i]) continue;

        float speed = std::sqrt(vxi * vxi + vyi * vyi);
        if (speed * dt > maxDisp && speed > 1e-8f) {
            float scale = maxDisp / (speed * dt);
            vxi *= scale;
            vyi *= scale;
            vel[2 * i]     = vxi;
            vel[2 * i + 1] = vyi;
        }

        pos[2 * i]     += vxi * dt;
        pos[2 * i + 1] += vyi * dt;
    }

    return totalSpeed;
}

// =====================================================================
// Lifecycle
// =====================================================================
void Simulation::start()
{
    m_active = true;
    if (!m_firstStarted) {
        m_firstStarted = true;
    }
}

void Simulation::stop()
{
    m_active = false;
}

void Simulation::pause()
{
    m_active = false;
}

void Simulation::resume()
{
    if (m_alpha > m_alphaMin) {
        m_active = true;
    }
}

void Simulation::restart()
{
    m_firstStarted = true;
    m_alpha     = 1.0f;
    m_tickCount = 0;
    m_active    = true;
}


