#pragma once

#include "Common.hpp"
#include "NetworkPackets.hpp"
#include "Interpolation.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace CrimsonCoop {

struct RemoteActorProxy {
    PlayerId      steamId{0};
    CharacterType character{CharacterType::None};
    std::string   playerName;
    uintptr_t     ccoiaAddress{0};         // Memory address of pa::ClientChildOnlyInGameActor
    uintptr_t     transformCompAddress{0}; // pa::ClientTransformActorComponent
    uintptr_t     aiBrainCompAddress{0};   // pa::ClientAIActorComponent
    uintptr_t     frameEventCompAddress{0};// pa::ClientFrameEventActorComponent
    uintptr_t     equipCompAddress{0};     // pa::ClientEquipSlotActorComponent
    bool          isAiDisabled{false};
    Vector3       lastPosition{0.0f, 0.0f, 0.0f};
    Rotation      lastRotation{0.0f, 0.0f, 0.0f};
    Vector3       lastVelocity{0.0f, 0.0f, 0.0f};
    uint32_t      currentActionId{0};
    uint64_t      lastUpdateTick{0};
};

class RemoteProxyManager {
public:
    RemoteProxyManager();
    ~RemoteProxyManager() = default;

    // Singleton access
    static RemoteProxyManager& Instance();

    // Register / Unregister connected remote players
    bool RegisterRemotePlayer(PlayerId steamId, CharacterType character, const std::string& name);
    void UnregisterRemotePlayer(PlayerId steamId);

    // Bind a resolved in-world CCOIA pointer to a registered remote player
    bool BindProxyActor(PlayerId steamId, uintptr_t ccoiaAddress);

    // Disable local game AI on this proxy actor so network replication has exclusive authority
    bool SuppressLocalAI(PlayerId steamId);

    // Restore local game AI (e.g. on disconnect or when returning to solo play)
    bool RestoreLocalAI(PlayerId steamId);

    // Update loop: samples interpolated transforms and drives the proxy models in the world
    void Update(double currentTime, TransformInterpolator& interpolator);

    // Replicate an animation or combat skill on the remote proxy
    bool TriggerRemoteAction(PlayerId steamId, uint32_t actionId, float playbackSpeed, int32_t targetEntityUid);

    // Query helpers
    bool IsRemoteProxy(uintptr_t ccoiaAddress) const;
    const RemoteActorProxy* GetProxyBySteamId(PlayerId steamId) const;
    const RemoteActorProxy* GetProxyByCharacter(CharacterType character) const;
    size_t GetActiveProxyCount() const;

    // Reset all proxies on world reconstruction / level transition
    void OnWorldRebuilt(uint64_t newGeneration);

private:
    mutable std::mutex m_mutex;
    std::unordered_map<PlayerId, RemoteActorProxy> m_proxies;
    std::unordered_map<uintptr_t, PlayerId> m_ccoiaToPlayer;
    uint64_t m_currentWorldGeneration{0};
};

} // namespace CrimsonCoop
