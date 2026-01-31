load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "imgui",
    srcs = [
        "imgui.cpp",
        "imgui_demo.cpp",
        "imgui_draw.cpp",
        "imgui_tables.cpp",
        "imgui_widgets.cpp",
        "backends/imgui_impl_glfw.cpp",
        "backends/imgui_impl_opengl3.cpp",
    ],
    hdrs = glob([
        "*.h",
        "backends/*.h",
    ], allow_empty = True),
    includes = [".", "backends"],
    deps = [
        "@glfw//:glfw",
    ],
    linkopts = select({
        "@platforms//os:windows": [
            "-DEFAULTLIB:opengl32.lib",
        ],
        "@platforms//os:linux": [
            "-lGL",
        ],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "imgui_headers",
    hdrs = glob(["*.h", "backends/*.h"], allow_empty = True),
    includes = [".", "backends"],
)
