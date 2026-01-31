#include "audio_clipper.h"
#include <GLFW/glfw3.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GL/gl.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include <vector>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

// Global content scale (shared with AudioClipper)
float g_ContentScale = 1.0f;
static bool g_FontNeedsRebuild = false;

void windowContentScaleCallback(GLFWwindow* window, float xscale, float yscale) {
    (void)window;
    float newScale = (xscale > yscale) ? xscale : yscale;
    if (newScale != g_ContentScale) {
        g_ContentScale = newScale;
        g_FontNeedsRebuild = true;
    }
}

void setupImGuiStyle(float scale) {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Scale all sizes
    style.WindowPadding = ImVec2(8 * scale, 8 * scale);
    style.WindowRounding = 0.0f;
    style.FramePadding = ImVec2(5 * scale, 4 * scale);
    style.FrameRounding = 4 * scale;
    style.ItemSpacing = ImVec2(8 * scale, 6 * scale);
    style.ItemInnerSpacing = ImVec2(4 * scale, 4 * scale);
    style.IndentSpacing = 20 * scale;
    style.ScrollbarSize = 14 * scale;
    style.ScrollbarRounding = 9 * scale;
    style.GrabMinSize = 10 * scale;
    style.GrabRounding = 3 * scale;
    
    // Modern dark theme colors
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.16f, 0.17f, 0.98f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.23f, 0.24f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.29f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.22f, 0.23f, 0.24f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.29f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.60f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.70f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.60f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.40f, 0.60f, 0.80f, 0.35f);
}

void rebuildFonts(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    
    // Load default font at scaled size
    ImFontConfig config;
    config.SizePixels = 16.0f * scale;
    io.Fonts->AddFontDefault(&config);
    
    // Don't use FontGlobalScale since we're already scaling the font size
    io.FontGlobalScale = 1.0f;
    
    // Rebuild font atlas
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    io.Fonts->Build();
    ImGui_ImplOpenGL3_CreateFontsTexture();
}

// Create a simple waveform icon programmatically
void setWindowIcon(GLFWwindow* window) {
    const int size = 32;
    std::vector<unsigned char> pixels(size * size * 4);
    
    // Background color (dark)
    unsigned char bgR = 35, bgG = 39, bgB = 42, bgA = 255;
    // Waveform color (blue)
    unsigned char wfR = 52, wfG = 152, wfB = 219, wfA = 255;
    // Accent color (orange)
    unsigned char acR = 243, acG = 156, acB = 18, acA = 255;
    
    // Fill background
    for (int i = 0; i < size * size * 4; i += 4) {
        pixels[i + 0] = bgR;
        pixels[i + 1] = bgG;
        pixels[i + 2] = bgB;
        pixels[i + 3] = bgA;
    }
    
    // Draw waveform
    int centerY = size / 2;
    for (int x = 2; x < size - 2; x++) {
        // Create a sine-like waveform pattern
        float t = static_cast<float>(x) / size * 3.14159f * 3.0f;
        float amplitude = std::sin(t) * std::sin(t * 0.5f) * 0.7f;
        int waveY = centerY + static_cast<int>(amplitude * (size / 2 - 4));
        
        // Draw vertical line from center to wave point
        int y1 = std::min(centerY, waveY);
        int y2 = std::max(centerY, waveY);
        for (int y = y1; y <= y2; y++) {
            int idx = (y * size + x) * 4;
            pixels[idx + 0] = wfR;
            pixels[idx + 1] = wfG;
            pixels[idx + 2] = wfB;
            pixels[idx + 3] = wfA;
        }
    }
    
    // Draw center line
    for (int x = 2; x < size - 2; x++) {
        int idx = (centerY * size + x) * 4;
        pixels[idx + 0] = 80;
        pixels[idx + 1] = 80;
        pixels[idx + 2] = 80;
        pixels[idx + 3] = 255;
    }
    
    // Draw playhead (orange vertical line)
    int playheadX = size * 2 / 3;
    for (int y = 4; y < size - 4; y++) {
        int idx = (y * size + playheadX) * 4;
        pixels[idx + 0] = acR;
        pixels[idx + 1] = acG;
        pixels[idx + 2] = acB;
        pixels[idx + 3] = acA;
    }
    
    // Add rounded corners (set alpha to 0)
    int cornerRadius = 4;
    for (int y = 0; y < cornerRadius; y++) {
        for (int x = 0; x < cornerRadius; x++) {
            float dist = std::sqrt((float)(cornerRadius - x - 1) * (cornerRadius - x - 1) + 
                                   (float)(cornerRadius - y - 1) * (cornerRadius - y - 1));
            if (dist > cornerRadius - 0.5f) {
                // Top-left
                int idx = (y * size + x) * 4;
                pixels[idx + 3] = 0;
                // Top-right
                idx = (y * size + (size - 1 - x)) * 4;
                pixels[idx + 3] = 0;
                // Bottom-left
                idx = ((size - 1 - y) * size + x) * 4;
                pixels[idx + 3] = 0;
                // Bottom-right
                idx = ((size - 1 - y) * size + (size - 1 - x)) * 4;
                pixels[idx + 3] = 0;
            }
        }
    }
    
    GLFWimage icon;
    icon.width = size;
    icon.height = size;
    icon.pixels = pixels.data();
    
    glfwSetWindowIcon(window, 1, &icon);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
#ifdef _WIN32
    // Set DPI awareness on Windows
    SetProcessDPIAware();
#endif

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // Enable GLFW to handle scaling hints
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    // GL 3.0 + GLSL 130 (or GL 3.3 + GLSL 330)
    #ifdef __APPLE__
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    #endif

    // Create window with reasonable initial size
    GLFWwindow* window = glfwCreateWindow(1200, 800, "Audio Clipper", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    // Set custom window icon
    setWindowIcon(window);

    // Get content scale for the window
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    g_ContentScale = (xscale > yscale) ? xscale : yscale;
    
    // Set callback for scale changes (when moving between monitors)
    glfwSetWindowContentScaleCallback(window, windowContentScaleCallback);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style with scaling
    setupImGuiStyle(g_ContentScale);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Build fonts with initial scale
    rebuildFonts(g_ContentScale);

    // Create application
    AudioClipper app;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Check if fonts need to be rebuilt due to scale change
        if (g_FontNeedsRebuild) {
            rebuildFonts(g_ContentScale);
            setupImGuiStyle(g_ContentScale);
            g_FontNeedsRebuild = false;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render UI
        app.render();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.13f, 0.14f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
