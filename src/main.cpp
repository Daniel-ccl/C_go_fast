#include "audio_engine.h"
#include "param_store.h"

#include <sys/mman.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#ifndef CGOFAST_PATCH_DIR
#define CGOFAST_PATCH_DIR "patches"
#endif

int main() {
    const bool memoria_bloqueada =
        mlockall(MCL_CURRENT | MCL_FUTURE) == 0;

    if (!memoria_bloqueada) {
        std::fprintf(
            stderr,
            "Advertencia: mlockall fallo: %s\n",
            std::strerror(errno)
        );
    }

    MotorAudio motor_audio;

    if (!motor_audio.iniciar("test.pd", CGOFAST_PATCH_DIR)) {
        if (memoria_bloqueada) {
            munlockall();
        }

        return 1;
    }

    std::puts("C_go_fast funcionando.");
    std::puts("f + Enter: alternar entre 440 Hz y 880 Hz");
    std::puts("q + Enter: salir");

    bool frecuencia_alta = false;
    int comando = 0;

    while ((comando = std::getchar()) != EOF) {
        if (comando == 'q' || comando == 'Q') {
            break;
        }

        if (comando == 'f' || comando == 'F') {
            frecuencia_alta = !frecuencia_alta;

            const float frecuencia =
                frecuencia_alta ? 880.0f : 440.0f;

            ParamStore::instance().set(
                ParamID::Frequency,
                frecuencia
            );

            std::printf("Frecuencia: %.0f Hz\n", frecuencia);
        }

        const int error = motor_audio.consumir_error_tiempo_real();

        if (error != 0) {
            std::fprintf(
                stderr,
                "Error en el callback de audio: %d\n",
                error
            );
        }
    }

    motor_audio.detener();

    if (memoria_bloqueada) {
        munlockall();
    }

    return 0;
}
