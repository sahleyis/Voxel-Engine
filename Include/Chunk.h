#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "FastNoiselite.h"
#include "Shader.h"
#include <atomic>
#include <algorithm>

const int CHUNK_WIDTH{ 32 };
const int CHUNK_HEIGHT{ 256 };
const int CHUNK_DEPTH{ 32 };

enum BlockType : uint8_t {
    BLOCK_AIR = 0,
    BLOCK_DIRT = 1,
    BLOCK_STONE = 2,
    BLOCK_GRASS = 3,
};

struct TerrainSettings {
    bool enableCaves{ false };
    float surfaceFrequency{ 0.015f };
    float caveFrequency{ 0.025f };
    float caveThreshold{ 0.45f };
    int caveOctaves{ 3 };
};

class Chunk {
private:
    int m_chunkX;
    int m_chunkY;
    int m_chunkZ;
    int m_maxHeight;

    std::vector<uint8_t> m_blocks;

    unsigned int m_VBO;
    unsigned int m_VAO;
    unsigned int m_EBO;

    std::vector<float> m_meshVertices;
    std::vector<unsigned int> m_meshIndices;

    std::atomic<bool> m_isDataReady{ false };
    std::atomic<bool> m_isBuffered{ false };

    unsigned int m_indexCount;
    bool m_isDirty;

    inline int getIndex(int x, int y, int z) const {
        return  x + (z * CHUNK_WIDTH) + (y * CHUNK_WIDTH * CHUNK_DEPTH);
    }

    inline bool isValidCoordinates(int x, int y, int z) const {
        return x >= 0 && x < CHUNK_WIDTH &&
            y >= 0 && y < CHUNK_HEIGHT &&
            z >= 0 && z < CHUNK_DEPTH;
    }

    void addFace(std::vector<float>& vertices, std::vector<unsigned int>& indices, float x, float y, float z, int faceType, float texID) {

        static constexpr float posOffsets[6][12] = {
            // 0: BACK (-Z)
            {  0.5f, -0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f },
            // 1: FRONT (+Z)
            { -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f },
            // 2: LEFT (-X)
            { -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f },
            // 3: RIGHT (+X)
            {  0.5f, -0.5f,  0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f },
            // 4: BOTTOM (-Y) -> The Corrected CCW Winding
            { -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f, -0.5f, -0.5f,  -0.5f, -0.5f, -0.5f },
            // 5: TOP (+Y)
            { -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f }
        };

        float uvs[8] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f
        };

        unsigned int baseIndex{ static_cast<unsigned int>(vertices.size() / 6) };

        for (int i{ 0 }; i < 4; i++) {
            vertices.push_back(x + posOffsets[faceType][i * 3 + 0]);
            vertices.push_back(y + posOffsets[faceType][i * 3 + 1]);
            vertices.push_back(z + posOffsets[faceType][i * 3 + 2]);
            vertices.push_back(uvs[i * 2 + 0]);
            vertices.push_back(uvs[i * 2 + 1]);
            vertices.push_back(texID);
        }

        indices.push_back(baseIndex + 0); indices.push_back(baseIndex + 1); indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 2); indices.push_back(baseIndex + 3); indices.push_back(baseIndex + 0);
    }

public:

    Chunk(int chunkX, int chunkZ, int chunkY = 1) : m_chunkX{ chunkX }, m_chunkZ{ chunkZ }, m_chunkY{ chunkY }, m_isDirty{ true }, m_indexCount{ 0 }, m_maxHeight{ 0 } {
        m_blocks.resize(static_cast<size_t>(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH), BLOCK_AIR);

        m_VAO = 0;
        m_VBO = 0;
        m_EBO = 0;
    }

    ~Chunk() {
        if (m_VBO != 0) {
            glDeleteBuffers(1, &m_VBO);
        }
        if (m_EBO != 0) {
            glDeleteBuffers(1, &m_EBO);
        }
        if (m_VAO != 0) {
            glDeleteVertexArrays(1, &m_VAO);
        }
    }

    uint8_t getBlock(int x, int y, int z) const {
        if (!isValidCoordinates(x, y, z)) {
            return BLOCK_AIR;
        }
        return m_blocks[static_cast<size_t>(getIndex(x, y, z))];
    }

    void setBlock(int x, int y, int z, uint8_t blockType) {
        if (isValidCoordinates(x, y, z)) {
            m_blocks[static_cast<size_t>(getIndex(x, y, z))] = blockType;
            m_isDirty = true;
        }
    }

    int getChunkX() const { return m_chunkX; }
    int getChunkY() const { return m_chunkY; }
    int getChunkZ() const { return m_chunkZ; }
    bool isDirty() const { return m_isDirty; }

    void buildMeshData() {
        m_meshVertices.clear();
        m_meshIndices.clear();
        m_meshVertices.reserve(24000);
        m_meshIndices.reserve(36000);

        for (int y{ 0 }; y <= m_maxHeight; y++) {
            for (int z{ 0 }; z < CHUNK_DEPTH; z++) {
                for (int x{ 0 }; x < CHUNK_WIDTH; x++) {

                    uint8_t blockType{ getBlock(x,y,z) };
                    if (blockType == BLOCK_AIR) continue;

                    float texID = (blockType == BLOCK_DIRT || blockType == BLOCK_GRASS) ? 0.0f : 1.0f;

                    if (getBlock(x, y, z - 1) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0, texID);
                    if (getBlock(x, y, z + 1) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1, texID);
                    if (getBlock(x - 1, y, z) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 2, texID);
                    if (getBlock(x + 1, y, z) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 3, texID);
                    if (getBlock(x, y - 1, z) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 4, texID);
                    if (getBlock(x, y + 1, z) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 5, texID);
                }
            }
        }

        m_indexCount = static_cast<unsigned int>(m_meshIndices.size());
        m_isDataReady = true;
    }

    void uploadToGPU() {
        if (!m_isDataReady || m_isBuffered) return;

        if (m_VAO == 0) {
            glGenVertexArrays(1, &m_VAO);
            glGenBuffers(1, &m_VBO);
            glGenBuffers(1, &m_EBO);
        }

        glBindVertexArray(m_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(m_meshVertices.size() * sizeof(float)), m_meshVertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizei>(m_meshIndices.size() * sizeof(unsigned int)), m_meshIndices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        m_meshVertices.clear();
        m_meshVertices.shrink_to_fit();
        m_meshIndices.clear();
        m_meshIndices.shrink_to_fit();

        m_isBuffered = true;
        m_isDirty = false;
    }

    void generateTerrain(FastNoiseLite surfaceNoise, TerrainSettings settings) {

        surfaceNoise.SetFrequency(settings.surfaceFrequency);
        surfaceNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        surfaceNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        surfaceNoise.SetFractalOctaves(4);

        FastNoiseLite caveNoise;
        caveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        caveNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
        caveNoise.SetFrequency(settings.caveFrequency);
        caveNoise.SetFractalOctaves(settings.caveOctaves);

        int currentHighestBlock{ 0 };

        for (int x{ 0 }; x < CHUNK_WIDTH; x++) {
            for (int z{ 0 }; z < CHUNK_DEPTH; z++) {

                float globalX{ static_cast<float>(x + (m_chunkX * CHUNK_WIDTH)) };
                float globalZ{ static_cast<float>(z + (m_chunkZ * CHUNK_DEPTH)) };

                float noiseVal = surfaceNoise.GetNoise(globalX, globalZ);
                int baseHeight = static_cast<int>(60.0f + (noiseVal * 35.0f));

                if (baseHeight > currentHighestBlock) {
                    currentHighestBlock = baseHeight;
                }

                int randomJitter = ((x * 37 + z * 13 + m_chunkX * 101 + m_chunkZ * 17) % 5 + 5) % 5;

                for (int y{ 0 }; y < CHUNK_HEIGHT; y++) {

                    if (y > baseHeight) {
                        setBlock(x, y, z, BLOCK_AIR);
                        continue;
                    }

                    if (settings.enableCaves) {
                        float globalY{ static_cast<float>(y) };
                        float caveVal{ caveNoise.GetNoise(globalX, globalY, globalZ) };
                        float depthFactor = (static_cast<float>(baseHeight - y) / 60.0f);
                        if (depthFactor > 1.0f) depthFactor = 1.0f;

                        float dynamicThreshold = settings.caveThreshold / (depthFactor + 0.1f);

                        if (caveVal > dynamicThreshold && y < baseHeight - 2) {
                            setBlock(x, y, z, BLOCK_AIR);
                            continue;
                        }
                    }

                    if (y == baseHeight) {
                        if (baseHeight > 66 + randomJitter) {
                            if ((x * 11 + z * 17) % 10 > 6) setBlock(x, y, z, BLOCK_DIRT);
                            else setBlock(x, y, z, BLOCK_STONE);
                        }
                        else {
                            setBlock(x, y, z, BLOCK_DIRT);
                        }
                    }
                    else if (y >= baseHeight - 3) {
                        if (baseHeight > 66 + randomJitter) {
                            setBlock(x, y, z, BLOCK_STONE);
                        }
                        else {
                            setBlock(x, y, z, BLOCK_DIRT);
                        }
                    }
                    else {
                        setBlock(x, y, z, BLOCK_STONE);
                    }
                }
            }
        }
        m_maxHeight = std::min(currentHighestBlock + 1, CHUNK_HEIGHT - 1);
    }

    void render(Shader& shader) {
        if (m_isDirty && m_isDataReady) {
            uploadToGPU();
        }

        if (!m_isBuffered) return;

        glm::mat4 model{ glm::mat4(1.0f) };
        model = glm::translate(model, glm::vec3(m_chunkX * CHUNK_WIDTH, 0.0f, m_chunkZ * CHUNK_DEPTH));

        shader.setMat4("model", model);

        glBindVertexArray(m_VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};