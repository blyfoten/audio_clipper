"""Module extension for loading external dependencies."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _audio_clipper_dependencies_impl(ctx):
    """Implementation of the module extension."""
    
    # PortAudio
    http_archive(
        name = "portaudio",
        build_file = "@//third_party:portaudio.BUILD",
        url = "https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz",
        strip_prefix = "portaudio",
    )
    
    # ImGui - using v1.89.9 (a known stable version)
    http_archive(
        name = "imgui",
        build_file = "@//third_party:imgui.BUILD",
        url = "https://github.com/ocornut/imgui/archive/v1.89.9.tar.gz",
        strip_prefix = "imgui-1.89.9",
    )
    
    # GLFW
    http_archive(
        name = "glfw",
        build_file = "@//third_party:glfw.BUILD",
        url = "https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip",
        strip_prefix = "glfw-3.3.8",
    )
    
    # dr_libs (header-only audio libraries: dr_wav, dr_mp3, dr_flac)
    http_archive(
        name = "dr_libs",
        build_file = "@//third_party:dr_libs.BUILD",
        url = "https://github.com/mackron/dr_libs/archive/da35f9d6c7374a95353fd1df1d394d44ab66cf01.zip",
        strip_prefix = "dr_libs-da35f9d6c7374a95353fd1df1d394d44ab66cf01",
    )

audio_clipper_dependencies = module_extension(
    implementation = _audio_clipper_dependencies_impl,
)
