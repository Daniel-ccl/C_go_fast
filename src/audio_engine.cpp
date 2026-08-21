#include "audio_engine.h"
#include "param_store.h"

#include <z_libpd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>

bool MotorAudio::BufferCircular::insertar(
    const float* audio_intercalado,
    const std::size_t cantidad) noexcept {

    if (cantidad > CAPACIDAD_BUFFER - disponibles) {
        return false;
    }

    for (std::size_t i = 0; i < cantidad; ++i) {
        izquierdo[posicion_escritura] = audio_intercalado[i * 2];
        derecho[posicion_escritura] = audio_intercalado[i * 2 + 1];

        posicion_escritura =
            (posicion_escritura + 1U) & MASCARA_BUFFER;
    }

    disponibles += cantidad;
    return true;
}

bool MotorAudio::BufferCircular::extraer(
    float* salida_izquierda,
    float* salida_derecha,
    const std::size_t cantidad) noexcept {

    if (cantidad > disponibles) {
        return false;
    }

    for (std::size_t i = 0; i < cantidad; ++i) {
        salida_izquierda[i] = izquierdo[posicion_lectura];
        salida_derecha[i] = derecho[posicion_lectura];

        posicion_lectura =
            (posicion_lectura + 1U) & MASCARA_BUFFER;
    }

    disponibles -= cantidad;
    return true;
}

void MotorAudio::BufferCircular::reiniciar() noexcept {
    posicion_escritura = 0;
    posicion_lectura = 0;
    disponibles = 0;
}

MotorAudio::~MotorAudio() {
    detener();
}

bool MotorAudio::iniciar(
    const char* nombre_parche,
    const char* directorio_parches) {

    if (cliente_ != nullptr) {
        return true;
    }

    error_tiempo_real_.store(0, std::memory_order_relaxed);
    ultima_frecuencia_ = -1.0f;
    buffer_.reiniciar();

    cliente_ = jack_client_open(
        "cgofast",
        JackNullOption,
        nullptr
    );

    if (cliente_ == nullptr) {
        std::fprintf(stderr, "No se pudo conectar con JACK.\n");
        return false;
    }

    const int frecuencia_muestreo =
        static_cast<int>(jack_get_sample_rate(cliente_));

    if (libpd_init() != 0) {
        std::fprintf(stderr, "No se pudo inicializar libpd.\n");
        detener();
        return false;
    }

    if (libpd_init_audio(0, 2, frecuencia_muestreo) != 0) {
        std::fprintf(stderr, "No se pudo configurar el audio de libpd.\n");
        detener();
        return false;
    }

    parche_ = libpd_openfile(nombre_parche, directorio_parches);

    if (parche_ == nullptr) {
        std::fprintf(
            stderr,
            "No se pudo abrir %s/%s.\n",
            directorio_parches,
            nombre_parche
        );

        detener();
        return false;
    }

    libpd_start_message(1);
    libpd_add_float(1.0f);
    libpd_finish_message("pd", "dsp");

    puerto_izquierdo_ = jack_port_register(
        cliente_,
        "salida_izquierda",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0
    );

    puerto_derecho_ = jack_port_register(
        cliente_,
        "salida_derecha",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0
    );

    if (puerto_izquierdo_ == nullptr ||
        puerto_derecho_ == nullptr) {

        std::fprintf(stderr, "No se pudieron crear los puertos JACK.\n");
        detener();
        return false;
    }

    if (jack_set_process_callback(
            cliente_,
            callback_jack,
            this) != 0) {

        std::fprintf(stderr, "No se pudo registrar el callback JACK.\n");
        detener();
        return false;
    }

    if (jack_activate(cliente_) != 0) {
        std::fprintf(stderr, "No se pudo activar el cliente JACK.\n");
        detener();
        return false;
    }

    jack_activado_ = true;

    if (!conectar_salidas_fisicas()) {
        std::fprintf(
            stderr,
            "Advertencia: el audio funciona, pero no se conectaron "
            "automáticamente las salidas físicas.\n"
        );
    }

    return true;
}

void MotorAudio::detener() noexcept {
    if (cliente_ != nullptr && jack_activado_) {
        jack_deactivate(cliente_);
        jack_activado_ = false;
    }

    if (parche_ != nullptr) {
        libpd_closefile(parche_);
        parche_ = nullptr;
    }

    if (cliente_ != nullptr) {
        jack_client_close(cliente_);
        cliente_ = nullptr;
    }

    puerto_izquierdo_ = nullptr;
    puerto_derecho_ = nullptr;

    buffer_.reiniciar();
}

bool MotorAudio::esta_activo() const noexcept {
    return cliente_ != nullptr && jack_activado_;
}

int MotorAudio::consumir_error_tiempo_real() noexcept {
    return error_tiempo_real_.exchange(
        0,
        std::memory_order_relaxed
    );
}

int MotorAudio::callback_jack(
    const jack_nframes_t cuadros,
    void* contexto) {

    auto* motor = static_cast<MotorAudio*>(contexto);
    return motor->procesar(cuadros);
}

int MotorAudio::procesar(const jack_nframes_t cuadros) noexcept {
    auto* salida_izquierda = static_cast<float*>(
        jack_port_get_buffer(puerto_izquierdo_, cuadros)
    );

    auto* salida_derecha = static_cast<float*>(
        jack_port_get_buffer(puerto_derecho_, cuadros)
    );

    const std::size_t cantidad =
        static_cast<std::size_t>(cuadros);

    if (cantidad > CAPACIDAD_BUFFER) {
        escribir_silencio(
            salida_izquierda,
            salida_derecha,
            cantidad
        );

        error_tiempo_real_.store(2, std::memory_order_relaxed);
        buffer_.reiniciar();
        return 0;
    }

    const float frecuencia_actual =
        ParamStore::instance().get(ParamID::Frequency);

    if (frecuencia_actual != ultima_frecuencia_) {
        libpd_float("freq", frecuencia_actual);
        ultima_frecuencia_ = frecuencia_actual;
    }

    while (buffer_.disponibles < cantidad) {
        const int resultado = libpd_process_float(
            1,
            nullptr,
            temporal_.data()
        );

        if (resultado != 0) {
            escribir_silencio(
                salida_izquierda,
                salida_derecha,
                cantidad
            );

            error_tiempo_real_.store(1, std::memory_order_relaxed);
            buffer_.reiniciar();
            return 0;
        }

        if (!buffer_.insertar(temporal_.data(), BLOQUE_PD)) {
            escribir_silencio(
                salida_izquierda,
                salida_derecha,
                cantidad
            );

            error_tiempo_real_.store(2, std::memory_order_relaxed);
            buffer_.reiniciar();
            return 0;
        }
    }

    if (!buffer_.extraer(
            salida_izquierda,
            salida_derecha,
            cantidad)) {

        escribir_silencio(
            salida_izquierda,
            salida_derecha,
            cantidad
        );

        error_tiempo_real_.store(3, std::memory_order_relaxed);
        buffer_.reiniciar();
    }

    return 0;
}

void MotorAudio::escribir_silencio(
    float* salida_izquierda,
    float* salida_derecha,
    const std::size_t cantidad) noexcept {

    std::fill_n(salida_izquierda, cantidad, 0.0f);
    std::fill_n(salida_derecha, cantidad, 0.0f);
}

bool MotorAudio::conectar_salidas_fisicas() noexcept {
    const char** puertos = jack_get_ports(
        cliente_,
        nullptr,
        nullptr,
        JackPortIsPhysical | JackPortIsInput
    );

    if (puertos == nullptr || puertos[0] == nullptr) {
        return false;
    }

    bool conectado = true;

    const int resultado_izquierdo = jack_connect(
        cliente_,
        jack_port_name(puerto_izquierdo_),
        puertos[0]
    );

    if (resultado_izquierdo != 0 &&
        resultado_izquierdo != EEXIST) {

        conectado = false;
    }

    if (puertos[1] != nullptr) {
        const int resultado_derecho = jack_connect(
            cliente_,
            jack_port_name(puerto_derecho_),
            puertos[1]
        );

        if (resultado_derecho != 0 &&
            resultado_derecho != EEXIST) {

            conectado = false;
        }
    } else {
        conectado = false;
    }

    jack_free(puertos);
    return conectado;
}
