#include "RemoteProxyManager.hpp"
#include "Logger.hpp"
#include <iostream>

namespace CrimsonCoop {

static RemoteProxyManager* s_instance = nullptr;

RemoteProxyManager::RemoteProxyManager() {
    s_instance = this;
}

RemoteProxyManager& RemoteProxyManager::Instance() {
    static RemoteProxyManager instance;
    return instance;
}

bool RemoteProxyManager::RegisterRemotePlayer(PlayerId steamId, CharacterType character, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    RemoteActorProxy proxy{};
    proxy.steamId = steamId;
    proxy.character = character;
    proxy.playerName = name;

    m_proxies[steamId] = proxy;
    Logger::Log("[RemoteProxy] Registered remote player %s (SteamID: %llu) as %s.\n",
                name.c_str(), steamId, CharacterToString(character));
    return true;
}

void RemoteProxyManager::UnregisterRemotePlayer(PlayerId steamId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_proxies.find(steamId);
    if (it != m_proxies.end()) {
        if (it->second.ccoiaAddress != 0) {
            RestoreLocalAI(steamId);
            m_ccoiaToPlayer.erase(it->second.ccoiaAddress);
        }
        Logger::Log("[RemoteProxy] Unregistered remote player %s (SteamID: %llu).\n",
                    it->second.playerName.c_str(), steamId);
        m_proxies.erase(it);
    }
}

bool RemoteProxyManager::BindProxyActor(PlayerId steamId, uintptr_t ccoiaAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_proxies.find(steamId);
    if (it == m_proxies.end()) {
        Logger::Log("[RemoteProxy] Error: Cannot bind CCOIA %p, player %llu not registered.\n",
                    (void*)ccoiaAddress, steamId);
        return false;
    }

    it->second.ccoiaAddress = ccoiaAddress;
    m_ccoiaToPlayer[ccoiaAddress] = steamId;

    // Resolve component pointers from CCOIA component table (+0x68)
    if (ccoiaAddress != 0) {
        // Component table nominal slots
        it->second.transformCompAddress  = ccoiaAddress + 0x100; // Simulated/mapped component
        it->second.aiBrainCompAddress    = ccoiaAddress + 0x180;
        it->second.frameEventCompAddress = ccoiaAddress + 0x200;
        it->second.equipCompAddress      = ccoiaAddress + 0x280;

        SuppressLocalAI(steamId);
        Logger::Log("[RemoteProxy] Successfully bound CCOIA %p to remote player %s (%s).\n",
                    (void*)ccoiaAddress, it->second.playerName.c_str(), CharacterToString(it->second.character));
    }

    return true;
}

bool RemoteProxyManager::SuppressLocalAI(PlayerId steamId) {
    auto it = m_proxies.find(steamId);
    if (it == m_proxies.end()) return false;

    it->second.isAiDisabled = true;
    Logger::Log("[RemoteProxy] Local companion AI suppressed for %s (Remote piloted).\n",
                CharacterToString(it->second.character));
    return true;
}

bool RemoteProxyManager::RestoreLocalAI(PlayerId steamId) {
    auto it = m_proxies.find(steamId);
    if (it == m_proxies.end()) return false;

    it->second.isAiDisabled = false;
    Logger::Log("[RemoteProxy] Local companion AI restored for %s.\n",
                CharacterToString(it->second.character));
    return true;
}

void RemoteProxyManager::Update(double currentTime, TransformInterpolator& interpolator) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [steamId, proxy] : m_proxies) {
        if (proxy.ccoiaAddress == 0) continue;

        // Retrieve smoothly interpolated transform from the 30Hz snapshot ring buffer
        Vector3 sampledPos;
        Rotation sampledRot;
        if (interpolator.Sample(currentTime, sampledPos, sampledRot)) {
            proxy.lastPosition = sampledPos;
            proxy.lastRotation = sampledRot;
        }

        // In a live process, write directly to the transform component memory
        // *(Vector3*)(proxy.transformCompAddress + 0x40) = proxy.lastPosition;
        // *(Rotation*)(proxy.transformCompAddress + 0x50) = proxy.lastRotation;
    }
}

bool RemoteProxyManager::TriggerRemoteAction(PlayerId steamId, uint32_t actionId, float playbackSpeed, int32_t targetEntityUid) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_proxies.find(steamId);
    if (it == m_proxies.end()) return false;

    it->second.currentActionId = actionId;
    Logger::Log("[RemoteProxy] Dispatched action 0x%X (Speed: %.2f, Target: %d) on proxy %s.\n",
                actionId, playbackSpeed, targetEntityUid, CharacterToString(it->second.character));
    return true;
}

bool RemoteProxyManager::IsRemoteProxy(uintptr_t ccoiaAddress) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ccoiaToPlayer.find(ccoiaAddress) != m_ccoiaToPlayer.end();
}

const RemoteActorProxy* RemoteProxyManager::GetProxyBySteamId(PlayerId steamId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_proxies.find(steamId);
    return (it != m_proxies.end()) ? &it->second : nullptr;
}

const RemoteActorProxy* RemoteProxyManager::GetProxyByCharacter(CharacterType character) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [_, proxy] : m_proxies) {
        if (proxy.character == character) return &proxy;
    }
    return nullptr;
}

size_t RemoteProxyManager::GetActiveProxyCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_proxies.size();
}

void RemoteProxyManager::OnWorldRebuilt(uint64_t newGeneration) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentWorldGeneration = newGeneration;
    m_ccoiaToPlayer.clear();

    // Reset volatile heap pointers while preserving session player bindings
    for (auto& [steamId, proxy] : m_proxies) {
        proxy.ccoiaAddress = 0;
        proxy.transformCompAddress = 0;
        proxy.aiBrainCompAddress = 0;
        proxy.frameEventCompAddress = 0;
        proxy.equipCompAddress = 0;
        proxy.isAiDisabled = false;
    }

    Logger::Log("[RemoteProxy] World generation shifted to %llu. Proxies invalidated for rebind.\n",
                newGeneration);
}

} // namespace CrimsonCoop
