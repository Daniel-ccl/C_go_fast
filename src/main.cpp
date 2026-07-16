#include <jack/jack.h>
#include <z_libpd.h>
#include <sys/mman.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include "param_store.h"

constexpr size_t PD_BLOCK = 64;
constexpr size_t RING_CAPACITY = 1 << 14;

struct RingBuffer {
    std::array<float, RING_CAPACITY> left{};
    std::array<float, RING_CAPACITY> right{};
    size_t write_pos = 0;
    size_t read_pos = 0;
    size_t available = 0;

    void push_block(const float *interleaved, size_t n) {
        for (size_t i = 0; i < n; i++) {
            left[write_pos] = interleaved[i * 2];
            right[write_pos] = interleaved[i * 2 + 1];
            write_pos = (write_pos + 1) % RING_CAPACITY;
        }
        available += n;
    }

    void pop(float *out_left, float *out_right, size_t n) {
        for (size_t i = 0; i < n; i++) {
            out_left[i] = left[read_pos];
            out_right[i] = right[read_pos];
            read_pos = (read_pos + 1) % RING_CAPACITY;
        }
        available -= n;
    }
};

static jack_port_t *out_left = nullptr;
static jack_port_t *out_right = nullptr;
static RingBuffer ring;

static int process(jack_nframes_t nframes, void *arg) {
    static float scratch[PD_BLOCK * 2];
    static float last_freq = 440.0f;

    float current_freq = ParamStore::instance().get(ParamID::Frequency);
    if (current_freq != last_freq) {
        libpd_float("freq", current_freq);
        last_freq = current_freq;
    }

    while (ring.available < nframes) {
        int err = libpd_process_float(1, nullptr, scratch);
        if (err != 0) {
            fprintf(stderr, "libpd_process_float failed: %d\n", err);
            return 0;
        }
        ring.push_block(scratch, PD_BLOCK);
    }

    float *left_buf = (float *)jack_port_get_buffer(out_left, nframes);
    float *right_buf = (float *)jack_port_get_buffer(out_right, nframes);
    ring.pop(left_buf, right_buf, nframes);
    return 0;
}

int main() {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        fprintf(stderr, "mlockall failed: %s (continuing without memory locking)\n", strerror(errno));
    }

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

    std::thread test_thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        ParamStore::instance().set(ParamID::Frequency, 880.0f);
    });
    test_thread.detach();

    printf("running - press enter to quit\n");
    getchar();

    jack_client_close(client);
    return 0;
}
