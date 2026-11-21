#pragma once

#include "Common.hpp"
#include <deque>
#include <mutex>

namespace CrimsonCoop {

struct TransformSnapshot {
    double    timestamp{0.0};
    Vector3   position;
    Rotation  rotation;
    Vector3   velocity;
};

class TransformInterpolator {
public:
    TransformInterpolator(double interpolationDelay = 0.05); // 50ms buffer window

    void AddSnapshot(const TransformSnapshot& snapshot);
    bool Sample(double renderTime, Vector3& outPos, Rotation& outRot) const;
    void Reset();

private:
    static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
    static float LerpAngle(float a, float b, float t);

    double m_interpolationDelay;
    mutable std::mutex m_mutex;
    std::deque<TransformSnapshot> m_buffer;
    static constexpr size_t MAX_SNAPSHOTS = 64;
};

} // namespace CrimsonCoop
