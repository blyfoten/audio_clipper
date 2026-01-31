load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# Common source files for GLFW 3.3.8
GLFW_COMMON_SRCS = [
    "src/context.c",
    "src/init.c",
    "src/input.c",
    "src/monitor.c",
    "src/vulkan.c",
    "src/window.c",
    "src/egl_context.c",
    "src/osmesa_context.c",
]

# Windows-specific files for GLFW 3.3.8
GLFW_WIN32_SRCS = [
    "src/win32_init.c",
    "src/win32_joystick.c",
    "src/win32_monitor.c",
    "src/win32_thread.c",
    "src/win32_time.c",
    "src/win32_window.c",
    "src/wgl_context.c",
]

# Linux-specific files for GLFW 3.3.8
GLFW_LINUX_SRCS = [
    "src/x11_init.c",
    "src/x11_monitor.c",
    "src/x11_window.c",
    "src/xkb_unicode.c",
    "src/glx_context.c",
    "src/linux_joystick.c",
    "src/posix_time.c",
    "src/posix_thread.c",
]

cc_library(
    name = "glfw",
    srcs = GLFW_COMMON_SRCS + select({
        "@platforms//os:windows": GLFW_WIN32_SRCS,
        "@platforms//os:linux": GLFW_LINUX_SRCS,
        "//conditions:default": [],
    }),
    hdrs = glob([
        "include/GLFW/*.h",
        "src/*.h",
        "src/win32_*.h",
    ], allow_empty = True),
    includes = ["include"],
    local_defines = select({
        "@platforms//os:windows": ["_GLFW_WIN32"],
        "@platforms//os:linux": ["_GLFW_X11"],
        "//conditions:default": [],
    }),
    linkopts = select({
        "@platforms//os:windows": [
            "-DEFAULTLIB:gdi32.lib",
            "-DEFAULTLIB:user32.lib",
            "-DEFAULTLIB:shell32.lib",
        ],
        "@platforms//os:linux": [
            "-lX11",
            "-lXrandr",
            "-lXinerama",
            "-lXi",
            "-lXcursor",
            "-lpthread",
            "-ldl",
        ],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "glfw_headers",
    hdrs = glob(["include/GLFW/*.h"]),
    includes = ["include"],
)
