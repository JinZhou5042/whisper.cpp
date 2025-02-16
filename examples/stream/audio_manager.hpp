#ifndef AUDIOMANAGER_HPP
#define AUDIOMANAGER_HPP

#include <vector>
#include <string>
#include <mutex>
#include <fstream>
#include <SDL2/SDL.h>


class AudioManager {
public:
    AudioManager(int sample_rate, float archive_interval_s, float recognition_interval_s);
    ~AudioManager();

    bool start();
    bool stop();
    void cleanup();

    void resetBuffer();

    void readContext(std::vector<float>& audio_data);

    static void captureAudio(void* userdata, uint8_t* new_data, int new_data_bytes);

    std::vector<float> getArchiveBuffer();

    void archiveAudio(std::vector<float>& audio_data);

    bool pollEvents();

    void wait(std::vector<float>& audio_context);

    std::vector<float> audio_buffer;

private:
    void writeHeader();
    void updateHeader();

    size_t context_pending_len_ = 0;

    int sample_rate_;
    float recognition_interval_s_;

    std::mutex mutex_;
    
    std::vector<float> context_buffer_;
    float context_duration_s_;
    size_t context_buffer_write_pos_;

    std::vector<float> archive_buffer_;
    float archive_interval_s_;
    size_t archive_buffer_write_pos_;

    int device_id_;
    bool capturing_;

    std::ofstream wav_file_;
    std::ofstream subtitles_file_;
    uint32_t wav_data_chunk_size_;
};

#endif // AUDIOMANAGER_HPP
