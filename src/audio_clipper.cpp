#include "audio_clipper.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

// Reference to global scale (defined in main.cpp)
extern float g_ContentScale;

AudioClipper::AudioClipper()
    : audioDuration(0.0),
      sampleRate(44100),
      channels(1),
      isRecording(false),
      isPlaying(false),
      volumeDb(0.0f),
      statusText("Ready to record"),
      statusColor(ImVec4(0.58f, 0.65f, 0.66f, 1.0f)),
      startMarkerX(-1),
      endMarkerX(-1),
      playbackPosition(0.0),
      draggingMarker(false),
      draggingType(None),
      waveformWidth(960),
      waveformHeight(150),
      showLoadDialog(false),
      showSaveWavDialog(false),
      showSaveMp3Dialog(false) {
    
    recorder = std::make_unique<AudioRecorder>();
    player = std::make_unique<AudioPlayer>();
    editor = std::make_unique<AudioEditor>();
    
    // Connect recorder callback
    recorder->setCallback([this](const std::vector<float> &data, int rate, int ch) {
        audioData = data;
        sampleRate = rate;
        channels = ch;
        audioDuration = static_cast<double>(data.size()) / (rate * ch);
        
        // Downsample for waveform display
        const int maxSamples = 2000;
        int downsampleFactor = std::max(1, static_cast<int>(data.size()) / maxSamples);
        waveformSamples.clear();
        waveformSamples.reserve(data.size() / downsampleFactor);
        for (size_t i = 0; i < data.size(); i += downsampleFactor) {
            waveformSamples.push_back(data[i]);
        }
        
        playbackPosition = 0.0;
        startMarkerX = -1;
        endMarkerX = -1;
        
        statusText = "Recording complete (" + std::to_string(audioDuration).substr(0, 4) + "s)";
        statusColor = ImVec4(0.15f, 0.68f, 0.38f, 1.0f);
        isRecording = false;
    });
    
    // Connect player callback
    player->setPositionCallback([this](double position) {
        playbackPosition = position;
    });
    
    player->setFinishedCallback([this]() {
        isPlaying = false;
        playbackPosition = 0.0;
    });
}

AudioClipper::~AudioClipper() = default;

void AudioClipper::render() {
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Audio Clipper", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoScrollWithMouse);
    
    // Title with proper padding
    ImGui::Spacing();
    ImGui::SetWindowFontScale(1.8f);
    ImGui::Text("Audio Clipper");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Waveform section - calculate available height
    float availableHeight = io.DisplaySize.y - scaled(280); // Reserve space for controls
    waveformHeight = static_cast<int>(std::max(availableHeight * 0.5f, scaled(150)));
    
    renderWaveform();
    ImGui::Spacing();
    
    // Controls section
    renderControls();
    ImGui::Spacing();
    
    // Volume and Save section
    ImGui::BeginGroup();
    renderVolumeSection();
    ImGui::SameLine(io.DisplaySize.x / 2);
    renderSaveSection();
    ImGui::EndGroup();
    ImGui::Spacing();
    
    // Status
    ImGui::TextColored(statusColor, "%s", statusText.c_str());
    
    ImGui::End();
    
    // Handle file dialogs
    loadAudioFile();
    
    // Handle save dialogs
    static char savePath[512] = "";
    if (showSaveWavDialog) {
        ImGui::OpenPopup("Save WAV File");
        showSaveWavDialog = false;
        savePath[0] = '\0';
    }
    if (showSaveMp3Dialog) {
        ImGui::OpenPopup("Save MP3 File");
        showSaveMp3Dialog = false;
        savePath[0] = '\0';
    }
    
    if (ImGui::BeginPopupModal("Save WAV File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("File Path", savePath, sizeof(savePath));
        if (ImGui::Button("Save", ImVec2(scaled(120), 0))) {
            if (strlen(savePath) > 0) {
                std::string path = savePath;
                if (path.find(".wav") == std::string::npos) {
                    path += ".wav";
                }
                bool success = editor->saveWAV(path, audioData, sampleRate, channels);
                if (success) {
                    statusText = "Saved: " + path;
                    statusColor = ImVec4(0.15f, 0.68f, 0.38f, 1.0f);
                } else {
                    statusText = "Error saving file";
                    statusColor = ImVec4(0.91f, 0.30f, 0.24f, 1.0f);
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(scaled(120), 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    
    if (ImGui::BeginPopupModal("Save MP3 File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("File Path", savePath, sizeof(savePath));
        if (ImGui::Button("Save", ImVec2(scaled(120), 0))) {
            if (strlen(savePath) > 0) {
                std::string path = savePath;
                if (path.find(".mp3") == std::string::npos) {
                    path += ".mp3";
                }
                bool success = editor->saveMP3(path, audioData, sampleRate, channels);
                if (success) {
                    statusText = "Saved: " + path;
                    statusColor = ImVec4(0.15f, 0.68f, 0.38f, 1.0f);
                } else {
                    statusText = "Error saving file (FFmpeg may not be installed)";
                    statusColor = ImVec4(0.91f, 0.30f, 0.24f, 1.0f);
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(scaled(120), 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AudioClipper::renderWaveform() {
    // Calculate child window size - use full width minus padding
    float childHeight = static_cast<float>(waveformHeight) + scaled(70);
    
    ImGui::BeginChild("Waveform", ImVec2(-1, childHeight), true, 
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    ImGui::Text("Waveform - Click and drag markers to set trim points");
    ImGui::Spacing();
    
    // Get available space for waveform
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y = static_cast<float>(waveformHeight);
    waveformWidth = static_cast<int>(canvasSize.x);
    
    // Draw waveform background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
    ImU32 waveformColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.20f, 0.60f, 0.86f, 1.0f));
    
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), bgColor);
    
    // Draw center line
    float centerY = canvasPos.y + canvasSize.y / 2;
    drawList->AddLine(ImVec2(canvasPos.x, centerY), 
                     ImVec2(canvasPos.x + canvasSize.x, centerY), lineColor);
    
    // Draw waveform
    if (!waveformSamples.empty() && audioDuration > 0) {
        // Find max amplitude
        float maxAmplitude = 0.0f;
        for (float sample : waveformSamples) {
            maxAmplitude = std::max(maxAmplitude, std::abs(sample));
        }
        if (maxAmplitude == 0.0f) maxAmplitude = 1.0f;
        
        float xScale = canvasSize.x / waveformSamples.size();
        float yScale = (canvasSize.y - scaled(20)) / (2.0f * maxAmplitude);
        
        for (size_t i = 0; i < waveformSamples.size() - 1; ++i) {
            float x1 = canvasPos.x + i * xScale;
            float y1 = centerY - (waveformSamples[i] * yScale);
            float x2 = canvasPos.x + (i + 1) * xScale;
            float y2 = centerY - (waveformSamples[i + 1] * yScale);
            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), waveformColor, scaled(1.5f));
        }
        
        // Draw time labels
        ImU32 textColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.58f, 0.65f, 0.66f, 1.0f));
        drawList->AddText(ImVec2(canvasPos.x + scaled(5), canvasPos.y + canvasSize.y - scaled(15)), textColor, "0.0s");
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << audioDuration << "s";
        drawList->AddText(ImVec2(canvasPos.x + canvasSize.x - scaled(50), canvasPos.y + canvasSize.y - scaled(15)), textColor, oss.str().c_str());
    } else {
        const char* msg = "No audio loaded. Record or load audio to see waveform.";
        ImVec2 textSize = ImGui::CalcTextSize(msg);
        drawList->AddText(ImVec2(canvasPos.x + (canvasSize.x - textSize.x) / 2, centerY - textSize.y / 2),
                         ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), msg);
    }
    
    // Draw selection region
    if (startMarkerX >= 0 && endMarkerX >= 0) {
        int startX = std::min(startMarkerX, endMarkerX);
        int endX = std::max(startMarkerX, endMarkerX);
        ImU32 selectionColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.20f, 0.60f, 0.86f, 0.3f));
        drawList->AddRectFilled(
            ImVec2(canvasPos.x + startX, canvasPos.y),
            ImVec2(canvasPos.x + endX, canvasPos.y + canvasSize.y),
            selectionColor
        );
    }
    
    // Draw markers
    float markerWidth = scaled(3.0f);
    if (startMarkerX >= 0) {
        float x = canvasPos.x + startMarkerX;
        ImU32 markerColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.91f, 0.30f, 0.24f, 1.0f));
        drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), markerColor, markerWidth);
        drawList->AddText(ImVec2(x - scaled(20), canvasPos.y + scaled(5)), markerColor, "Start");
        if (audioDuration > 0) {
            double time = pixelToTime(startMarkerX);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << time << "s";
            drawList->AddText(ImVec2(x - scaled(20), canvasPos.y + canvasSize.y - scaled(15)), markerColor, oss.str().c_str());
        }
    }
    
    if (endMarkerX >= 0) {
        float x = canvasPos.x + endMarkerX;
        ImU32 markerColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.68f, 0.38f, 1.0f));
        drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), markerColor, markerWidth);
        drawList->AddText(ImVec2(x - scaled(15), canvasPos.y + scaled(5)), markerColor, "End");
        if (audioDuration > 0) {
            double time = pixelToTime(endMarkerX);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << time << "s";
            drawList->AddText(ImVec2(x - scaled(20), canvasPos.y + canvasSize.y - scaled(15)), markerColor, oss.str().c_str());
        }
    }
    
    // Draw playback cursor
    if (audioDuration > 0) {
        int cursorX = timeToPixel(playbackPosition);
        float x = canvasPos.x + cursorX;
        ImU32 cursorColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.61f, 0.07f, 1.0f));
        drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), cursorColor, markerWidth);
        
        // Draw triangle
        float triSize = scaled(7);
        ImVec2 p1(x - triSize, canvasPos.y);
        ImVec2 p2(x + triSize, canvasPos.y);
        ImVec2 p3(x, canvasPos.y + triSize * 1.5f);
        drawList->AddTriangleFilled(p1, p2, p3, cursorColor);
        
        // Draw circle at center
        drawList->AddCircleFilled(ImVec2(x, centerY), scaled(5.0f), cursorColor);
    }
    
    // Handle mouse interaction - use invisible button to capture mouse
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("waveform_canvas", canvasSize);
    
    if (ImGui::IsItemHovered() || draggingMarker) {
        ImVec2 mousePos = ImGui::GetMousePos();
        int mouseX = static_cast<int>(mousePos.x - canvasPos.x);
        mouseX = std::max(0, std::min(mouseX, waveformWidth));
        
        if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered()) {
            // Check if clicking near markers
            int playbackX = timeToPixel(playbackPosition);
            if (isNearMarker(mouseX, playbackX)) {
                draggingType = Playback;
                draggingMarker = true;
            } else if (startMarkerX >= 0 && isNearMarker(mouseX, startMarkerX)) {
                draggingType = Start;
                draggingMarker = true;
            } else if (endMarkerX >= 0 && isNearMarker(mouseX, endMarkerX)) {
                draggingType = End;
                draggingMarker = true;
            } else {
                // Place or move marker
                if (startMarkerX < 0) {
                    startMarkerX = mouseX;
                    draggingType = Start;
                    draggingMarker = true;
                } else if (endMarkerX < 0) {
                    endMarkerX = mouseX;
                    draggingType = End;
                    draggingMarker = true;
                } else {
                    int distToStart = std::abs(mouseX - startMarkerX);
                    int distToEnd = std::abs(mouseX - endMarkerX);
                    if (distToStart < distToEnd) {
                        startMarkerX = mouseX;
                        draggingType = Start;
                    } else {
                        endMarkerX = mouseX;
                        draggingType = End;
                    }
                    draggingMarker = true;
                }
            }
        }
        
        if (draggingMarker && ImGui::IsMouseDragging(0)) {
            switch (draggingType) {
                case Start:
                    startMarkerX = mouseX;
                    break;
                case End:
                    endMarkerX = mouseX;
                    break;
                case Playback:
                    playbackPosition = pixelToTime(mouseX);
                    break;
                case None:
                    break;
            }
        }
        
        if (ImGui::IsMouseReleased(0)) {
            draggingMarker = false;
            draggingType = None;
        }
    }
    
    // Move cursor below canvas for buttons
    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y + scaled(10)));
    
    // Waveform controls
    if (ImGui::Button("Remove Selected", ImVec2(scaled(140), 0))) {
        removeSelectedRegion();
    }
    ImGui::SameLine();
    if (ImGui::Button("Trim to Selection", ImVec2(scaled(140), 0))) {
        trimToSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Markers", ImVec2(scaled(120), 0))) {
        clearMarkers();
    }
    
    ImGui::EndChild();
}

void AudioClipper::renderControls() {
    ImGui::BeginChild("Controls", ImVec2(-1, scaled(90)), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Recording section
    ImGui::Text("Recording");
    ImGui::SameLine(ImGui::GetWindowWidth() / 2);
    ImGui::Text("Playback");
    ImGui::Spacing();
    
    ImGui::BeginGroup();
    if (isRecording) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.91f, 0.30f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.16f, 0.16f, 1.0f));
        if (ImGui::Button("Recording...", ImVec2(scaled(110), 0))) {
            stopRecording();
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.91f, 0.30f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.16f, 0.16f, 1.0f));
        if (ImGui::Button("Record", ImVec2(scaled(110), 0))) {
            toggleRecording();
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(scaled(70), 0))) {
        stopRecording();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.68f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.13f, 0.59f, 0.33f, 1.0f));
    if (ImGui::Button("Load Audio", ImVec2(scaled(110), 0))) {
        showLoadDialog = true;
    }
    ImGui::PopStyleColor(2);
    ImGui::EndGroup();
    
    ImGui::SameLine(ImGui::GetWindowWidth() / 2);
    ImGui::BeginGroup();
    bool hasAudio = !audioData.empty();
    if (!hasAudio) ImGui::BeginDisabled();
    if (isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.68f, 0.38f, 1.0f));
        if (ImGui::Button("Pause", ImVec2(scaled(110), 0))) {
            playAudio();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.68f, 0.38f, 1.0f));
        if (ImGui::Button("Play", ImVec2(scaled(110), 0))) {
            playAudio();
        }
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop##playback", ImVec2(scaled(70), 0))) {
        stopPlayback();
    }
    if (!hasAudio) ImGui::EndDisabled();
    ImGui::EndGroup();
    
    ImGui::EndChild();
}

void AudioClipper::renderVolumeSection() {
    ImGui::BeginGroup();
    ImGui::Text("Volume Adjustment");
    ImGui::PushItemWidth(scaled(180));
    if (ImGui::SliderFloat("dB", &volumeDb, -20.0f, 20.0f, "%.1f dB")) {
        // Volume display updated automatically
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Apply", ImVec2(scaled(70), 0))) {
        applyVolume();
    }
    ImGui::EndGroup();
}

void AudioClipper::renderSaveSection() {
    ImGui::BeginGroup();
    ImGui::Text("Save Audio");
    bool hasAudio = !audioData.empty();
    if (!hasAudio) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.61f, 0.35f, 0.71f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.27f, 0.68f, 1.0f));
    if (ImGui::Button("WAV", ImVec2(scaled(90), 0))) {
        if (!audioData.empty()) {
            showSaveWavDialog = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("MP3", ImVec2(scaled(90), 0))) {
        if (!audioData.empty()) {
            showSaveMp3Dialog = true;
        }
    }
    ImGui::PopStyleColor(2);
    if (!hasAudio) ImGui::EndDisabled();
    ImGui::EndGroup();
}

void AudioClipper::toggleRecording() {
    if (!isRecording) {
        isRecording = true;
        statusText = "Recording...";
        statusColor = ImVec4(0.91f, 0.30f, 0.24f, 1.0f);
        recorder->startRecording(sampleRate, channels);
    } else {
        stopRecording();
    }
}

void AudioClipper::stopRecording() {
    if (isRecording) {
        recorder->stopRecording();
    }
}

void AudioClipper::loadAudioFile() {
    // For now, use a simple file path input
    static char filepath[512] = "";
    if (showLoadDialog) {
        ImGui::OpenPopup("Load Audio File");
        showLoadDialog = false;
        filepath[0] = '\0';
    }
    
    if (ImGui::BeginPopupModal("Load Audio File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("File Path", filepath, sizeof(filepath));
        if (ImGui::Button("Load", ImVec2(scaled(120), 0))) {
            if (strlen(filepath) > 0) {
                auto result = editor->loadAudioFile(filepath);
                if (result.success) {
                    audioData = result.data;
                    sampleRate = result.sampleRate;
                    channels = result.channels;
                    audioDuration = static_cast<double>(audioData.size()) / (sampleRate * channels);
                    
                    // Downsample for waveform
                    const int maxSamples = 2000;
                    int downsampleFactor = std::max(1, static_cast<int>(audioData.size()) / maxSamples);
                    waveformSamples.clear();
                    waveformSamples.reserve(audioData.size() / downsampleFactor);
                    for (size_t i = 0; i < audioData.size(); i += downsampleFactor) {
                        waveformSamples.push_back(audioData[i]);
                    }
                    
                    playbackPosition = 0.0;
                    startMarkerX = -1;
                    endMarkerX = -1;
                    
                    statusText = "Loaded: " + std::string(filepath);
                    statusColor = ImVec4(0.15f, 0.68f, 0.38f, 1.0f);
                } else {
                    statusText = "Error loading file: " + result.error;
                    statusColor = ImVec4(0.91f, 0.30f, 0.24f, 1.0f);
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(scaled(120), 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AudioClipper::playAudio() {
    if (audioData.empty()) return;
    
    if (isPlaying) {
        player->pause();
        isPlaying = false;
    } else {
        player->play(audioData, sampleRate, channels, playbackPosition);
        isPlaying = true;
    }
}

void AudioClipper::stopPlayback() {
    player->stop();
    isPlaying = false;
    playbackPosition = 0.0;
}

void AudioClipper::applyVolume() {
    if (audioData.empty()) return;
    
    audioData = editor->adjustVolume(audioData, volumeDb);
    
    // Regenerate waveform samples
    const int maxSamples = 2000;
    int downsampleFactor = std::max(1, static_cast<int>(audioData.size()) / maxSamples);
    waveformSamples.clear();
    waveformSamples.reserve(audioData.size() / downsampleFactor);
    for (size_t i = 0; i < audioData.size(); i += downsampleFactor) {
        waveformSamples.push_back(audioData[i]);
    }
    
    std::ostringstream oss;
    oss << "Volume adjusted by " << std::fixed << std::setprecision(1) << volumeDb << " dB";
    statusText = oss.str();
    statusColor = ImVec4(0.20f, 0.60f, 0.86f, 1.0f);
}

void AudioClipper::removeSelectedRegion() {
    if (audioData.empty()) return;
    
    auto markers = getMarkers();
    if (!markers.hasMarkers) return;
    
    if (markers.startTime >= markers.endTime) return;
    
    audioData = editor->removeRegion(audioData, sampleRate, channels, markers.startTime, markers.endTime);
    audioDuration = static_cast<double>(audioData.size()) / (sampleRate * channels);
    
    // Regenerate waveform
    const int maxSamples = 2000;
    int downsampleFactor = std::max(1, static_cast<int>(audioData.size()) / maxSamples);
    waveformSamples.clear();
    waveformSamples.reserve(audioData.size() / downsampleFactor);
    for (size_t i = 0; i < audioData.size(); i += downsampleFactor) {
        waveformSamples.push_back(audioData[i]);
    }
    
    clearMarkers();
    
    std::ostringstream oss;
    oss << "Removed region: " << std::fixed << std::setprecision(2) 
        << markers.startTime << "s to " << markers.endTime << "s";
    statusText = oss.str();
    statusColor = ImVec4(0.91f, 0.30f, 0.24f, 1.0f);
}

void AudioClipper::trimToSelection() {
    if (audioData.empty()) return;
    
    auto markers = getMarkers();
    if (!markers.hasMarkers) return;
    
    if (markers.startTime >= markers.endTime) return;
    
    audioData = editor->trimToRegion(audioData, sampleRate, channels, markers.startTime, markers.endTime);
    audioDuration = static_cast<double>(audioData.size()) / (sampleRate * channels);
    
    // Regenerate waveform
    const int maxSamples = 2000;
    int downsampleFactor = std::max(1, static_cast<int>(audioData.size()) / maxSamples);
    waveformSamples.clear();
    waveformSamples.reserve(audioData.size() / downsampleFactor);
    for (size_t i = 0; i < audioData.size(); i += downsampleFactor) {
        waveformSamples.push_back(audioData[i]);
    }
    
    clearMarkers();
    
    std::ostringstream oss;
    oss << "Trimmed to: " << std::fixed << std::setprecision(2) 
        << markers.startTime << "s to " << markers.endTime << "s";
    statusText = oss.str();
    statusColor = ImVec4(0.20f, 0.60f, 0.86f, 1.0f);
}

void AudioClipper::clearMarkers() {
    startMarkerX = -1;
    endMarkerX = -1;
}

double AudioClipper::pixelToTime(int x) const {
    if (waveformWidth <= 0 || audioDuration <= 0) return 0.0;
    return (static_cast<double>(x) / waveformWidth) * audioDuration;
}

int AudioClipper::timeToPixel(double time) const {
    if (audioDuration <= 0) return 0;
    return static_cast<int>((time / audioDuration) * waveformWidth);
}

bool AudioClipper::isNearMarker(int x, int markerX) const {
    return std::abs(x - markerX) < static_cast<int>(scaled(10));
}

MarkerInfo AudioClipper::getMarkers() const {
    MarkerInfo info;
    info.hasMarkers = (startMarkerX >= 0 && endMarkerX >= 0);
    if (info.hasMarkers) {
        int startX = std::min(startMarkerX, endMarkerX);
        int endX = std::max(startMarkerX, endMarkerX);
        info.startTime = pixelToTime(startX);
        info.endTime = pixelToTime(endX);
    } else {
        info.startTime = 0.0;
        info.endTime = 0.0;
    }
    return info;
}
