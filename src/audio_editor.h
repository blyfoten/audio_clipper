#ifndef AUDIO_EDITOR_H
#define AUDIO_EDITOR_H

#include <string>
#include <vector>

struct LoadResult {
    bool success;
    std::vector<float> data;
    int sampleRate;
    int channels;
    std::string error;
};

class AudioEditor {
public:
    AudioEditor();
    ~AudioEditor();
    
    LoadResult loadAudioFile(const std::string &filename);
    bool saveWAV(const std::string &filename, const std::vector<float> &data, int sampleRate, int channels);
    bool saveMP3(const std::string &filename, const std::vector<float> &data, int sampleRate, int channels);
    
    std::vector<float> adjustVolume(const std::vector<float> &data, double volumeDb);
    std::vector<float> trimToRegion(const std::vector<float> &data, int sampleRate, int channels, double startTime, double endTime);
    std::vector<float> removeRegion(const std::vector<float> &data, int sampleRate, int channels, double startTime, double endTime);

private:
    std::string findFFmpeg() const;
    bool convertToWAV(const std::string &inputFile, const std::string &outputFile) const;
};

#endif // AUDIO_EDITOR_H
