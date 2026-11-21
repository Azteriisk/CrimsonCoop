#pragma once

#include <cstdint>
#include <string>
#include <cmath>

namespace CrimsonCoop {

using PlayerId = uint64_t; // SteamID64 representation
constexpr uint8_t MAX_PLAYERS = 3;

enum class CharacterType : uint8_t {
    None    = 0,
    Kliff   = 1,
    Damiane = 2,
    Oongka  = 3
};

inline const char* CharacterToString(CharacterType type) {
    switch (type) {
        case CharacterType::Kliff:   return "Kliff";
        case CharacterType::Damiane: return "Damiane";
        case CharacterType::Oongka:  return "Oongka";
        default:                     return "Unassigned";
    }
}

struct Vector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    Vector3() = default;
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }

    float Length() const { return std::sqrt(x * x + y * y + z * z); }
};

struct Rotation {
    float pitch{0.0f};
    float yaw{0.0f};
    float roll{0.0f};
};

} // namespace CrimsonCoop
