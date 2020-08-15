#include "mesh_loaders.h"

#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <meshoptimizer.h>

#include <algorithm>

#include "logging.h"

bool loadTexture(const char* path, TextureInfo* out, bool flipImage)
{
	int twidth;
	int theight;
	int numChannels;
	assert(out);

	stbi_set_flip_vertically_on_load(flipImage);
	uint8_t* data = stbi_load(path, &twidth, &theight, &numChannels, 4);
	
	if(!data) 
	{
		magma::log::error("Failed to load texture from path {}", path);
		return false;
	}

	out->numc = 4; 
	out->data = data;
	out->format = VK_FORMAT_R8G8B8A8_UNORM;
	out->extent = {static_cast<uint32_t>(twidth), static_cast<uint32_t>(theight), 1};
	return true;
}

struct RemappedData
{
	std::vector<Vertex> remappedVertices;
	std::vector<unsigned int> remappedIndices;
};

static RemappedData meshOptimize(const Vertex* vertices, uint32_t vcount, uint32_t indexCount, const unsigned int* indices = nullptr)
{
	RemappedData data = {};
	
	std::vector<unsigned int> remapTable = {};
	remapTable.resize(indexCount);//total nums of indices
	std::size_t newVertCount = meshopt_generateVertexRemap(remapTable.data(), 
		indices, indexCount,
		vertices, vcount, sizeof(Vertex)
	);
	
	std::vector<Vertex> vertexBuffer = {};
	vertexBuffer.resize(newVertCount);
	meshopt_remapVertexBuffer(vertexBuffer.data(), vertices, vcount, sizeof(Vertex), remapTable.data());
	std::vector<unsigned int> indexBuffer = {};
	indexBuffer.resize(indexCount);
	meshopt_remapIndexBuffer(indexBuffer.data(), indices, indexCount, remapTable.data());

	return RemappedData {
		vertexBuffer,
		indexBuffer
	};
}

bool loadGLTF(const char* path, Mesh* geom, Animation* animation)
{
	assert(geom);

	tinygltf::Model gltfModel = {};
	tinygltf::TinyGLTF loader = {};
	
	std::string err = {};
	std::string warn = {};
	const std::string path_string = {path};

	bool loadStatus = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, path_string);
	if(!loadStatus)
	{
		magma::log::error("Failed to load gltf model! reason {}", err);
		return false;
	}
	
	//find first node, containing actual mesh data
	auto& meshNode = std::find_if(gltfModel.nodes.begin(), gltfModel.nodes.end(),
		[](tinygltf::Node& node) {return node.mesh >= 0;}
	);

	uint32_t meshId = static_cast<uint32_t>(meshNode->mesh);

	auto& mesh = gltfModel.meshes[meshId];
	auto& attributesMap = mesh.primitives[0].attributes;

	int accessorPositionPos = attributesMap["POSITION"];
	int accessorNormalPos = attributesMap["NORMAL"];
	int accessorUVPos = attributesMap["TEXCOORD_0"];
	int accessorJointIdPos = attributesMap["JOINTS_0"];
	int accessorWeightPos = attributesMap["WEIGHTS_0"];
	int accessorIndexPos = mesh.primitives[0].indices;

	auto& posAccessor = gltfModel.accessors[accessorPositionPos];
	auto& normalAccessor = gltfModel.accessors[accessorNormalPos];
	auto& uvAccessor = gltfModel.accessors[accessorUVPos];
	auto& jointIdAccessor = gltfModel.accessors[accessorJointIdPos];
	auto& weightAccessor = gltfModel.accessors[accessorWeightPos];
	auto& indexAccessor = gltfModel.accessors[accessorIndexPos];

	int posBufferViewId = posAccessor.bufferView;
	int normalBufferViewId = normalAccessor.bufferView;
	int uvBufferViewId = uvAccessor.bufferView;
	int jointBufferViewId = jointIdAccessor.bufferView;
	int weightBufferViewId = weightAccessor.bufferView;
	int indexBufferViewId = indexAccessor.bufferView;

	const std::size_t numberOfPositions = posAccessor.count;
	const std::size_t numberOfNormals = normalAccessor.count;
	const std::size_t numberOfUVs = uvAccessor.count;
	const std::size_t numberOfJointIds = jointIdAccessor.count;
	const std::size_t numberOfWeights = weightAccessor.count;
	const std::size_t numberOfIndices = indexAccessor.count;

	auto& posBufferView = gltfModel.bufferViews[posBufferViewId];
	auto& normalBufferView = gltfModel.bufferViews[normalBufferViewId];
	auto& uvBufferView = gltfModel.bufferViews[uvBufferViewId];
	auto& jointIdBufferView = gltfModel.bufferViews[jointBufferViewId];
	auto& weightBufferView = gltfModel.bufferViews[weightBufferViewId];
	auto& indexBufferView = gltfModel.bufferViews[indexBufferViewId];

	const std::size_t posByteOffset = posBufferView.byteOffset;
	const std::size_t normalByteOffset = normalBufferView.byteOffset;
	const std::size_t uvByteOffset = uvBufferView.byteOffset;
	const std::size_t jointIdByteOffset = jointIdBufferView.byteOffset;
	const std::size_t weightByteOffset = weightBufferView.byteOffset;
	const std::size_t indexByteOffset = indexBufferView.byteOffset;

	const int posBufferId = posBufferView.buffer;
	const int norBufferId = normalBufferView.buffer;
	const int uvBufferId = uvBufferView.buffer;
	const int jointBufferId = jointIdBufferView.buffer;
	const int weightBufferId = weightBufferView.buffer;
	const int indexBufferId = indexBufferView.buffer;
	
	auto& posBuffer = gltfModel.buffers[posBufferId];
	auto& normalBuffer = gltfModel.buffers[norBufferId];
	auto& uvBuffer = gltfModel.buffers[uvBufferId];
	auto& jointIdBuffer = gltfModel.buffers[jointBufferId];
	auto& weightBuffer = gltfModel.buffers[weightBufferId];

	auto& indexBuffer = gltfModel.buffers[indexBufferId];
	
	const uint8_t* posBufferData = posBuffer.data.data() + posByteOffset;
	const uint8_t* normalBufferData = normalBuffer.data.data() + normalByteOffset;
	const uint8_t* uvBufferData = uvBuffer.data.data() + uvByteOffset;
	const uint8_t* jointIdData = jointIdBuffer.data.data() + jointIdByteOffset;
	const uint8_t* weightData = weightBuffer.data.data() + weightByteOffset; 
	const uint8_t* indexBufferData = indexBuffer.data.data() + indexByteOffset;
	
	const Vec3* positions = (Vec3*)posBufferData;
	const Vec3* normals = (Vec3*)normalBufferData;
	const Vec2* uvs = (Vec2*)uvBufferData;
	const uint16_t* jointIds = (uint16_t*)jointIdData;
	const Vec4* weights = (Vec4*)weightData;
	const uint16_t* rawIndices = (uint16_t*)indexBufferData;

	std::vector<Vertex> vertices = {};
	vertices.resize(numberOfPositions);

	for(std::size_t i = 0; i < numberOfPositions; i++)
	{
		Vertex vertex = {};
		vertex.position = positions[i];
		vertex.normal   = normals[i];
		vertex.uv       = uvs[i];
		
		vertex.jointIds.x = static_cast<float>(jointIds[i*4]);
		vertex.jointIds.y = static_cast<float>(jointIds[i*4 + 1]);
		vertex.jointIds.z = static_cast<float>(jointIds[i*4 + 2]);
		vertex.jointIds.w = static_cast<float>(jointIds[i*4 + 3]);
		
		vertex.weights  = weights[i];
		vertices[i] = vertex;
		magma::log::debug("pos {} vert {} uv {}",positions[i],normals[i],uvs[i]);
	}
	
	std::vector<unsigned int> indices = {};
	indices.resize(numberOfIndices);
	for(std::size_t i = 0; i < numberOfIndices; i++)
	{
		indices[i] = rawIndices[i];
	}

	RemappedData remap = meshOptimize(vertices.data(), vertices.size(), numberOfIndices, indices.data());
	geom->vertexBuffer = remap.remappedVertices;
	geom->indexBuffer = remap.remappedIndices;


	//anim data
	if(animation)
	{
		int skinId = meshNode->skin;
		assert(skinId >= 0);
		auto& skin = gltfModel.skins[skinId];
		int matAccessorId = skin.inverseBindMatrices;
		auto& invBindMatAccessor = gltfModel.accessors[matAccessorId];
		int matCount = invBindMatAccessor.count;
		assert(matCount==skin.joints.size());
		int invBindBuffViewId = invBindMatAccessor.bufferView;
		auto& invBindBuffView = gltfModel.bufferViews[invBindBuffViewId];
		const std::size_t matBufferOffset = invBindBuffView.byteOffset;
		auto& invBindMatBuffer = gltfModel.buffers[invBindBuffView.buffer];
		const uint8_t* invBindMatBufferRaw = invBindMatBuffer.data.data() + matBufferOffset;
		const mat4x4* bindMatBuffer = (mat4x4*)invBindMatBufferRaw; 

		auto& joints = animation->bindPose;
		auto& gltfJointsOrder = skin.joints;

		//gltf joint id --> bindmatrix array id 
		std::unordered_map<int, int> jointsRemapper;
		for(uint32_t i = 0; i < skin.joints.size(); i++)
		{
			jointsRemapper[skin.joints[i]] = i;
		}
	

		joints.resize(skin.joints.size());
		for(uint32_t i = 0; i < matCount; i++)
		{
			int jointId = skin.joints[i];
			joints[i].invBindTransform = bindMatBuffer[i];
			for(int childs : gltfModel.nodes[jointId].children)
			{
				joints[jointsRemapper[childs]].parentId = i;
			}
		}

		//build global transforms
		auto& gltfAnimation = gltfModel.animations[0];
		const std::size_t gltfSamplersCount = gltfAnimation.channels.size();

		std::vector<AnimationSampler> samplers = {};
		samplers.resize(gltfSamplersCount);

		for(uint32_t i = 0; i < gltfSamplersCount; i++)
		{
			auto& gltfChannel = gltfAnimation.channels[i];
			auto& gltfSampler = gltfAnimation.samplers[gltfChannel.sampler]; 

			auto& sampler = samplers[i];
			sampler.jointId = gltfChannel.target_node;

			auto& gltfInputAccessor = gltfModel.accessors[gltfSampler.input];
			auto& gltfOutputAccessor = gltfModel.accessors[gltfSampler.output];

			auto& gltfInputBufferView = gltfModel.bufferViews[gltfInputAccessor.bufferView];
			auto& gltfOutputBufferView = gltfModel.bufferViews[gltfOutputAccessor.bufferView];

			auto& gltfInputBuffer = gltfModel.buffers[gltfInputBufferView.buffer];
			auto& gltfOutputBuffer = gltfModel.buffers[gltfOutputBufferView.buffer];

			const std::size_t inputCount = gltfInputAccessor.count;
			const std::size_t outputCount = gltfOutputAccessor.count;
			const std::size_t inputByteOffset = gltfInputBufferView.byteOffset;
			const std::size_t inputByteLength = gltfInputBufferView.byteLength;
			const std::size_t outputByteOffset =  gltfOutputBufferView.byteOffset;
			const std::size_t outputByteLength = gltfOutputBufferView.byteLength;

			const uint8_t* rawInputData = gltfInputBuffer.data.data() + inputByteOffset;
			const uint8_t* rawOutputData = gltfOutputBuffer.data.data() + outputByteOffset;

			sampler.timeCodes.resize(inputCount);
			sampler.outputs.resize(outputByteLength);

			memcpy(sampler.timeCodes.data(), rawInputData, inputByteLength);
			memcpy(sampler.outputs.data(), rawOutputData, outputByteLength);

			const char* channelType = gltfChannel.target_path.c_str(); 

			if(strcmp(channelType, "rotation") == 0)
			{
				sampler.channel = CHANNEL_ROTATE_BIT;
			}
			else if(strcmp(channelType, "translation") == 0)
			{
				sampler.channel = CHANNEL_TRANSLATE_BIT;
			}
			else if(strcmp(channelType, "scale") == 0)
			{
				sampler.channel = CHANNEL_SCALE_BIT;
			}
			else
			{
				assert(!"WTF");
			}

			samplers[i] = sampler;
		}

		auto& keyFrames = animation->keyFrames;
		keyFrames.resize(samplers[0].timeCodes.size());


		for(uint32_t i = 0; i < keyFrames.size(); i++)
		{
			keyFrames[i].currentJointLocalTransforms.resize(joints.size());
			auto& jointTransforms = keyFrames[i].currentJointLocalTransforms;

			for(const auto& sampler : samplers)
			{
				uint32_t jointId = jointsRemapper[sampler.jointId];
				keyFrames[i].frameTime = sampler.timeCodes[i];

				if(sampler.channel & CHANNEL_ROTATE_BIT)
				{
					Quat* quats = (Quat*)sampler.outputs.data();
					jointTransforms[jointId].rotation = quats[i];
					jointTransforms[jointId].channelBits |= CHANNEL_ROTATE_BIT;
				}
				else if(sampler.channel & CHANNEL_TRANSLATE_BIT)
				{
					Vec3* translations = (Vec3*)sampler.outputs.data();
					jointTransforms[jointId].translation = translations[i];
					jointTransforms[jointId].channelBits |= CHANNEL_TRANSLATE_BIT;
				}
				else if(sampler.channel & CHANNEL_SCALE_BIT)
				{
					Vec3* scales = (Vec3*)sampler.outputs.data();
					jointTransforms[jointId].scale = scales[i];
					jointTransforms[jointId].channelBits |= CHANNEL_SCALE_BIT;
				}
				else
				{
					assert(!"WTF");
				}
			}
		}

		//compute global Transforms
		for(auto& keyFrame : keyFrames)
		{
			keyFrame.currentJointGlobalTransforms.resize(joints.size());
			generateGlobalJointTransforms(*animation, &keyFrame);
		}

	}//animation

	return true;
}

bool loadOBJ(const char* path, Mesh* geom)
{
	assert(path);
	assert(geom);
	auto meshUnloader = [](fastObjMesh* mesh)
	{
		fast_obj_destroy(mesh);
	};
	
	std::unique_ptr<fastObjMesh, decltype(meshUnloader)> mesh = {
		fast_obj_read(path),
		meshUnloader
	};

	if(!mesh.get())
	{
		magma::log::error("Failed to load mesh from {}", path);
		return false;
	}

	//assume triangle mesh here
	std::vector<Vertex> vertices = {};
	assert(mesh->face_vertices[0] == 3);
	const uint32_t vertexCount = mesh->face_vertices[0];
	vertices.resize(mesh->face_count * mesh->face_vertices[0]);

	//loop through each face
	for(uint32_t i = 0; i < mesh->face_count; i++)
	{
		//loop through each vertex in a face
		for(uint32_t j = 0; j < vertexCount; j++)
		{
			const uint32_t vi = mesh->indices[3 * i + j].p;
			const uint32_t vn = mesh->indices[3 * i + j].n;
			const uint32_t vt = mesh->indices[3 * i + j].t;

			const std::size_t out_index = i * vertexCount + j;

			vertices[out_index].position.x = mesh->positions[3 * vi + 0];
			vertices[out_index].position.y = mesh->positions[3 * vi + 1];
			vertices[out_index].position.z = mesh->positions[3 * vi + 2];
			
			vertices[out_index].normal.x = mesh->normals[3 * vn + 0];
			vertices[out_index].normal.y = mesh->normals[3 * vn + 1];
			vertices[out_index].normal.z = mesh->normals[3 * vn + 2];

			vertices[out_index].uv.u = mesh->texcoords[2 * vt + 0];
			vertices[out_index].uv.v = mesh->texcoords[2 * vt + 1];
		}
	}

	RemappedData data = meshOptimize(vertices.data(), vertices.size(), 3 * mesh->face_count);
	geom->vertexBuffer = data.remappedVertices;
	geom->indexBuffer = data.remappedIndices;

	return true;
}
