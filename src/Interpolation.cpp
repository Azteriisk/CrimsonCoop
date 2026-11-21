#include "Interpolation.hpp"
#include <algorithm>

namespace CrimsonCoop {

TransformInterpolator::TransformInterpolator(double interpolationDelay)
    : m_interpolationDelay(interpolationDelay) {}

void TransformInterpolator::AddSnapshot(const TransformSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Keep snapshots strictly ordered by timestamp
    if (!m_buffer.empty() && snapshot.timestamp <= m_buffer.back().timestamp) {
        return; // Drop out-of-order or duplicate packet
    }

    m_buffer.push_back(snapshot);
    if (m_buffer.size() > MAX_SNAPSHOTS) {
        m_buffer.pop_front();
    }
}

float TransformInterpolator::LerpAngle(float a, float b, float t) {
    float diff = std::fmod(b - a + 180.0f, 360.0f) - 180.0f;
    return a + diff * t;
}

bool TransformInterpolator::Sample(double renderTime, Vector3& outPos, Rotation& outRot) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_buffer.empty()) {
        return false;
    }

    // Target sample time is slightly behind real-time to allow continuous interpolation
    double targetTime = renderTime - m_interpolationDelay;

    // If only one snapshot exists or target is before the oldest snapshot
    if (m_buffer.size() == 1 || targetTime <= m_buffer.front().timestamp) {
        outPos = m_buffer.front().position;
        outRot = m_buffer.front().rotation;
        return true;
    }

    // If target is beyond the newest snapshot, extrapolate via velocity
    if (targetTime >= m_buffer.back().timestamp) {
        const auto& newest = m_buffer.back();
        float delta = static_cast<float>(targetTime - newest.timestamp);
        // Clamp extrapolation to 250ms to prevent runaway wandering
        delta = std::min(delta, 0.25f);
        outPos = newest.position + (newest.velocity * delta);
        outRot = newest.rotation;
        return true;
    }

    // Find the two surrounding snapshots for interpolation
    for (size_t i = 0; i < m_buffer.size() - 1; ++i) {
        const auto& s0 = m_buffer[i];
        const auto& s1 = m_buffer[i + 1];

        if (targetTime >= s0.timestamp && targetTime <= s1.timestamp) {
            double range = s1.timestamp - s0.timestamp;
            float t = (range > 0.0001) ? static_cast<float>((targetTime - s0.timestamp) / range) : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);

            outPos.x = Lerp(s0.position.x, s1.position.x, t);
            outPos.y = Lerp(s0.position.y, s1.position.y, t);
            outPos.z = Lerp(s0.position.z, s1.position.z, t);

            outRot.pitch = Lerp(s0.rotation.pitch, s1.rotation.pitch, t);
            outRot.yaw   = LerpAngle(s0.rotation.yaw, s1.rotation.yaw, t);
            outRot.roll  = Lerp(s0.rotation.roll, s1.rotation.roll, t);
            return true;
        }
    }

    outPos = m_buffer.back().position;
    outRot = m_buffer.back().rotation;
    return true;
}

void TransformInterpolator::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.clear();
}

} // namespace CrimsonCoop
