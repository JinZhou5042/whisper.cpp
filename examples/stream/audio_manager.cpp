#include "audio_manager.hpp"

#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <SDL2/SDL.h>
#include <fftw3.h>

AudioManager::AudioManager(int sample_rate, float archive_interval_s, float recognition_interval_s) {
    sample_rate_ = sample_rate;
    archive_interval_s_ = archive_interval_s;
    recognition_interval_s_ = recognition_interval_s;

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        exit(1);
    }

    SDL_AudioSpec audio_spec;
    SDL_zero(audio_spec);

    audio_spec.freq = sample_rate_;
    audio_spec.format = AUDIO_F32;
    audio_spec.channels = 1;
    audio_spec.samples = 1024;    // 64ms under 16kHz
    audio_spec.callback = AudioManager::captureAudio;
    audio_spec.userdata = this;

    device_id_ = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &audio_spec, nullptr, 0);
    if (!device_id_) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        exit(1);
    }

    wav_file_.open("audio.wav", std::ios::binary);
    if (!wav_file_.is_open()) {
        throw std::runtime_error("Failed to open WAV file for writing");
    }
    subtitles_file_.open("subtitles.txt", std::ios::trunc);
    if (!subtitles_file_.is_open()) {
        throw std::runtime_error("Failed to open subtitles file for writing");
    }
    writeHeader();
}

AudioManager::~AudioManager() {
    if (device_id_) {
        SDL_CloseAudioDevice(device_id_);
    }
    if (wav_file_.is_open()) {
        updateHeader();
        wav_file_.close();
    }
    if (subtitles_file_.is_open()) {
        subtitles_file_.close();
    }
}

bool AudioManager::start() {
    if (!device_id_) {
        std::cerr << "Audio device is not initialized!" << std::endl;
        exit(1);
    }
    SDL_PauseAudioDevice(device_id_, 0);
    capturing_ = true;
    return capturing_;
}

bool AudioManager::stop() {
    if (!capturing_) {
        return false;
    }

    SDL_PauseAudioDevice(device_id_, 1);
    capturing_ = false;
    return true;
}

void AudioManager::captureAudio(void* userdata, uint8_t* new_data, int new_data_bytes) {
    
    auto* am = static_cast<AudioManager*>(userdata);

    std::lock_guard<std::mutex> lock(am->mutex_);

    if (!am->capturing_) return;
    
    if (new_data_bytes % sizeof(float) != 0) {
        throw std::invalid_argument("new_data_bytes is not aligned with float size");
    }

    size_t new_data_len = new_data_bytes / sizeof(float);
    float* float_data = reinterpret_cast<float*>(new_data);

    am->audio_buffer.insert(am->audio_buffer.end(), float_data, float_data + new_data_len);

    am->context_pending_len_ += new_data_len;
}


void AudioManager::resetBuffer() {
    // if (context_pending_len_ < audio_buffer.size()) {
        // audio_buffer.resize(context_pending_len_);
        // std::vector<float>(audio_buffer).swap(audio_buffer);
    // }
    // clean the buffer
    audio_buffer.clear();
}

void AudioManager::wait(std::vector<float>& audio_context) {
    while (context_pending_len_ < sample_rate_ * recognition_interval_s_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    audio_context = audio_buffer;
}

void AudioManager::archiveAudio(std::vector<float>& audio_data) {
    for (const float sample : audio_data) {
        wav_file_.write(reinterpret_cast<const char*>(&sample), sizeof(float));
    }
    wav_data_chunk_size_ += audio_data.size() * sizeof(float);
    
    if (false) {
        wav_file_.flush();
        updateHeader();
    }
}

void AudioManager::cleanup() {
    if (device_id_ != 0) {
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }
    SDL_Quit();
}

bool AudioManager::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            cleanup();
            return false;
        }
    }
    return true;
}

void AudioManager::writeHeader() {
    // RIFF header
    wav_file_.write("RIFF", 4);
    uint32_t file_size_placeholder = 0;
    wav_file_.write(reinterpret_cast<const char*>(&file_size_placeholder), 4);
    wav_file_.write("WAVE", 4);

    // fmt subchunk
    wav_file_.write("fmt ", 4);
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 3;
    uint16_t num_channels = 1;
    uint32_t byte_rate = sample_rate_ * num_channels * sizeof(float);
    uint16_t block_align = num_channels * sizeof(float);
    uint16_t bits_per_sample = 32;

    wav_file_.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    wav_file_.write(reinterpret_cast<const char*>(&audio_format), 2);
    wav_file_.write(reinterpret_cast<const char*>(&num_channels), 2);
    wav_file_.write(reinterpret_cast<const char*>(&sample_rate_), 4);
    wav_file_.write(reinterpret_cast<const char*>(&byte_rate), 4);
    wav_file_.write(reinterpret_cast<const char*>(&block_align), 2);
    wav_file_.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

    // data subchunk
    wav_file_.write("data", 4);
    uint32_t subchunk2_size_placeholder = 0;
    wav_file_.write(reinterpret_cast<const char*>(&subchunk2_size_placeholder), 4);
}


void AudioManager::updateHeader() {
    if (!wav_file_.is_open()) return;

    uint32_t file_size = 36 + wav_data_chunk_size_;
    wav_file_.seekp(4, std::ios::beg);
    wav_file_.write(reinterpret_cast<const char*>(&file_size), 4);

    wav_file_.seekp(40, std::ios::beg);
    wav_file_.write(reinterpret_cast<const char*>(&wav_data_chunk_size_), 4);
}
