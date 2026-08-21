#pragma once

#include <array>
#include <atomic>
#include <cstddef>

enum class ParamID : std::size_t {
    Frequency,
    Count
};

class ParamStore final {
public:
    static ParamStore& instance() noexcept {
        static ParamStore almacen;
        return almacen;
    }

    ParamStore(const ParamStore&) = delete;
    ParamStore& operator=(const ParamStore&) = delete;

    void set(const ParamID id, const float valor) noexcept {
        valores_[indice(id)].store(
            valor,
            std::memory_order_relaxed
        );
    }

    [[nodiscard]] float get(const ParamID id) const noexcept {
        return valores_[indice(id)].load(
            std::memory_order_relaxed
        );
    }

private:
    static constexpr std::size_t CANTIDAD_PARAMETROS =
        static_cast<std::size_t>(ParamID::Count);

    static constexpr std::size_t indice(const ParamID id) noexcept {
        return static_cast<std::size_t>(id);
    }

    ParamStore() noexcept {
        for (auto& valor : valores_) {
            valor.store(0.0f, std::memory_order_relaxed);
        }

        valores_[indice(ParamID::Frequency)].store(
            440.0f,
            std::memory_order_relaxed
        );
    }

    std::array<std::atomic<float>, CANTIDAD_PARAMETROS> valores_{};
};
