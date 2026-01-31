#include "audio_player.h"
#include <algorithm>
#include <chrono>
#include <thread>

AudioPlayer::AudioPlayer()
    : stream(nullptr),
      playing(false),
      paused(false),
      currentFrame(0),
      currentSampleRate(44100),
      currentChannels(1) {
    Pa_Initialize();
}

AudioPlayer::~AudioPlayer() {
    stop();
    Pa_Terminate();
}

bool AudioPlayer::play(const std::vector<float> &data, int sampleRate, int channels, double startPosition) {
    stop(); // Stop any current playback
    
    if (data.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(dataMutex);
    audioData = data;
    currentSampleRate = sampleRate;
    currentChannels = channels;
    
    size_t startFrame = static_cast<size_t>(startPosition * sampleRate);
    currentFrame.store(std::min(startFrame, audioData.size() / channels));
    
    playing.store(true);
    paused.store(false);
    
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        playing.store(false);
        return false;
    }
    
    outputParameters.channelCount = channels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;
    
    PaError err = Pa_OpenStream(
        &stream,
        nullptr,
        &outputParameters,
        sampleRate,
        paFramesPerBufferUnspecified,
        paClipOff,
        playCallback,
        this
    );
    
    if (err != paNoError) {
        playing.store(false);
        return false;
    }
    
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        Pa_CloseStream(stream);
        stream = nullptr;
        playing.store(false);
        return false;
    }
    
    // Start position update thread
    positionUpdateThread = std::thread(&AudioPlayer::updatePosition, this);
    
    return true;
}

void AudioPlayer::pause() {
    if (!playing.load()) {
        return;
    }
    
    paused.store(true);
    if (stream) {
        Pa_StopStream(stream);
    }
}

void AudioPlayer::stop() {
    playing.store(false);
    paused.store(false);
    
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = nullptr;
    }
    
    if (positionUpdateThread.joinable()) {
        positionUpdateThread.join();
    }
    
    currentFrame.store(0);
    if (finishedCallback) {
        finishedCallback();
    }
}

double AudioPlayer::getCurrentPosition() const {
    if (currentSampleRate <= 0) {
        return 0.0;
    }
    return static_cast<double>(currentFrame.load()) / currentSampleRate;
}

int AudioPlayer::playCallback(const void *inputBuffer, void *outputBuffer,
                              unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo *timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void *userData) {
    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;
    
    AudioPlayer *player = static_cast<AudioPlayer *>(userData);
    
    if (!player->playing.load() || player->paused.load()) {
        return paComplete;
    }
    
    float *output = static_cast<float *>(outputBuffer);
    size_t framesToWrite = framesPerBuffer;
    size_t currentFrame = player->currentFrame.load();
    size_t totalFrames = player->audioData.size() / player->currentChannels;
    
    std::lock_guard<std::mutex> lock(player->dataMutex);
    
    size_t framesAvailable = totalFrames - currentFrame;
    framesToWrite = std::min(framesToWrite, framesAvailable);
    
    if (framesToWrite == 0) {
        player->playing.store(false);
        return paComplete;
    }
    
    size_t samplesToWrite = framesToWrite * player->currentChannels;
    size_t startSample = currentFrame * player->currentChannels;
    
    std::copy(
        player->audioData.begin() + startSample,
        player->audioData.begin() + startSample + samplesToWrite,
        output
    );
    
    // Zero out remaining buffer if needed
    if (framesToWrite < framesPerBuffer) {
        std::fill(
            output + samplesToWrite,
            output + (framesPerBuffer * player->currentChannels),
            0.0f
        );
    }
    
    player->currentFrame.store(currentFrame + framesToWrite);
    
    return paContinue;
}

void AudioPlayer::updatePosition() {
    while (playing.load()) {
        if (!paused.load()) {
            double position = getCurrentPosition();
            if (positionCallback) {
                positionCallback(position);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
