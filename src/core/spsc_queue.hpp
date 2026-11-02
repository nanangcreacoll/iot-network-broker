#pragma once

#include <atomic>
#include <cstddef>
#include <concepts>
#include <new>
#include <optional>
#include <span>
#include <vector>

namespace iot::core
{
#if defined(__cpp_lib_hardware_interference_size)
    using std::hardware_destructive_interference_size;
#else
    inline constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

    /**
     * @brief High-performance Lock-Free Single-Producer Single-Consumer (SPSC) Ring Buffer.
     * @tparam T Type of element stored. Must be copy/move constructible.
     * @tparam Capacity Total buffer capacity. Must be a power of two for fast bitwise masking.
     */
    template <typename T, std::size_t Capacity>
        requires(Capacity > 1 && (Capacity & (Capacity - 1)) == 0) // Enforce power of 2 at compile-time
    class SPSCQueue
    {
    public:
        using value_type = T;

        SPSCQueue() : buffer(Capacity) {}

        SPSCQueue(const SPSCQueue &) = delete;
        SPSCQueue &operator=(const SPSCQueue &) = delete;
        SPSCQueue(SPSCQueue &&) = delete;
        SPSCQueue &operator=(SPSCQueue &&) = delete;

        /**
         * @brief Pushes an item into the ring buffer (Producer Thread Only).
         * @tparam U Forwarding type for perfect forwarding.
         * @param item Item to insert.
         * @return true if successfully inserted, false if buffer full.
         */
        template <typename U>
            requires std::is_assignable_v<T &, U &&>
        [[nodiscard]] bool push(U &&item) noexcept
        {
            const auto currentHead = this->head.load(std::memory_order_relaxed);

            // Use cached tail to avoid reading atomic consumer tail on every push
            if (currentHead - this->cachedTail == Capacity)
            {
                this->cachedTail = this->tail.load(std::memory_order_acquire);
                if (currentHead - this->cachedTail == Capacity)
                {
                    return false;
                }
            }

            this->buffer[currentHead & iot::core::SPSCQueue<T, Capacity>::Mask] = std::forward<U>(item);

            // Release store guarantees buffer assignment is visible to consumer before head updates
            this->head.store(currentHead + 1, std::memory_order_release);
            return true;
        }

        /**
         * @brief Pops an item from the ring buffer (Consumer Thread Only).
         * @return std::optional containing the popped value, or std::nullopt if empty.
         */
        [[nodiscard]] std::optional<T> pop() noexcept
        {
            const auto currentTail = this->tail.load(std::memory_order_relaxed);

            // Use cached head to avoid reading atomic producer head on every pop
            if (currentTail == this->cachedHead)
            {
                this->cachedHead = this->head.load(std::memory_order_acquire);
                if (currentTail == this->cachedHead)
                {
                    return std::nullopt; // Queue empty
                }
            }

            T item = std::move(this->buffer[currentTail & iot::core::SPSCQueue<T, Capacity>::Mask]);

            // Release store guarantees item move is completed before tail updates
            this->tail.store(currentTail + 1, std::memory_order_release);
            return item;
        }

        /**
         * @brief Checks if the queue is empty.
         * @note Safe to call from producer or consumer, but result may be stale immediately.
         */
        [[nodiscard]] bool empty() const noexcept
        {
            return this->head.load(std::memory_order_relaxed) == this->tail.load(std::memory_order_relaxed);
        }

        /**
         * @brief Returns approximate number of items currently in the queue.
         */
        [[nodiscard]] std::size_t size() const noexcept
        {
            const auto currentHead = this->head.load(std::memory_order_relaxed);
            const auto currentTail = this->tail.load(std::memory_order_relaxed);
            return (currentHead >= currentTail) ? (currentHead - currentTail) : 0;
        }

        [[nodiscard]] constexpr std::size_t capacity() const noexcept
        {
            return Capacity;
        }

    private:
        static constexpr std::size_t Mask = Capacity - 1;

        // Buffer container
        std::vector<T> buffer;

        // Producer state (aligned to prevent false sharing with consumer state)
        alignas(iot::core::hardware_destructive_interference_size) std::atomic<std::size_t> head{0};
        std::size_t cachedTail{0};

        // Consumer state (aligned to prevent false sharing with producer state)
        alignas(iot::core::hardware_destructive_interference_size) std::atomic<std::size_t> tail{0};
        std::size_t cachedHead{0};
    };

} // namespace iot::core