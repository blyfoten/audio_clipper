#include "audio_editor.h"

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <cmath>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <unistd.h>
#include <limits.h>
#endif

AudioEditor::AudioEditor() = default;
AudioEditor::~AudioEditor() = default;

LoadResult AudioEditor::loadAudioFile(const std::string &filename) {
    LoadResult result;
    result.success = false;
    
    // Check file extension
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // If it's not WAV, convert to WAV first using FFmpeg
    std::string wavFile = filename;
    if (ext != "wav") {
        std::string tempWav = filename + ".temp.wav";
        if (!convertToWAV(filename, tempWav)) {
            result.error = "Failed to convert audio file. FFmpeg may not be installed.";
            return result;
        }
        wavFile = tempWav;
    }
    
    // Load WAV file using dr_wav
    drwav wav;
    if (!drwav_init_file(&wav, wavFile.c_str(), nullptr)) {
        result.error = "Failed to open audio file";
        if (wavFile != filename) {
            std::remove(wavFile.c_str());
        }
        return result;
    }
    
    result.sampleRate = static_cast<int>(wav.sampleRate);
    result.channels = static_cast<int>(wav.channels);
    
    // Read all samples as float
    size_t totalSamples = wav.totalPCMFrameCount * wav.channels;
    result.data.resize(totalSamples);
    
    drwav_uint64 framesRead = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, result.data.data());
    
    if (framesRead == 0) {
        result.error = "Failed to read audio data";
        drwav_uninit(&wav);
        if (wavFile != filename) {
            std::remove(wavFile.c_str());
        }
        return result;
    }
    
    // Resize to actual frames read
    result.data.resize(framesRead * wav.channels);
    
    drwav_uninit(&wav);
    
    // Clean up temp file
    if (wavFile != filename) {
        std::remove(wavFile.c_str());
    }
    
    result.success = true;
    return result;
}

bool AudioEditor::saveWAV(const std::string &filename, const std::vector<float> &data, int sampleRate, int channels) {
    if (data.empty() || sampleRate <= 0 || channels <= 0) {
        return false;
    }
    
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = static_cast<drwav_uint32>(channels);
    format.sampleRate = static_cast<drwav_uint32>(sampleRate);
    format.bitsPerSample = 32;
    
    drwav wav;
    if (!drwav_init_file_write(&wav, filename.c_str(), &format, nullptr)) {
        return false;
    }
    
    drwav_uint64 frames = data.size() / channels;
    drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, frames, data.data());
    
    drwav_uninit(&wav);
    
    return framesWritten == frames;
}

bool AudioEditor::saveMP3(const std::string &filename, const std::vector<float> &data, int sampleRate, int channels) {
    if (data.empty() || sampleRate <= 0 || channels <= 0) {
        return false;
    }
    
    // Save to temporary WAV file first
    std::string tempWav = filename + ".temp.wav";
    if (!saveWAV(tempWav, data, sampleRate, channels)) {
        return false;
    }
    
    // Convert to MP3 using FFmpeg
    std::string ffmpeg = findFFmpeg();
    if (ffmpeg.empty()) {
        std::remove(tempWav.c_str());
        return false;
    }
    
    std::stringstream cmd;
    cmd << "\"" << ffmpeg << "\" -i \"" << tempWav << "\" -codec:a libmp3lame -b:a 192k -y \"" << filename << "\"";
    
    int result = system(cmd.str().c_str());
    std::remove(tempWav.c_str());
    
    return result == 0;
}

std::vector<float> AudioEditor::adjustVolume(const std::vector<float> &data, double volumeDb) {
    double linearGain = std::pow(10.0, volumeDb / 20.0);
    
    std::vector<float> result = data;
    for (float &sample : result) {
        sample *= static_cast<float>(linearGain);
        // Clamp to [-1, 1]
        sample = std::max(-1.0f, std::min(1.0f, sample));
    }
    
    return result;
}

std::vector<float> AudioEditor::trimToRegion(const std::vector<float> &data, int sampleRate, int channels, double startTime, double endTime) {
    if (data.empty() || sampleRate <= 0 || channels <= 0) {
        return data;
    }
    
    size_t startSample = static_cast<size_t>(startTime * sampleRate * channels);
    size_t endSample = static_cast<size_t>(endTime * sampleRate * channels);
    
    startSample = std::min(startSample, data.size());
    endSample = std::min(endSample, data.size());
    
    if (startSample >= endSample) {
        return std::vector<float>();
    }
    
    return std::vector<float>(data.begin() + startSample, data.begin() + endSample);
}

std::vector<float> AudioEditor::removeRegion(const std::vector<float> &data, int sampleRate, int channels, double startTime, double endTime) {
    if (data.empty() || sampleRate <= 0 || channels <= 0) {
        return data;
    }
    
    size_t startSample = static_cast<size_t>(startTime * sampleRate * channels);
    size_t endSample = static_cast<size_t>(endTime * sampleRate * channels);
    
    startSample = std::min(startSample, data.size());
    endSample = std::min(endSample, data.size());
    
    if (startSample >= endSample) {
        return data;
    }
    
    std::vector<float> result;
    result.reserve(data.size() - (endSample - startSample));
    result.insert(result.end(), data.begin(), data.begin() + startSample);
    result.insert(result.end(), data.begin() + endSample, data.end());
    
    return result;
}

std::string AudioEditor::findFFmpeg() const {
#ifdef _WIN32
    // Check common Windows locations
    const char *paths[] = {
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\Program Files (x86)\\ffmpeg\\bin\\ffmpeg.exe",
    };
    
    for (const char *path : paths) {
        if (PathFileExistsA(path)) {
            return std::string(path);
        }
    }
    
    // Check PATH
    char pathEnv[32767];
    if (GetEnvironmentVariableA("PATH", pathEnv, sizeof(pathEnv))) {
        std::string pathStr(pathEnv);
        std::istringstream iss(pathStr);
        std::string dir;
        while (std::getline(iss, dir, ';')) {
            std::string ffmpegPath = dir + "\\ffmpeg.exe";
            if (PathFileExistsA(ffmpegPath.c_str())) {
                return ffmpegPath;
            }
        }
    }
#else
    // Check if ffmpeg is in PATH
    if (system("which ffmpeg > /dev/null 2>&1") == 0) {
        return "ffmpeg";
    }
    
    // Check common Linux locations
    const char *paths[] = {
        "/usr/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
    };
    
    for (const char *path : paths) {
        if (access(path, X_OK) == 0) {
            return std::string(path);
        }
    }
#endif
    
    return "";
}

bool AudioEditor::convertToWAV(const std::string &inputFile, const std::string &outputFile) const {
    std::string ffmpeg = findFFmpeg();
    if (ffmpeg.empty()) {
        return false;
    }
    
    std::stringstream cmd;
    cmd << "\"" << ffmpeg << "\" -i \"" << inputFile << "\" -y \"" << outputFile << "\"";
    
    int result = system(cmd.str().c_str());
    return result == 0;
}
