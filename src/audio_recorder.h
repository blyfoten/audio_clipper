#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <portaudio.h>

class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();
    
    bool startRecording(int sampleRate, int channels);
    void stopRecording();
    bool isRecording() const { return recording.load(); }
    
    void setCallback(std::function<void(const std::vector<float>&, int, int)> callback) {
        this->callback = callback;
    }

private:
    static int recordCallback(const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo *timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData);
    
    PaStream *stream;
    std::atomic<bool> recording;
    std::vector<float> recordedData;
    std::mutex dataMutex;
    int currentSampleRate;
    int currentChannels;
    std::function<void(const std::vector<float>&, int, int)> callback;
};

#endif // AUDIO_RECORDER_H
