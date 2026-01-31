# Audio Clipper

A simple and elegant GUI tool for recording, trimming, and adjusting audio clips. Save your recordings as MP3 or WAV files.

## Features

- 🎤 **Record Audio** - Record audio directly from your microphone
- ✂️ **Trim Audio** - Cut audio clips to your desired start and end times
- 🔊 **Volume Adjustment** - Adjust volume levels with a simple slider (-20dB to +20dB)
- ▶️ **Playback** - Preview your audio before saving
- 💾 **Export** - Save your audio as MP3 or WAV format

## Requirements

- Python 3.7 or higher
- FFmpeg (required for MP3 export)

### Installing FFmpeg

**Windows:**
1. Download FFmpeg from https://ffmpeg.org/download.html
2. Extract and add to your system PATH

**macOS:**
```bash
brew install ffmpeg
```

**Linux:**
```bash
sudo apt-get install ffmpeg
```

## Installation

1. Install Python dependencies:
```bash
pip install -r requirements.txt
```

**Note for Windows users:** If you encounter issues installing `pyaudio`, you may need to install it using:
```bash
pip install pipwin
pipwin install pyaudio
```

Or download the appropriate wheel file from: https://www.lfd.uci.edu/~gohlke/pythonlibs/#pyaudio

## Usage

Run the application:
```bash
python audio_clipper.py
```

### How to Use

1. **Record Audio:**
   - Click the "● Record" button to start recording
   - Click "Stop" when finished
   - The duration will be displayed automatically

2. **Trim Audio:**
   - Enter start and end times in seconds
   - Click "Apply Trim" to trim the audio

3. **Adjust Volume:**
   - Use the volume slider to adjust the volume (-20dB to +20dB)
   - Click "Apply Volume" to apply the change

4. **Play Audio:**
   - Click "▶ Play" to preview your audio
   - Click "⏹ Stop" to stop playback

5. **Save Audio:**
   - Click "💾 Save as WAV" or "💾 Save as MP3"
   - Choose a location and filename
   - Your audio will be saved with all applied edits

## Notes

- All edits (trim and volume) are applied to a working copy of your audio
- You can apply multiple edits before saving
- MP3 export requires FFmpeg to be installed on your system

