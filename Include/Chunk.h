#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "FastNoiselite.h"
#include "Shader.h"
#include <atomic>

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

	void addFace(std::vector<float>& vertices, std::vector<unsigned int>& indices, float x, float y, float z, int faceType, float texX, float texY) {

		// Only storing Local XYZ now. U and V are calculated dynamically.
		static constexpr float posOffsets[6][12] = {
			// 0: BACK (-Z) 
			{  0.5f, -0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f },
			// 1: FRONT (+Z) 
			{ -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f },
			// 2: LEFT (-X) 
			{ -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f },
			// 3: RIGHT (+X) 
			{  0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f },
			// 4: BOTTOM (-Y) 
			{ -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f },
			// 5: TOP (+Y) 
			{ -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f }
		};

		const float atlasColumns{ 9.0f }; // Number of blocks left to right
		const float atlasRows{ 11.0f };   // Number of blocks top to bottom

		const float texStepX{ 1.0f / atlasColumns };
		const float texStepY{ 1.0f / atlasRows };

		float uMin = texX * texStepX;
		float uMax = uMin + texStepX;

		// Inverting Y so that texY=0 is the top row of the image
		float vMax = 1.0f - (texY * texStepY);
		float vMin = vMax - texStepY;

		float uvs[8] = {
			uMin, vMin,
			uMax, vMin,
			uMax, vMax,
			uMin, vMax
		};

		unsigned int baseIndex{ static_cast<unsigned int>(vertices.size() / 5) };

		for (int i{ 0 }; i < 4; i++) {
			vertices.push_back(x + posOffsets[faceType][i * 3 + 0]); // X
			vertices.push_back(y + posOffsets[faceType][i * 3 + 1]); // Y
			vertices.push_back(z + posOffsets[faceType][i * 3 + 2]); // Z
			vertices.push_back(uvs[i * 2 + 0]);                      // U
			vertices.push_back(uvs[i * 2 + 1]);                      // V
		}

		indices.push_back(baseIndex + 0); indices.push_back(baseIndex + 1); indices.push_back(baseIndex + 2);
		indices.push_back(baseIndex + 2); indices.push_back(baseIndex + 3); indices.push_back(baseIndex + 0);
	}


 
public:

	Chunk(int chunkX, int chunkZ, int chunkY = 1) : m_chunkX{ chunkX }, m_chunkZ{ chunkZ }, m_chunkY{ chunkY }, m_maxHeight{ 0 }, m_isDirty { true }, m_indexCount{ 0 } {
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

	int getChunkX() const {
		return m_chunkX;
	}

	int getChunkY() const {
		return m_chunkY;
	}

	int getChunkZ() const {
		return m_chunkZ;
	}

	bool isDirty() const {
		return m_isDirty;
	}

	void buildMeshData() {
		m_meshVertices.clear();
		m_meshIndices.clear();
		m_meshVertices.reserve(24000);
		m_meshIndices.reserve(36000);

		for (int y{ 0 }; y < m_maxHeight; y++) {
			for (int z{ 0 }; z < CHUNK_DEPTH; z++) {
				for (int x{ 0 }; x < CHUNK_WIDTH; x++) {

					uint8_t blockType{ getBlock(x,y,z) };

					// Setup isolated texture coordinates for different faces of the same block
					float texX = 0.0f, texY = 0.0f;             // Default / Sides
					float texTopX = 0.0f, texTopY = 0.0f;       // Top Face
					float texBottomX = 0.0f, texBottomY = 0.0f; // Bottom Face

					switch (blockType) {
					case BLOCK_GRASS:
						texX = 5.0f; texY = 7.0f;             // SIDE (Grass overlapping dirt)
						texTopX = 5.0f; texTopY = 8.0f;       // TOP (Pure Grass)
						texBottomX = 5.0f; texBottomY = 9.0f; // BOTTOM (Pure Dirt)
						break;
					case BLOCK_DIRT:
						texX = 5.0f; texY = 9.0f;
						texTopX = texX; texTopY = texY;
						texBottomX = texX; texBottomY = texY;
						break;
					case BLOCK_STONE:
						texX = 5.0f; texY = 2.0f;
						texTopX = texX; texTopY = texY;
						texBottomX = texX; texBottomY = texY;
						break;
					}

					// Notice how the last arguments explicitly pass the Top/Bottom variables now
					if (getBlock(x, y, z - 1) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0, texX, texY);
					if (getBlock(x, y, z + 1) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1, texX, texY);
					if (getBlock(x - 1, y, z) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 2, texX, texY);
					if (getBlock(x + 1, y, z) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 3, texX, texY);
					if (getBlock(x, y - 1, z) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 4, texBottomX, texBottomY);
					if (getBlock(x, y + 1, z) == BLOCK_AIR) addFace(m_meshVertices, m_meshIndices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 5, texTopX, texTopY);
				}
			}
		}

		m_indexCount =  static_cast<unsigned int>(m_meshIndices.size()) ;
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
		// Note: Changed sizeof(float) to sizeof(unsigned int) here for indices!
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizei>(m_meshIndices.size() * sizeof(unsigned int)), m_meshIndices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);

		// Clear system memory since it's safely on the GPU now
		m_meshVertices.clear();
		m_meshVertices.shrink_to_fit();
		m_meshIndices.clear();
		m_meshIndices.shrink_to_fit();

		m_isBuffered = true;
		m_isDirty = false;
	}

	// Add TerrainSettings as a parameter
	void generateTerrain(FastNoiseLite surfaceNoise, TerrainSettings settings) {

		surfaceNoise.SetFrequency(settings.surfaceFrequency);

		// Setup a dedicated Fractal Noise generator for caves
		FastNoiseLite caveNoise;
		caveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
		caveNoise.SetFractalType(FastNoiseLite::FractalType_Ridged); // Ridged makes distinct tunnel-like structures
		caveNoise.SetFrequency(settings.caveFrequency);
		caveNoise.SetFractalOctaves(settings.caveOctaves);

		int currentHighestBlock{ 0 };

		for (int x{ 0 }; x < CHUNK_WIDTH; x++) {
			for (int z{ 0 }; z < CHUNK_DEPTH; z++) {

				float globalX{ static_cast<float>(x + (m_chunkX * CHUNK_WIDTH)) };
				float globalZ{ static_cast<float>(z + (m_chunkZ * CHUNK_DEPTH)) };

				int baseHeight = static_cast<int>((surfaceNoise.GetNoise(globalX, globalZ) + 1.0f) * 0.5f * 20.0f + 60.0f);

				if (baseHeight > currentHighestBlock) {
					currentHighestBlock = baseHeight;
				}

				for (int y{ 0 }; y < CHUNK_HEIGHT; y++) {

					if (y > baseHeight) {
						setBlock(x, y, z, BLOCK_AIR);
						continue;
					}

					float globalY{ static_cast<float>(y) };
					float caveVal{ caveNoise.GetNoise(globalX, globalY, globalZ) };

					// Fractal noise returns -1 to 1. 
					// We carve air if the noise is ABOVE the threshold.
					// We multiply the threshold by a depth factor so caves get smaller near the surface
					float depthFactor = (static_cast<float>(baseHeight - y) / 60.0f);
					if (depthFactor > 1.0f) depthFactor = 1.0f; // Cap it underground

					float dynamicThreshold = settings.caveThreshold / (depthFactor + 0.1f);

					if (caveVal > dynamicThreshold && y < baseHeight - 2) {
						setBlock(x, y, z, BLOCK_AIR);
					}
					else {
						if (y == baseHeight) setBlock(x, y, z, BLOCK_GRASS);
						else if (y > baseHeight - 4) setBlock(x, y, z, BLOCK_DIRT);
						else setBlock(x, y, z, BLOCK_STONE);
					}
				}
			}
		}
		m_maxHeight = std::min(currentHighestBlock + 1, CHUNK_HEIGHT - 1);
	}

	void render(Shader& shader) {

		// If the block data changed, and the background thread finished generating the mesh, send it to the GPU
		if (m_isDirty && m_isDataReady) {
			uploadToGPU();
		}

		// Don't try to draw if the GPU doesn't have the data yet
		if (!m_isBuffered) return;

		glm::mat4 model{ glm::mat4(1.0f) };
		model = glm::translate(model, glm::vec3(m_chunkX * CHUNK_WIDTH, 0.0f, m_chunkZ * CHUNK_DEPTH));

		shader.setMat4("model", model);

		glBindVertexArray(m_VAO);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}


};
