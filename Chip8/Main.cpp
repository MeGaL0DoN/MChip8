#define MINIAUDIO_IMPLEMENTATION

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "nfd/nfd.hpp"

#include <array>
#include <map>
#include <sstream>
#include <iostream>   
#include <filesystem>

#include "Shader.h"
#include "ChipCore.h"

ChipCore chipCore {};
bool pause { false };
bool pixelBorders { false };

int menuBarHeight;
GLFWwindow* window;

Shader pixelShader;
unsigned int VBO;

float pixel_XGap, pixel_YGap;
float widthUnit, heightUnit;
int viewport_width, viewport_height;

const std::wstring defaultPath { std::filesystem::current_path().wstring()};
const nfdnfilteritem_t filterItem[2] = { {L"ROM File", L"ch8,bin"} };

void draw() {
    for (int x = 0; x < ChipCore::SCRWidth; x++)
    {
        for (int y = 0; y < ChipCore::SCRHeight; y++)
        {
            if (chipCore.getPixel(x, y))
            {
                pixelShader.setFloat2("offset", x * (widthUnit + pixel_XGap), y * (heightUnit + pixel_YGap));
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }
    }
}

std::array<float, 12> getVertices()
{
    pixel_XGap = pixelBorders ? 2.0f / viewport_width : 0; 
    pixel_YGap = pixelBorders ? 2.0f / viewport_height : 0;

    widthUnit = 2.0f / (ChipCore::SCRWidth) - pixel_XGap;
    heightUnit = 2.0f / (ChipCore::SCRHeight) - pixel_YGap;

    return {
        -1.0f + widthUnit, 1.0f, 0.0f,  // top right
        -1.0f + widthUnit, 1.0f - heightUnit, 0.0f,  // bottom right
        -1.0f, 1.0f - heightUnit, 0.0f,  // bottom left
        -1.0f, 1.0f, 0.0f   // top left 
    };
}

void updateVertices()
{
    auto vertices = getVertices();
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
}

void setBuffers()
{
    unsigned int VAO, EBO;
    constexpr unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    auto vertices = getVertices();
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

std::wstring currentROMPAth {};
void loadROM(const wchar_t* path)
{
    chipCore.loadROM(path);
    pause = false;
    currentROMPAth = path;
}

void renderImGUI()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) 
    {
        if (ImGui::BeginMenu("File")) 
        {
            if (ImGui::MenuItem("Load ROM")) 
            {
                NFD::UniquePathN outPath;
                nfdresult_t result = NFD::OpenDialog(outPath, filterItem, 1, defaultPath.c_str());

                if (result == NFD_OKAY) 
                    loadROM(outPath.get());
            }
            else if (ImGui::MenuItem("Reload ROM", "(Esc)"))
                loadROM(currentROMPAth.c_str());

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings", "Ctrl+Q"))
        {
            static bool showForegroundPicker { false };
            static bool showBackgroundPicker { false };

            ImGui::SliderInt("CPU Frequency", &chipCore.CPUfrequency, 60, 1200);
            ImGui::Separator();
            ImGui::Checkbox("Enable Sound", &chipCore.enableSound);
            ImGui::SeparatorText("UI");

            if (ImGui::Checkbox("Pixel Borders", &pixelBorders))
                updateVertices();

            static ImVec4 foregroundColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            static ImVec4 backgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Foreground Color");
            ImGui::SameLine();

            if (ImGui::ArrowButton("foregroundPicker", ImGuiDir_Down))
            {
                showForegroundPicker = !showForegroundPicker;
                showBackgroundPicker = false;
            }

            if (showForegroundPicker)
            {
                if (ImGui::ColorPicker3("Pick a Color", (float*)&foregroundColor))
                    pixelShader.setFloat4("color", (float*)&foregroundColor);
            }

            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Background Color");
            ImGui::SameLine();

            if (ImGui::ArrowButton("backgroundPicker", ImGuiDir_Down))
            {
                showBackgroundPicker = !showBackgroundPicker;
                showForegroundPicker = false;
            }

            if (showBackgroundPicker)
            {
                if (ImGui::ColorPicker3("Pick a Color", (float*)&backgroundColor))
                    glClearColor(backgroundColor.x, backgroundColor.y, backgroundColor.z, 1.0f);
            }

            ImGui::SeparatorText("Misc.");
            if (ImGui::Button("Reset to Default"))
            {
                foregroundColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                backgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
                pixelBorders = false;

                pixelShader.setFloat4("color", (float*)&foregroundColor);
                glClearColor(backgroundColor.x, backgroundColor.y, backgroundColor.z, 1.0f);
                updateVertices();

                chipCore.CPUfrequency = 500;
                chipCore.enableSound = true;
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Quirks"))
        {
            ImGui::Checkbox("VFReset", &Quirks::VFReset);
            ImGui::Checkbox("Shifting", &Quirks::Shifting);
            ImGui::Checkbox("Jumping", &Quirks::Jumping);
            ImGui::Checkbox("Clipping", &Quirks::Clipping);
            ImGui::Checkbox("Memory Increment", &Quirks::MemoryIncrement);

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Reset to Default")) Quirks::Reset();
            ImGui::EndMenu();
        }
        if (pause)
        {
            ImGui::Separator();
            ImGui::Text("Paused");
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);
    draw();
    renderImGUI();
    glfwSwapBuffers(window);
}

std::map<int, uint8_t> keyConfig{};
void loadKeyConfig()
{
    std::ifstream configFile("data/keyConfig.ini");
    std::string line;

    while (std::getline(configFile, line)) {
        std::stringstream ss(line);
        char chipKey;
        int bindValue;

        ss >> chipKey;
        ss.ignore(1);
        ss >> bindValue;

        keyConfig[bindValue] = std::stoi(std::string{ chipKey }, 0, 16);
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == 1)
    {
        if (key == GLFW_KEY_ESCAPE)
        {
            loadROM(currentROMPAth.c_str());
            return;
        }
        if (key == GLFW_KEY_TAB)
        {
            pause = !pause;
            return;
        }
    }

    auto keyInd = keyConfig.find(scancode);

    if (keyInd != keyConfig.end())
        chipCore.setKey(keyInd->second, action);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    viewport_width = width; viewport_height = height - menuBarHeight;
    glViewport(0, 0, viewport_width, viewport_height);
    updateVertices();
    render();
}

void setWindowSize()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::BeginMainMenuBar();
    menuBarHeight = static_cast<int>(ImGui::GetWindowSize().y);
    ImGui::EndMainMenuBar();
    ImGui::Render();

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    viewport_width = { static_cast<int>(mode->width * 0.65f) };
    viewport_height = viewport_width / 2;

    glfwSetWindowSize(window, viewport_width, viewport_height + menuBarHeight);
    glfwSetWindowAspectRatio(window, viewport_width, viewport_height + menuBarHeight);
    glViewport(0, 0, viewport_width, viewport_height);
}

bool setGLFW()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1, 1, "Chip8", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    return true;
}

void setImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); 
    io.IniFilename = "data/imgui.ini";

    const int resolutionX = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
    io.Fonts->AddFontFromFileTTF("data/roboto.ttf", (resolutionX / 1920) * 17);

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

int main() {
    if (!setGLFW()) return -1;
    setImGUI();
    setWindowSize();
    setBuffers();

    pixelShader = Shader("data/Shaders/vertexShader.glsl", "data/Shaders/fragmentShader.glsl");
    pixelShader.use();

    float whiteColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pixelShader.setFloat4("color", whiteColor);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    NFD::Guard nfdGuard;
    loadKeyConfig();
    loadROM(L"ROMs/chipLogo.ch8");

    double lastFrameTime = glfwGetTime();
    double cpuFrequencyTimer{};
    double renderTimer{};

    while (!glfwWindowShouldClose(window)) 
    {
        double currentFrameTime = glfwGetTime();
        double deltaTime = currentFrameTime - lastFrameTime;

        cpuFrequencyTimer += deltaTime;
        renderTimer += deltaTime;

        if (cpuFrequencyTimer >= 1.0 / chipCore.CPUfrequency)
        {
            if (!pause) chipCore.emulateCycle();
            cpuFrequencyTimer = 0;
        }
        if (renderTimer >= 1.0 / 60)
        {
            renderTimer = 0;
            if (!pause) chipCore.updateTimers();

            glfwPollEvents();
            render();
        }

        lastFrameTime = currentFrameTime;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();

	return 0;
}