#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push,0)
#include "stb_image.h"
#pragma warning(pop)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Shader.h"
#include "Camera.h"
#include "Chunk.h"
#include "FastNoiselite.h"
#include <iostream>
#include <string>
#include <string_view>
#include <map>
#include <cmath>
#include <future>
#include <chrono>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef _WIN32 
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement{ 0x00000001 };
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

TerrainSettings engineSettings;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void calculateFrameRate(GLFWwindow* window);
void terrainGeneration(int& renderDistance, int& currentChunkX, int& currentChunkZ, FastNoiseLite& noise, TerrainSettings engineSettings, std::map<std::pair<int, int>, Chunk*>& activeChunks, std::map<std::pair<int, int>, std::future<Chunk*>>& loadingChunks);
void UnloadChunks(int renderDistance, int currentChunkX, int currentChunkZ, std::map<std::pair<int, int>, Chunk*>& activeChunks);

const unsigned int SCR_WIDTH{ 1200 };
const unsigned int SCR_HEIGHT{ 720 };

glm::vec3 worldOrigin{ 0.0f, 0.0f, 3.0f };
Camera camera(worldOrigin);

bool firstMouse{ true };
double lastX{ 400 };
double lastY{ 300 };

float deltaTime{ 0.0f };
float lastFrame{ 0.0f };

double lastTime{ glfwGetTime() };
int frameCount{ 0 };

bool cursorCaptured{ true };
bool shiftWasPressed{ false };

unsigned int loadTexture(char const* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format{ static_cast<GLenum>((nrChannels == 4) ? GL_RGBA : GL_RGB) };
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format), width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        stbi_image_free(data);
    }
    else {
        std::cout << "\n\nCRITICAL ERROR: FAILED TO LOAD TEXTURE: " << path << "\n\n";
    }

    return textureID;
}

int main()
{
    using glm::vec3;
    using glm::mat4;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Voxel Engine", NULL, NULL);
    if (window == NULL)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        return -1;
    }

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    Shader ourShader("shaders/6.1.coordinate_systems.vs", "shaders/6.1.coordinate_systems.fs");

    unsigned int texDirt = loadTexture("assets/dirt.png");
    unsigned int texStone = loadTexture("assets/stone.png");

    ourShader.use();
    ourShader.setInt("texDirt", 0);
    ourShader.setInt("texStone", 1);

    std::map<std::pair<int, int>, Chunk*> activeChunks;
    std::map<std::pair<int, int>, std::future<Chunk*>> loadingChunks;

    FastNoiseLite noise;

    int renderDistance{ 4 };

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        calculateFrameRate(window);

        int currentChunkX{ static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH)) };
        int currentChunkZ{ static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH)) };

        terrainGeneration(renderDistance, currentChunkX, currentChunkZ, noise, engineSettings, activeChunks, loadingChunks);
        UnloadChunks(renderDistance, currentChunkX, currentChunkZ, activeChunks);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texDirt);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texStone);

        ourShader.use();
        glm::mat4 projection{ glm::perspective(glm::radians(camera.Zoom), static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT), 0.1f, 1000.0f) };
        glm::mat4 view{ camera.GetViewMatrix() };

        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        for (auto const& [coord, chunk] : activeChunks) {
            chunk->render(ourShader);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Engine Diagnostics & Generation");
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Active Chunks: %zu", activeChunks.size());
        ImGui::Separator();

        ImGui::Checkbox("Enable Caves", &engineSettings.enableCaves);
        ImGui::SliderFloat("Surface Freq", &engineSettings.surfaceFrequency, 0.001f, 0.05f);
        ImGui::SliderFloat("Cave Freq", &engineSettings.caveFrequency, 0.005f, 0.1f);
        ImGui::SliderFloat("Cave Threshold", &engineSettings.caveThreshold, 0.1f, 0.8f);
        ImGui::SliderInt("Cave Complexity", &engineSettings.caveOctaves, 1, 5);

        if (ImGui::Button("Regenerate World", ImVec2(150, 30))) {
            for (auto const& [coord, chunk] : activeChunks) {
                delete chunk;
            }
            activeChunks.clear();
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    bool shiftIsPressed{ glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS };

    if (shiftIsPressed and !shiftWasPressed) {
        cursorCaptured = !cursorCaptured;
        if (cursorCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    shiftWasPressed = shiftIsPressed;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.ProcessKeyboard(FORWARD, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.ProcessKeyboard(LEFT, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.ProcessKeyboard(RIGHT, deltaTime);
    }
}

void framebuffer_size_callback([[maybe_unused]] GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback([[maybe_unused]] GLFWwindow* window, double xpos, double ypos) {
    using glm::vec3;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xOffset{ static_cast<float>(xpos - lastX) };
    float yOffset{ static_cast<float>(lastY - ypos) };
    lastX = xpos;
    lastY = ypos;

    if (cursorCaptured) {
        camera.ProcessMouseMovement(xOffset, yOffset);
    }
    else {
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            camera.ProcessMouseMovement(xOffset, yOffset);
        }
        else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            float panSpeed = 0.05f;
            camera.Position -= camera.Right * (xOffset * panSpeed);
            camera.Position += camera.Up * (yOffset * panSpeed);
        }
    }
}

void scroll_callback([[maybe_unused]] GLFWwindow* window, [[maybe_unused]] double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void calculateFrameRate(GLFWwindow* window)
{
    double currentTime{ glfwGetTime() };
    frameCount++;

    if (currentTime - lastTime >= 1.0) {
        double fps = static_cast<double>(frameCount) / (currentTime - lastTime);
        std::string title = "Engine FPS: " + std::to_string(fps) + "FPS";
        glfwSetWindowTitle(window, title.c_str());

        frameCount = 0;
        lastTime = currentTime;
    }
}

void terrainGeneration(int& renderDistance, int& currentChunkX, int& currentChunkZ, FastNoiseLite& noise, TerrainSettings settings, std::map<std::pair<int, int>, Chunk*>& activeChunks, std::map<std::pair<int, int>, std::future<Chunk*>>& loadingChunks) {

    const size_t MaxConcurrentThreads{ 4 };
    const int MaxChunksPerFrame{ 1 };
    int chunksSpawnedThisFrame{ 0 };

    for (int x{ -renderDistance }; x <= renderDistance; x++) {
        for (int z{ -renderDistance }; z <= renderDistance; z++) {

            int targetChunkX{ currentChunkX + x };
            int targetChunkZ{ currentChunkZ + z };

            std::pair<int, int> chunkCoord{ targetChunkX, targetChunkZ };

            if (activeChunks.find(chunkCoord) == activeChunks.end() && loadingChunks.find(chunkCoord) == loadingChunks.end()) {

                if (loadingChunks.size() >= MaxConcurrentThreads || chunksSpawnedThisFrame >= MaxChunksPerFrame) {
                    goto POLL_THREADS;
                }

                chunksSpawnedThisFrame++;

                loadingChunks[chunkCoord] = std::async(std::launch::async, [targetChunkX, targetChunkZ, noise, settings]() {
                    Chunk* newChunk = new Chunk(targetChunkX, targetChunkZ);
                    newChunk->generateTerrain(noise, settings);
                    newChunk->buildMeshData();
                    return newChunk;
                    });
            }
        }
    }

POLL_THREADS:
    for (auto it = loadingChunks.begin(); it != loadingChunks.end();) {
        if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            Chunk* completedChunk = it->second.get();
            activeChunks[it->first] = completedChunk;
            it = loadingChunks.erase(it);
        }
        else {
            ++it;
        }
    }
}

void UnloadChunks(int renderDistance, int currentChunkX, int currentChunkZ, std::map<std::pair<int, int>, Chunk*>& activeChunks) {
    int unloadDistance = renderDistance + 1;

    for (auto it = activeChunks.begin(); it != activeChunks.end();) {

        std::pair<int, int> coord = it->first;
        Chunk* chunk{ it->second };

        int distanceX{ std::abs(coord.first - currentChunkX) };
        int distanceZ{ std::abs(coord.second - currentChunkZ) };

        if (distanceX > unloadDistance || distanceZ > unloadDistance) {
            delete chunk;
            it = activeChunks.erase(it);
        }
        else {
            ++it;
        }
    }
}