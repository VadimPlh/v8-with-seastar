#pragma once

#include <atomic>

#include "seastar/core/future.hh"

class semaphore {
public:
    semaphore(int64_t max_thread_count_)
    : free_slots(max_thread_count_) {}

    seastar::future<> lock() {
        return seastar::do_until([this] {
                auto local_free_slots = free_slots.load();
                if (local_free_slots > 0) {
                    return free_slots.compare_exchange_strong(local_free_slots, local_free_slots - 1);
                }
                return false;
            },
            []{ return seastar::make_ready_future<>();}
        );
    }

    seastar::future<> unlock() {
        free_slots.fetch_add(1);
        return seastar::make_ready_future<>();
    }

private:
    std::atomic<int64_t> free_slots;
};