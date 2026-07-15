#include <jack/jack.h>
#include <z_libpd.h>
#include <cstdio>
#include <vector>

static jack_port_t *out_left = nullptr;
static jack_port_t *out_right = nullptr;

static int process(jack_nframes_t nframes, void *arg) {
    if (nframes % 64 != 0) return 0;

    static std::vector<float> interleaved;
    interleaved.resize(nframes * 2);

    libpd_process_float(nframes / 64, nullptr, interleaved.data());

    float *left  = (float *)jack_port_get_buffer(out_left, nframes);
    float *right = (float *)jack_port_get_buffer(out_right, nframes);

    for (jack_nframes_t i = 0; i < nframes; i++) {
        left[i]  = interleaved[i * 2];
        right[i] = interleaved[i * 2 + 1];
    }
    return 0;
}

int main() {
    jack_client_t *client = jack_client_open("cgofast_poc", JackNullOption, nullptr);
    if (!client) { fprintf(stderr, "could not connect to jack server\n"); return 1; }

    int sample_rate = jack_get_sample_rate(client);

    libpd_init();
    libpd_init_audio(0, 2, sample_rate);

    void *patch = libpd_openfile("test.pd", "patches");
    if (!patch) { fprintf(stderr, "could not open patch\n"); return 1; }

    libpd_start_message(1);
    libpd_add_float(1);
    libpd_finish_message("pd", "dsp");

    out_left  = jack_port_register(client, "out_left",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out_right = jack_port_register(client, "out_right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    jack_set_process_callback(client, process, nullptr);
    if (jack_activate(client)) { fprintf(stderr, "could not activate jack client\n"); return 1; }

    const char **ports = jack_get_ports(client, nullptr, nullptr, JackPortIsPhysical | JackPortIsInput);
    if (ports) {
        jack_connect(client, jack_port_name(out_left), ports[0]);
        if (ports[1]) jack_connect(client, jack_port_name(out_right), ports[1]);
        jack_free(ports);
    }

    printf("running - press enter to quit\n");
    getchar();

    jack_client_close(client);
    return 0;
}
