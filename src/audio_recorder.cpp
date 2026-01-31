#include "audio_recorder.h"
#include <algorithm>
#include <cmath>

AudioRecorder::AudioRecorder()
    : stream(nullptr),
      recording(false),
      currentSampleRate(44100),
      currentChannels(1) {
    Pa_Initialize();
}

AudioRecorder::~AudioRecorder() {
    stopRecording();
    Pa_Terminate();
}

bool AudioRecorder::startRecording(int sampleRate, int channels) {
    if (recording.load()) {
        return false;
    }
    
    stopRecording(); // Ensure previous recording is stopped
    
    currentSampleRate = sampleRate;
    currentChannels = channels;
    recordedData.clear();
    recording.store(true);
    
    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        recording.store(false);
        return false;
    }
    
    inputParameters.channelCount = channels;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;
    
    PaError err = Pa_OpenStream(
        &stream,
        &inputParameters,
        nullptr,
        sampleRate,
        paFramesPerBufferUnspecified,
        paClipOff,
        recordCallback,
        this
    );
    
    if (err != paNoError) {
        recording.store(false);
        return false;
    }
    
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        Pa_CloseStream(stream);
        stream = nullptr;
        recording.store(false);
        return false;
    }
    
    return true;
}

void AudioRecorder::stopRecording() {
    if (!recording.load()) {
        return;
    }
    
    recording.store(false);
    
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = nullptr;
    }
    
    // Call callback with recorded data
    std::lock_guard<std::mutex> lock(dataMutex);
    if (!recordedData.empty() && callback) {
        callback(recordedData, currentSampleRate, currentChannels);
    }
}

int AudioRecorder::recordCallback(const void *inputBuffer, void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo *timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData) {
    (void)outputBuffer;
    (void)timeInfo;
    (void)statusFlags;
    
    AudioRecorder *recorder = static_cast<AudioRecorder *>(userData);
    
    if (!recorder->recording.load()) {
        return paComplete;
    }
    
    const float *input = static_cast<const float *>(inputBuffer);
    unsigned long numSamples = framesPerBuffer * recorder->currentChannels;
    
    std::lock_guard<std::mutex> lock(recorder->dataMutex);
    recorder->recordedData.insert(
        recorder->recordedData.end(),
        input,
        input + numSamples
    );
    
    return paContinue;
}
