# Audio Clipper

A simple and elegant cross-platform GUI application for recording, trimming, and adjusting audio clips. Built with C++, ImGui, and Bazel.

![Audio Clipper Screenshot](docs/screenshot.png)

## Features

- 🎤 **Record Audio** - Record directly from your microphone
- ✂️ **Trim Audio** - Visual waveform editor with draggable markers
- 🔊 **Volume Adjustment** - Adjust levels from -20dB to +20dB
- ▶️ **Playback** - Preview audio with visual playhead
- 💾 **Export** - Save as WAV or MP3 format
- 📁 **Load Audio** - Import WAV files (MP3/FLAC via FFmpeg)

## Requirements

- **Bazel** (or Bazelisk) - Build system
- **C++17 compiler** - MSVC (Windows) or GCC/Clang (Linux)
- **OpenGL** - Usually available system-wide
- **FFmpeg** (optional) - For MP3 export and loading non-WAV files

All other dependencies (ImGui, GLFW, PortAudio, dr_libs) are automatically downloaded by Bazel.

## Building

### Windows

1. Install [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/) with C++ workload
2. Install [Bazelisk](https://github.com/bazelbuild/bazelisk/releases) (recommended) or Bazel 7.0+

```cmd
bazelisk build --config=windows //:audio_clipper
```

Run the application:
```cmd
.\bazel-bin\audio_clipper.exe
```

### Linux

1. Install build essentials and dependencies:
```bash
sudo apt-get install build-essential libasound2-dev libgl1-mesa-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxi-dev libxcursor-dev
```

2. Install Bazel or Bazelisk

```bash
bazel build //:audio_clipper
```

Run the application:
```bash
./bazel-bin/audio_clipper
```

## Installing FFmpeg (Optional)

FFmpeg is required for MP3 export and importing non-WAV audio files.

**Windows:**
1. Download from https://ffmpeg.org/download.html
2. Extract and add the `bin` folder to your system PATH

**Linux:**
```bash
sudo apt-get install ffmpeg
```

**macOS:**
```bash
brew install ffmpeg
```

## Usage

1. **Record Audio:**
   - Click "Record" to start recording from your microphone
   - Click "Stop" when finished

2. **Load Audio:**
   - Click "Load Audio" and enter the file path
   - Supports WAV natively; MP3/FLAC/OGG require FFmpeg

3. **Edit Audio:**
   - Click on the waveform to place start/end markers
   - Drag markers to adjust selection
   - Use "Trim to Selection" to keep only selected region
   - Use "Remove Selected" to delete selected region
   - Use "Clear Markers" to reset

4. **Adjust Volume:**
   - Use the slider to set volume adjustment (-20dB to +20dB)
   - Click "Apply" to apply the change

5. **Playback:**
   - Click "Play" to preview your audio
   - The orange playhead shows current position
   - Drag the playhead to seek

6. **Save:**
   - Click "WAV" or "MP3" to export
   - Enter the desired file path

## Project Structure

```
audio_clipper/
├── src/
│   ├── main.cpp           # Application entry point, GLFW/ImGui setup
│   ├── audio_clipper.*    # Main UI and application logic
│   ├── audio_recorder.*   # PortAudio recording
│   ├── audio_player.*     # PortAudio playback
│   └── audio_editor.*     # Audio processing (trim, volume, file I/O)
├── third_party/           # Bazel BUILD files for dependencies
├── BUILD                  # Main build file
├── MODULE.bazel           # Bazel module configuration
└── repositories.bzl       # External dependency definitions
```

## Dependencies

| Library | Purpose | License |
|---------|---------|---------|
| [ImGui](https://github.com/ocornut/imgui) | Immediate mode GUI | MIT |
| [GLFW](https://github.com/glfw/glfw) | Window/OpenGL context | Zlib |
| [PortAudio](http://www.portaudio.com/) | Cross-platform audio I/O | MIT |
| [dr_libs](https://github.com/mackron/dr_libs) | WAV file reading/writing | Public Domain |

## License

MIT License - See [LICENSE](LICENSE) for details.
