#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <portaudio.h>

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    
    bool play(const std::vector<float> &data, int sampleRate, int channels, double startPosition = 0.0);
    void pause();
    void stop();
    bool isPlaying() const { return playing.load() && !paused.load(); }
    double getCurrentPosition() const;
    
    void setPositionCallback(std::function<void(double)> callback) {
        this->positionCallback = callback;
    }
    
    void setFinishedCallback(std::function<void()> callback) {
        this->finishedCallback = callback;
    }

private:
    static int playCallback(const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);
    
    void updatePosition();
    
    PaStream *stream;
    std::atomic<bool> playing;
    std::atomic<bool> paused;
    std::vector<float> audioData;
    std::atomic<size_t> currentFrame;
    std::mutex dataMutex;
    int currentSampleRate;
    int currentChannels;
    std::thread positionUpdateThread;
    std::function<void(double)> positionCallback;
    std::function<void()> finishedCallback;
};

#endif // AUDIO_PLAYER_H
