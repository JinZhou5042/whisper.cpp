// Real-time speech recognition of input from a microphone
//
// A very quick-n-dirty implementation serving mainly as a proof of concept.
//
// #include "common-sdl.h"
// #include "common.h"
#include "whisper.h"
#include "params.cpp"
#include "audio_manager.hpp"
#include "translator.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <fstream>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <cmath>
#include <fftw3.h>
#include <ctime>
#include <regex>
#include <csignal>

#include <SDL2/SDL.h>


static double getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    double timestamp = std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
    return timestamp;
}

static std::string removeParens(const std::string& input) {
    std::string result;
    std::string current_token;
    bool discardRound = false;
    bool discardSquare = false;

    for (char ch : input) {
        if (ch == '(') {
            discardRound = true;
        } else if (ch == ')') {
            discardRound = false;
        } else if (ch == '[') {
            discardSquare = true;
        } else if (ch == ']') {
            discardSquare = false;
        } else if (!discardRound && !discardSquare) {
            if (ch == ' ') {
                if (!current_token.empty()) {
                    result += current_token + " ";
                    current_token.clear();
                }
            } else {
                current_token += ch;
            }
        }
    }

    if (!current_token.empty()) {
        result += current_token;
    }

    return result;
}

AudioManager* g_audioManager = nullptr;


[[noreturn]] static void handle_sigstp([[maybe_unused]] int signal) {
    if (g_audioManager != nullptr) {
        g_audioManager->cleanup(); 
    }
    exit(0);
}

int main(int argc, char ** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);

    Params params;
    if (!params.parse(argc, argv)) {
        return 1;
    }

    struct whisper_context_params cparams = whisper_context_default_params();

    cparams.use_gpu    = params.use_gpu;
    cparams.flash_attn = params.flash_attn;

    struct whisper_context * ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.n_threads        = 1;            // 1 thread
    wparams.audio_ctx        = 0;            // recognize immediately
    wparams.max_tokens       = 0;            // maximum number of tokens to generate
    wparams.language         = "en";         // only English
    wparams.print_progress   = false;        // no progress bar
    wparams.print_special    = false;        // no special tokens
    wparams.print_realtime   = false;        // we print the output ourselves
    wparams.no_timestamps    = true;         // no timestamps in the output
    wparams.single_segment   = true;         // best for real-time
    wparams.tdrz_enable      = false;        // disable TDRZ

    wparams.temperature = 0.0f;              // prefer deterministic

    printf("[Start speaking]\n");

    int sample_rate = 16000;

    AudioManager audio_manager(sample_rate, params.archive_interval_s, params.recognition_interval_s);
    g_audioManager = &audio_manager;
    signal(SIGTSTP, handle_sigstp);

    std::string translated_text;

    audio_manager.start();

    double time_start = getCurrentTimestamp();

    std::ofstream temp_file;
    temp_file.open("temp.txt", std::ios::trunc);
    // first remove the file if it exists

    while (audio_manager.pollEvents()) {
        std::vector<float> audio_context;

        audio_manager.wait(audio_context);

        if (!audio_context.empty()) {
            if (whisper_full(ctx, wparams, audio_context.data(), audio_context.size()) != 0) {
                std::cerr << "Failed to recognize audio\n";
                continue;
            }

            std::string audio_text = "";
            const int n_segments = whisper_full_n_segments(ctx);
            for (int i = 0; i < n_segments; ++i) {
                audio_text += whisper_full_get_segment_text(ctx, i);
            }

            // std::string clean_text = removeParens(audio_text);
            std::cout << "\033[H\033[J" << std::flush;
            std::cout << audio_text << std::flush;

            /* TODO: currently the text is flashing out of no reason
            translate_text(params.translate, clean_text, translated_text);
            std::cout << translated_text << std::flush;
            */

            double time_now = getCurrentTimestamp();
            char last_char = audio_text.back();
            if ((time_now - time_start > 15.0) && (last_char == '.' || last_char == '?' || last_char == '!')) {
                temp_file << audio_text << "\n" << std::flush;

                translate_text(params.translate, audio_text, translated_text);

                temp_file << translated_text << "\n\n" << std::flush;
                audio_manager.resetBuffer();
                time_start = time_now;
            }   
        }
    }
    audio_manager.stop();

    whisper_print_timings(ctx);
    whisper_free(ctx);


    return 0;
}
