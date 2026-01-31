#ifndef AUDIO_CLIPPER_H
#define AUDIO_CLIPPER_H

#include "imgui.h"
#include <vector>
#include <string>
#include <memory>
#include "audio_recorder.h"
#include "audio_player.h"
#include "audio_editor.h"

struct MarkerInfo {
    bool hasMarkers;
    double startTime;
    double endTime;
};

// Global DPI scale (set from main.cpp)
extern float g_ContentScale;

class AudioClipper {
public:
    AudioClipper();
    ~AudioClipper();

    void render();
    
    // UI actions
    void toggleRecording();
    void stopRecording();
    void loadAudioFile();
    void playAudio();
    void stopPlayback();
    void applyVolume();
    void removeSelectedRegion();
    void trimToSelection();
    void clearMarkers();

private:
    void renderWaveform();
    void renderControls();
    void renderVolumeSection();
    void renderSaveSection();
    
    // Helper to get scaled value
    float scaled(float value) const { return value * g_ContentScale; }
    
    // Audio components
    std::unique_ptr<AudioRecorder> recorder;
    std::unique_ptr<AudioPlayer> player;
    std::unique_ptr<AudioEditor> editor;
    
    // Audio data
    std::vector<float> audioData;
    std::vector<float> waveformSamples;
    double audioDuration;
    int sampleRate;
    int channels;
    
    // UI state
    bool isRecording;
    bool isPlaying;
    float volumeDb;
    std::string statusText;
    ImVec4 statusColor;
    
    // Waveform state
    int startMarkerX;
    int endMarkerX;
    double playbackPosition;
    bool draggingMarker;
    enum MarkerType { None, Start, End, Playback } draggingType;
    int waveformWidth;
    int waveformHeight;
    
    // File dialogs
    bool showLoadDialog;
    bool showSaveWavDialog;
    bool showSaveMp3Dialog;
    
    // Helper functions
    double pixelToTime(int x) const;
    int timeToPixel(double time) const;
    bool isNearMarker(int x, int markerX) const;
    MarkerInfo getMarkers() const;
};

#endif // AUDIO_CLIPPER_H
