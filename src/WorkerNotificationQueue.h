#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace vfpc {

// Plugin-local transport only: no SDK objects, callbacks or borrowed text are stored.
class WorkerNotificationQueue {
public:
    static constexpr std::size_t Capacity = 64;
    static constexpr std::size_t TextLimit = 256 * 1024;
    static constexpr std::size_t AttemptBudget = 8;
    static constexpr const char* OverflowMessage =
        "Some background notifications could not be displayed; check the vFPC log.";

    struct Line {
        std::string text;
        bool urgent = false;
    };
    struct Batch {
        std::array<Line, 3> lines{};
        std::size_t count = 0;

        // Called inside Enqueue's exception boundary, including string formatting.
        void Add(std::string text, bool urgent) {
            if (count == lines.size()) throw std::length_error("notification batch");
            lines[count++] = { std::move(text), urgent };
        }
    };
    enum class Outcome { Accepted, Closed, Full, Failed };
    struct Statistics {
        std::uint64_t rejected, failed, delivery_failed, discarded, closed;
    };

    // Builder runs synchronously outside the lock; workers never wait for UI/space.
    template<class Builder>
    Outcome Enqueue(Builder&& build) noexcept {
        try {
            if (closing_.load(std::memory_order_acquire)) {
                Increment(closed_);
                return Outcome::Closed;
            }
            Batch batch = std::forward<Builder>(build)();
            if (!batch.count || batch.count > batch.lines.size())
                throw std::length_error("notification batch");
            std::size_t bytes = 0;
            for (std::size_t i = 0; i < batch.count; ++i) {
                if (batch.lines[i].text.size() > TextLimit - bytes) return Reject();
                bytes += batch.lines[i].text.size();
            }
            std::lock_guard lock(mutex_);
            if (closing_.load(std::memory_order_relaxed)) {
                Increment(closed_);
                return Outcome::Closed;
            }
            if (count_ == Capacity || bytes > TextLimit - bytes_) return Reject();
            batches_[(head_ + count_) % Capacity] = std::move(batch);
            ++count_;
            bytes_ += bytes;
            return Outcome::Accepted;
        }
        catch (...) {
            Increment(failed_);
            Increment(pending_loss_);
            return Outcome::Failed;
        }
    }

    // Consumer guard excludes parallel/nested drains, not host callback/destructor overlap.
    // SDK affinity and unload exclusion remain host assumptions, not properties of this queue.
    template<class Sink>
    std::size_t Drain(Sink&& sink) noexcept {
        if (draining_.test_and_set(std::memory_order_acquire)) return 0;
        struct Release {
            std::atomic_flag& flag;
            ~Release() { flag.clear(std::memory_order_release); }
        } release{draining_};
        std::size_t attempts = 0;
        try {
            std::uint64_t lost = 0;
            {
                std::unique_lock lock(mutex_, std::try_to_lock);
                if (!lock || closing_.load(std::memory_order_relaxed)) return 0;
                lost = pending_loss_.exchange(0, std::memory_order_relaxed);
            }
            if (lost && !closing_.load(std::memory_order_acquire)) {
                ++attempts;
                Deliver(sink, OverflowMessage, true);
            }
            while (attempts < AttemptBudget) {
                Batch batch;
                {
                    std::unique_lock lock(mutex_, std::try_to_lock);
                    if (!lock || closing_.load(std::memory_order_relaxed) || !count_)
                        break;
                    if (batches_[head_].count > AttemptBudget - attempts) break;
                    batch = std::move(batches_[head_]);
                    for (std::size_t i = 0; i < batch.count; ++i)
                        bytes_ -= batch.lines[i].text.size();
                    head_ = (head_ + 1) % Capacity;
                    --count_;
                }
                // No queue lock across SDK calls. A sink may enqueue or reenter Drain.
                for (std::size_t i = 0; i < batch.count; ++i) {
                    if (closing_.load(std::memory_order_acquire)) {
                        Increment(discarded_, batch.count - i);
                        break;
                    }
                    ++attempts;
                    Deliver(sink, batch.lines[i].text.c_str(), batch.lines[i].urgent);
                }
            }
        }
        catch (...) { Increment(failed_); }
        return attempts;
    }

    // Close before requesting stops. Never hold this lock while joining a worker.
    void Close() noexcept {
        closing_.store(true, std::memory_order_release);
        try { std::lock_guard lock(mutex_); }
        catch (...) { Increment(failed_); }
    }

    // Only after workers join (and host callbacks are quiescent); never flush to SDK.
    void Discard() noexcept {
        Close();
        try {
            std::lock_guard lock(mutex_);
            while (count_) {
                auto& batch = batches_[head_];
                Increment(discarded_, batch.count);
                batch = Batch{};
                head_ = (head_ + 1) % Capacity;
                --count_;
            }
            bytes_ = 0;
        }
        catch (...) { Increment(failed_); }
    }

    Statistics Stats() const noexcept {
        return { rejected_.load(), failed_.load(), delivery_failed_.load(),
                 discarded_.load(), closed_.load() };
    }

private:
    static_assert(std::is_nothrow_move_assignable_v<Batch>);
    static void Increment(std::atomic<std::uint64_t>& counter,
                          std::uint64_t amount = 1) noexcept {
        auto previous = counter.load(std::memory_order_relaxed);
        for (;;) {
            const auto room = (std::numeric_limits<std::uint64_t>::max)() - previous;
            const auto next = previous + (amount > room ? room : amount);
            if (counter.compare_exchange_weak(previous, next, std::memory_order_relaxed))
                return;
        }
    }
    Outcome Reject() noexcept {
        Increment(rejected_);
        Increment(pending_loss_);
        return Outcome::Full;
    }
    template<class Sink>
    void Deliver(Sink& sink, const char* text, bool urgent) noexcept {
        try { sink(text, urgent); }
        catch (...) { Increment(delivery_failed_); }
    }

    std::mutex mutex_;
    std::array<Batch, Capacity> batches_{};
    std::size_t head_ = 0, count_ = 0, bytes_ = 0;
    std::atomic_bool closing_{false};
    std::atomic_flag draining_ = ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> pending_loss_{0}, rejected_{0}, failed_{0};
    std::atomic<std::uint64_t> delivery_failed_{0}, discarded_{0}, closed_{0};
};

} // namespace vfpc
