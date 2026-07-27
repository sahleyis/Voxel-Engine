#pragma once
#include <vector>
#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "FastNoiselite.h"

const int CHUNK_WIDTH{ 32 };
const int CHUNK_HEIGHT{ 256 };
const int CHUNK_DEPTH{ 32 };

enum BlockType : uint8_t {
	BLOCK_AIR = 0,
	BLOCK_DIRT = 1,
	BLOCK_STONE = 2,
	BLOCK_GRASS = 3,
};	

class Chunk { 
private:
	int m_chunkX;
	int m_chunkZ;

	std::vector<uint8_t> m_blocks;

	unsigned int m_VBO;
	unsigned int m_VAO;
	unsigned int m_EBO;

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

	void addFace(std::vector<float>& vertices, float x, float y, float z, int faceType) {
		// A simple face definition for a 1x1x1 block
		// Each row is: localX, localY, localZ, U, V
		float faces[6][30] = {
			// BACK FACE (-Z)
			{ x + 0.5f, y - 0.5f, z - 0.5f, 0.0f, 0.0f,
			  x - 0.5f, y - 0.5f, z - 0.5f, 1.0f, 0.0f,
			  x - 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f,
			  x - 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f,
			  x + 0.5f, y + 0.5f, z - 0.5f, 0.0f, 1.0f,
			  x + 0.5f, y - 0.5f, z - 0.5f, 0.0f, 0.0f },
			  // FRONT FACE (+Z)
			  { x - 0.5f, y - 0.5f, z + 0.5f, 0.0f, 0.0f,
				x + 0.5f, y - 0.5f, z + 0.5f, 1.0f, 0.0f,
				x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 1.0f,
				x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 1.0f,
				x - 0.5f, y + 0.5f, z + 0.5f, 0.0f, 1.0f,
				x - 0.5f, y - 0.5f, z + 0.5f, 0.0f, 0.0f },
				// LEFT FACE (-X)
				{ x - 0.5f, y + 0.5f, z + 0.5f, 1.0f, 0.0f,
				  x - 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f,
				  x - 0.5f, y - 0.5f, z - 0.5f, 0.0f, 1.0f,
				  x - 0.5f, y - 0.5f, z - 0.5f, 0.0f, 1.0f,
				  x - 0.5f, y - 0.5f, z + 0.5f, 0.0f, 0.0f,
				  x - 0.5f, y + 0.5f, z + 0.5f, 1.0f, 0.0f },
				  // RIGHT FACE (+X)
				  { x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 0.0f,
					x + 0.5f, y - 0.5f, z + 0.5f, 0.0f, 0.0f,
					x + 0.5f, y - 0.5f, z - 0.5f, 0.0f, 1.0f,
					x + 0.5f, y - 0.5f, z - 0.5f, 0.0f, 1.0f,
					x + 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f,
					x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 0.0f },
					// BOTTOM FACE (-Y)
					{ x - 0.5f, y - 0.5f, z - 0.5f, 0.0f, 1.0f,
					  x + 0.5f, y - 0.5f, z - 0.5f, 1.0f, 1.0f,
					  x + 0.5f, y - 0.5f, z + 0.5f, 1.0f, 0.0f,
					  x + 0.5f, y - 0.5f, z + 0.5f, 1.0f, 0.0f,
					  x - 0.5f, y - 0.5f, z + 0.5f, 0.0f, 0.0f,
					  x - 0.5f, y - 0.5f, z - 0.5f, 0.0f, 1.0f },
					  // TOP FACE (+Y)
					  { x - 0.5f, y + 0.5f, z - 0.5f, 0.0f, 1.0f,
						x - 0.5f, y + 0.5f, z + 0.5f, 0.0f, 0.0f,
						x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 0.0f,
						x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 0.0f,
						x + 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f,
						x - 0.5f, y + 0.5f, z - 0.5f, 0.0f, 1.0f }
		};

		// Insert the 30 floats (6 vertices * 5 floats) for the requested face
		for (int i{ 0 }; i<30; i++) {
			vertices.push_back(faces[faceType][i]);
		}
	}
 
public:

	Chunk(int chunkX, int chunkZ) : m_chunkX{ chunkX }, m_chunkZ{ chunkZ }, m_isDirty{ true }, m_indexCount{ 0 } {
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

	int getChunkZ() const {
		return m_chunkZ;
	}

	bool isDirty() const {
		return m_isDirty;
	}

	void generateMesh() {
		std::vector<float> meshVertices;

		for (int y{ 0 }; y < CHUNK_HEIGHT; y++) {
			for (int z{ 0 }; z < CHUNK_DEPTH; z++) {
				for (int x{ 0 }; x < CHUNK_WIDTH; x++) {

					if (getBlock(x, y, z) == BLOCK_AIR)  continue;

					if (getBlock(x, y, z - 1) == BLOCK_AIR) addFace(meshVertices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0);
					if (getBlock(x, y, z + 1) == BLOCK_AIR) addFace(meshVertices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1);
					if (getBlock(x - 1, y, z) == BLOCK_AIR) addFace(meshVertices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 2);
					if (getBlock(x + 1, y, z) == BLOCK_AIR) addFace(meshVertices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 3);
					if (getBlock(x, y - 1, z) == BLOCK_AIR) addFace(meshVertices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 4);
					if (getBlock(x, y + 1, z) == BLOCK_AIR) addFace(meshVertices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 5);
				}
			}
		}

		if (m_VAO == 0) {
			glGenVertexArrays(1, &m_VAO);
			glGenBuffers(1, &m_VBO);
		}

		glBindVertexArray(m_VAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(meshVertices.size() * sizeof(float)), meshVertices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		m_indexCount = static_cast<unsigned int>(meshVertices.size() / 5);
		m_isDirty = false;
	}

	void generateTerrain(FastNoiseLite& noise) {
		for (int x{ 0 }; x < CHUNK_WIDTH; x++) {
			for (int z{ 0 }; z < CHUNK_DEPTH; z++) {

				float globalX{ static_cast<float>(x + (m_chunkX * CHUNK_WIDTH)) };
				float globalZ{ static_cast<float>(z + (m_chunkZ * CHUNK_DEPTH)) };

				float noiseVal{ noise.GetNoise(globalX, globalZ) };

				int range{ static_cast<int>(((noiseVal + 1.0f) / 2.0f) * 30.0f) };
				int terrainHeight{ 10 + range };

				for (int y{ 0 }; y <= terrainHeight; y++) {
					if (y == terrainHeight) {
						setBlock(x, y, z, BLOCK_GRASS);
					}
					else if (y >= terrainHeight - 3) {
						setBlock(x, y, z, BLOCK_DIRT);
					}
					else {
						setBlock(x, y, z, BLOCK_STONE);
					}
				}
			}
		}
	}

	void render(Shader& shader) {

		if (m_isDirty) {
			generateMesh();
		}

		glm::mat4 model{ glm::mat4(1.0f) };
		model = glm::translate(model, glm::vec3(m_chunkX * CHUNK_WIDTH, 0.0f, m_chunkZ * CHUNK_DEPTH));

		shader.setMat4("model", model);;

		glBindVertexArray(m_VAO);
		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_indexCount));
	}


};