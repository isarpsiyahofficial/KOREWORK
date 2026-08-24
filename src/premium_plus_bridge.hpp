#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace korework::premium_plus {

inline int skillSlotFromVirtualKey(int virtualKey) noexcept {
    if (virtualKey >= '1' && virtualKey <= '9') return virtualKey - '1';
    if (virtualKey == '0') return 9;
    return -1;
}

inline bool isBasicAttackKey(int virtualKey) noexcept {
    return virtualKey == 'R' || virtualKey == 'r';
}

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

inline constexpr wchar_t kMappingName[] = L"Local\\PremiumPlusCombo.Rogue.GameBridge.v1";
inline constexpr std::uint32_t kMagic = 0x50435042u;
inline constexpr std::uint32_t kVersion = 1u;
inline constexpr std::size_t kRingSize = 8192u;

enum EventFlags : std::uint32_t {
    KeyDown = 0x01u,
    KeyUp = 0x02u,
    Extended = 0x04u
};

struct Event {
    volatile LONG64 sequence;
    std::uint32_t virtualKey;
    std::uint32_t scanCode;
    std::uint32_t flags;
    std::uint32_t reserved;
};

struct SharedState {
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG64 writeSequence;
    volatile LONG64 gameHeartbeatMs;
    Event events[kRingSize];
};

class GameBridge final {
public:
    GameBridge() = default;
    GameBridge(const GameBridge&) = delete;
    GameBridge& operator=(const GameBridge&) = delete;
    ~GameBridge() { close(); }

    bool connected() noexcept {
        if (!shared_ && !open()) return false;
        heartbeat();
        return true;
    }

    template <typename Fn>
    std::size_t drainKeyDowns(Fn&& onKeyDown, std::size_t maximum = kRingSize) noexcept {
        if (!shared_ && !open()) return 0;
        heartbeat();

        const LONG64 write = InterlockedCompareExchange64(&shared_->writeSequence, 0, 0);
        if (write <= readSequence_) return 0;
        if (write - readSequence_ > static_cast<LONG64>(kRingSize))
            readSequence_ = write - static_cast<LONG64>(kRingSize);

        std::size_t drained = 0;
        while (readSequence_ < write && drained < maximum) {
            const LONG64 sequence = readSequence_ + 1;
            const Event& event = shared_->events[static_cast<std::size_t>(sequence) % kRingSize];
            const LONG64 published = InterlockedCompareExchange64(
                const_cast<volatile LONG64*>(&event.sequence), 0, 0);
            if (published != sequence) break;

            MemoryBarrier();
            if ((event.flags & KeyDown) != 0)
                std::forward<Fn>(onKeyDown)(static_cast<int>(event.virtualKey));
            readSequence_ = sequence;
            ++drained;
        }
        heartbeat();
        return drained;
    }

private:
    bool open() noexcept {
        HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, kMappingName);
        if (mapping == nullptr) return false;
        void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState));
        if (view == nullptr) {
            CloseHandle(mapping);
            return false;
        }

        auto* state = static_cast<SharedState*>(view);
        if (state->magic != kMagic || state->version != kVersion) {
            UnmapViewOfFile(view);
            CloseHandle(mapping);
            return false;
        }

        mapping_ = mapping;
        shared_ = state;
        readSequence_ = InterlockedCompareExchange64(&shared_->writeSequence, 0, 0);
        heartbeat();
        return true;
    }

    void heartbeat() noexcept {
        if (shared_ != nullptr)
            InterlockedExchange64(&shared_->gameHeartbeatMs, static_cast<LONG64>(GetTickCount64()));
    }

    void close() noexcept {
        if (shared_ != nullptr) {
            InterlockedExchange64(&shared_->gameHeartbeatMs, 0);
            UnmapViewOfFile(shared_);
            shared_ = nullptr;
        }
        if (mapping_ != nullptr) {
            CloseHandle(mapping_);
            mapping_ = nullptr;
        }
        readSequence_ = 0;
    }

    HANDLE mapping_{};
    SharedState* shared_{};
    LONG64 readSequence_{};
};

#else

class GameBridge final {
public:
    bool connected() noexcept { return false; }
    template <typename Fn>
    std::size_t drainKeyDowns(Fn&&, std::size_t = 0) noexcept { return 0; }
};

#endif

} // namespace korework::premium_plus
