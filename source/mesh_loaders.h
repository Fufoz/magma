#ifndef MAGMA_MESH_LOADERS
#define MAGMA_MESH_LOADERS

#include <vector>

#include "maths.h"
#include "animation.h"
#include "vk_types.h"

enum VertexLayout
{
	LAYOUT_POSITIONS = 0b00001,
	LAYOUT_NORMALS   = 0b00010,
	LAYOUT_UVS       = 0b00100,
	LAYOUT_WEIGHTS   = 0b01000,
	LAYOUT_JOINTS    = 0b10000
};

struct Vertex
{
	Vec3 position;
	Vec3 normal;
	Vec2 uv;
	Vec4 jointIds;
	Vec4 weights;
};

struct Mesh
{
	std::vector<Vertex> vertexBuffer;
	std::vector<unsigned int> indexBuffer;
};

struct TextureInfo
{
	VkFormat format;
	VkExtent3D extent;
	uint8_t  numc;
	uint8_t* data;
};

bool load_texture(const char* path, TextureInfo* out, bool flipImage = true);

bool load_OBJ(const char* path, Mesh* geom);

bool load_GLTF(const char* path, Mesh* geom, Animation* animation = nullptr);

#endif