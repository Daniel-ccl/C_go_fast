#pragma once
#include <array>
#include <atomic>
#include <cstddef>

enum class ParamID {
    Frequency,
    Count
};

class ParamStore {
public:
    static ParamStore& instance() {
        static ParamStore store;
        return store;
    }

    void set(ParamID id, float value) {
        values_[static_cast<size_t>(id)].store(value, std::memory_order_relaxed);
    }

    float get(ParamID id) const {
        return values_[static_cast<size_t>(id)].load(std::memory_order_relaxed);
    }

private:
    ParamStore() {
        values_[static_cast<size_t>(ParamID::Frequency)].store(440.0f, std::memory_order_relaxed);
    }

    std::array<std::atomic<float>, static_cast<size_t>(ParamID::Count)> values_;
};
