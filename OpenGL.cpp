// Librarys and Headers needed
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

// Making the program use the Discrete GPU
#ifdef _WIN32 
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement{ 0x00000001 };
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif


// Function Declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void calculateFrameRate(GLFWwindow* window);
void terrainGeneration(int& renderDistance, int& currentChunkX, int& currentChunkZ, FastNoiseLite& noise, std::map<std::pair<int, int>, Chunk*>& activeChunks);
void UnloadChunks(int renderDistance, int currentChunkX, int currentChunkZ, std::map<std::pair<int, int>, Chunk*>& activeChunks);

// Settings
const unsigned int SCR_WIDTH{ 1200 };
const unsigned int SCR_HEIGHT{ 720 };

// Camera Object initialisation
glm::vec3 worldOrigin{ 0.0f, 0.0f, 3.0f };
Camera camera(worldOrigin);

// Window settings
bool firstMouse{ true };
double lastX{ 400 };
double lastY{ 300 };

float deltaTime{ 0.0f };
float lastFrame{ 0.0f };

double lastTime{ glfwGetTime() };
int frameCount{ 0 };

int main()
{
   
    // Window Configuration and Initialisation
    using glm::vec3;
    using glm::mat4;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Window Creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
        
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Loading ALl OpenGL function Pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
    
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Enabling Depth testing 
    glEnable(GL_DEPTH_TEST);

    // Initialisation our Shader object
    Shader ourShader("6.1.coordinate_systems.vs", "6.1.coordinate_systems.fs");

    
    // Vertex Buffer and Array Objects
    // Element Bufer Objects
    


    // Creating The texture
    unsigned int DirtBlock;
    
    // The Texture
    glGenTextures(1, &DirtBlock);
    glBindTexture(GL_TEXTURE_2D, DirtBlock);
    // setting The Texture Wrapping Parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // setting The Texture Filtering Parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Loading the image, Creating the texture and generating the Mipmaps
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
    unsigned char* data = stbi_load("GrassBlock.jpg", &width, &height, &nrChannels, 0);
    if (data)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);
    
    
    // tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
    // -------------------------------------------------------------------------------------------
    ourShader.use();
    ourShader.setInt("DirtBlock", 0);
    //ourShader.setInt("texture2", 1);

    std::map<std::pair<int, int>, Chunk*> activeChunks;

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.02f);

    int renderDistance{ 4 };

    // Render Loop
    while (!glfwWindowShouldClose(window))
    {
        
        // Input and Camera
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Rendering
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        calculateFrameRate(window);

        int currentChunkX{ static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH)) };
        int currentChunkZ{ static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH)) };
        
        terrainGeneration(renderDistance, currentChunkX, currentChunkZ, noise, activeChunks);
        UnloadChunks(renderDistance, currentChunkX, currentChunkZ, activeChunks);
        // bind textures on corresponding texture units
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, DirtBlock);

        ourShader.use();
        glm::mat4 projection{ glm::perspective(glm::radians(camera.Zoom), static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT), 0.1f, 100.0f) };
        glm::mat4 view{ camera.GetViewMatrix()};

        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        for (auto const& [coord, chunk] : activeChunks) {
            chunk->render(ourShader);
        }
        // swapping buffers nad polling events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Terminating glfw
    glfwTerminate();
    return 0;
}


// Processing all Input Queries
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

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


// this function excecutes whenever the widow is resized
void framebuffer_size_callback([[maybe_unused]] GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
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

    camera.ProcessMouseMovement(xOffset, yOffset);
}

void scroll_callback([[maybe_unused]] GLFWwindow* window, [[maybe_unused]]double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void calculateFrameRate(GLFWwindow* window)
{
    double currentTime{ glfwGetTime() };
    frameCount++;

    if (currentTime - lastTime >= 1.0) {
        double fps = static_cast<double>(frameCount) / (currentTime - lastTime);
        [[maybe_unused]] double msPerFrame = 1000.0 / static_cast<double>(frameCount);

        std::string title = "Engine FPS: " + std::to_string(fps) + "FPS";
        glfwSetWindowTitle(window, title.c_str());

        frameCount = 0;
        lastTime = currentTime;
    }
}


void terrainGeneration(int& renderDistance, int& currentChunkX, int& currentChunkZ, FastNoiseLite& noise, std::map<std::pair<int, int>, Chunk*>& activeChunks) {
    for (int x{ -renderDistance }; x <= renderDistance; x++) {
        for (int z{ -renderDistance }; z <= renderDistance; z++) {

            int targetChunkX{ currentChunkX + x };
            int targetChunkZ{ currentChunkZ + z };

            std::pair<int, int> chunkCoord{ targetChunkX, targetChunkZ };

            if (activeChunks.find(chunkCoord) == activeChunks.end()) {
                Chunk* newChunk{ new Chunk(targetChunkX, targetChunkZ) };

                newChunk->generateTerrain(noise);
                activeChunks[chunkCoord] = newChunk;
            }
        }
    }
}

void UnloadChunks(int renderDistance, int currentChunkX, int currentChunkZ, std::map<std::pair<int, int>, Chunk*>& activeChunks) {
    int unloadDistance = renderDistance - 1;

    for (auto it = activeChunks.begin(); it != activeChunks.end();) {

        std::pair<int, int> coord = it->first;
        Chunk* chunk{ it->second };

        int distanceX{ std::abs(coord.first - currentChunkX) };
        int distanceZ{ std::abs(coord.second - currentChunkZ) };

        if (distanceX > unloadDistance and distanceZ > unloadDistance) {
            delete chunk;
            it = activeChunks.erase(it);
        }
        else {
            ++it;
        }
    }
}