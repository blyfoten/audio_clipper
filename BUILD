load("@rules_cc//cc:defs.bzl", "cc_binary")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "audio_clipper",
    srcs = [
        "src/main.cpp",
        "src/audio_clipper.cpp",
        "src/audio_clipper.h",
        "src/audio_recorder.cpp",
        "src/audio_recorder.h",
        "src/audio_player.cpp",
        "src/audio_player.h",
        "src/audio_editor.cpp",
        "src/audio_editor.h",
    ],
    deps = [
        "@imgui//:imgui",
        "@glfw//:glfw",
        "@portaudio//:portaudio",
        "@dr_libs//:dr_libs",
    ],
    linkopts = select({
        "@platforms//os:windows": [
            "-DEFAULTLIB:opengl32.lib",
            "-DEFAULTLIB:shlwapi.lib",
        ],
        "@platforms//os:linux": [
            "-lGL",
            "-lpthread",
        ],
        "//conditions:default": [],
    }),
)
