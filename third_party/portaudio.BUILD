load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# Common source files
PORTAUDIO_COMMON_SRCS = [
    "src/common/pa_allocation.c",
    "src/common/pa_converters.c",
    "src/common/pa_cpuload.c",
    "src/common/pa_debugprint.c",
    "src/common/pa_dither.c",
    "src/common/pa_front.c",
    "src/common/pa_process.c",
    "src/common/pa_ringbuffer.c",
    "src/common/pa_stream.c",
    "src/common/pa_trace.c",
]

# Windows-specific source files (WASAPI + WMME)
PORTAUDIO_WIN_SRCS = [
    "src/os/win/pa_win_coinitialize.c",
    "src/os/win/pa_win_hostapis.c",
    "src/os/win/pa_win_util.c",
    "src/os/win/pa_win_waveformat.c",
    "src/os/win/pa_win_wdmks_utils.c",
    "src/os/win/pa_x86_plain_converters.c",
    "src/hostapi/wmme/pa_win_wmme.c",
    "src/hostapi/wasapi/pa_win_wasapi.c",
    "src/hostapi/wdmks/pa_win_wdmks.c",
    "src/hostapi/dsound/pa_win_ds.c",
    "src/hostapi/dsound/pa_win_ds_dynlink.c",
]

# Linux-specific source files (ALSA)
PORTAUDIO_LINUX_SRCS = [
    "src/os/unix/pa_unix_hostapis.c",
    "src/os/unix/pa_unix_util.c",
    "src/hostapi/alsa/pa_linux_alsa.c",
]

cc_library(
    name = "portaudio",
    srcs = PORTAUDIO_COMMON_SRCS + select({
        "@platforms//os:windows": PORTAUDIO_WIN_SRCS,
        "@platforms//os:linux": PORTAUDIO_LINUX_SRCS,
        "//conditions:default": [],
    }),
    hdrs = glob([
        "include/*.h",
        "src/common/*.h",
        "src/os/win/*.h",
        "src/os/unix/*.h",
        "src/hostapi/**/*.h",
    ], allow_empty = True),
    includes = [
        "include",
        "src/common",
    ] + select({
        "@platforms//os:windows": [
            "src/os/win",
        ],
        "@platforms//os:linux": [
            "src/os/unix",
        ],
        "//conditions:default": [],
    }),
    local_defines = select({
        "@platforms//os:windows": [
            "PA_USE_WMME=1",
            "PA_USE_WASAPI=1",
            "PA_USE_WDMKS=1",
            "PA_USE_DS=1",
        ],
        "@platforms//os:linux": [
            "PA_USE_ALSA=1",
        ],
        "//conditions:default": [],
    }),
    linkopts = select({
        "@platforms//os:windows": [
            "-DEFAULTLIB:ole32.lib",
            "-DEFAULTLIB:winmm.lib",
            "-DEFAULTLIB:uuid.lib",
            "-DEFAULTLIB:ksuser.lib",
            "-DEFAULTLIB:dsound.lib",
            "-DEFAULTLIB:advapi32.lib",
        ],
        "@platforms//os:linux": [
            "-lasound",
            "-lpthread",
        ],
        "//conditions:default": [],
    }),
)
