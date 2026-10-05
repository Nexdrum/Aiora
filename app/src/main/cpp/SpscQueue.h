#pragma once
#include <array>
#include <atomic>
#include <cstddef>

namespace aiora {

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2);
public:
    bool push(const T& item) noexcept {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto next = (h + 1) % Capacity;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        data_[h] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) noexcept {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false;
        out = data_[t];
        tail_.store((t + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    std::array<T, Capacity> data_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

} // namespace aiora
