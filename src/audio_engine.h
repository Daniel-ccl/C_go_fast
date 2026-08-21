#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <jack/jack.h>

class MotorAudio final {
public:
    MotorAudio() = default;
    ~MotorAudio();

    MotorAudio(const MotorAudio&) = delete;
    MotorAudio& operator=(const MotorAudio&) = delete;

    bool iniciar(const char* nombre_parche, const char* directorio_parches);
    void detener() noexcept;

    [[nodiscard]] bool esta_activo() const noexcept;
    int consumir_error_tiempo_real() noexcept;

private:
    static constexpr std::size_t BLOQUE_PD = 64;
    static constexpr std::size_t CAPACIDAD_BUFFER = 1U << 14U;
    static constexpr std::size_t MASCARA_BUFFER = CAPACIDAD_BUFFER - 1U;

    struct BufferCircular {
        std::array<float, CAPACIDAD_BUFFER> izquierdo{};
        std::array<float, CAPACIDAD_BUFFER> derecho{};

        std::size_t posicion_escritura = 0;
        std::size_t posicion_lectura = 0;
        std::size_t disponibles = 0;

        bool insertar(const float* audio_intercalado,
                      std::size_t cantidad) noexcept;

        bool extraer(float* salida_izquierda,
                     float* salida_derecha,
                     std::size_t cantidad) noexcept;

        void reiniciar() noexcept;
    };

    static int callback_jack(jack_nframes_t cuadros, void* contexto);
    int procesar(jack_nframes_t cuadros) noexcept;

    static void escribir_silencio(
        float* salida_izquierda,
        float* salida_derecha,
        std::size_t cantidad) noexcept;

    bool conectar_salidas_fisicas() noexcept;

    jack_client_t* cliente_ = nullptr;
    jack_port_t* puerto_izquierdo_ = nullptr;
    jack_port_t* puerto_derecho_ = nullptr;

    void* parche_ = nullptr;
    bool jack_activado_ = false;

    BufferCircular buffer_;
    std::array<float, BLOQUE_PD * 2> temporal_{};

    std::atomic<int> error_tiempo_real_{0};
    float ultima_frecuencia_ = -1.0f;
};
