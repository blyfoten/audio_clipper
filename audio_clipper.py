import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import pyaudio
import wave
import threading
import os
import subprocess
import sys
from pydub import AudioSegment
from pydub.playback import play
import io
import numpy as np

# Configure pydub to use ffmpeg
def setup_ffmpeg():
    """Setup ffmpeg path for pydub"""
    ffmpeg_paths = [
        r"C:\ffmpeg\bin\ffmpeg.exe",
        r"C:\Program Files\ffmpeg\bin\ffmpeg.exe",
        r"C:\Program Files (x86)\ffmpeg\bin\ffmpeg.exe",
    ]
    
    # Check PATH
    path_env = os.environ.get("PATH", "")
    for path_dir in path_env.split(os.pathsep):
        if path_dir.strip():
            ffmpeg_paths.append(os.path.join(path_dir, "ffmpeg.exe"))
    
    for path in ffmpeg_paths:
        try:
            # Use absolute path to be sure
            abs_path = os.path.abspath(path) if not os.path.isabs(path) else path
            if os.path.exists(abs_path):
                AudioSegment.converter = abs_path
                AudioSegment.ffmpeg = abs_path
                # Set ffprobe in same directory
                ffprobe_path = abs_path.replace("ffmpeg.exe", "ffprobe.exe")
                if os.path.exists(ffprobe_path):
                    AudioSegment.ffprobe = ffprobe_path
                else:
                    # Fallback: try same directory
                    ffprobe_path = os.path.join(os.path.dirname(abs_path), "ffprobe.exe")
                    if os.path.exists(ffprobe_path):
                        AudioSegment.ffprobe = ffprobe_path
                return abs_path
        except Exception:
            # Skip paths that cause errors
            continue
    
    return None

# Setup ffmpeg on import
_ffmpeg_path = setup_ffmpeg()

class AudioClipper:
    def __init__(self, root, debug=False):
        self.root = root
        self.root.title("Audio Clipper - Record, Trim & Adjust")
        self.root.geometry("1000x750")
        self.root.configure(bg='#2b2b2b')
        
        # Debug logging (can be enabled via environment variable AUDIO_CLIPPER_DEBUG=1)
        self.debug_enabled = debug or os.environ.get('AUDIO_CLIPPER_DEBUG', '0') == '1'
        
        # Audio variables
        self.audio = None
        self.audio_data = None
        self.is_recording = False
        self.is_playing = False
        self.playback_thread = None
        self.recording_thread = None
        self.playback_process = None
        
        # Waveform data
        self.waveform_data = None
        self.waveform_samples = None
        self.audio_duration = 0
        
        # Cursor markers
        self.start_marker_x = None
        self.end_marker_x = None
        self.dragging_marker = None
        self.marker_width = 3
        self.marker_grab_width = 10
        
        # Playback cursor
        self.playback_cursor_x = None
        self.playback_position = 0.0  # Current playback position in seconds
        self.playback_start_time = 0.0  # Start time when playback begins
        
        # Selection region
        self.selection_rect = None
        
        # Waveform cache for double buffering
        self.waveform_drawn = False
        self.waveform_image_id = None
        
        # PyAudio setup
        self.chunk = 1024
        self.format = pyaudio.paInt16
        self.channels = 1
        self.rate = 44100
        self.frames = []
        self.p = pyaudio.PyAudio()
        
        # Trim markers
        self.trim_start = 0
        self.trim_end = 0
        
        self.setup_ui()
    
    def debug_log(self, message):
        """Print debug message only if debug logging is enabled"""
        if self.debug_enabled:
            print(f"[DEBUG] {message}")
        
    def setup_ui(self):
        # Main container with padding
        main_frame = tk.Frame(self.root, bg='#2b2b2b', padx=20, pady=20)
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Title
        title_label = tk.Label(
            main_frame, 
            text="Audio Clipper", 
            font=('Arial', 24, 'bold'),
            bg='#2b2b2b',
            fg='#ffffff'
        )
        title_label.pack(pady=(0, 15))
        
        # Waveform section
        waveform_frame = tk.Frame(main_frame, bg='#3c3c3c', relief=tk.RAISED, bd=2, padx=15, pady=15)
        waveform_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 15))
        
        tk.Label(
            waveform_frame,
            text="Waveform - Click and drag markers to set trim points",
            font=('Arial', 12, 'bold'),
            bg='#3c3c3c',
            fg='#ffffff'
        ).pack(anchor=tk.W, pady=(0, 10))
        
        # Waveform canvas
        canvas_frame = tk.Frame(waveform_frame, bg='#2b2b2b')
        canvas_frame.pack(fill=tk.BOTH, expand=True)
        
        self.waveform_canvas = tk.Canvas(
            canvas_frame,
            bg='#1e1e1e',
            highlightthickness=0,
            height=200
        )
        self.waveform_canvas.pack(fill=tk.BOTH, expand=True)
        
        # Bind mouse events for marker dragging
        self.waveform_canvas.bind("<Button-1>", self.on_canvas_click)
        self.waveform_canvas.bind("<B1-Motion>", self.on_canvas_drag)
        self.waveform_canvas.bind("<ButtonRelease-1>", self.on_canvas_release)
        self.waveform_canvas.bind("<Motion>", self.on_canvas_hover)
        
        # Waveform controls
        waveform_controls = tk.Frame(waveform_frame, bg='#3c3c3c')
        waveform_controls.pack(fill=tk.X, pady=(10, 0))
        
        remove_selection_btn = tk.Button(
            waveform_controls,
            text="✂️ Remove Selected",
            font=('Arial', 10),
            bg='#e74c3c',
            fg='white',
            activebackground='#c0392b',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=5,
            command=self.remove_selected_region
        )
        remove_selection_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        trim_to_selection_btn = tk.Button(
            waveform_controls,
            text="✂️ Trim to Selection",
            font=('Arial', 10),
            bg='#3498db',
            fg='white',
            activebackground='#2980b9',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=5,
            command=self.trim_to_selection
        )
        trim_to_selection_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        clear_markers_btn = tk.Button(
            waveform_controls,
            text="Clear Markers",
            font=('Arial', 10),
            bg='#7f8c8d',
            fg='white',
            activebackground='#5f6e6f',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=5,
            command=self.clear_markers
        )
        clear_markers_btn.pack(side=tk.LEFT)
        
        # Recording section
        record_frame = tk.Frame(main_frame, bg='#3c3c3c', relief=tk.RAISED, bd=2, padx=15, pady=15)
        record_frame.pack(fill=tk.X, pady=(0, 10))
        
        controls_row = tk.Frame(record_frame, bg='#3c3c3c')
        controls_row.pack(fill=tk.X)
        
        record_col = tk.Frame(controls_row, bg='#3c3c3c')
        record_col.pack(side=tk.LEFT, fill=tk.X, expand=True)
        
        tk.Label(
            record_col,
            text="Recording",
            font=('Arial', 12, 'bold'),
            bg='#3c3c3c',
            fg='#ffffff'
        ).pack(anchor=tk.W, pady=(0, 5))
        
        record_btn_frame = tk.Frame(record_col, bg='#3c3c3c')
        record_btn_frame.pack(fill=tk.X)
        
        self.record_btn = tk.Button(
            record_btn_frame,
            text="● Record",
            font=('Arial', 11),
            bg='#e74c3c',
            fg='white',
            activebackground='#c0392b',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=8,
            command=self.toggle_recording
        )
        self.record_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        self.stop_record_btn = tk.Button(
            record_btn_frame,
            text="Stop",
            font=('Arial', 11),
            bg='#7f8c8d',
            fg='white',
            activebackground='#5f6e6f',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=8,
            command=self.stop_recording,
            state=tk.DISABLED
        )
        self.stop_record_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        self.load_audio_btn = tk.Button(
            record_btn_frame,
            text="📁 Load Audio",
            font=('Arial', 11),
            bg='#27ae60',
            fg='white',
            activebackground='#229954',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=8,
            command=self.load_audio_file
        )
        self.load_audio_btn.pack(side=tk.LEFT)
        
        playback_col = tk.Frame(controls_row, bg='#3c3c3c')
        playback_col.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(20, 0))
        
        tk.Label(
            playback_col,
            text="Playback",
            font=('Arial', 12, 'bold'),
            bg='#3c3c3c',
            fg='#ffffff'
        ).pack(anchor=tk.W, pady=(0, 5))
        
        playback_btn_frame = tk.Frame(playback_col, bg='#3c3c3c')
        playback_btn_frame.pack(fill=tk.X)
        
        self.play_btn = tk.Button(
            playback_btn_frame,
            text="▶ Play",
            font=('Arial', 11),
            bg='#27ae60',
            fg='white',
            activebackground='#229954',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=8,
            command=self.play_audio,
            state=tk.DISABLED
        )
        self.play_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        self.stop_play_btn = tk.Button(
            playback_btn_frame,
            text="⏹ Stop",
            font=('Arial', 11),
            bg='#7f8c8d',
            fg='white',
            activebackground='#5f6e6f',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=8,
            command=self.stop_playback,
            state=tk.DISABLED
        )
        self.stop_play_btn.pack(side=tk.LEFT)
        
        # Status label
        self.status_label = tk.Label(
            record_frame,
            text="Ready to record",
            font=('Arial', 10),
            bg='#3c3c3c',
            fg='#95a5a6'
        )
        self.status_label.pack(anchor=tk.W, pady=(10, 0))
        
        # Volume and Save section
        bottom_frame = tk.Frame(main_frame, bg='#3c3c3c', relief=tk.RAISED, bd=2, padx=15, pady=15)
        bottom_frame.pack(fill=tk.X)
        
        volume_col = tk.Frame(bottom_frame, bg='#3c3c3c')
        volume_col.pack(side=tk.LEFT, fill=tk.X, expand=True)
        
        tk.Label(
            volume_col,
            text="Volume Adjustment",
            font=('Arial', 11, 'bold'),
            bg='#3c3c3c',
            fg='#ffffff'
        ).pack(anchor=tk.W, pady=(0, 5))
        
        volume_controls = tk.Frame(volume_col, bg='#3c3c3c')
        volume_controls.pack(fill=tk.X)
        
        tk.Label(
            volume_controls,
            text="dB:",
            font=('Arial', 9),
            bg='#3c3c3c',
            fg='#ffffff'
        ).pack(side=tk.LEFT, padx=(0, 5))
        
        self.volume_var = tk.DoubleVar(value=0.0)
        volume_scale = tk.Scale(
            volume_controls,
            from_=-20,
            to=20,
            orient=tk.HORIZONTAL,
            variable=self.volume_var,
            bg='#3c3c3c',
            fg='#ffffff',
            troughcolor='#4c4c4c',
            activebackground='#3498db',
            length=200,
            command=self.update_volume_display
        )
        volume_scale.pack(side=tk.LEFT, padx=(0, 10))
        
        self.volume_label = tk.Label(
            volume_controls,
            text="0.0 dB",
            font=('Arial', 9),
            bg='#3c3c3c',
            fg='#ffffff',
            width=8
        )
        self.volume_label.pack(side=tk.LEFT)
        
        apply_volume_btn = tk.Button(
            volume_controls,
            text="Apply",
            font=('Arial', 9),
            bg='#3498db',
            fg='white',
            activebackground='#2980b9',
            activeforeground='white',
            relief=tk.FLAT,
            padx=12,
            pady=5,
            command=self.apply_volume
        )
        apply_volume_btn.pack(side=tk.LEFT, padx=(10, 0))
        
        save_col = tk.Frame(bottom_frame, bg='#3c3c3c')
        save_col.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(20, 0))
        
        tk.Label(
            save_col,
            text="Save Audio",
            font=('Arial', 11, 'bold'),
            bg='#3c3c3c',
            fg='#ffffff'
        ).pack(anchor=tk.W, pady=(0, 5))
        
        save_btn_frame = tk.Frame(save_col, bg='#3c3c3c')
        save_btn_frame.pack(fill=tk.X)
        
        save_wav_btn = tk.Button(
            save_btn_frame,
            text="💾 WAV",
            font=('Arial', 10),
            bg='#9b59b6',
            fg='white',
            activebackground='#8e44ad',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=8,
            command=lambda: self.save_audio('wav'),
            state=tk.DISABLED
        )
        save_wav_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        save_mp3_btn = tk.Button(
            save_btn_frame,
            text="💾 MP3",
            font=('Arial', 10),
            bg='#9b59b6',
            fg='white',
            activebackground='#8e44ad',
            activeforeground='white',
            relief=tk.FLAT,
            padx=15,
            pady=8,
            command=lambda: self.save_audio('mp3'),
            state=tk.DISABLED
        )
        save_mp3_btn.pack(side=tk.LEFT)
        
        self.save_wav_btn = save_wav_btn
        self.save_mp3_btn = save_mp3_btn
        
    def generate_waveform(self):
        """Generate waveform data from audio"""
        if self.audio_data is None:
            return
        
        try:
            # Export to raw audio data
            raw_audio = self.audio_data.raw_data
            samples = np.frombuffer(raw_audio, dtype=np.int16)
            
            # Downsample for display (take every Nth sample)
            downsample_factor = max(1, len(samples) // 2000)
            self.waveform_samples = samples[::downsample_factor]
            self.audio_duration = len(self.audio_data) / 1000.0  # Duration in seconds
            
            self.draw_waveform(force_redraw=True)
        except Exception as e:
            print(f"Error generating waveform: {e}")
    
    def draw_waveform(self, force_redraw=False):
        """Draw the waveform on the canvas with optimized updates"""
        # Use update_idletasks to ensure canvas is sized
        self.waveform_canvas.update_idletasks()
        canvas_width = self.waveform_canvas.winfo_width()
        canvas_height = self.waveform_canvas.winfo_height()
        
        if canvas_width <= 1 or canvas_height <= 1:
            # Schedule redraw after a short delay
            self.root.after(100, lambda: self.draw_waveform(force_redraw))
            return
        
        # Only redraw waveform if forced or not already drawn
        if force_redraw or not self.waveform_drawn:
            # Delete everything to redraw waveform
            self.waveform_canvas.delete("all")
            self.waveform_drawn = False
            
            if self.waveform_samples is None:
                self.waveform_canvas.create_text(
                    canvas_width // 2,
                    canvas_height // 2,
                    text="No audio loaded. Record or load audio to see waveform.",
                    fill='#7f8c8d',
                    font=('Arial', 12),
                    tags=('waveform_bg',)
                )
                return
            
            # Center line
            center_y = canvas_height // 2
            self.waveform_canvas.create_line(
                0, center_y, canvas_width, center_y, 
                fill='#4c4c4c', width=1, tags=('waveform_bg',)
            )
            
            # Draw waveform
            if len(self.waveform_samples) > 0:
                max_amplitude = np.max(np.abs(self.waveform_samples))
                if max_amplitude == 0:
                    max_amplitude = 1
                
                x_scale = canvas_width / len(self.waveform_samples)
                y_scale = (canvas_height - 40) / (2 * max_amplitude)
                
                points = []
                for i, sample in enumerate(self.waveform_samples):
                    x = i * x_scale
                    y = center_y - (sample * y_scale)
                    points.append((x, y))
                
                # Draw waveform as a line
                if len(points) > 1:
                    for i in range(len(points) - 1):
                        self.waveform_canvas.create_line(
                            points[i][0], points[i][1],
                            points[i+1][0], points[i+1][1],
                            fill='#3498db',
                            width=1,
                            tags=('waveform_bg',)
                        )
            
            # Draw time labels (static)
            if self.audio_duration > 0:
                self.waveform_canvas.create_text(
                    10, canvas_height - 15,
                    text="0.0s",
                    fill='#95a5a6',
                    font=('Arial', 9),
                    anchor='w',
                    tags=('waveform_bg',)
                )
                self.waveform_canvas.create_text(
                    canvas_width - 10, canvas_height - 15,
                    text=f"{self.audio_duration:.2f}s",
                    fill='#95a5a6',
                    font=('Arial', 9),
                    anchor='e',
                    tags=('waveform_bg',)
                )
            
            self.waveform_drawn = True
        
        # Always update markers and selection (these change frequently)
        self.update_markers()
    
    def update_markers(self):
        """Update only markers and selection region (fast update)"""
        # Delete only marker-related elements (including playback cursor)
        self.waveform_canvas.delete('marker', 'marker_handle', 'marker_label', 'selection', 'time_label', 'playback_cursor', 'playback_cursor_handle')
        
        canvas_width = self.waveform_canvas.winfo_width()
        canvas_height = self.waveform_canvas.winfo_height()
        
        if canvas_width <= 1 or canvas_height <= 1:
            return
        
        # Draw playback cursor (always visible if set)
        if self.playback_cursor_x is not None:
            # Recalculate position from cursor_x to ensure consistency
            if canvas_width > 0 and self.audio_duration > 0:
                calculated_position = (self.playback_cursor_x / canvas_width) * self.audio_duration
                # Only update playback_position if it's significantly different (avoid race conditions)
                if abs(calculated_position - self.playback_position) > 0.01:
                    self.playback_position = calculated_position
            self.debug_log(f"update_markers: Drawing cursor at x={self.playback_cursor_x:.1f}, is_playing={self.is_playing}, position={self.playback_position:.3f}s")
            self.draw_playback_cursor(self.playback_cursor_x)
        else:
            self.debug_log("update_markers: No cursor to draw (cursor_x is None)")
        
        # Draw selection region if markers exist
        if self.start_marker_x is not None and self.end_marker_x is not None:
            start_x = min(self.start_marker_x, self.end_marker_x)
            end_x = max(self.start_marker_x, self.end_marker_x)
            
            # Draw selection rectangle
            self.selection_rect = self.waveform_canvas.create_rectangle(
                start_x, 0, end_x, canvas_height,
                fill='#3498db',
                stipple='gray25',
                outline='#2980b9',
                width=2,
                tags=('selection',)
            )
            self.waveform_canvas.tag_lower(self.selection_rect, 'waveform_bg')
        
        # Draw markers
        if self.start_marker_x is not None:
            self.draw_marker(self.start_marker_x, '#e74c3c', 'Start')
        
        if self.end_marker_x is not None:
            self.draw_marker(self.end_marker_x, '#27ae60', 'End')
        
        # Draw marker time labels
        if self.audio_duration > 0:
            if self.start_marker_x is not None:
                time = (self.start_marker_x / canvas_width) * self.audio_duration
                self.waveform_canvas.create_text(
                    self.start_marker_x, canvas_height - 15,
                    text=f"{time:.2f}s",
                    fill='#e74c3c',
                    font=('Arial', 9, 'bold'),
                    anchor='center',
                    tags=('time_label',)
                )
            
            if self.end_marker_x is not None:
                time = (self.end_marker_x / canvas_width) * self.audio_duration
                self.waveform_canvas.create_text(
                    self.end_marker_x, canvas_height - 15,
                    text=f"{time:.2f}s",
                    fill='#27ae60',
                    font=('Arial', 9, 'bold'),
                    anchor='center',
                    tags=('time_label',)
                )
    
    def draw_marker(self, x, color, label):
        """Draw a vertical marker line"""
        canvas_height = self.waveform_canvas.winfo_height()
        
        # Draw marker line
        self.waveform_canvas.create_line(
            x, 0, x, canvas_height - 25,
            fill=color,
            width=self.marker_width,
            tags=('marker', f'marker_{label}')
        )
        
        # Draw marker handle (invisible but larger for easier grabbing)
        self.waveform_canvas.create_line(
            x, 0, x, canvas_height - 25,
            fill='',
            width=self.marker_grab_width,
            tags=('marker_handle', f'handle_{label}')
        )
        
        # Draw marker label
        self.waveform_canvas.create_text(
            x, 5,
            text=label,
            fill=color,
            font=('Arial', 9, 'bold'),
            anchor='s',
            tags=('marker_label', f'label_{label}')
        )
    
    def draw_playback_cursor(self, x):
        """Draw the playback cursor - make it very visible"""
        canvas_height = self.waveform_canvas.winfo_height()
        
        # Draw playback cursor line (bright yellow/orange, thicker for visibility)
        self.waveform_canvas.create_line(
            x, 0, x, canvas_height - 25,
            fill='#f39c12',
            width=3,  # Thicker line
            tags=('playback_cursor',)
        )
        
        # Draw cursor handle for dragging (only when not playing)
        if not self.is_playing:
            self.waveform_canvas.create_line(
                x, 0, x, canvas_height - 25,
                fill='',
                width=self.marker_grab_width,
                tags=('playback_cursor_handle',)
            )
        
        # Draw larger cursor indicator triangle at top
        self.waveform_canvas.create_polygon(
            x - 7, 0,
            x + 7, 0,
            x, 12,
            fill='#f39c12',
            outline='#e67e22',
            width=2,
            tags=('playback_cursor',)
        )
        
        # Draw a circle at the center line for better visibility
        center_y = canvas_height // 2
        self.waveform_canvas.create_oval(
            x - 5, center_y - 5,
            x + 5, center_y + 5,
            fill='#f39c12',
            outline='#e67e22',
            width=2,
            tags=('playback_cursor',)
        )
    
    def _update_playback_cursor(self):
        """Update playback cursor position during playback"""
        # Double-check is_playing is still True (prevent race conditions)
        if not self.is_playing:
            self.debug_log("_update_playback_cursor: is_playing=False, returning early")
            return
        if self.audio is None:
            return
        
        canvas_width = self.waveform_canvas.winfo_width()
        if canvas_width <= 0 or self.audio_duration <= 0:
            if self.is_playing:
                self.root.after(50, self._update_playback_cursor)
            return
        
        # Calculate current position using time tracking
        import time
        if not hasattr(self, '_playback_start_real_time'):
            # Wait for _play_audio to set these values
            # If not set yet, schedule a retry
            self.debug_log(f"_update_playback_cursor: Waiting for _play_audio to initialize, cursor_x={self.playback_cursor_x}, position={self.playback_position:.3f}s, is_playing={self.is_playing}")
            if self.is_playing:
                # Keep retrying until _play_audio initializes
                self.root.after(50, self._update_playback_cursor)
            else:
                self.debug_log("_update_playback_cursor: is_playing=False while waiting, stopping")
            return
        
        current_time = time.time()
        elapsed = current_time - self._playback_start_real_time
        self.playback_position = self._playback_start_position + elapsed
        
        # Update cursor position
        old_cursor_x = self.playback_cursor_x
        if self.playback_position >= self.audio_duration:
            # Reached the end - loop back to start (or last paused position)
            self.is_playing = False
            # Reset to beginning or last paused position
            if hasattr(self, '_last_paused_position'):
                self.playback_position = self._last_paused_position
            else:
                self.playback_position = 0.0
            self.playback_cursor_x = (self.playback_position / self.audio_duration) * canvas_width if self.audio_duration > 0 else 0
            # Stop the audio playback
            try:
                if self.playback_process is not None and self.playback_process.poll() is None:
                    self.playback_process.terminate()
                    try:
                        self.playback_process.wait(timeout=0.5)
                    except subprocess.TimeoutExpired:
                        self.playback_process.kill()
                self.playback_process = None
            except:
                pass
            self.root.after(0, self._update_playback_ui)
        else:
            self.playback_cursor_x = (self.playback_position / self.audio_duration) * canvas_width
        
        # Update display if cursor moved
        if old_cursor_x != self.playback_cursor_x:
            self.update_markers()
        
        # Schedule next update - double-check is_playing before scheduling
        if self.is_playing:
            self.root.after(50, self._update_playback_cursor)  # Update every 50ms for smooth animation
        else:
            self.debug_log("_update_playback_cursor: is_playing=False, stopping animation loop")
    
    def pixel_to_time(self, x):
        """Convert pixel position to time in seconds"""
        canvas_width = self.waveform_canvas.winfo_width()
        if canvas_width <= 0 or self.audio_duration <= 0:
            return 0
        return (x / canvas_width) * self.audio_duration
    
    def time_to_pixel(self, time):
        """Convert time in seconds to pixel position"""
        canvas_width = self.waveform_canvas.winfo_width()
        if self.audio_duration <= 0:
            return 0
        return (time / self.audio_duration) * canvas_width
    
    def on_canvas_click(self, event):
        """Handle mouse click on canvas"""
        if self.waveform_samples is None:
            return
        
        x = event.x
        canvas_width = self.waveform_canvas.winfo_width()
        
        # PRIORITY 1: Check if clicking near playback cursor (when not playing)
        if not self.is_playing and self.playback_cursor_x is not None:
            if abs(x - self.playback_cursor_x) < self.marker_grab_width:
                self.dragging_marker = 'playback'
                return
        
        # PRIORITY 2: Check if clicking near trim markers
        if self.start_marker_x is not None:
            if abs(x - self.start_marker_x) < self.marker_grab_width:
                self.dragging_marker = 'start'
                return
        
        if self.end_marker_x is not None:
            if abs(x - self.end_marker_x) < self.marker_grab_width:
                self.dragging_marker = 'end'
                return
        
        # PRIORITY 3: Clicking empty space sets trim markers (not playback cursor)
        # If no marker nearby, place or move the nearest marker
        if self.start_marker_x is None:
            self.start_marker_x = x
            self.dragging_marker = 'start'
        elif self.end_marker_x is None:
            self.end_marker_x = x
            self.dragging_marker = 'end'
        else:
            # Determine which marker is closer
            dist_to_start = abs(x - self.start_marker_x)
            dist_to_end = abs(x - self.end_marker_x)
            
            if dist_to_start < dist_to_end:
                self.start_marker_x = x
                self.dragging_marker = 'start'
            else:
                self.end_marker_x = x
                self.dragging_marker = 'end'
        
        # Only update markers (fast), not the entire waveform
        self.update_markers()
    
    def on_canvas_drag(self, event):
        """Handle mouse drag on canvas"""
        if self.dragging_marker is None:
            return
        
        x = max(0, min(event.x, self.waveform_canvas.winfo_width()))
        canvas_width = self.waveform_canvas.winfo_width()
        
        if self.dragging_marker == 'playback':
            if not self.is_playing:  # Only allow dragging when not playing
                self.playback_cursor_x = x
                self.playback_position = (x / canvas_width) * self.audio_duration if self.audio_duration > 0 else 0
                # Clear any paused position since user manually moved cursor
                if hasattr(self, '_last_paused_position'):
                    delattr(self, '_last_paused_position')
        elif self.dragging_marker == 'start':
            self.start_marker_x = x
        elif self.dragging_marker == 'end':
            self.end_marker_x = x
        
        # Only update markers (fast), not the entire waveform
        self.update_markers()
    
    def on_canvas_release(self, event):
        """Handle mouse release"""
        self.dragging_marker = None
    
    def on_canvas_hover(self, event):
        """Handle mouse hover - change cursor when over markers"""
        if self.waveform_samples is None:
            return
        
        x = event.x
        
        # Check if hovering over playback cursor (when not playing)
        if not self.is_playing and self.playback_cursor_x is not None and abs(x - self.playback_cursor_x) < self.marker_grab_width:
            self.waveform_canvas.config(cursor='sb_h_double_arrow')
        # Check if hovering over a marker
        elif (self.start_marker_x is not None and abs(x - self.start_marker_x) < self.marker_grab_width) or \
           (self.end_marker_x is not None and abs(x - self.end_marker_x) < self.marker_grab_width):
            self.waveform_canvas.config(cursor='sb_h_double_arrow')
        else:
            self.waveform_canvas.config(cursor='arrow')
    
    def clear_markers(self):
        """Clear all markers"""
        self.start_marker_x = None
        self.end_marker_x = None
        self.update_markers()
    
    def remove_selected_region(self):
        """Remove the selected region from audio"""
        if self.audio_data is None:
            messagebox.showwarning("No Audio", "Please record audio first.")
            return
        
        if self.start_marker_x is None or self.end_marker_x is None:
            messagebox.showwarning("No Selection", "Please place start and end markers to select a region.")
            return
        
        canvas_width = self.waveform_canvas.winfo_width()
        start_time = (min(self.start_marker_x, self.end_marker_x) / canvas_width) * self.audio_duration
        end_time = (max(self.start_marker_x, self.end_marker_x) / canvas_width) * self.audio_duration
        
        start_ms = int(start_time * 1000)
        end_ms = int(end_time * 1000)
        
        if start_ms >= end_ms:
            messagebox.showerror("Invalid Selection", "Start marker must be before end marker.")
            return
        
        # Remove the selected region
        part1 = self.audio_data[:start_ms]
        part2 = self.audio_data[end_ms:]
        self.audio_data = part1 + part2
        self.audio = self.audio_data
        
        # Regenerate waveform
        self.generate_waveform()
        self.clear_markers()
        
        self.status_label.config(text=f"Removed region: {start_time:.2f}s to {end_time:.2f}s", fg='#e74c3c')
    
    def trim_to_selection(self):
        """Trim audio to the selected region"""
        if self.audio_data is None:
            messagebox.showwarning("No Audio", "Please record audio first.")
            return
        
        if self.start_marker_x is None or self.end_marker_x is None:
            messagebox.showwarning("No Selection", "Please place start and end markers to select a region.")
            return
        
        canvas_width = self.waveform_canvas.winfo_width()
        start_time = (min(self.start_marker_x, self.end_marker_x) / canvas_width) * self.audio_duration
        end_time = (max(self.start_marker_x, self.end_marker_x) / canvas_width) * self.audio_duration
        
        start_ms = int(start_time * 1000)
        end_ms = int(end_time * 1000)
        
        if start_ms >= end_ms:
            messagebox.showerror("Invalid Selection", "Start marker must be before end marker.")
            return
        
        # Trim to selection
        self.audio = self.audio_data[start_ms:end_ms]
        self.audio_data = self.audio  # Update original data
        
        # Regenerate waveform
        self.generate_waveform()
        self.clear_markers()
        
        self.status_label.config(text=f"Trimmed to: {start_time:.2f}s to {end_time:.2f}s", fg='#3498db')
    
    def toggle_recording(self):
        if not self.is_recording:
            self.start_recording()
        else:
            self.stop_recording()
    
    def start_recording(self):
        self.is_recording = True
        self.frames = []
        self.record_btn.config(state=tk.DISABLED, text="● Recording...")
        self.stop_record_btn.config(state=tk.NORMAL)
        self.status_label.config(text="Recording...", fg='#e74c3c')
        
        self.recording_thread = threading.Thread(target=self._record_audio, daemon=True)
        self.recording_thread.start()
    
    def _record_audio(self):
        stream = self.p.open(
            format=self.format,
            channels=self.channels,
            rate=self.rate,
            input=True,
            frames_per_buffer=self.chunk
        )
        
        while self.is_recording:
            data = stream.read(self.chunk)
            self.frames.append(data)
        
        stream.stop_stream()
        stream.close()
    
    def stop_recording(self):
        self.is_recording = False
        self.record_btn.config(state=tk.NORMAL, text="● Record")
        self.stop_record_btn.config(state=tk.DISABLED)
        
        if self.frames:
            # Save to temporary WAV file
            temp_file = "temp_recording.wav"
            wf = wave.open(temp_file, 'wb')
            wf.setnchannels(self.channels)
            wf.setsampwidth(self.p.get_sample_size(self.format))
            wf.setframerate(self.rate)
            wf.writeframes(b''.join(self.frames))
            wf.close()
            
            # Create AudioSegment from recorded data
            self.audio_data = AudioSegment.from_wav(temp_file)
            self.audio = self.audio_data
            
            # Clean up temp file
            try:
                os.remove(temp_file)
            except:
                pass
            
            # Generate waveform
            self.generate_waveform()
            
            # Initialize playback cursor at start
            self.playback_cursor_x = 0
            self.playback_position = 0.0
            self.update_markers()
            
            # Update UI
            duration = len(self.audio_data) / 1000.0
            self.status_label.config(text=f"Recording complete ({duration:.2f}s)", fg='#27ae60')
            self.play_btn.config(state=tk.NORMAL)
            self.save_wav_btn.config(state=tk.NORMAL)
            self.save_mp3_btn.config(state=tk.NORMAL)
        else:
            self.status_label.config(text="No audio recorded", fg='#e74c3c')
    
    def load_audio_file(self):
        """Load an audio file (MP3 or WAV)"""
        # Stop any current playback
        if self.is_playing:
            self.pause_playback()
        
        # Open file dialog
        filename = filedialog.askopenfilename(
            title="Load Audio File",
            filetypes=[
                ("Audio files", "*.mp3 *.wav *.m4a *.flac *.ogg"),
                ("MP3 files", "*.mp3"),
                ("WAV files", "*.wav"),
                ("All files", "*.*")
            ]
        )
        
        if not filename:
            return
        
        try:
            # Check if FFmpeg is needed (for MP3 and other formats)
            file_ext = os.path.splitext(filename)[1].lower()
            needs_ffmpeg = file_ext in ['.mp3', '.m4a', '.flac', '.ogg']
            
            if needs_ffmpeg:
                # Try to find ffmpeg - check setup_ffmpeg first, then manual search
                ffmpeg_path = setup_ffmpeg()
                
                if not ffmpeg_path:
                    # Manual search with better error reporting
                    possible_paths = [
                        r"C:\ffmpeg\bin\ffmpeg.exe",
                        r"C:\Program Files\ffmpeg\bin\ffmpeg.exe",
                        r"C:\Program Files (x86)\ffmpeg\bin\ffmpeg.exe",
                    ]
                    
                    # Check PATH
                    path_env = os.environ.get("PATH", "")
                    for path_dir in path_env.split(os.pathsep):
                        if path_dir.strip():
                            possible_paths.append(os.path.join(path_dir, "ffmpeg.exe"))
                    
                    found_path = None
                    for path in possible_paths:
                        try:
                            if os.path.exists(path):
                                found_path = path
                                self.debug_log(f"Found ffmpeg at: {path}")
                                break
                        except Exception as e:
                            self.debug_log(f"Error checking path {path}: {e}")
                            continue
                    
                    if not found_path:
                        self.status_label.config(
                            text="Error: FFmpeg not found. Required for MP3 files.",
                            fg='#e74c3c'
                        )
                        return
                    else:
                        ffmpeg_path = found_path
                
                # Temporarily set ffmpeg and ffprobe paths for loading
                original_converter = AudioSegment.converter
                original_ffmpeg = AudioSegment.ffmpeg
                original_ffprobe = getattr(AudioSegment, 'ffprobe', None)
                try:
                    # Set ffmpeg path
                    AudioSegment.converter = ffmpeg_path
                    AudioSegment.ffmpeg = ffmpeg_path
                    # Set ffprobe path (needed for reading metadata)
                    ffprobe_path = ffmpeg_path.replace("ffmpeg.exe", "ffprobe.exe")
                    if os.path.exists(ffprobe_path):
                        AudioSegment.ffprobe = ffprobe_path
                    else:
                        # Fallback: try to find ffprobe in the same directory
                        ffprobe_path = os.path.join(os.path.dirname(ffmpeg_path), "ffprobe.exe")
                        if os.path.exists(ffprobe_path):
                            AudioSegment.ffprobe = ffprobe_path
                        else:
                            # If ffprobe not found, pydub will use ffmpeg as fallback
                            AudioSegment.ffprobe = ffmpeg_path
                    # Verify ffmpeg path is set correctly before loading
                    self.debug_log(f"Loading file with ffmpeg={AudioSegment.ffmpeg}, converter={AudioSegment.converter}")
                    if not os.path.exists(AudioSegment.ffmpeg):
                        raise FileNotFoundError(f"FFmpeg not found at: {AudioSegment.ffmpeg}")
                    
                    self.audio_data = AudioSegment.from_file(filename)
                finally:
                    # Restore original settings
                    AudioSegment.converter = original_converter
                    AudioSegment.ffmpeg = original_ffmpeg
                    if original_ffprobe is not None:
                        AudioSegment.ffprobe = original_ffprobe
                    elif hasattr(AudioSegment, 'ffprobe'):
                        # Only delete if it was set by us
                        pass
            else:
                # WAV files can be loaded directly
                self.audio_data = AudioSegment.from_wav(filename)
            
            # Set working copy
            self.audio = self.audio_data
            
            # Reset markers and cursor
            self.start_marker_x = None
            self.end_marker_x = None
            self.playback_cursor_x = 0
            self.playback_position = 0.0
            
            # Generate waveform
            self.generate_waveform()
            
            # Update UI
            duration = len(self.audio_data) / 1000.0
            filename_short = os.path.basename(filename)
            self.status_label.config(text=f"Loaded: {filename_short} ({duration:.2f}s)", fg='#27ae60')
            self.play_btn.config(state=tk.NORMAL)
            self.save_wav_btn.config(state=tk.NORMAL)
            self.save_mp3_btn.config(state=tk.NORMAL)
            
        except FileNotFoundError as e:
            error_msg = str(e)
            if "ffmpeg" in error_msg.lower() or "The system cannot find the file" in error_msg:
                self.status_label.config(
                    text="Error: FFmpeg not found. Please ensure FFmpeg is installed.",
                    fg='#e74c3c'
                )
            else:
                self.status_label.config(
                    text=f"Error loading file: {error_msg}",
                    fg='#e74c3c'
                )
        except Exception as e:
            import traceback
            error_details = traceback.format_exc()
            self.debug_log(f"Load audio error: {error_details}")
            self.status_label.config(
                text=f"Error loading file: {str(e)}",
                fg='#e74c3c'
            )
    
    def play_audio(self):
        if self.audio is None:
            return
        
        # If already playing, pause it
        if self.is_playing:
            self.debug_log("play_audio: Already playing, pausing")
            self.pause_playback()
            return
        
        # Prevent multiple simultaneous calls - check if thread is already running
        if self.playback_thread is not None and self.playback_thread.is_alive():
            self.debug_log("play_audio: Thread already running, ignoring duplicate call")
            return
        
        # Initialize cursor if not set
        if self.playback_cursor_x is None:
            self.playback_cursor_x = 0
            self.playback_position = 0.0
        
        # Sync playback_position from cursor position if cursor was manually moved
        # (unless we're resuming from a pause)
        if not hasattr(self, '_last_paused_position'):
            # Update playback_position from current cursor position
            canvas_width = self.waveform_canvas.winfo_width()
            if canvas_width > 0 and self.audio_duration > 0 and self.playback_cursor_x is not None:
                self.playback_position = (self.playback_cursor_x / canvas_width) * self.audio_duration
        else:
            # Resuming from pause - use last paused position
            self.playback_position = self._last_paused_position
            canvas_width = self.waveform_canvas.winfo_width()
            if canvas_width > 0 and self.audio_duration > 0:
                self.playback_cursor_x = (self.playback_position / self.audio_duration) * canvas_width
            delattr(self, '_last_paused_position')
        # Capture the position before starting thread to avoid race conditions
        playback_start_pos = self.playback_position
        self.playback_start_time = self.playback_position
        
        self.debug_log(f"play_audio: Starting - position={playback_start_pos:.3f}s, cursor_x={self.playback_cursor_x}")
        
        # Update cursor position immediately to show where playback will start
        canvas_width = self.waveform_canvas.winfo_width()
        if canvas_width > 0 and self.audio_duration > 0:
            self.playback_cursor_x = (self.playback_position / self.audio_duration) * canvas_width
            self.debug_log(f"play_audio: Updated cursor_x to {self.playback_cursor_x:.1f} (position={self.playback_position:.3f}s, canvas={canvas_width}, duration={self.audio_duration:.3f}s)")
            # Force immediate visual update
            self.update_markers()
            self.waveform_canvas.update_idletasks()
            self.debug_log(f"play_audio: After update_markers, cursor_x={self.playback_cursor_x}")
        
        # Set is_playing and position BEFORE starting thread to avoid race conditions
        self.is_playing = True
        self.playback_position = playback_start_pos  # Ensure position is set
        self.play_btn.config(state=tk.NORMAL, text="⏸ Pause")
        self.stop_play_btn.config(state=tk.NORMAL)
        
        self.debug_log(f"play_audio: Set is_playing=True, position={self.playback_position:.3f}s, cursor_x={self.playback_cursor_x}")
        
        # Pass the start position to the playback function
        self.debug_log(f"play_audio: Creating thread with start_pos={playback_start_pos:.3f}s")
        self.playback_thread = threading.Thread(target=lambda: self._play_audio(playback_start_pos), daemon=True)
        self.playback_thread.start()
        self.debug_log(f"play_audio: Thread started, is_alive={self.playback_thread.is_alive()}")
        
        # Start cursor update loop - but only if is_playing is True
        # Give _play_audio a moment to initialize before starting cursor updates
        def start_cursor_updates():
            if self.is_playing:
                self.debug_log(f"play_audio: Starting cursor update loop, is_playing={self.is_playing}")
                self._update_playback_cursor()
            else:
                self.debug_log("play_audio: is_playing=False when starting cursor updates, retrying...")
                self.root.after(100, start_cursor_updates)  # Retry after 100ms
        self.root.after(50, start_cursor_updates)  # Start after a small delay to let thread initialize
    
    def pause_playback(self):
        """Pause playback without resetting position"""
        self.debug_log(f"pause_playback: Called - is_playing={self.is_playing}, process={self.playback_process is not None}")
        self.is_playing = False
        # Save current position for potential resume
        if hasattr(self, 'playback_position'):
            self._last_paused_position = self.playback_position
            self.debug_log(f"pause_playback: Saved paused position={self._last_paused_position:.3f}s")
        try:
            if self.playback_process is not None:
                process_alive = self.playback_process.poll() is None
                self.debug_log(f"pause_playback: Process exists, alive={process_alive}")
                if process_alive:
                    try:
                        self.debug_log("pause_playback: Terminating process")
                        self.playback_process.terminate()
                        try:
                            self.playback_process.wait(timeout=0.5)
                            self.debug_log("pause_playback: Process terminated successfully")
                        except subprocess.TimeoutExpired:
                            self.debug_log("pause_playback: Process didn't terminate, killing")
                            self.playback_process.kill()
                            self.playback_process.wait()
                    except Exception as e:
                        self.debug_log(f"pause_playback: Error terminating process: {e}")
                else:
                    self.debug_log("pause_playback: Process already finished")
            else:
                self.debug_log("pause_playback: No playback process to stop")
            self.playback_process = None
        except Exception as e:
            self.debug_log(f"pause_playback: Exception: {e}")
        finally:
            # Update UI to show paused state - cursor stays where it is
            self.play_btn.config(state=tk.NORMAL, text="▶ Play")
            self.update_markers()  # Update cursor display
            self.debug_log(f"pause_playback: Complete - is_playing={self.is_playing}")
    
    def _find_ffplay(self):
        """Find ffplay executable"""
        # Common locations
        possible_paths = [
            "ffplay",  # In PATH
            r"C:\ffmpeg\bin\ffplay.exe",
            r"C:\Program Files\ffmpeg\bin\ffplay.exe",
            r"C:\Program Files (x86)\ffmpeg\bin\ffplay.exe",
        ]
        
        # Also check PATH environment variable
        path_env = os.environ.get("PATH", "")
        for path_dir in path_env.split(os.pathsep):
            if path_dir.strip():
                possible_paths.append(os.path.join(path_dir, "ffplay.exe"))
        
        for path in possible_paths:
            if os.path.exists(path) or (not os.path.dirname(path) and self._check_command(path)):
                return path
        
        return None
    
    def _check_command(self, cmd):
        """Check if a command exists in PATH"""
        try:
            subprocess.run([cmd, "-version"], 
                         stdout=subprocess.DEVNULL, 
                         stderr=subprocess.DEVNULL,
                         timeout=1,
                         creationflags=subprocess.CREATE_NO_WINDOW if hasattr(subprocess, 'CREATE_NO_WINDOW') and sys.platform == "win32" else 0)
            return True
        except:
            return False
    
    def _play_audio(self, start_position=None):
        import time
        temp_playback = None
        try:
            self.debug_log(f"_play_audio: ENTERED - start_position={start_position}, is_playing={self.is_playing}, audio={self.audio is not None}")
            # CRITICAL: Set is_playing to True IMMEDIATELY at the start of the thread
            # This prevents race conditions with pause_playback or other methods
            self.is_playing = True
            self.debug_log("_play_audio: Set is_playing=True at start")
            
            # Use provided start_position or fall back to self.playback_position
            if start_position is None:
                start_position = self.playback_position
            
            self.debug_log(f"_play_audio: Using start_position={start_position:.3f}s")
            
            # Update self.playback_position to match start_position
            self.playback_position = start_position
            
            # Update cursor position FIRST, before doing anything else
            canvas_width = self.waveform_canvas.winfo_width()
            if canvas_width > 0 and self.audio_duration > 0:
                self.playback_cursor_x = (start_position / self.audio_duration) * canvas_width
                self.playback_position = start_position  # Ensure position is set
                # Double-check is_playing is still True (in case something reset it)
                if not self.is_playing:
                    self.debug_log("_play_audio: WARNING - is_playing was False, resetting to True")
                    self.is_playing = True
                self.debug_log(f"_play_audio: Set cursor_x={self.playback_cursor_x:.1f}, position={self.playback_position:.3f}s, is_playing={self.is_playing}")
                # Force immediate update in main thread - ensure state is correct
                def update_with_state():
                    # Ensure state is synchronized - re-read all values in main thread
                    if self.playback_cursor_x is not None and canvas_width > 0 and self.audio_duration > 0:
                        # Recalculate position from cursor to ensure consistency
                        self.playback_position = (self.playback_cursor_x / canvas_width) * self.audio_duration
                        # CRITICAL: Ensure is_playing is True in main thread too
                        self.is_playing = True
                        self.debug_log(f"update_with_state: Synced position={self.playback_position:.3f}s from cursor_x={self.playback_cursor_x:.1f}, is_playing={self.is_playing}")
                    self.update_markers()
                self.root.after_idle(update_with_state)
            
            # If starting from a specific position, trim audio
            audio_to_play = self.audio
            if start_position > 0:
                start_ms = int(start_position * 1000)
                if start_ms < len(self.audio):
                    audio_to_play = self.audio[start_ms:]
            
            # Export to WAV for playback
            temp_playback = "temp_playback.wav"
            audio_to_play.export(temp_playback, format="wav")
            
            # Record start time for cursor tracking
            self._playback_start_real_time = time.time()
            self._playback_start_position = start_position
            self.debug_log(f"_play_audio: Set tracking - start_time={self._playback_start_real_time:.3f}, start_position={self._playback_start_position:.3f}s")
            
            # Update cursor again after tracking is set
            if canvas_width > 0 and self.audio_duration > 0:
                # Ensure position is still correct
                self.playback_position = start_position
                def update_with_state():
                    if self.playback_cursor_x is not None:
                        self.update_markers()
                self.root.after_idle(update_with_state)
            
            # Use subprocess to play audio - more stable on Windows
            if sys.platform == "win32":
                # Try using ffplay (from ffmpeg) first - most reliable and controllable
                ffplay_path = self._find_ffplay()
                if ffplay_path:
                    try:
                        self.playback_process = subprocess.Popen(
                            [ffplay_path, "-nodisp", "-autoexit", "-loglevel", "quiet", temp_playback],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            creationflags=subprocess.CREATE_NO_WINDOW if hasattr(subprocess, 'CREATE_NO_WINDOW') else 0
                        )
                    except (FileNotFoundError, OSError) as e:
                        print(f"ffplay error: {e}")
                        ffplay_path = None  # Force fallback
                
                if not ffplay_path:
                    # Fallback: Use threading with pydub but catch all exceptions
                    # Play in a separate thread and monitor it
                    try:
                        if self.audio is not None:
                            # Calculate duration and wait for it
                            duration_seconds = len(self.audio) / 1000.0
                            
                            # Play in a thread that we can monitor
                            def play_thread():
                                try:
                                    play(self.audio)
                                except Exception as play_error:
                                    print(f"Play error in thread: {play_error}")
                            
                            play_thread_obj = threading.Thread(target=play_thread, daemon=True)
                            play_thread_obj.start()
                            
                            # Wait for playback duration or until stopped
                            elapsed = 0
                            while self.is_playing and elapsed < duration_seconds:
                                threading.Event().wait(0.1)
                                elapsed += 0.1
                                if not play_thread_obj.is_alive():
                                    break
                        return
                    except Exception as e:
                        print(f"Pydub playback error: {e}")
                        return
            else:
                # Linux/Mac - use aplay or afplay
                try:
                    if sys.platform == "darwin":
                        self.playback_process = subprocess.Popen(
                            ["afplay", temp_playback],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL
                        )
                    else:
                        self.playback_process = subprocess.Popen(
                            ["aplay", temp_playback],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL
                        )
                except FileNotFoundError:
                    # Fallback to pydub
                    try:
                        if self.audio is not None:
                            play(self.audio)
                    except Exception as e:
                        print(f"Pydub playback error: {e}")
                    return
            
            # Wait for playback to finish or be stopped
            if self.playback_process:
                try:
                    while self.is_playing:
                        if self.playback_process.poll() is not None:
                            # Process finished
                            break
                        threading.Event().wait(0.1)
                except Exception as e:
                    print(f"Error waiting for playback: {e}")
            
        except Exception as e:
            self.debug_log(f"_play_audio: EXCEPTION - {e}")
            import traceback
            traceback.print_exc()
        finally:
            # Stop playback process if still running
            if self.playback_process:
                try:
                    if self.playback_process.poll() is None:
                        # Process still running, terminate it
                        self.playback_process.terminate()
                        # Wait a bit for graceful termination
                        try:
                            self.playback_process.wait(timeout=0.5)
                        except subprocess.TimeoutExpired:
                            try:
                                self.playback_process.kill()
                            except:
                                pass
                except Exception as e:
                    print(f"Error stopping playback process: {e}")
                finally:
                    self.playback_process = None
            
            # Clean up temp file
            if temp_playback:
                try:
                    # Wait a bit before removing to ensure file is released
                    threading.Event().wait(0.3)
                    if os.path.exists(temp_playback):
                        os.remove(temp_playback)
                except Exception as e:
                    print(f"Error removing temp file: {e}")
            
            self.is_playing = False
            
            # Update UI safely
            try:
                self.root.after(0, self._update_playback_ui)
            except:
                pass
    
    def _update_playback_ui(self):
        """Update playback UI after playback ends"""
        self.play_btn.config(state=tk.NORMAL, text="▶ Play")
        self.stop_play_btn.config(state=tk.DISABLED)
        # Ensure cursor is visible at end position
        self.update_markers()
    
    def stop_playback(self):
        """Stop playback and reset to beginning"""
        self.is_playing = False
        # Clear paused position when stopping
        if hasattr(self, '_last_paused_position'):
            delattr(self, '_last_paused_position')
        try:
            if self.playback_process is not None and self.playback_process.poll() is None:
                try:
                    self.playback_process.terminate()
                    # Wait a bit for graceful termination
                    try:
                        self.playback_process.wait(timeout=0.5)
                    except subprocess.TimeoutExpired:
                        self.playback_process.kill()
                except Exception as e:
                    print(f"Error stopping playback process: {e}")
            self.playback_process = None
        except Exception as e:
            print(f"Error stopping playback: {e}")
        finally:
            # Reset cursor to start when stopped
            if hasattr(self, '_playback_start_real_time'):
                delattr(self, '_playback_start_real_time')
            # Reset cursor to beginning
            self.playback_cursor_x = 0
            self.playback_position = 0.0
            self.update_markers()
            self._update_playback_ui()
    
    def update_volume_display(self, value=None):
        vol = self.volume_var.get()
        self.volume_label.config(text=f"{vol:.1f} dB")
    
    def apply_volume(self):
        if self.audio is None:
            messagebox.showwarning("No Audio", "Please record audio first.")
            return
        
        try:
            volume_change = self.volume_var.get()
            # Apply volume to current audio (which may already be trimmed)
            self.audio = self.audio + volume_change
            self.audio_data = self.audio  # Update original data
            self.generate_waveform()
            self.status_label.config(text=f"Volume adjusted by {volume_change:.1f} dB", fg='#3498db')
            messagebox.showinfo("Success", "Volume adjustment applied!")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to adjust volume: {str(e)}")
    
    def save_audio(self, format_type):
        if self.audio is None:
            messagebox.showwarning("No Audio", "Please record and process audio first.")
            return
        
        try:
            if format_type == 'wav':
                filename = filedialog.asksaveasfilename(
                    defaultextension=".wav",
                    filetypes=[("WAV files", "*.wav"), ("All files", "*.*")]
                )
                if filename:
                    # Ensure directory exists
                    dirname = os.path.dirname(filename)
                    if dirname and not os.path.exists(dirname):
                        os.makedirs(dirname, exist_ok=True)
                    
                    self.audio.export(filename, format="wav")
                    filename_short = os.path.basename(filename)
                    self.status_label.config(text=f"Saved: {filename_short}", fg='#27ae60')
            elif format_type == 'mp3':
                # Ensure ffmpeg is available for MP3 export
                ffmpeg_path = setup_ffmpeg()
                if not ffmpeg_path:
                    self.status_label.config(
                        text="Error: FFmpeg not found. Required for MP3 export.",
                        fg='#e74c3c'
                    )
                    return
                
                filename = filedialog.asksaveasfilename(
                    defaultextension=".mp3",
                    filetypes=[("MP3 files", "*.mp3"), ("All files", "*.*")]
                )
                if filename:
                    # Ensure directory exists
                    dirname = os.path.dirname(filename)
                    if dirname and not os.path.exists(dirname):
                        os.makedirs(dirname, exist_ok=True)
                    
                    # Set ffmpeg path for this export
                    original_converter = AudioSegment.converter
                    original_ffmpeg = AudioSegment.ffmpeg
                    original_ffprobe = getattr(AudioSegment, 'ffprobe', None)
                    try:
                        AudioSegment.converter = ffmpeg_path
                        AudioSegment.ffmpeg = ffmpeg_path
                        # Also set ffprobe if available
                        ffprobe_path = ffmpeg_path.replace("ffmpeg.exe", "ffprobe.exe")
                        if os.path.exists(ffprobe_path):
                            AudioSegment.ffprobe = ffprobe_path
                        self.audio.export(filename, format="mp3", bitrate="192k")
                        filename_short = os.path.basename(filename)
                        self.status_label.config(text=f"Saved: {filename_short}", fg='#27ae60')
                    finally:
                        # Restore original settings
                        AudioSegment.converter = original_converter
                        AudioSegment.ffmpeg = original_ffmpeg
                        if original_ffprobe is not None:
                            AudioSegment.ffprobe = original_ffprobe
        except FileNotFoundError as e:
            error_msg = str(e)
            if "ffmpeg" in error_msg.lower() or "The system cannot find the file" in error_msg:
                self.status_label.config(
                    text="Error: FFmpeg not found. Please ensure FFmpeg is installed.",
                    fg='#e74c3c'
                )
            else:
                self.status_label.config(
                    text=f"Error saving file: {error_msg}",
                    fg='#e74c3c'
                )
        except Exception as e:
            import traceback
            error_details = traceback.format_exc()
            self.debug_log(f"Save error details: {error_details}")
            self.status_label.config(
                text=f"Error saving file: {str(e)}",
                fg='#e74c3c'
            )
    
    def __del__(self):
        if hasattr(self, 'p'):
            self.p.terminate()

def main():
    # Enable debug logging if --debug flag is passed or AUDIO_CLIPPER_DEBUG env var is set
    debug = '--debug' in sys.argv or os.environ.get('AUDIO_CLIPPER_DEBUG', '0') == '1'
    
    root = tk.Tk()
    app = AudioClipper(root, debug=debug)
    
    # Update waveform on window resize (only for the root window)
    def on_resize(event):
        if event.widget == root:
            app.draw_waveform()
    
    root.bind('<Configure>', on_resize)
    
    # Initial waveform draw after window is shown
    root.after(100, app.draw_waveform)
    
    root.mainloop()

if __name__ == "__main__":
    main()
