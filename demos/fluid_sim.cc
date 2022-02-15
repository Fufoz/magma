#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <magma.h>

#include <vector>
#include <array>

static constexpr int SWAPCHAIN_IMAGE_COUNT = 2;

#define WARP_PICTURE_MODE
#define DRAW_FLUID_PARAMS
// #undef DRAW_FLUID_PARAMS
// #undef WARP_PICTURE_MODE

enum Pipeline
{
	PIPE_ADVECTION,
	PIPE_VORTICITY_CURL,
	PIPE_VORTICITY_FORCE,
	PIPE_JACOBI_SOLVER_PRESSURE,
	PIPE_JACOBI_SOLVER_VISCOCITY,
	PIPE_EXTERNAL_FORCES,
	PIPE_DIVERGENCE,
	PIPE_GRADIENT_SUBTRACT,
	PIPE_PRESENT,
	PIPE_COUNT
};

enum RenderTarget
{
	RT_VELOCITY_FIRST,
	RT_VELOCITY_SECOND,
	RT_CURL_FIRST,
	RT_CURL_SECOND,
	RT_PRESSURE_FIRST,
	RT_PRESSURE_SECOND,
	RT_COLOR_FIRST,
	RT_COLOR_SECOND,
	RT_MAX_COUNT
};

enum DescrSetIndex
{
	DSI_ADVECT_VELOCITY,
	DSI_VORTICITY_CURL,
	DSI_VORTICITY_FORCE,
	DSI_VISCOCITY_1,
	DSI_VISCOCITY_2,
	DSI_FORCES,
	DSI_FORCES_COLOR,
	DSI_DIVERGENCE,
	DSI_PRESSURE_1,
	DSI_PRESSURE_2,
	DSI_GRADIENT_SUBTRACT,
	DSI_ADVECT_COLOR,
	DSI_PRESENT,
	DSI_INDEX_COUNT
};

struct AdvectConstants
{
	float gridScale;
	float timestep;
	float dissipation;
};
	
struct SolverConstants
{
	float alpha;
	float beta;
	float texelSize;
};
static constexpr std::size_t JACOBI_ITERATIONS = 50;

struct ForceConstants
{
	Vec4 force;
	Vec2 mousePos;
	float impulseRadius;
};

struct VorticityConstants
{
	float confinement;
	float timestep;
	float texelSize;
};

struct FluidContext
{
	VulkanGlobalContext vkCtx;
	SwapChain swapchain;
	WindowInfo window;	

	std::array<VkPipeline, PIPE_COUNT> pipelines;
	std::array<VkDescriptorSetLayout, PIPE_COUNT> descrSetLayouts;
	std::array<VkPipelineLayout, PIPE_COUNT> pipeLayouts;
	std::array<VkRenderPass, PIPE_COUNT> renderPasses;
	std::array<ImageResource, RT_MAX_COUNT> simTextures;
	std::array<VkFramebuffer, RT_MAX_COUNT> frameBuffers;
	std::array<VkCommandBuffer, SWAPCHAIN_IMAGE_COUNT> commandBuffers;
	VkDescriptorSet descrSetsPerFrame[SWAPCHAIN_IMAGE_COUNT][DSI_INDEX_COUNT];

	VkSampler defaultSampler = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	
	Buffer deviceVertexBuffer;
	Buffer deviceIndexBuffer;

	float dx;
	float timeStep;
	float kv;
	float impulseRadius;
};

static void init_imgui_context(FluidContext* ctx)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	VkDescriptorPoolSize pool_sizes[] =
	{
	    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
	    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
	    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
	    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
	    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
	    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
	    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
	    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
	    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
	    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
	    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
	pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool bigAssDescriptorPool = VK_NULL_HANDLE;
	vkCreateDescriptorPool(ctx->vkCtx.logicalDevice, &pool_info, nullptr, &bigAssDescriptorPool);

	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ctx->window.windowHandle, true);

	ImGui_ImplVulkan_InitInfo imguiVulkanInitInfo = {};
	imguiVulkanInitInfo.Instance  = ctx->vkCtx.instance;
	imguiVulkanInitInfo.PhysicalDevice = ctx->vkCtx.physicalDevice;
	imguiVulkanInitInfo.Device = ctx->vkCtx.logicalDevice;
	imguiVulkanInitInfo.QueueFamily = ctx->vkCtx.queueFamIdx;
	imguiVulkanInitInfo.Queue = ctx->vkCtx.graphicsQueue;
	imguiVulkanInitInfo.PipelineCache = VK_NULL_HANDLE;
	imguiVulkanInitInfo.DescriptorPool = bigAssDescriptorPool;
	imguiVulkanInitInfo.Subpass = 0;
	imguiVulkanInitInfo.MinImageCount = SWAPCHAIN_IMAGE_COUNT;
	imguiVulkanInitInfo.ImageCount = SWAPCHAIN_IMAGE_COUNT;
	imguiVulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	imguiVulkanInitInfo.Allocator = nullptr;

	ImGui_ImplVulkan_Init(&imguiVulkanInitInfo, ctx->renderPasses[PIPE_PRESENT]);

	//fonts
	auto pool = createCommandPool(ctx->vkCtx);
	auto cmdBuffer = begin_tmp_commands(ctx->vkCtx, pool);
	ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
	end_tmp_commands(ctx->vkCtx, pool, cmdBuffer);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

static bool create_fluid_context(FluidContext* ctx)
{
	magma::log::initLogging();

	const std::vector<const char *> desiredLayers = {
		{"VK_LAYER_KHRONOS_validation"}
	};

	const std::vector<const char *> desiredExtensions = {
		{VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
	};

	if (!initVulkanGlobalContext(desiredLayers, desiredExtensions, &ctx->vkCtx))
	{
		return false;
	}

	const std::size_t width = 600;
	const std::size_t height = 600;
	if (!initPlatformWindow(ctx->vkCtx, width, height, "fluid_sim", &ctx->window))
	{
		return false;
	}

	if (!createSwapChain(ctx->vkCtx, ctx->window, SWAPCHAIN_IMAGE_COUNT, &ctx->swapchain))
	{
		return false;
	}

	ctx->dx = 1.f / (float)std::max(width, height);
	ctx->timeStep = 0.005f;
	ctx->kv = 1.5f;//kinematic viscocity
	ctx->impulseRadius = 0.25f;
	
	return true;
}

static void insert_image_memory_barrier(
	FluidContext* ctx, VkCommandBuffer cmdBuffer, VkImage image,
	VkPipelineStageFlags srcPipeStage, VkPipelineStageFlags dstPipeStage, 
	VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
	VkImageLayout oldLayout, VkImageLayout newLayout)
{
	
	VkImageMemoryBarrier memoryBarrier = {};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = srcAccessMask;
	memoryBarrier.dstAccessMask = dstAccessMask;
	memoryBarrier.oldLayout = oldLayout;
	memoryBarrier.newLayout = newLayout;
	memoryBarrier.srcQueueFamilyIndex = ctx->vkCtx.queueFamIdx;
	memoryBarrier.dstQueueFamilyIndex = ctx->vkCtx.queueFamIdx;
	memoryBarrier.image = image;
	
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;
	memoryBarrier.subresourceRange = subresourceRange;
	vkCmdPipelineBarrier(cmdBuffer,
		srcPipeStage,
		dstPipeStage,
		0, 
		0, nullptr,
		0, nullptr,
		1, &memoryBarrier
	);
}

static bool initialise_fluid_textures(FluidContext* ctx)
{

	VkExtent3D textureSize = {ctx->window.windowExtent.width, ctx->window.windowExtent.height, 1};
	for(std::size_t textureIndex = 0; textureIndex < ctx->simTextures.size(); textureIndex++)
	{
		ctx->simTextures[textureIndex] = createResourceImage(
			ctx->vkCtx,
			textureSize,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
		);
	}

	std::vector<uint8_t> hostBuffer = {};
	const std::size_t numc = 4;
	hostBuffer.resize(ctx->window.windowExtent.width * ctx->window.windowExtent.height * numc * sizeof(float));
	std::memset(hostBuffer.data(), 0, hostBuffer.size());

	Buffer stagingBuffer = createBuffer(
		ctx->vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		hostBuffer.size()
	);

	copyDataToHostVisibleBuffer(ctx->vkCtx, 0, hostBuffer.data(), hostBuffer.size(), &stagingBuffer);

	auto tmpCmdPool = createCommandPool(ctx->vkCtx);
	for(auto&& texture : ctx->simTextures)
	{
		pushTextureToDeviceLocalImage(tmpCmdPool, ctx->vkCtx, stagingBuffer, textureSize, &texture);
	}
	// vkDestroyCommandPool(ctx->logicalDevice, tmpCmdPool, nullptr);
	destroyBuffer(ctx->vkCtx.logicalDevice, &stagingBuffer);
		
	bool status = false;
		
	ctx->defaultSampler = createDefaultSampler(ctx->vkCtx.logicalDevice, &status);

#if defined(WARP_PICTURE_MODE)
	TextureInfo girlTexture = {};
	status &= loadTexture("images/girl_with_a_pearl.png", &girlTexture, false);
	const std::size_t textureByteSize = girlTexture.numc * girlTexture.extent.width * girlTexture.extent.height;

	auto deviceGirlTexture = createResourceImage(
		ctx->vkCtx, girlTexture.extent, girlTexture.format,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
	);

	Buffer stagingTextureBuffer = createBuffer(
		ctx->vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		textureByteSize 
	);

	copyDataToHostVisibleBuffer(ctx->vkCtx, 0, girlTexture.data, textureByteSize, &stagingTextureBuffer);
		
	pushTextureToDeviceLocalImage(tmpCmdPool, ctx->vkCtx, stagingTextureBuffer, girlTexture.extent, &deviceGirlTexture);

	auto cmdBuffer = begin_tmp_commands(ctx->vkCtx, tmpCmdPool);

	VkImageSubresourceLayers layers = {};
	layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	layers.mipLevel = 0;
	layers.baseArrayLayer = 0;
	layers.layerCount = 1;
	VkOffset3D offsetFirst = {0, 0, 0};
	VkOffset3D offsetSecond = {girlTexture.extent.width, girlTexture.extent.height, 1};

	VkImageBlit blitInfo = {};
	blitInfo.srcSubresource = layers;
	blitInfo.srcOffsets[0] = offsetFirst;
	blitInfo.srcOffsets[1] = offsetSecond;
	blitInfo.dstSubresource = layers;
	blitInfo.dstOffsets[0] = offsetFirst;
	blitInfo.dstOffsets[1] = offsetSecond;

	insert_image_memory_barrier(
		ctx,
		cmdBuffer,
		deviceGirlTexture.image, 
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		deviceGirlTexture.layout,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	);
	deviceGirlTexture.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	insert_image_memory_barrier(
		ctx,
		cmdBuffer,
		ctx->simTextures[RT_COLOR_FIRST].image, 
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		ctx->simTextures[RT_COLOR_FIRST].layout,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);
	ctx->simTextures[RT_COLOR_FIRST].layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	vkCmdBlitImage(
		cmdBuffer, 
		deviceGirlTexture.image,
		deviceGirlTexture.layout,
		ctx->simTextures[RT_COLOR_FIRST].image,
		ctx->simTextures[RT_COLOR_FIRST].layout,
		1, &blitInfo,
		VK_FILTER_NEAREST
	);

	insert_image_memory_barrier(
		ctx,
		cmdBuffer,
		deviceGirlTexture.image, 
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		0,
		deviceGirlTexture.layout,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	insert_image_memory_barrier(
		ctx,
		cmdBuffer,
		ctx->simTextures[RT_COLOR_FIRST].image, 
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		0,
		ctx->simTextures[RT_COLOR_FIRST].layout,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	end_tmp_commands(ctx->vkCtx, tmpCmdPool, cmdBuffer);
		// vkDestroyCommandPool(ctx->logicalDevice, tmpCmdPool, nullptr);
		// destroyBuffer(ctx->logicalDevice, &stagingBuffer);
#endif
	return status;
}

static void destroy_fluid_context()
{

}

static void init_vertex_and_index_buffers(FluidContext* ctx)
{
	std::array<Vec3, 4> cubeVerts = {
		Vec3{1.f, 1.f, 0.f},
		Vec3{-1.f, 1.f, 0.f},
		Vec3{-1.f, -1.f, 0.f},
		Vec3{1.f, -1.f, 0.f}
	};
	std::array<std::uint32_t, 2 * 3> indicies = {
		0, 1, 3,
		1, 2, 3
	};
	const std::size_t vertexBufferSize = cubeVerts.size() * sizeof(Vec3);
	const std::size_t indexBufferSize = indicies.size() * sizeof(std::uint32_t);
	auto stagingVertexBuffer = createBuffer(
		ctx->vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		vertexBufferSize
	);
	copyDataToHostVisibleBuffer(ctx->vkCtx, 0, cubeVerts.data(), vertexBufferSize, &stagingVertexBuffer);
	
	ctx->deviceVertexBuffer = createBuffer(
		ctx->vkCtx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize
	);

	VkCommandPoolCreateInfo commandPoolCI = {};
	commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCI.pNext = nullptr;
	commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCI.queueFamilyIndex = ctx->vkCtx.queueFamIdx;
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	vkCreateCommandPool(ctx->vkCtx.logicalDevice, &commandPoolCI, nullptr, &cmdPool);
	pushDataToDeviceLocalBuffer(cmdPool, ctx->vkCtx, stagingVertexBuffer, &ctx->deviceVertexBuffer, ctx->vkCtx.graphicsQueue);
	auto stagingIndexBuffer = createBuffer(
		ctx->vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexBufferSize
	);
	copyDataToHostVisibleBuffer(ctx->vkCtx, 0, indicies.data(), indexBufferSize, &stagingIndexBuffer);
	
	ctx->deviceIndexBuffer = createBuffer(
		ctx->vkCtx,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize
	);
	pushDataToDeviceLocalBuffer(cmdPool, ctx->vkCtx, stagingIndexBuffer, &ctx->deviceIndexBuffer, ctx->vkCtx.graphicsQueue);
	destroyBuffer(ctx->vkCtx.logicalDevice, &stagingVertexBuffer);
	destroyBuffer(ctx->vkCtx.logicalDevice, &stagingIndexBuffer);
	destroyCommandPool(ctx->vkCtx.logicalDevice, cmdPool);
}

static VkPipeline create_advect_pipeline(FluidContext* ctx,
	const VkGraphicsPipelineCreateInfo& commonPipeCI)
{
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCI = {};
	shaderStageCI[0] = fillShaderStageCreateInfo(
		ctx->vkCtx.logicalDevice,
		"shaders/spv/fluid_cube_vert.spv",
		VK_SHADER_STAGE_VERTEX_BIT
	);
	shaderStageCI[1] = fillShaderStageCreateInfo(
		ctx->vkCtx.logicalDevice,
		"shaders/spv/fluid_advect_quantity.spv",
		VK_SHADER_STAGE_FRAGMENT_BIT
	);

	std::array<VkDescriptorSetLayoutBinding, 2> descrSetLayoutBinding = {};
	descrSetLayoutBinding[0].binding = 0;
	descrSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descrSetLayoutBinding[0].descriptorCount = 1;
	descrSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descrSetLayoutBinding[0].pImmutableSamplers = &ctx->defaultSampler;

	descrSetLayoutBinding[1].binding = 1;
	descrSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descrSetLayoutBinding[1].descriptorCount = 1;
	descrSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descrSetLayoutBinding[1].pImmutableSamplers = &ctx->defaultSampler;

	VkDescriptorSetLayoutCreateInfo descrSetCI = {};
	descrSetCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descrSetCI.bindingCount = descrSetLayoutBinding.size();
	descrSetCI.pBindings = descrSetLayoutBinding.data();

	VkDescriptorSetLayout advectDescrSetLayout = {};
	vkCreateDescriptorSetLayout(ctx->vkCtx.logicalDevice, &descrSetCI, nullptr, &advectDescrSetLayout);

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(AdvectConstants);

	VkPipelineLayoutCreateInfo advectPipeLayoutCI = {};
	advectPipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	advectPipeLayoutCI.setLayoutCount = 1;
	advectPipeLayoutCI.pSetLayouts = &advectDescrSetLayout;
	advectPipeLayoutCI.pushConstantRangeCount = 1;
	advectPipeLayoutCI.pPushConstantRanges = &pushConstantRange;

	VkPipelineLayout advectPipeLayout = VK_NULL_HANDLE;
	vkCreatePipelineLayout(ctx->vkCtx.logicalDevice, &advectPipeLayoutCI, nullptr, &advectPipeLayout);

	VkAttachmentDescription attachmentDescr = {};
	attachmentDescr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescr.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//hmm
	attachmentDescr.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference attachmentRef = {};
	attachmentRef.attachment = 0;
	attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescr = {};
	subpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescr.colorAttachmentCount = 1;
	subpassDescr.pColorAttachments = &attachmentRef;

	VkSubpassDependency subpassDep = {};
	subpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDep.dstSubpass = 0;
	subpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDep.srcAccessMask = 0;
	subpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDep.dependencyFlags = VK_FLAGS_NONE;

	VkRenderPassCreateInfo advectRenderPassCI = {};
	advectRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	advectRenderPassCI.attachmentCount = 1;
	advectRenderPassCI.pAttachments = &attachmentDescr;
	advectRenderPassCI.subpassCount = 1;
	advectRenderPassCI.pSubpasses = &subpassDescr;
	advectRenderPassCI.dependencyCount = 1;
	advectRenderPassCI.pDependencies = &subpassDep;

	VkRenderPass advectRenderPass = VK_NULL_HANDLE;
	vkCreateRenderPass(ctx->vkCtx.logicalDevice, &advectRenderPassCI, nullptr, &advectRenderPass);
	
	VkGraphicsPipelineCreateInfo pipeCI = commonPipeCI;
	pipeCI.stageCount = shaderStageCI.size();
	pipeCI.pStages = shaderStageCI.data();
	pipeCI.layout = advectPipeLayout;
	pipeCI.renderPass = advectRenderPass;
	pipeCI.subpass = 0;
	
	ctx->descrSetLayouts[PIPE_ADVECTION] = advectDescrSetLayout;
	ctx->pipeLayouts[PIPE_ADVECTION] = advectPipeLayout;
	ctx->renderPasses[PIPE_ADVECTION] = advectRenderPass;
	VkPipeline pipeline = VK_NULL_HANDLE;
	vkCreateGraphicsPipelines(ctx->vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &pipeline);

	return pipeline;
}

static VkPipeline create_curl_pipeline(FluidContext* ctx,
	const VkGraphicsPipelineCreateInfo& commonPipeCI)
{
	//creating curl pipe
	std::array<VkPipelineShaderStageCreateInfo, 2> curlShaderStageCI = {};
	curlShaderStageCI[0] = fillShaderStageCreateInfo(
		ctx->vkCtx.logicalDevice,
		"shaders/spv/fluid_cube_vert.spv",
		VK_SHADER_STAGE_VERTEX_BIT
	);
	curlShaderStageCI[1] = fillShaderStageCreateInfo(
		ctx->vkCtx.logicalDevice,
		"shaders/spv/fluid_vorticity_curl.spv",
		VK_SHADER_STAGE_FRAGMENT_BIT
	);

	std::array<VkDescriptorSetLayoutBinding, 1> curlDescrSetLayoutBinding = {};
	curlDescrSetLayoutBinding[0].binding = 0;
	curlDescrSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	curlDescrSetLayoutBinding[0].descriptorCount = 1;
	curlDescrSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	curlDescrSetLayoutBinding[0].pImmutableSamplers = &ctx->defaultSampler;

	VkDescriptorSetLayoutCreateInfo curlDescrSetCI = {};
	curlDescrSetCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	curlDescrSetCI.bindingCount = curlDescrSetLayoutBinding.size();
	curlDescrSetCI.pBindings = curlDescrSetLayoutBinding.data();

	VkDescriptorSetLayout curlDescrSetLayout = {};
	vkCreateDescriptorSetLayout(ctx->vkCtx.logicalDevice, &curlDescrSetCI, nullptr, &curlDescrSetLayout);

	VkPushConstantRange curlPushConstantRange = {};
	curlPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	curlPushConstantRange.offset = 0;
	curlPushConstantRange.size = sizeof(float);

	VkPipelineLayoutCreateInfo curlPipeLayoutCI = {};
	curlPipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	curlPipeLayoutCI.setLayoutCount = 1;
	curlPipeLayoutCI.pSetLayouts = &curlDescrSetLayout;
	curlPipeLayoutCI.pushConstantRangeCount = 1;
	curlPipeLayoutCI.pPushConstantRanges = &curlPushConstantRange;

	VkPipelineLayout curlPipeLayout = VK_NULL_HANDLE;
	vkCreatePipelineLayout(ctx->vkCtx.logicalDevice, &curlPipeLayoutCI, nullptr, &curlPipeLayout);

	VkAttachmentDescription curlAttachmentDescr = {};
	curlAttachmentDescr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	curlAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
	curlAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	curlAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	curlAttachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	curlAttachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	curlAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	curlAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference curlAttachmentRef = {};
	curlAttachmentRef.attachment = 0;
	curlAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription curlSubpassDescr = {};
	curlSubpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	curlSubpassDescr.colorAttachmentCount = 1;
	curlSubpassDescr.pColorAttachments = &curlAttachmentRef;

	VkSubpassDependency curlSubpassDep = {};
	curlSubpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
	curlSubpassDep.dstSubpass = 0;
	curlSubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	curlSubpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	curlSubpassDep.srcAccessMask = 0;
	curlSubpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	curlSubpassDep.dependencyFlags = VK_FLAGS_NONE;

	VkRenderPassCreateInfo curlRenderPassCI = {};
	curlRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	curlRenderPassCI.attachmentCount = 1;
	curlRenderPassCI.pAttachments = &curlAttachmentDescr;
	curlRenderPassCI.subpassCount = 1;
	curlRenderPassCI.pSubpasses = &curlSubpassDescr;
	curlRenderPassCI.dependencyCount = 1;
	curlRenderPassCI.pDependencies = &curlSubpassDep;

	VkRenderPass curlRenderPass = VK_NULL_HANDLE;
	vkCreateRenderPass(ctx->vkCtx.logicalDevice, &curlRenderPassCI, nullptr, &curlRenderPass);
	
	VkGraphicsPipelineCreateInfo pipeCI = commonPipeCI;
	
	pipeCI.stageCount = curlShaderStageCI.size();
	pipeCI.pStages = curlShaderStageCI.data();
	pipeCI.layout = curlPipeLayout;
	pipeCI.renderPass = curlRenderPass;
	pipeCI.subpass = 0;

	ctx->descrSetLayouts[PIPE_VORTICITY_CURL] = curlDescrSetLayout;
	ctx->pipeLayouts[PIPE_VORTICITY_CURL] = curlPipeLayout;
	ctx->renderPasses[PIPE_VORTICITY_CURL] = curlRenderPass;

	VkPipeline pipeline = VK_NULL_HANDLE;
	vkCreateGraphicsPipelines(ctx->vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &pipeline);
	return pipeline;
}

static VkPipeline create_vorticity_pipeline(FluidContext* ctx,
	const VkGraphicsPipelineCreateInfo& commonPipeCI)
{
	std::array<VkPipelineShaderStageCreateInfo, 2> vforceShaderStageCI = {};
	vforceShaderStageCI[0] = fillShaderStageCreateInfo(
		ctx->vkCtx.logicalDevice,
		"shaders/spv/fluid_cube_vert.spv",
		VK_SHADER_STAGE_VERTEX_BIT
	);
	vforceShaderStageCI[1] = fillShaderStageCreateInfo(
		ctx->vkCtx.logicalDevice,
		"shaders/spv/fluid_vorticity_force.spv",
		VK_SHADER_STAGE_FRAGMENT_BIT
	);

	std::array<VkDescriptorSetLayoutBinding, 2> vforceDescrSetLayoutBinding = {};
	vforceDescrSetLayoutBinding[0].binding = 0;
	vforceDescrSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	vforceDescrSetLayoutBinding[0].descriptorCount = 1;
	vforceDescrSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	vforceDescrSetLayoutBinding[0].pImmutableSamplers = &ctx->defaultSampler;

	vforceDescrSetLayoutBinding[1].binding = 1;
	vforceDescrSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	vforceDescrSetLayoutBinding[1].descriptorCount = 1;
	vforceDescrSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	vforceDescrSetLayoutBinding[1].pImmutableSamplers = &ctx->defaultSampler;

	VkDescriptorSetLayoutCreateInfo vforceDescrSetCI = {};
	vforceDescrSetCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	vforceDescrSetCI.bindingCount = vforceDescrSetLayoutBinding.size();
	vforceDescrSetCI.pBindings = vforceDescrSetLayoutBinding.data();

	VkDescriptorSetLayout vforceDescrSetLayout = {};
	vkCreateDescriptorSetLayout(ctx->vkCtx.logicalDevice, &vforceDescrSetCI, nullptr, &vforceDescrSetLayout);

	VkPushConstantRange vforcePushConstantRange = {};
	vforcePushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	vforcePushConstantRange.offset = 0;
	vforcePushConstantRange.size = sizeof(VorticityConstants);

	VkPipelineLayoutCreateInfo vforcePipeLayoutCI = {};
	vforcePipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	vforcePipeLayoutCI.setLayoutCount = 1;
	vforcePipeLayoutCI.pSetLayouts = &vforceDescrSetLayout;
	vforcePipeLayoutCI.pushConstantRangeCount = 1;
	vforcePipeLayoutCI.pPushConstantRanges = &vforcePushConstantRange;

	VkPipelineLayout vforcePipeLayout = VK_NULL_HANDLE;
	vkCreatePipelineLayout(ctx->vkCtx.logicalDevice, &vforcePipeLayoutCI, nullptr, &vforcePipeLayout);

	VkAttachmentDescription vforceAttachmentDescr = {};
	vforceAttachmentDescr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vforceAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
	vforceAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	vforceAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	vforceAttachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	vforceAttachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	vforceAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vforceAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference vforceAttachmentRef = {};
	vforceAttachmentRef.attachment = 0;
	vforceAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription vforceSubpassDescr = {};
	vforceSubpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	vforceSubpassDescr.colorAttachmentCount = 1;
	vforceSubpassDescr.pColorAttachments = &vforceAttachmentRef;

	VkSubpassDependency vforceSubpassDep = {};
	vforceSubpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
	vforceSubpassDep.dstSubpass = 0;
	vforceSubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	vforceSubpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	vforceSubpassDep.srcAccessMask = 0;
	vforceSubpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	vforceSubpassDep.dependencyFlags = VK_FLAGS_NONE;

	VkRenderPassCreateInfo vforceRenderPassCI = {};
	vforceRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	vforceRenderPassCI.attachmentCount = 1;
	vforceRenderPassCI.pAttachments = &vforceAttachmentDescr;
	vforceRenderPassCI.subpassCount = 1;
	vforceRenderPassCI.pSubpasses = &vforceSubpassDescr;
	vforceRenderPassCI.dependencyCount = 1;
	vforceRenderPassCI.pDependencies = &vforceSubpassDep;

	VkRenderPass vforceRenderPass = VK_NULL_HANDLE;
	vkCreateRenderPass(ctx->vkCtx.logicalDevice, &vforceRenderPassCI, nullptr, &vforceRenderPass);

	VkGraphicsPipelineCreateInfo pipeCI = commonPipeCI;
	pipeCI.stageCount = vforceShaderStageCI.size();
	pipeCI.pStages = vforceShaderStageCI.data();
	pipeCI.layout = vforcePipeLayout;
	pipeCI.renderPass = vforceRenderPass;
	pipeCI.subpass = 0;

	ctx->descrSetLayouts[PIPE_VORTICITY_FORCE] = vforceDescrSetLayout;
	ctx->pipeLayouts[PIPE_VORTICITY_FORCE] = vforcePipeLayout;
	ctx->renderPasses[PIPE_VORTICITY_FORCE] = vforceRenderPass;

	VkPipeline pipeline = VK_NULL_HANDLE;
	vkCreateGraphicsPipelines(ctx->vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &pipeline);
	return pipeline;
}
static VkPipeline create_present_pipeline(FluidContext* ctx,
	const VkGraphicsPipelineCreateInfo& commonPipeCI)
{
		std::array<VkPipelineShaderStageCreateInfo, 2> projectShaders = {};

		//creating present pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> presentShaders = {};
		presentShaders[0] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_cube_vert.spv",
			VK_SHADER_STAGE_VERTEX_BIT
		);
		presentShaders[1] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_ink_present.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		VkDescriptorSetLayoutBinding presentDescrSetLayoutBinding = {};
		presentDescrSetLayoutBinding.binding = 0;
		presentDescrSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		presentDescrSetLayoutBinding.descriptorCount = 1;
		presentDescrSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		presentDescrSetLayoutBinding.pImmutableSamplers = &ctx->defaultSampler;

		VkDescriptorSetLayoutCreateInfo presentDescrSetLayoutCI = {};
		presentDescrSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		presentDescrSetLayoutCI.bindingCount = 1;
		presentDescrSetLayoutCI.pBindings = &presentDescrSetLayoutBinding;

		VkDescriptorSetLayout presentDescrSetLayout = VK_NULL_HANDLE;
		vkCreateDescriptorSetLayout(
			ctx->vkCtx.logicalDevice,
			&presentDescrSetLayoutCI,
			nullptr,
			&presentDescrSetLayout
		);

		VkPipelineLayoutCreateInfo presentPipeLayoutCI = {};
		presentPipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		presentPipeLayoutCI.setLayoutCount = 1;
		presentPipeLayoutCI.pSetLayouts = &presentDescrSetLayout;
		
		VkPipelineLayout presentPipeLayout = VK_NULL_HANDLE;
		vkCreatePipelineLayout(ctx->vkCtx.logicalDevice, &presentPipeLayoutCI, nullptr, &presentPipeLayout);

		VkAttachmentDescription presentColorAttachmentDescr = {};
		presentColorAttachmentDescr.format = ctx->swapchain.imageFormat;
		presentColorAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
		presentColorAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		presentColorAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		presentColorAttachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		presentColorAttachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		presentColorAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		presentColorAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference presentColorAttachmentRef = {};
		presentColorAttachmentRef.attachment = 0;
		presentColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription presentSubpassDescr = {};
		presentSubpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		presentSubpassDescr.colorAttachmentCount = 1;
		presentSubpassDescr.pColorAttachments = &presentColorAttachmentRef;

		VkSubpassDependency presentSubpassDependency = {};
		presentSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		presentSubpassDependency.dstSubpass = 0;
		presentSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		presentSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		presentSubpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		presentSubpassDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		presentSubpassDependency.dependencyFlags = 0;

		VkRenderPassCreateInfo presentRenderPassCI = {};
		presentRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		presentRenderPassCI.attachmentCount = 1;
		presentRenderPassCI.pAttachments = &presentColorAttachmentDescr;
		presentRenderPassCI.subpassCount = 1;
		presentRenderPassCI.pSubpasses = &presentSubpassDescr;
		presentRenderPassCI.dependencyCount = 1;
		presentRenderPassCI.pDependencies = &presentSubpassDependency;

		VkRenderPass presentRenderPass = VK_NULL_HANDLE;
		vkCreateRenderPass(ctx->vkCtx.logicalDevice, &presentRenderPassCI, nullptr, &presentRenderPass);

	VkGraphicsPipelineCreateInfo pipeCI = commonPipeCI;
	pipeCI.stageCount =presentShaders.size();
	pipeCI.pStages = presentShaders.data();
	pipeCI.layout = presentPipeLayout;
	pipeCI.renderPass = presentRenderPass;
	pipeCI.subpass = 0;

	ctx->descrSetLayouts[PIPE_PRESENT] = presentDescrSetLayout;
	ctx->pipeLayouts[PIPE_PRESENT] = presentPipeLayout;
	ctx->renderPasses[PIPE_PRESENT] = presentRenderPass;

	VkPipeline pipeline = VK_NULL_HANDLE;
	vkCreateGraphicsPipelines(ctx->vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &pipeline);
	return pipeline;

}

static VkPipeline create_project_pipeline(FluidContext* ctx,
	const VkGraphicsPipelineCreateInfo& commonPipeCI)
{
		// creating project pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> projectShaders = {};

		projectShaders[0] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_cube_vert.spv",
			VK_SHADER_STAGE_VERTEX_BIT
		);
		projectShaders[1] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_project_gradient_subtract.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		VkPushConstantRange projectConstantRange = {};
		projectConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		projectConstantRange.offset = 0;
		projectConstantRange.size = sizeof(float);//dx

		std::array<VkDescriptorSetLayoutBinding, 2> projectDescrSetLayoutBinding = {};
		projectDescrSetLayoutBinding[0].binding = 0;
		projectDescrSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		projectDescrSetLayoutBinding[0].descriptorCount = 1;
		projectDescrSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		projectDescrSetLayoutBinding[0].pImmutableSamplers = &ctx->defaultSampler;
		
		projectDescrSetLayoutBinding[1].binding = 1;
		projectDescrSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		projectDescrSetLayoutBinding[1].descriptorCount = 1;
		projectDescrSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		projectDescrSetLayoutBinding[1].pImmutableSamplers = &ctx->defaultSampler;

		VkDescriptorSetLayoutCreateInfo projectDescrSetLayoutCI = {};
		projectDescrSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		projectDescrSetLayoutCI.bindingCount = projectDescrSetLayoutBinding.size();
		projectDescrSetLayoutCI.pBindings = projectDescrSetLayoutBinding.data();

		VkDescriptorSetLayout projectDescrSetLayout = VK_NULL_HANDLE;
		vkCreateDescriptorSetLayout(
			ctx->vkCtx.logicalDevice,
			&projectDescrSetLayoutCI,
			nullptr,
			&projectDescrSetLayout
		);

		VkPipelineLayoutCreateInfo projectPipeLayoutCI = {};
		projectPipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		projectPipeLayoutCI.setLayoutCount = 1;
		projectPipeLayoutCI.pSetLayouts = &projectDescrSetLayout;
		projectPipeLayoutCI.pushConstantRangeCount = 1;
		projectPipeLayoutCI.pPushConstantRanges = &projectConstantRange;
		
		VkPipelineLayout projectPipeLayout = VK_NULL_HANDLE;
		vkCreatePipelineLayout(ctx->vkCtx.logicalDevice, &projectPipeLayoutCI, nullptr, &projectPipeLayout);

		VkAttachmentDescription projectColorAttachmentDescr = {};
		projectColorAttachmentDescr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		projectColorAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
		projectColorAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		projectColorAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		projectColorAttachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		projectColorAttachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		projectColorAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		projectColorAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference projectColorAttachmentRef = {};
		projectColorAttachmentRef.attachment = 0;
		projectColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription projectSubpassDescr = {};
		projectSubpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		projectSubpassDescr.colorAttachmentCount = 1;
		projectSubpassDescr.pColorAttachments = &projectColorAttachmentRef;

		VkSubpassDependency projectSubpassDependency = {};
		projectSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		projectSubpassDependency.dstSubpass = 0;
		projectSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		projectSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		projectSubpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		projectSubpassDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		projectSubpassDependency.dependencyFlags = 0;

		VkRenderPassCreateInfo projectRenderPassCI = {};
		projectRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		projectRenderPassCI.attachmentCount = 1;
		projectRenderPassCI.pAttachments = &projectColorAttachmentDescr;
		projectRenderPassCI.subpassCount = 1;
		projectRenderPassCI.pSubpasses = &projectSubpassDescr;
		projectRenderPassCI.dependencyCount = 1;
		projectRenderPassCI.pDependencies = &projectSubpassDependency;

		VkRenderPass projectRenderPass = VK_NULL_HANDLE;
		vkCreateRenderPass(ctx->vkCtx.logicalDevice, &projectRenderPassCI, nullptr, &projectRenderPass);

	VkGraphicsPipelineCreateInfo pipeCI = commonPipeCI;
	pipeCI.stageCount = projectShaders.size();
	pipeCI.pStages = projectShaders.data();
	pipeCI.layout = projectPipeLayout;
	pipeCI.renderPass = projectRenderPass;
	pipeCI.subpass = 0;

	ctx->descrSetLayouts[PIPE_GRADIENT_SUBTRACT] = projectDescrSetLayout;
	ctx->pipeLayouts[PIPE_GRADIENT_SUBTRACT] = projectPipeLayout;
	ctx->renderPasses[PIPE_GRADIENT_SUBTRACT] = projectRenderPass;

	VkPipeline pipeline = VK_NULL_HANDLE;
	vkCreateGraphicsPipelines(ctx->vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &pipeline);
	return pipeline;
}

static VkPipeline create_divergence_pipeline(FluidContext* ctx,
	const VkGraphicsPipelineCreateInfo& commonPipeCI)
{
		// creating divergence pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> divShaders = {};
		divShaders[0] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_cube_vert.spv",
			VK_SHADER_STAGE_VERTEX_BIT
		);
		divShaders[1] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_project_divergence.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		VkPushConstantRange divConstantRange = {};
		divConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		divConstantRange.offset = 0;
		divConstantRange.size = sizeof(float);//dx

		VkDescriptorSetLayoutBinding divDescrSetLayoutBinding = {};
		divDescrSetLayoutBinding.binding = 0;
		divDescrSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		divDescrSetLayoutBinding.descriptorCount = 1;
		divDescrSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		divDescrSetLayoutBinding.pImmutableSamplers = &ctx->defaultSampler;

		VkDescriptorSetLayoutCreateInfo divDescrSetLayoutCI = {};
		divDescrSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		divDescrSetLayoutCI.bindingCount = 1;
		divDescrSetLayoutCI.pBindings = &divDescrSetLayoutBinding;

		VkDescriptorSetLayout divDescrSetLayout = VK_NULL_HANDLE;
		vkCreateDescriptorSetLayout(
			ctx->vkCtx.logicalDevice,
			&divDescrSetLayoutCI,
			nullptr,
			&divDescrSetLayout
		);

		VkPipelineLayoutCreateInfo divPipeLayoutCI = {};
		divPipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		divPipeLayoutCI.setLayoutCount = 1;
		divPipeLayoutCI.pSetLayouts = &divDescrSetLayout;
		divPipeLayoutCI.pushConstantRangeCount = 1;
		divPipeLayoutCI.pPushConstantRanges = &divConstantRange;
		
		VkPipelineLayout divPipeLayout = VK_NULL_HANDLE;
		vkCreatePipelineLayout(ctx->vkCtx.logicalDevice, &divPipeLayoutCI, nullptr, &divPipeLayout);

		VkAttachmentDescription divColorAttachmentDescr = {};
		divColorAttachmentDescr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		divColorAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
		divColorAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		divColorAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		divColorAttachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		divColorAttachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		divColorAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		divColorAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference divColorAttachmentRef = {};
		divColorAttachmentRef.attachment = 0;
		divColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription divSubpassDescr = {};
		divSubpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		divSubpassDescr.colorAttachmentCount = 1;
		divSubpassDescr.pColorAttachments = &divColorAttachmentRef;

		VkSubpassDependency divSubpassDependency = {};
		divSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		divSubpassDependency.dstSubpass = 0;
		divSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		divSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		divSubpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		divSubpassDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		divSubpassDependency.dependencyFlags = 0;

		VkRenderPassCreateInfo divRenderPassCI = {};
		divRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		divRenderPassCI.attachmentCount = 1;
		divRenderPassCI.pAttachments = &divColorAttachmentDescr;
		divRenderPassCI.subpassCount = 1;
		divRenderPassCI.pSubpasses = &divSubpassDescr;
		divRenderPassCI.dependencyCount = 1;
		divRenderPassCI.pDependencies = &divSubpassDependency;

		VkRenderPass divRenderPass = VK_NULL_HANDLE;
		vkCreateRenderPass(ctx->vkCtx.logicalDevice, &divRenderPassCI, nullptr, &divRenderPass);

	VkGraphicsPipelineCreateInfo pipeCI = commonPipeCI;
	pipeCI.stageCount = divShaders.size();
	pipeCI.pStages = divShaders.data();
	pipeCI.layout = divPipeLayout;
	pipeCI.renderPass = divRenderPass;
	pipeCI.subpass = 0;

	ctx->descrSetLayouts[PIPE_DIVERGENCE] = divDescrSetLayout;
	ctx->pipeLayouts[PIPE_DIVERGENCE] = divPipeLayout;
	ctx->renderPasses[PIPE_DIVERGENCE] = divRenderPass;

	VkPipeline pipeline = VK_NULL_HANDLE;
	vkCreateGraphicsPipelines(ctx->vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &pipeline);
	return pipeline;
}

static VkPipeline create_force_pipeline(FluidContext* ctx,
	const VkGraphicsPipelineCreateInfo& commonPipeCI)
{
		// creating force pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> forceShaders = {};
		forceShaders[0] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_cube_vert.spv",
			VK_SHADER_STAGE_VERTEX_BIT
		);
		forceShaders[1] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_apply_force.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		VkPushConstantRange forceConstantRange = {};
		forceConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		forceConstantRange.offset = 0;
		forceConstantRange.size = sizeof(ForceConstants);

		VkDescriptorSetLayoutBinding forceDescrSetLayoutBinding = {};
		forceDescrSetLayoutBinding.binding = 0;
		forceDescrSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		forceDescrSetLayoutBinding.descriptorCount = 1;
		forceDescrSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		forceDescrSetLayoutBinding.pImmutableSamplers = &ctx->defaultSampler;

		VkDescriptorSetLayoutCreateInfo forceDescrSetLayoutCI = {};
		forceDescrSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		forceDescrSetLayoutCI.bindingCount = 1;
		forceDescrSetLayoutCI.pBindings = &forceDescrSetLayoutBinding;

		VkDescriptorSetLayout forceDescrSetLayout = VK_NULL_HANDLE;
		vkCreateDescriptorSetLayout(
			ctx->vkCtx.logicalDevice,
			&forceDescrSetLayoutCI,
			nullptr,
			&forceDescrSetLayout
		);

		VkPipelineLayoutCreateInfo forcePipeLayoutCI = {};
		forcePipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		forcePipeLayoutCI.setLayoutCount = 1;
		forcePipeLayoutCI.pSetLayouts = &forceDescrSetLayout;
		forcePipeLayoutCI.pushConstantRangeCount = 1;
		forcePipeLayoutCI.pPushConstantRanges = &forceConstantRange;
		
		VkPipelineLayout forcePipeLayout = VK_NULL_HANDLE;
		vkCreatePipelineLayout(ctx->vkCtx.logicalDevice, &forcePipeLayoutCI, nullptr, &forcePipeLayout);

		VkAttachmentDescription forceColorAttachmentDescr = {};
		forceColorAttachmentDescr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		forceColorAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
		forceColorAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		forceColorAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		forceColorAttachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		forceColorAttachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		forceColorAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		forceColorAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference forceColorAttachmentRef = {};
		forceColorAttachmentRef.attachment = 0;
		forceColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription forceSubpassDescr = {};
		forceSubpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		forceSubpassDescr.colorAttachmentCount = 1;
		forceSubpassDescr.pColorAttachments = &forceColorAttachmentRef;

		VkSubpassDependency forceSubpassDependency = {};
		forceSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		forceSubpassDependency.dstSubpass = 0;
		forceSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		forceSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		forceSubpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		forceSubpassDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		forceSubpassDependency.dependencyFlags = 0;

		VkRenderPassCreateInfo forceRenderPassCI = {};
		forceRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		forceRenderPassCI.attachmentCount = 1;
		forceRenderPassCI.pAttachments = &forceColorAttachmentDescr;
		forceRenderPassCI.subpassCount = 1;
		forceRenderPassCI.pSubpasses = &forceSubpassDescr;
		forceRenderPassCI.dependencyCount = 1;
		forceRenderPassCI.pDependencies = &forceSubpassDependency;

		VkRenderPass forceRenderPass = VK_NULL_HANDLE;
		vkCreateRenderPass(ctx->vkCtx.logicalDevice, &forceRenderPassCI, nullptr, &forceRenderPass);

	VkGraphicsPipelineCreateInfo pipeCI = commonPipeCI;
	pipeCI.stageCount = forceShaders.size();
	pipeCI.pStages = forceShaders.data();
	pipeCI.layout = forcePipeLayout;
	pipeCI.renderPass = forceRenderPass;
	pipeCI.subpass = 0;

	ctx->descrSetLayouts[PIPE_EXTERNAL_FORCES] = forceDescrSetLayout;
	ctx->pipeLayouts[PIPE_EXTERNAL_FORCES] = forcePipeLayout;
	ctx->renderPasses[PIPE_EXTERNAL_FORCES] = forceRenderPass;

	VkPipeline pipeline = VK_NULL_HANDLE;
	vkCreateGraphicsPipelines(ctx->vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &pipeline);
	return pipeline;
}

struct JacobiPipes
{
	VkPipeline viscocityPipe;
	VkPipeline pressurePipe;
};

static JacobiPipes create_jacobi_pipelines(FluidContext* ctx,
	const VkGraphicsPipelineCreateInfo& commonPipeCI)
{
		// creating jacobi solver pipelines

		std::array<VkPipelineShaderStageCreateInfo, 2> jacobiShadersVisc = {};
		jacobiShadersVisc[0] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_cube_vert.spv",
			VK_SHADER_STAGE_VERTEX_BIT
		);
		jacobiShadersVisc[1] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_jacobi_solver.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		std::array<VkPipelineShaderStageCreateInfo, 2> jacobiShadersPressure = {};
		jacobiShadersPressure[0] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_cube_vert.spv",
			VK_SHADER_STAGE_VERTEX_BIT
		);
		jacobiShadersPressure[1] = fillShaderStageCreateInfo(
			ctx->vkCtx.logicalDevice,
			"shaders/spv/fluid_jacobi_solver_pressure.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		std::array<VkDescriptorSetLayoutBinding, 2> descrSetLayoutBinding = {};
		descrSetLayoutBinding[0].binding = 0;
		descrSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descrSetLayoutBinding[0].descriptorCount = 1;
		descrSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		descrSetLayoutBinding[0].pImmutableSamplers = &ctx->defaultSampler;

		descrSetLayoutBinding[1].binding = 1;
		descrSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descrSetLayoutBinding[1].descriptorCount = 1;
		descrSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		descrSetLayoutBinding[1].pImmutableSamplers = &ctx->defaultSampler;

		VkDescriptorSetLayoutCreateInfo descrSetCI = {};
		descrSetCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descrSetCI.bindingCount = descrSetLayoutBinding.size();
		descrSetCI.pBindings = descrSetLayoutBinding.data();

		VkPushConstantRange pushConstantRangeJacobi = {};
		pushConstantRangeJacobi.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRangeJacobi.offset = 0;
		pushConstantRangeJacobi.size = sizeof(SolverConstants);

		VkDescriptorSetLayout jacobiDescrSetLayout = {};
		vkCreateDescriptorSetLayout(ctx->vkCtx.logicalDevice, &descrSetCI, nullptr, &jacobiDescrSetLayout);

		VkPipelineLayoutCreateInfo jacobiPipeLayoutCI = {};
		jacobiPipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		jacobiPipeLayoutCI.setLayoutCount = 1;
		jacobiPipeLayoutCI.pSetLayouts = &jacobiDescrSetLayout;
		jacobiPipeLayoutCI.pushConstantRangeCount = 1;
		jacobiPipeLayoutCI.pPushConstantRanges = &pushConstantRangeJacobi;

		VkPipelineLayout jacobiPipeLayout = VK_NULL_HANDLE;
		vkCreatePipelineLayout(ctx->vkCtx.logicalDevice, &jacobiPipeLayoutCI, nullptr, &jacobiPipeLayout);

		VkAttachmentDescription jacobiAttachmentDescr = {};
		jacobiAttachmentDescr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		jacobiAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
		jacobiAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		jacobiAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		jacobiAttachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		jacobiAttachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		jacobiAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		jacobiAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference jacobiAttachmentRef = {};
		jacobiAttachmentRef.attachment = 0;
		jacobiAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription jacobiSubpassDescr = {};
		jacobiSubpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		jacobiSubpassDescr.colorAttachmentCount = 1;
		jacobiSubpassDescr.pColorAttachments = &jacobiAttachmentRef;

		VkSubpassDependency jacobiSubpassDep = {};
		jacobiSubpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
		jacobiSubpassDep.dstSubpass = 0;
		jacobiSubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		jacobiSubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		jacobiSubpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		jacobiSubpassDep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		jacobiSubpassDep.dependencyFlags = VK_FLAGS_NONE;

		VkRenderPassCreateInfo jacobiRenderPassCI = {};
		jacobiRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		jacobiRenderPassCI.attachmentCount = 1;
		jacobiRenderPassCI.pAttachments = &jacobiAttachmentDescr;
		jacobiRenderPassCI.subpassCount = 1;
		jacobiRenderPassCI.pSubpasses = &jacobiSubpassDescr;
		jacobiRenderPassCI.dependencyCount = 1;
		jacobiRenderPassCI.pDependencies = &jacobiSubpassDep;

		VkRenderPass jacobiRenderPass = VK_NULL_HANDLE;
		vkCreateRenderPass(ctx->vkCtx.logicalDevice, &jacobiRenderPassCI, nullptr, &jacobiRenderPass);
		

	VkGraphicsPipelineCreateInfo pipeCI[2] = {commonPipeCI, commonPipeCI};
	pipeCI[0].stageCount = jacobiShadersVisc.size();
	pipeCI[0].pStages = jacobiShadersVisc.data();
	pipeCI[0].layout = jacobiPipeLayout;
	pipeCI[0].renderPass = jacobiRenderPass;
	pipeCI[0].subpass = 0;
	
	pipeCI[1].stageCount = jacobiShadersPressure.size();
	pipeCI[1].pStages = jacobiShadersPressure.data();
	pipeCI[1].layout = jacobiPipeLayout;
	pipeCI[1].renderPass = jacobiRenderPass;
	pipeCI[1].subpass = 0;

	ctx->descrSetLayouts[PIPE_JACOBI_SOLVER_VISCOCITY] = jacobiDescrSetLayout;
	ctx->pipeLayouts[PIPE_JACOBI_SOLVER_VISCOCITY] = jacobiPipeLayout;
	ctx->renderPasses[PIPE_JACOBI_SOLVER_VISCOCITY] = jacobiRenderPass;
	ctx->descrSetLayouts[PIPE_JACOBI_SOLVER_PRESSURE] = jacobiDescrSetLayout;
	ctx->pipeLayouts[PIPE_JACOBI_SOLVER_PRESSURE] = jacobiPipeLayout;
	ctx->renderPasses[PIPE_JACOBI_SOLVER_PRESSURE] = jacobiRenderPass;

	VkPipeline pipelines[2] = {};
	vkCreateGraphicsPipelines(ctx->vkCtx.logicalDevice, VK_NULL_HANDLE, 2, pipeCI, nullptr, pipelines);
	return {pipelines[0], pipelines[1]};
}

static void create_pipelines(FluidContext* ctx)
{
	std::array<VkVertexInputBindingDescription, 1> bindingDescrs = {};
	bindingDescrs[0].binding = 0;
	bindingDescrs[0].stride = sizeof(Vec3);
	bindingDescrs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 1> vertexAttribDescrs = {};
	vertexAttribDescrs[0].location = 0;
	vertexAttribDescrs[0].binding = 0;
	vertexAttribDescrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttribDescrs[0].offset = 0;

	auto vertexInputStateCI = fillVertexInputStateCreateInfo(
		bindingDescrs.data(), bindingDescrs.size(),
		vertexAttribDescrs.data(), vertexAttribDescrs.size());
	auto inputAssemblyStateCI = fillInputAssemblyCreateInfo();

	VkViewport viewport = {
		0.f, 0.f,
		static_cast<float>(ctx->window.windowExtent.width),
		static_cast<float>(ctx->window.windowExtent.height),
		0.f, 1.f
	};

	VkRect2D scissors = {};
	scissors.offset = {0, 0};
	scissors.extent = {ctx->window.windowExtent.width, ctx->window.windowExtent.height};
	auto viewportStateCI = fillViewportStateCreateInfo(viewport, scissors);

	auto rasterStateCI = fillRasterizationStateCreateInfo(
		VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE
	);

	auto multisampleStateCI = fillMultisampleStateCreateInfo();

	auto depthStencilCI = fillDepthStencilStateCreateInfo(VK_COMPARE_OP_LESS_OR_EQUAL);

	//color blending is used for mixing color of transparent objects
	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = VK_FALSE;
	auto colorBlendStateCI = fillColorBlendStateCreateInfo(blendAttachmentState);

	auto dynStateCI = fillDynamicStateCreateInfo(nullptr, 0);

	VkGraphicsPipelineCreateInfo commonPipeStateInfo = {}; 
	commonPipeStateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	commonPipeStateInfo.pVertexInputState = &vertexInputStateCI;
	commonPipeStateInfo.pInputAssemblyState = &inputAssemblyStateCI;
	commonPipeStateInfo.pViewportState = &viewportStateCI;
	commonPipeStateInfo.pRasterizationState = &rasterStateCI;
	commonPipeStateInfo.pMultisampleState = &multisampleStateCI;
	commonPipeStateInfo.pDepthStencilState = &depthStencilCI;
	commonPipeStateInfo.pColorBlendState = &colorBlendStateCI;
	commonPipeStateInfo.pDynamicState = &dynStateCI;

	ctx->pipelines[PIPE_ADVECTION] = create_advect_pipeline(ctx, commonPipeStateInfo);
	ctx->pipelines[PIPE_VORTICITY_CURL] = create_curl_pipeline(ctx, commonPipeStateInfo);
	ctx->pipelines[PIPE_VORTICITY_FORCE] = create_vorticity_pipeline(ctx, commonPipeStateInfo);
	auto jacobiPipes = create_jacobi_pipelines(ctx, commonPipeStateInfo);
	ctx->pipelines[PIPE_JACOBI_SOLVER_VISCOCITY] = jacobiPipes.viscocityPipe;
	ctx->pipelines[PIPE_JACOBI_SOLVER_PRESSURE] = jacobiPipes.pressurePipe;
	ctx->pipelines[PIPE_EXTERNAL_FORCES] = create_force_pipeline(ctx, commonPipeStateInfo);
	ctx->pipelines[PIPE_DIVERGENCE] = create_divergence_pipeline(ctx, commonPipeStateInfo);
	ctx->pipelines[PIPE_GRADIENT_SUBTRACT] = create_project_pipeline(ctx, commonPipeStateInfo);
	ctx->pipelines[PIPE_PRESENT] = create_present_pipeline(ctx, commonPipeStateInfo);
}

static void allocate_descriptor_sets(FluidContext* ctx)
{
	//velocity advection stage descriptor sets
	VkDescriptorPoolSize descrPoolSize = {};
	descrPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	descrPoolSize.descriptorCount = 64;

	VkDescriptorPoolCreateInfo descrPoolCreateInfo = {};
	descrPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descrPoolCreateInfo.maxSets = DSI_INDEX_COUNT * SWAPCHAIN_IMAGE_COUNT;
	descrPoolCreateInfo.poolSizeCount = 1;
	descrPoolCreateInfo.pPoolSizes = &descrPoolSize;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	vkCreateDescriptorPool(ctx->vkCtx.logicalDevice, &descrPoolCreateInfo, nullptr, &descriptorPool);

	VkDescriptorSetAllocateInfo descrSetAllocateInfo = {};
	descrSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descrSetAllocateInfo.descriptorPool = descriptorPool;
	descrSetAllocateInfo.descriptorSetCount = DSI_INDEX_COUNT;

	VkDescriptorSetLayout layouts[DSI_INDEX_COUNT] = {
		ctx->descrSetLayouts[PIPE_ADVECTION],//advect_vel1
		ctx->descrSetLayouts[PIPE_VORTICITY_CURL],
		ctx->descrSetLayouts[PIPE_VORTICITY_FORCE],
		ctx->descrSetLayouts[PIPE_JACOBI_SOLVER_VISCOCITY],//visc1
		ctx->descrSetLayouts[PIPE_JACOBI_SOLVER_VISCOCITY],//visc2
		ctx->descrSetLayouts[PIPE_EXTERNAL_FORCES],//forces
		ctx->descrSetLayouts[PIPE_EXTERNAL_FORCES], //color forces
		ctx->descrSetLayouts[PIPE_DIVERGENCE],//project_div
		ctx->descrSetLayouts[PIPE_JACOBI_SOLVER_PRESSURE],//project_pressure_1
		ctx->descrSetLayouts[PIPE_JACOBI_SOLVER_PRESSURE],//project_pressure_2
		ctx->descrSetLayouts[PIPE_GRADIENT_SUBTRACT],//project_grad_sub
		ctx->descrSetLayouts[PIPE_ADVECTION],//advect_col1
		ctx->descrSetLayouts[PIPE_PRESENT] //final present
	};

	descrSetAllocateInfo.pSetLayouts = layouts;

	for(std::size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++)
	{
		auto allocateStatus = vkAllocateDescriptorSets(
			ctx->vkCtx.logicalDevice,
			&descrSetAllocateInfo,
			ctx->descrSetsPerFrame[i]
		);
	}
}

static void update_pressure_descr_set(FluidContext* ctx, int imageIndex, int divergenceTextureIndex)
{
	VkDescriptorImageInfo pressureImageInfo1 = {};
	pressureImageInfo1.sampler = ctx->defaultSampler;
	pressureImageInfo1.imageView = ctx->simTextures[RT_PRESSURE_FIRST].view;
	pressureImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo pressureImageInfo2 = {};
	pressureImageInfo2.sampler = ctx->defaultSampler;
	pressureImageInfo2.imageView = ctx->simTextures[RT_PRESSURE_SECOND].view;
	pressureImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo divergentVelImageInfo = {};
	divergentVelImageInfo.sampler = ctx->defaultSampler;
	divergentVelImageInfo.imageView = ctx->simTextures[divergenceTextureIndex].view;
	divergentVelImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::array<VkWriteDescriptorSet, 4> pressureWriteDescrSets = {};

	pressureWriteDescrSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	pressureWriteDescrSets[0].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_PRESSURE_1];
	pressureWriteDescrSets[0].dstBinding = 0;
	pressureWriteDescrSets[0].dstArrayElement = 0;
	pressureWriteDescrSets[0].descriptorCount = 1;
	pressureWriteDescrSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pressureWriteDescrSets[0].pImageInfo = &pressureImageInfo1;

	pressureWriteDescrSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	pressureWriteDescrSets[1].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_PRESSURE_1];
	pressureWriteDescrSets[1].dstBinding = 1;
	pressureWriteDescrSets[1].dstArrayElement = 0;
	pressureWriteDescrSets[1].descriptorCount = 1;
	pressureWriteDescrSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pressureWriteDescrSets[1].pImageInfo = &divergentVelImageInfo;
	
	pressureWriteDescrSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	pressureWriteDescrSets[2].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_PRESSURE_2];
	pressureWriteDescrSets[2].dstBinding = 0;
	pressureWriteDescrSets[2].dstArrayElement = 0;
	pressureWriteDescrSets[2].descriptorCount = 1;
	pressureWriteDescrSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pressureWriteDescrSets[2].pImageInfo = &pressureImageInfo2;
	
	pressureWriteDescrSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	pressureWriteDescrSets[3].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_PRESSURE_2];
	pressureWriteDescrSets[3].dstBinding = 1;
	pressureWriteDescrSets[3].dstArrayElement = 0;
	pressureWriteDescrSets[3].descriptorCount = 1;
	pressureWriteDescrSets[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pressureWriteDescrSets[3].pImageInfo = &divergentVelImageInfo;

	vkUpdateDescriptorSets(
		ctx->vkCtx.logicalDevice,
		pressureWriteDescrSets.size(),
		pressureWriteDescrSets.data(),
		0, nullptr
	);
}

static void update_viscocity_descr_set(FluidContext* ctx, int imageIndex, int velocityTextureIndex)
{
	VkDescriptorImageInfo viscImageInfo1 = {};
	viscImageInfo1.sampler = ctx->defaultSampler;
	viscImageInfo1.imageView = ctx->simTextures[RT_VELOCITY_FIRST].view;
	// viscImageInfo1.imageView = simTextures[velocityTextureIndex].view;
	viscImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo viscImageInfo2 = {};
	viscImageInfo2.sampler = ctx->defaultSampler;
	viscImageInfo2.imageView = ctx->simTextures[RT_VELOCITY_SECOND].view;
	// viscImageInfo2.imageView = simTextures[velocityTextureIndex == RT_VELOCITY_FIRST ?
	// 	RT_VELOCITY_SECOND : RT_VELOCITY_FIRST].view;
	viscImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::array<VkWriteDescriptorSet, 4> viscWriteDescrSets = {};

	viscWriteDescrSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	viscWriteDescrSets[0].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_VISCOCITY_1];
	viscWriteDescrSets[0].dstBinding = 0;
	viscWriteDescrSets[0].dstArrayElement = 0;
	viscWriteDescrSets[0].descriptorCount = 1;
	viscWriteDescrSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	viscWriteDescrSets[0].pImageInfo = &viscImageInfo1;

	viscWriteDescrSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	viscWriteDescrSets[1].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_VISCOCITY_1];
	viscWriteDescrSets[1].dstBinding = 1;
	viscWriteDescrSets[1].dstArrayElement = 0;
	viscWriteDescrSets[1].descriptorCount = 1;
	viscWriteDescrSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	viscWriteDescrSets[1].pImageInfo = &viscImageInfo1;
	
	viscWriteDescrSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	viscWriteDescrSets[2].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_VISCOCITY_2];
	viscWriteDescrSets[2].dstBinding = 0;
	viscWriteDescrSets[2].dstArrayElement = 0;
	viscWriteDescrSets[2].descriptorCount = 1;
	viscWriteDescrSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	viscWriteDescrSets[2].pImageInfo = &viscImageInfo2;
	
	viscWriteDescrSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	viscWriteDescrSets[3].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_VISCOCITY_2];
	viscWriteDescrSets[3].dstBinding = 1;
	viscWriteDescrSets[3].dstArrayElement = 0;
	viscWriteDescrSets[3].descriptorCount = 1;
	viscWriteDescrSets[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	viscWriteDescrSets[3].pImageInfo = &viscImageInfo2;

	vkUpdateDescriptorSets(
		ctx->vkCtx.logicalDevice,
		viscWriteDescrSets.size(),
		viscWriteDescrSets.data(),
		0, nullptr
	);
}

static void update_advect_velocity_descriptor_set(FluidContext* ctx, int imageIndex, int velocityTextureIndex)
{
	std::array<VkWriteDescriptorSet, 2> advectVelocityWriteDescrSet = {}; 

	VkDescriptorImageInfo velocityImageInfo = {};
	velocityImageInfo.sampler = ctx->defaultSampler;
	velocityImageInfo.imageView = ctx->simTextures[velocityTextureIndex].view;
	velocityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	advectVelocityWriteDescrSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	advectVelocityWriteDescrSet[0].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_ADVECT_VELOCITY];
	advectVelocityWriteDescrSet[0].dstBinding = 0;
	advectVelocityWriteDescrSet[0].dstArrayElement = 0;
	advectVelocityWriteDescrSet[0].descriptorCount = 1;
	advectVelocityWriteDescrSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	advectVelocityWriteDescrSet[0].pImageInfo = &velocityImageInfo;

	advectVelocityWriteDescrSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	advectVelocityWriteDescrSet[1].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_ADVECT_VELOCITY];
	advectVelocityWriteDescrSet[1].dstBinding = 1;
	advectVelocityWriteDescrSet[1].dstArrayElement = 0;
	advectVelocityWriteDescrSet[1].descriptorCount = 1;
	advectVelocityWriteDescrSet[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	advectVelocityWriteDescrSet[1].pImageInfo = &velocityImageInfo;

	vkUpdateDescriptorSets(
		ctx->vkCtx.logicalDevice,
		advectVelocityWriteDescrSet.size(),
		advectVelocityWriteDescrSet.data(),
		0, nullptr
	);
}

static void update_curl_descriptor_set(FluidContext* ctx, int imageIndex, int velocityTextureIndex)
{
	std::array<VkWriteDescriptorSet, 1> curlVelocityWriteDescrSet = {}; 

	VkDescriptorImageInfo velocityImageInfo = {};
	velocityImageInfo.sampler = ctx->defaultSampler;
	velocityImageInfo.imageView = ctx->simTextures[velocityTextureIndex].view;
	velocityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	curlVelocityWriteDescrSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	curlVelocityWriteDescrSet[0].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_VORTICITY_CURL];
	curlVelocityWriteDescrSet[0].dstBinding = 0;
	curlVelocityWriteDescrSet[0].dstArrayElement = 0;
	curlVelocityWriteDescrSet[0].descriptorCount = 1;
	curlVelocityWriteDescrSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	curlVelocityWriteDescrSet[0].pImageInfo = &velocityImageInfo;

	vkUpdateDescriptorSets(
		ctx->vkCtx.logicalDevice,
		curlVelocityWriteDescrSet.size(),
		curlVelocityWriteDescrSet.data(),
		0, nullptr
	);
}

static void update_vorticity_force_descriptor_set(FluidContext* ctx, int imageIndex, int curlTextureIndex, int velocityTextureIndex)
{
	std::array<VkWriteDescriptorSet, 2> vforceWriteDescrSet = {}; 

	VkDescriptorImageInfo velocityImageInfo = {};
	velocityImageInfo.sampler = ctx->defaultSampler;
	velocityImageInfo.imageView = ctx->simTextures[velocityTextureIndex].view;
	velocityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo curlImageInfo = {};
	curlImageInfo.sampler = ctx->defaultSampler;
	curlImageInfo.imageView = ctx->simTextures[curlTextureIndex].view;
	curlImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vforceWriteDescrSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	vforceWriteDescrSet[0].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_VORTICITY_FORCE];
	vforceWriteDescrSet[0].dstBinding = 0;
	vforceWriteDescrSet[0].dstArrayElement = 0;
	vforceWriteDescrSet[0].descriptorCount = 1;
	vforceWriteDescrSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	vforceWriteDescrSet[0].pImageInfo = &curlImageInfo;

	vforceWriteDescrSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	vforceWriteDescrSet[1].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_VORTICITY_FORCE];
	vforceWriteDescrSet[1].dstBinding = 1;
	vforceWriteDescrSet[1].dstArrayElement = 0;
	vforceWriteDescrSet[1].descriptorCount = 1;
	vforceWriteDescrSet[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	vforceWriteDescrSet[1].pImageInfo = &velocityImageInfo;

	vkUpdateDescriptorSets(
		ctx->vkCtx.logicalDevice,
		vforceWriteDescrSet.size(),
		vforceWriteDescrSet.data(),
		0, nullptr
	);
}

static void update_advect_color_descriptor_sets(FluidContext* ctx, int imageIndex, int velocityTextureIndex, int colorTextureIndex)
{
	std::array<VkWriteDescriptorSet, 2> advectColorWriteDescrSet = {}; 

	VkDescriptorImageInfo velocityImageInfo = {};
	velocityImageInfo.sampler = ctx->defaultSampler;
	velocityImageInfo.imageView = ctx->simTextures[velocityTextureIndex].view;
	velocityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo colorImageInfo = {};
	colorImageInfo.sampler = ctx->defaultSampler;
	colorImageInfo.imageView = ctx->simTextures[colorTextureIndex].view;
	colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	advectColorWriteDescrSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	advectColorWriteDescrSet[0].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_ADVECT_COLOR];
	advectColorWriteDescrSet[0].dstBinding = 0;
	advectColorWriteDescrSet[0].dstArrayElement = 0;
	advectColorWriteDescrSet[0].descriptorCount = 1;
	advectColorWriteDescrSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	advectColorWriteDescrSet[0].pImageInfo = &velocityImageInfo;

	advectColorWriteDescrSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	advectColorWriteDescrSet[1].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_ADVECT_COLOR];
	advectColorWriteDescrSet[1].dstBinding = 1;
	advectColorWriteDescrSet[1].dstArrayElement = 0;
	advectColorWriteDescrSet[1].descriptorCount = 1;
	advectColorWriteDescrSet[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	advectColorWriteDescrSet[1].pImageInfo = &colorImageInfo;

	vkUpdateDescriptorSets(
		ctx->vkCtx.logicalDevice,
		advectColorWriteDescrSet.size(),
		advectColorWriteDescrSet.data(),
		0, nullptr
	);
}
	
static void update_forces_descr_set(FluidContext* ctx, int imageIndex, int textureIndex, int descrSetIndex)
{
	VkWriteDescriptorSet forceWriteDescrSet = {}; 
	VkDescriptorImageInfo forceImageInfo = {};
	forceImageInfo.sampler = ctx->defaultSampler;
	forceImageInfo.imageView = ctx->simTextures[textureIndex].view;
	forceImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	forceWriteDescrSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	forceWriteDescrSet.dstSet = ctx->descrSetsPerFrame[imageIndex][descrSetIndex];
	forceWriteDescrSet.dstBinding = 0;
	forceWriteDescrSet.dstArrayElement = 0;
	forceWriteDescrSet.descriptorCount = 1;
	forceWriteDescrSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	forceWriteDescrSet.pImageInfo = &forceImageInfo;

	vkUpdateDescriptorSets(ctx->vkCtx.logicalDevice, 1, &forceWriteDescrSet, 0, nullptr);
}
	
static void update_divergence_descr_set(FluidContext* ctx, int imageIndex, int textureIndex)
{
	VkWriteDescriptorSet divWriteDescrSet = {}; 
	VkDescriptorImageInfo divImageInfo = {};
	divImageInfo.sampler = ctx->defaultSampler;
	divImageInfo.imageView = ctx->simTextures[textureIndex].view;
	divImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	divWriteDescrSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	divWriteDescrSet.dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_DIVERGENCE];
	divWriteDescrSet.dstBinding = 0;
	divWriteDescrSet.dstArrayElement = 0;
	divWriteDescrSet.descriptorCount = 1;
	divWriteDescrSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	divWriteDescrSet.pImageInfo = &divImageInfo;

	vkUpdateDescriptorSets(ctx->vkCtx.logicalDevice, 1, &divWriteDescrSet, 0, nullptr);
}

static void update_pressure_subtract_descr_set(FluidContext* ctx, int imageIndex, int velocityTextureIndex, int pressureTextureIndex)
{
	std::array<VkWriteDescriptorSet, 2> subWriteDescrSet = {}; 

	VkDescriptorImageInfo subImageInfo1 = {};
	subImageInfo1.sampler = ctx->defaultSampler;
	subImageInfo1.imageView = ctx->simTextures[velocityTextureIndex].view;
	subImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo subImageInfo2 = {};
	subImageInfo2.sampler = ctx->defaultSampler;
	subImageInfo2.imageView = ctx->simTextures[pressureTextureIndex].view;
	subImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	subWriteDescrSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	subWriteDescrSet[0].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_GRADIENT_SUBTRACT];
	subWriteDescrSet[0].dstBinding = 0;
	subWriteDescrSet[0].dstArrayElement = 0;
	subWriteDescrSet[0].descriptorCount = 1;
	subWriteDescrSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	subWriteDescrSet[0].pImageInfo = &subImageInfo1;

	subWriteDescrSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	subWriteDescrSet[1].dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_GRADIENT_SUBTRACT];
	subWriteDescrSet[1].dstBinding = 1;
	subWriteDescrSet[1].dstArrayElement = 0;
	subWriteDescrSet[1].descriptorCount = 1;
	subWriteDescrSet[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	subWriteDescrSet[1].pImageInfo = &subImageInfo2;

	vkUpdateDescriptorSets(
		ctx->vkCtx.logicalDevice,
		subWriteDescrSet.size(),
		subWriteDescrSet.data(),
		0,
		nullptr
	);
}

static void update_present_descr_set(FluidContext* ctx, int imageIndex, int colorTextureIndex)
{
	VkDescriptorImageInfo presentImageInfo = {};
	presentImageInfo.sampler = ctx->defaultSampler;
	presentImageInfo.imageView = ctx->simTextures[colorTextureIndex].view;
	presentImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet presentWriteDescrSet = {};
	presentWriteDescrSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	presentWriteDescrSet.dstSet = ctx->descrSetsPerFrame[imageIndex][DSI_PRESENT];
	presentWriteDescrSet.dstBinding = 0;
	presentWriteDescrSet.descriptorCount = 1;
	presentWriteDescrSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	presentWriteDescrSet.pImageInfo = &presentImageInfo;

	vkUpdateDescriptorSets(ctx->vkCtx.logicalDevice, 1, &presentWriteDescrSet, 0, nullptr);
}

static void create_frame_buffers(FluidContext* ctx)
{
	ctx->frameBuffers[RT_VELOCITY_FIRST] = create_frame_buffer(
		ctx->vkCtx.logicalDevice,
		ctx->simTextures[RT_VELOCITY_FIRST].view,
		ctx->renderPasses[PIPE_ADVECTION],
		ctx->window.windowExtent.width,
		ctx->window.windowExtent.height
	);

	ctx->frameBuffers[RT_VELOCITY_SECOND] = create_frame_buffer(
		ctx->vkCtx.logicalDevice,
		ctx->simTextures[RT_VELOCITY_SECOND].view,
		ctx->renderPasses[PIPE_ADVECTION],
		ctx->window.windowExtent.width,
		ctx->window.windowExtent.height
	);

	ctx->frameBuffers[RT_CURL_FIRST] = create_frame_buffer(
		ctx->vkCtx.logicalDevice,
		ctx->simTextures[RT_CURL_FIRST].view,
		ctx->renderPasses[PIPE_VORTICITY_CURL],
		ctx->window.windowExtent.width,
		ctx->window.windowExtent.height
	);

	ctx->frameBuffers[RT_CURL_SECOND] = create_frame_buffer(
		ctx->vkCtx.logicalDevice,
		ctx->simTextures[RT_CURL_SECOND].view,
		ctx->renderPasses[PIPE_VORTICITY_CURL],
		ctx->window.windowExtent.width,
		ctx->window.windowExtent.height
	);

	ctx->frameBuffers[RT_PRESSURE_FIRST] = create_frame_buffer(
		ctx->vkCtx.logicalDevice,
		ctx->simTextures[RT_PRESSURE_FIRST].view,
		ctx->renderPasses[PIPE_JACOBI_SOLVER_PRESSURE],
		ctx->window.windowExtent.width,
		ctx->window.windowExtent.height
	);

	ctx->frameBuffers[RT_PRESSURE_SECOND] = create_frame_buffer(
		ctx->vkCtx.logicalDevice,
		ctx->simTextures[RT_PRESSURE_SECOND].view,
		ctx->renderPasses[PIPE_JACOBI_SOLVER_PRESSURE],
		ctx->window.windowExtent.width,
		ctx->window.windowExtent.height
	);

	ctx->frameBuffers[RT_COLOR_FIRST] = create_frame_buffer(
		ctx->vkCtx.logicalDevice,
		ctx->simTextures[RT_COLOR_FIRST].view,
		ctx->renderPasses[PIPE_ADVECTION],
		ctx->window.windowExtent.width,
		ctx->window.windowExtent.height
	);

	ctx->frameBuffers[RT_COLOR_SECOND] = create_frame_buffer(
		ctx->vkCtx.logicalDevice,
		ctx->simTextures[RT_COLOR_SECOND].view,
		ctx->renderPasses[PIPE_ADVECTION],
		ctx->window.windowExtent.width,
		ctx->window.windowExtent.height
	);

	//present frame buffers to render to
	for(std::size_t i = 0; i < ctx->swapchain.imageCount; i++)
	{
		ctx->swapchain.runtime.frameBuffers[i] = create_frame_buffer(
			ctx->vkCtx.logicalDevice,
			ctx->swapchain.runtime.imageViews[i],
			ctx->renderPasses[PIPE_PRESENT],
			ctx->window.windowExtent.width,
			ctx->window.windowExtent.height
		);
	}

}

static void insert_full_memory_barrier(VkCommandBuffer cmdBuffer)
{
	VkMemoryBarrier memoryBarrier = {};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

	vkCmdPipelineBarrier(
		cmdBuffer,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0,
		1, &memoryBarrier,
		0, nullptr,
		0, nullptr
	);
}

static void clear_pressure_texture(FluidContext* ctx, VkCommandBuffer commandBuffer, int textureIndex)
{
	VkClearColorValue clearColor = {};
	clearColor.float32[0] = 0.f;
	clearColor.float32[0] = 0.f;
	clearColor.float32[0] = 0.f;
	clearColor.float32[0] = 0.f;

	VkImageSubresourceRange range = {};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
		
	insert_image_memory_barrier(
		ctx,
		commandBuffer,
		ctx->simTextures[textureIndex].image,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		ctx->simTextures[textureIndex].layout,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	vkCmdClearColorImage(
		commandBuffer,
		ctx->simTextures[textureIndex].image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&clearColor,
		1,
		&range
	);

	insert_image_memory_barrier(
		ctx,
		commandBuffer,
		ctx->simTextures[textureIndex].image,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	ctx->simTextures[textureIndex].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static void cmd_begin_debug_label(FluidContext* ctx, VkCommandBuffer commandBuffer, const char* labelName, Vec4 color)
{
	if(ctx->vkCtx.hasDebugUtilsExtension)
	{
		VkDebugUtilsLabelEXT labelInfo = {};
		labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		labelInfo.pLabelName = labelName;
		labelInfo.color[0] = color.R; 
		labelInfo.color[1] = color.G;
		labelInfo.color[2] = color.B; 
		labelInfo.color[3] = color.A;
		vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &labelInfo);
	}
}

static void cmd_end_debug_label(FluidContext* ctx, VkCommandBuffer commandBuffer)
{
	if(ctx->vkCtx.hasDebugUtilsExtension)
	{
		vkCmdEndDebugUtilsLabelEXT(commandBuffer);
	}
}

static void assign_names_to_vulkan_objects(FluidContext* ctx)
{
	const char* RenderTargetNames[RT_MAX_COUNT] = 
	{
		"RT_VELOCITY_FIRST",
		"RT_VELOCITY_SECOND",
		"RT_CURL_FIRST",
		"RT_CURL_SECOND",
		"RT_PRESSURE_FIRST",
		"RT_PRESSURE_SECOND",
		"RT_COLOR_FIRST",
		"RT_COLOR_SECOND",
	};

	const char* DescrSetNames[DSI_INDEX_COUNT] = 
	{
		"DSI_ADVECT_VELOCITY",
		"DSI_VORTICITY_CURL",
		"DSI_VORTICITY_FORCE",
		"DSI_VISCOCITY_1",
		"DSI_VISCOCITY_2",
		"DSI_FORCES",
		"DSI_FORCES_COLOR",
		"DSI_DIVERGENCE",
		"DSI_PRESSURE_1",
		"DSI_PRESSURE_2",
		"DSI_GRADIENT_SUBTRACT",
		"DSI_ADVECT_COLOR",
		"DSI_PRESENT",
	};

	for(std::size_t textureIndex = 0; textureIndex < ctx->simTextures.size(); textureIndex++)
	{
		VkDebugUtilsObjectNameInfoEXT debugTextureInfo = {};
		debugTextureInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		debugTextureInfo.objectType = VK_OBJECT_TYPE_IMAGE;
		debugTextureInfo.objectHandle = (uint64_t)ctx->simTextures[textureIndex].image;
		debugTextureInfo.pObjectName = RenderTargetNames[textureIndex];
						
		vkSetDebugUtilsObjectNameEXT(ctx->vkCtx.logicalDevice, &debugTextureInfo);
	}

	for(std::size_t imageIndex = 0; imageIndex < SWAPCHAIN_IMAGE_COUNT; imageIndex++)
	{
		for(std::size_t descrSetIndex = 0; descrSetIndex < DSI_INDEX_COUNT; descrSetIndex++)
		{
			VkDebugUtilsObjectNameInfoEXT debugTextureInfo = {};
			debugTextureInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			debugTextureInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
			debugTextureInfo.objectHandle = (uint64_t)ctx->descrSetsPerFrame[imageIndex][descrSetIndex];
			debugTextureInfo.pObjectName = DescrSetNames[descrSetIndex];
			
			vkSetDebugUtilsObjectNameEXT(ctx->vkCtx.logicalDevice, &debugTextureInfo);
		}
	}
}

static void allocate_command_buffers(FluidContext* ctx, int commandBufferCount)
{
	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmdPoolCreateInfo.queueFamilyIndex = ctx->vkCtx.queueFamIdx;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	vkCreateCommandPool(ctx->vkCtx.logicalDevice, &cmdPoolCreateInfo, nullptr, &commandPool);

	VkCommandBufferAllocateInfo cmdBufferAllocInfo = {};
	cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferAllocInfo.commandPool = commandPool;
	cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufferAllocInfo.commandBufferCount = commandBufferCount;
	
	vkAllocateCommandBuffers(
		ctx->vkCtx.logicalDevice,
		&cmdBufferAllocInfo,
		ctx->commandBuffers.data()
	);
}

static int record_advect_velocity_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputVelocityTextureIndex)
{
	AdvectConstants advectConstants = {};
	advectConstants.timestep = ctx->timeStep;
	advectConstants.gridScale = ctx->dx;
#if defined(WARP_PICTURE_MODE)
	advectConstants.dissipation = 1.f;//0.99f;
#else
	advectConstants.dissipation = 0.99f;
#endif

	update_advect_velocity_descriptor_set(ctx, commandBufferIndex, inputVelocityTextureIndex);
	int advectVelocityRenderTarget = inputVelocityTextureIndex == RT_VELOCITY_FIRST ? 
		RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;
	{

		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "advect velocity pass", {0.713f, 0.921f, 0.556f, 1.f});

		VkRenderPassBeginInfo advectPassBeginInfo = {};
		advectPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		advectPassBeginInfo.renderPass = ctx->renderPasses[PIPE_ADVECTION];
		advectPassBeginInfo.framebuffer = ctx->frameBuffers[advectVelocityRenderTarget];
		advectPassBeginInfo.renderArea.offset = {0, 0};
		advectPassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		advectPassBeginInfo.clearValueCount = 0;
		advectPassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &advectPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_ADVECTION]);
		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_ADVECTION],
			0,
			1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_ADVECT_VELOCITY],
			0, nullptr
		);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdPushConstants(
			ctx->commandBuffers[commandBufferIndex], ctx->pipeLayouts[PIPE_ADVECTION],
			VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			sizeof(AdvectConstants), &advectConstants
		);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}
	// insert_full_memory_barrier(commandBuffers[commandBufferIndex]);
	insert_image_memory_barrier(
		ctx,
		ctx->commandBuffers[commandBufferIndex],
		ctx->simTextures[advectVelocityRenderTarget].image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	return advectVelocityRenderTarget;
}

static int record_curl_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputVelocityTextureIndex)
{
	update_curl_descriptor_set(ctx, commandBufferIndex, inputVelocityTextureIndex);
	int curlRenderTarget = inputVelocityTextureIndex == RT_VELOCITY_FIRST ? RT_CURL_FIRST : RT_CURL_SECOND;
	//curl render pass
	{
		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "curl render pass", {0.513f, 0.321f, 0.956f, 1.f});

		VkRenderPassBeginInfo curlPassBeginInfo = {};
		curlPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		curlPassBeginInfo.renderPass = ctx->renderPasses[PIPE_VORTICITY_CURL];
		curlPassBeginInfo.framebuffer = ctx->frameBuffers[curlRenderTarget];
		curlPassBeginInfo.renderArea.offset = {0, 0};
		curlPassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		curlPassBeginInfo.clearValueCount = 0;
		curlPassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &curlPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_VORTICITY_CURL]);
		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_VORTICITY_CURL],
			0,
			1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_VORTICITY_CURL],
			0, nullptr
		);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdPushConstants(
			ctx->commandBuffers[commandBufferIndex], ctx->pipeLayouts[PIPE_VORTICITY_CURL],
			VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			sizeof(float), &ctx->dx
		);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}

	insert_image_memory_barrier(
		ctx,
		ctx->commandBuffers[commandBufferIndex],
		ctx->simTextures[curlRenderTarget].image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	return curlRenderTarget;
}

static int record_vorticity_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int curlInputTextureIndex,
	int inputVelocityTextureIndex)
{
	update_vorticity_force_descriptor_set(ctx, commandBufferIndex, curlInputTextureIndex, inputVelocityTextureIndex);
	int vorticityRenderTarget = inputVelocityTextureIndex == RT_VELOCITY_FIRST ?
		RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;
	//vorticity force render pass
	{
		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "vorticity force render pass", {0.213f, 0.121f, 0.556f, 1.f});

		VkRenderPassBeginInfo vforcePassBeginInfo = {};
		vforcePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		vforcePassBeginInfo.renderPass = ctx->renderPasses[PIPE_VORTICITY_FORCE];
		vforcePassBeginInfo.framebuffer = ctx->frameBuffers[vorticityRenderTarget];
		vforcePassBeginInfo.renderArea.offset = {0, 0};
		vforcePassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		vforcePassBeginInfo.clearValueCount = 0;
		vforcePassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &vforcePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_VORTICITY_FORCE]);
		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_VORTICITY_FORCE],
			0,
			1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_VORTICITY_FORCE],
			0, nullptr
		);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		VorticityConstants vforceConstants = {};
		vforceConstants.confinement = 1.f;
		vforceConstants.timestep = ctx->timeStep;
		vforceConstants.texelSize = ctx->dx;

		vkCmdPushConstants(
			ctx->commandBuffers[commandBufferIndex], ctx->pipeLayouts[PIPE_VORTICITY_FORCE],
			VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			sizeof(VorticityConstants), &vforceConstants
		);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}

	insert_image_memory_barrier(
		ctx,
		ctx->commandBuffers[commandBufferIndex],
		ctx->simTextures[vorticityRenderTarget].image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	return vorticityRenderTarget;
}

static int record_viscocity_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputVelocityTextureIndex)
{
	update_viscocity_descr_set(ctx, commandBufferIndex, inputVelocityTextureIndex);
	int viscPassRenderTarget = inputVelocityTextureIndex == RT_VELOCITY_FIRST ? 
		RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;

	cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "viscocity pass", {0.854f, 0.556f, 0.921f, 1.f});

	for(std::size_t i = 0; i < JACOBI_ITERATIONS; i++)
	{

		VkRenderPassBeginInfo viscPassBeginInfo = {};
		viscPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		viscPassBeginInfo.renderPass = ctx->renderPasses[PIPE_JACOBI_SOLVER_VISCOCITY];
		viscPassBeginInfo.framebuffer = ctx->frameBuffers[viscPassRenderTarget];
		viscPassBeginInfo.renderArea.offset = {0, 0};
		viscPassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		viscPassBeginInfo.clearValueCount = 0;
		viscPassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &viscPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_JACOBI_SOLVER_VISCOCITY]);

		int descriptorSetIndex = viscPassRenderTarget == RT_VELOCITY_FIRST ? DSI_VISCOCITY_2 : DSI_VISCOCITY_1;

		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_JACOBI_SOLVER_VISCOCITY],
			0, 1, &ctx->descrSetsPerFrame[commandBufferIndex][descriptorSetIndex],
			0, nullptr
		);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		SolverConstants solverConstants = {};
		solverConstants.alpha = (ctx->dx * ctx->dx) / (ctx->kv * ctx->timeStep);
		solverConstants.beta = 4 + solverConstants.alpha;
		solverConstants.texelSize = ctx->dx;
		vkCmdPushConstants(
			ctx->commandBuffers[commandBufferIndex],
			ctx->pipeLayouts[PIPE_JACOBI_SOLVER_VISCOCITY],
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(SolverConstants), &solverConstants
		);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		insert_image_memory_barrier(
			ctx,
			ctx->commandBuffers[commandBufferIndex],
			ctx->simTextures[viscPassRenderTarget].image,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		viscPassRenderTarget = viscPassRenderTarget == RT_VELOCITY_FIRST ? RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;

	}
	cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);

	return viscPassRenderTarget;
}

static int record_force_velocity_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int viscocityTetxtureIndex)
{
	RenderTarget forcePassRenderTarget = viscocityTetxtureIndex == RT_VELOCITY_FIRST ? 
		RT_VELOCITY_FIRST: RT_VELOCITY_SECOND;
	update_forces_descr_set(ctx, commandBufferIndex, viscocityTetxtureIndex == RT_VELOCITY_FIRST ? 
		RT_VELOCITY_SECOND: RT_VELOCITY_FIRST, DSI_FORCES);

		
	//force velocity pass
	{
		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "Force velocity pass", {0.254f, 0.329f, 0.847f, 1.f});
		VkRenderPassBeginInfo forcePassBeginInfo = {};
		forcePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		forcePassBeginInfo.renderPass = ctx->renderPasses[PIPE_EXTERNAL_FORCES];
		forcePassBeginInfo.framebuffer = ctx->frameBuffers[forcePassRenderTarget];
		forcePassBeginInfo.renderArea.offset = {0, 0};
		forcePassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		forcePassBeginInfo.clearValueCount = 0;
		forcePassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &forcePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_EXTERNAL_FORCES]);

		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_EXTERNAL_FORCES],
			0, 1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_FORCES],
			0, nullptr
		);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			
			
		ForceConstants forceConsts = {};
		forceConsts.impulseRadius = 0.025f;

		static bool isMouseBeingDragged = false;
		static Vec2 prevMousePos = {};

		if(isMouseBtnPressed(MouseBtn::LeftBtn) && !isMouseBeingDragged)
		{
			isMouseBeingDragged = true;
			auto pos = getMousePos();
			prevMousePos.x = (float)pos.x * ctx->dx;
			prevMousePos.y = (float)pos.y * ctx->dx;
			// magma::log::error("prev = {} {}", prevMousePos.x, prevMousePos.y);
		}
		else if(!isMouseBtnPressed(MouseBtn::LeftBtn))
		{
			isMouseBeingDragged = false;
		}
		else if(isMouseBeingDragged)
		{
			auto currentMousePos = getMousePos();

			forceConsts.mousePos = {(float)currentMousePos.x * ctx->dx, (float)currentMousePos.y * ctx->dx};
			// magma::log::error("current = {} {}", forceConsts.mousePos.x, forceConsts.mousePos.y);

			forceConsts.force = {
				(forceConsts.mousePos.x - prevMousePos.x)* 15000.f, 
				(forceConsts.mousePos.y - prevMousePos.y)* 15000.f,
				0.f, 0.f
			};

			prevMousePos = forceConsts.mousePos;
		}

		vkCmdPushConstants(ctx->commandBuffers[commandBufferIndex], ctx->pipeLayouts[PIPE_EXTERNAL_FORCES],
			VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ForceConstants), &forceConsts);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}
		
	insert_image_memory_barrier(
		ctx,
		ctx->commandBuffers[commandBufferIndex],
		ctx->simTextures[forcePassRenderTarget].image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	return forcePassRenderTarget;
}

static int record_force_color_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputColorTextureIndex)
{
	//force color pass
	update_forces_descr_set(ctx, commandBufferIndex, inputColorTextureIndex, DSI_FORCES_COLOR);
	int forceColorPassRenderTarget = inputColorTextureIndex == RT_COLOR_FIRST ?
		RT_COLOR_SECOND : RT_COLOR_FIRST;
	{
		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "Force color pass", {0.2f, 0.1f, 1.f, 1.f});

		VkRenderPassBeginInfo forcePassBeginInfo = {};
		forcePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		forcePassBeginInfo.renderPass = ctx->renderPasses[PIPE_EXTERNAL_FORCES];
		forcePassBeginInfo.framebuffer = ctx->frameBuffers[forceColorPassRenderTarget];
		forcePassBeginInfo.renderArea.offset = {0, 0};
		forcePassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		forcePassBeginInfo.clearValueCount = 0;
		forcePassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &forcePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_EXTERNAL_FORCES]);

		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_EXTERNAL_FORCES],
			0, 1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_FORCES_COLOR],
			0, nullptr
		);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			
			
		ForceConstants forceConsts = {};
		forceConsts.impulseRadius = 0.025f;

		static bool isMouseBeingDragged = false;

		if(isMouseBtnPressed(MouseBtn::LeftBtn) && !isMouseBeingDragged)
		{
			isMouseBeingDragged = true;
		}
		else if(!isMouseBtnPressed(MouseBtn::LeftBtn))
		{
			isMouseBeingDragged = false;
		}
		else if(isMouseBeingDragged)
		{
			auto currentMousePos = getMousePos();
			forceConsts.mousePos = {(float)currentMousePos.x * ctx->dx, (float)currentMousePos.y * ctx->dx};
			forceConsts.force = {0.082, 0.976, 0.901, 1.f};
		}

		vkCmdPushConstants(ctx->commandBuffers[commandBufferIndex], ctx->pipeLayouts[PIPE_EXTERNAL_FORCES],
			VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ForceConstants), &forceConsts);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}
		
	insert_image_memory_barrier(
		ctx,
		ctx->commandBuffers[commandBufferIndex],
		ctx->simTextures[forceColorPassRenderTarget].image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	return forceColorPassRenderTarget;
}

static int record_divergence_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputVelocityTextureIndex)
{
	int divergencePassRenderTarget = inputVelocityTextureIndex == RT_VELOCITY_FIRST ?
		RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;

	update_divergence_descr_set(ctx, commandBufferIndex, inputVelocityTextureIndex);
	//divergence pass
	{
		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "Divergence pass", {0.254f, 0.847f, 0.839f, 1.f});

		VkRenderPassBeginInfo divPassBeginInfo = {};
		divPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		divPassBeginInfo.renderPass = ctx->renderPasses[PIPE_DIVERGENCE];
		divPassBeginInfo.framebuffer = ctx->frameBuffers[divergencePassRenderTarget];
		divPassBeginInfo.renderArea.offset = {0, 0};
		divPassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		divPassBeginInfo.clearValueCount = 0;
		divPassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &divPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_DIVERGENCE]);

		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_DIVERGENCE],
			0, 1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_DIVERGENCE],
			0, nullptr
		);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdPushConstants(ctx->commandBuffers[commandBufferIndex], ctx->pipeLayouts[PIPE_DIVERGENCE],
			VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &ctx->dx);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}

	insert_image_memory_barrier(
		ctx,
		ctx->commandBuffers[commandBufferIndex],
		ctx->simTextures[divergencePassRenderTarget].image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	return divergencePassRenderTarget;
}

static int record_pressure_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int divergenceInputTextureIndex)
{
	update_pressure_descr_set(ctx, commandBufferIndex, divergenceInputTextureIndex);

	cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "Pressure pass", {0.996f, 0.933f, 0.384f, 1.f});
	//pressure pass
	clear_pressure_texture(ctx, ctx->commandBuffers[commandBufferIndex], RT_PRESSURE_FIRST);
	for(std::size_t i = 0; i < JACOBI_ITERATIONS; i++)
	{
		bool evenIteration = !(bool)(i % 2);
			
		VkRenderPassBeginInfo pressurePassBeginInfo = {};
		pressurePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		pressurePassBeginInfo.renderPass = ctx->renderPasses[PIPE_JACOBI_SOLVER_PRESSURE];
		pressurePassBeginInfo.framebuffer = ctx->frameBuffers[evenIteration ? RT_PRESSURE_SECOND : RT_PRESSURE_FIRST];
		pressurePassBeginInfo.renderArea.offset = {0, 0};
		pressurePassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		pressurePassBeginInfo.clearValueCount = 0;
		pressurePassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &pressurePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_JACOBI_SOLVER_PRESSURE]);

		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_JACOBI_SOLVER_PRESSURE],
			0, 1, &ctx->descrSetsPerFrame[commandBufferIndex][evenIteration ? DSI_PRESSURE_1 : DSI_PRESSURE_2],
			0, nullptr
		);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		SolverConstants solverConstants = {};
		solverConstants.alpha = -(ctx->dx * ctx->dx) ;
		solverConstants.beta = 4;
		solverConstants.texelSize = ctx->dx;
		vkCmdPushConstants(
			ctx->commandBuffers[commandBufferIndex],
			ctx->pipeLayouts[PIPE_JACOBI_SOLVER_PRESSURE],
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(SolverConstants), &solverConstants
		);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		insert_image_memory_barrier(
			ctx,
			ctx->commandBuffers[commandBufferIndex],
			ctx->simTextures[evenIteration ? RT_PRESSURE_SECOND : RT_PRESSURE_FIRST].image,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

	}

	cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	return JACOBI_ITERATIONS % 2 == 0 ? RT_PRESSURE_SECOND : RT_PRESSURE_FIRST;	
}

static int record_pressure_subtract_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputVelocityTextureIndex,
	int inputPressureTextureIndex,
	int outputRenderTarget)
{
	//pressure subtract pass

	int subtractPassRenderTarget = outputRenderTarget;

	update_pressure_subtract_descr_set(ctx, commandBufferIndex, inputVelocityTextureIndex, inputPressureTextureIndex);
	{
		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "Pressure subtract pass",{0.384f, 0.996f, 0.639f,1.f});

		VkRenderPassBeginInfo subtractPassBeginInfo = {};
		subtractPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		subtractPassBeginInfo.renderPass = ctx->renderPasses[PIPE_GRADIENT_SUBTRACT];
		subtractPassBeginInfo.framebuffer = ctx->frameBuffers[subtractPassRenderTarget];
		subtractPassBeginInfo.renderArea.offset = {0, 0};
		subtractPassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		subtractPassBeginInfo.clearValueCount = 0;
		subtractPassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &subtractPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_GRADIENT_SUBTRACT]);

		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_GRADIENT_SUBTRACT],
			0, 1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_GRADIENT_SUBTRACT],
			0, nullptr
		);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdPushConstants(ctx->commandBuffers[commandBufferIndex], ctx->pipeLayouts[PIPE_GRADIENT_SUBTRACT],
			VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &ctx->dx);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}

	insert_image_memory_barrier(
		ctx,
		ctx->commandBuffers[commandBufferIndex],
		ctx->simTextures[subtractPassRenderTarget].image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	return subtractPassRenderTarget;
}

static int record_advect_color_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputVelocityTextureIndex,
	int inputColorTextureIndex)
{
	AdvectConstants advectConstants = {};
	advectConstants.timestep = ctx->timeStep;
	advectConstants.gridScale = ctx->dx;
#if defined(WARP_PICTURE_MODE)
	advectConstants.dissipation = 1.f;//0.99f;
#else
	advectConstants.dissipation = 0.99f;
#endif

	update_advect_color_descriptor_sets(ctx, commandBufferIndex, inputVelocityTextureIndex, inputColorTextureIndex);

	int advectColorRenderTarget = inputColorTextureIndex == RT_COLOR_FIRST ?
		RT_COLOR_SECOND : RT_COLOR_FIRST;
	//  advect for color
	{
		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "Advect color pass", {0.556f, 0.384f, 0.996f, 1.f});

		VkRenderPassBeginInfo advectColorPassBeginInfo = {};
		advectColorPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		advectColorPassBeginInfo.renderPass = ctx->renderPasses[PIPE_ADVECTION];
		advectColorPassBeginInfo.framebuffer = ctx->frameBuffers[advectColorRenderTarget];
		advectColorPassBeginInfo.renderArea.offset = {0, 0};
		advectColorPassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		advectColorPassBeginInfo.clearValueCount = 0;
		advectColorPassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &advectColorPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_ADVECTION]);
		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_ADVECTION],
			0,
			1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_ADVECT_COLOR],
			0, nullptr
		);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdPushConstants(
			ctx->commandBuffers[commandBufferIndex], ctx->pipeLayouts[PIPE_ADVECTION],
			VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			sizeof(AdvectConstants), &advectConstants
		);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);

		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}

	insert_image_memory_barrier(
		ctx,
		ctx->commandBuffers[commandBufferIndex],
		ctx->simTextures[advectColorRenderTarget].image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	return advectColorRenderTarget;
}

static void record_present_render_pass(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputColorTextureIndex)
{
	//present pass
	update_present_descr_set(ctx, commandBufferIndex, inputColorTextureIndex);
	{
		cmd_begin_debug_label(ctx, ctx->commandBuffers[commandBufferIndex], "Present pass", {0.996f, 0.384f, 0.447f, 1.f});
		VkRenderPassBeginInfo presentColorPassBeginInfo = {};
		presentColorPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		presentColorPassBeginInfo.renderPass = ctx->renderPasses[PIPE_PRESENT];
		presentColorPassBeginInfo.framebuffer = ctx->swapchain.runtime.frameBuffers[commandBufferIndex];
		presentColorPassBeginInfo.renderArea.offset = {0, 0};
		presentColorPassBeginInfo.renderArea.extent = ctx->window.windowExtent;
		presentColorPassBeginInfo.clearValueCount = 0;
		presentColorPassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(ctx->commandBuffers[commandBufferIndex], &presentColorPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(ctx->commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[PIPE_PRESENT]);
		vkCmdBindDescriptorSets(
			ctx->commandBuffers[commandBufferIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ctx->pipeLayouts[PIPE_PRESENT],
			0,
			1, &ctx->descrSetsPerFrame[commandBufferIndex][DSI_PRESENT],
			0, nullptr
		);
			
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(ctx->commandBuffers[commandBufferIndex], 0, 1, &ctx->deviceVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(ctx->commandBuffers[commandBufferIndex], ctx->deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		const std::uint32_t indexCount = 6; 
		vkCmdDrawIndexed(ctx->commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);
#if defined(DRAW_FLUID_PARAMS)
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ctx->commandBuffers[commandBufferIndex]);
#endif
		vkCmdEndRenderPass(ctx->commandBuffers[commandBufferIndex]);
			
		cmd_end_debug_label(ctx, ctx->commandBuffers[commandBufferIndex]);
	}
}

static void record_command_buffer(
	FluidContext* ctx,
	int commandBufferIndex,
	int inputVelocityTextureIndex,
	int inputColorTextureIndex,
	int* outputVelocityTextureIndex,
	int* outputColorTextureIndex)
{
	auto windowExtent = ctx->window.windowExtent;

	const float dx = 1.f / (float)std::max(windowExtent.width, windowExtent.height);
	const float timeStep = 0.005f;
	const float kv = 1.5f;//kinematic viscocity

	AdvectConstants advectConstants = {};
	advectConstants.timestep = timeStep;
	advectConstants.gridScale = dx;
#if defined(WARP_PICTURE_MODE)
	advectConstants.dissipation = 1.f;//0.99f;
#else
	advectConstants.dissipation = 0.99f;
#endif
	VkCommandBufferBeginInfo cmdBuffBeginInfo = {};
	cmdBuffBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vkBeginCommandBuffer(ctx->commandBuffers[commandBufferIndex], &cmdBuffBeginInfo);
		
	int advectVelocityRenderTarget = record_advect_velocity_render_pass(
		ctx, commandBufferIndex, inputVelocityTextureIndex
	);
	int curlRenderTarget = record_curl_render_pass(
		ctx, commandBufferIndex, advectVelocityRenderTarget
	);
	int vorticityRenderTarget = record_vorticity_render_pass(
		ctx, commandBufferIndex, curlRenderTarget, advectVelocityRenderTarget
	);
	int viscPassRenderTarget = record_viscocity_render_pass(
		ctx, commandBufferIndex, vorticityRenderTarget
	);
	int forcePassRenderTarget = record_force_velocity_render_pass(
		ctx, commandBufferIndex, viscPassRenderTarget
	);
#if !defined(WARP_PICTURE_MODE)
	int forceColorPassRenderTarget = record_force_color_render_pass(
		ctx, commandBufferIndex, inputColorTextureIndex
	);
#endif
	int divergencePassRenderTarget = record_divergence_render_pass(
		ctx, commandBufferIndex, forcePassRenderTarget
	);
	int pressurePassRenderTarget = record_pressure_render_pass(
		ctx, commandBufferIndex, divergencePassRenderTarget
	);
	int subtractPassRenderTarget = record_pressure_subtract_render_pass(
		ctx, commandBufferIndex, forcePassRenderTarget, pressurePassRenderTarget, divergencePassRenderTarget
	);
	int advectColorRenderTarget = record_advect_color_render_pass(
		ctx, commandBufferIndex, subtractPassRenderTarget,
#if defined(WARP_PICTURE_MODE)
			inputColorTextureIndex
#else
			forceColorPassRenderTarget
#endif
	);
	record_present_render_pass(ctx, commandBufferIndex, advectColorRenderTarget);


	vkEndCommandBuffer(ctx->commandBuffers[commandBufferIndex]);

	*outputVelocityTextureIndex = subtractPassRenderTarget;
	*outputColorTextureIndex = advectColorRenderTarget;

}

static void begin_imgui_frame()
{
	ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
		
	// static bool open = true;
	// ImGui::ShowDemoWindow(&open);
	ImGui::Begin("Fluid params");
	    ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

	static float radius = 0.25f;
	static float dissipation = 0.99f;
	ImGui::Text("Float value = %f",radius);
	ImGuiSliderFlags flags= ImGuiSliderFlags_::ImGuiSliderFlags_None;
	ImGui::DragFloat("Force impulse radius", &radius, 0.02f, 0.f, 1.f, "%.3f", flags);
	ImGui::DragFloat("dissipation", &dissipation, 0.0009f, 0.9f, 0.99f, "%.3f", flags);
	ImGui::End();

	ImGui::Render();
}

static void run_simulation_loop(FluidContext* ctx)
{
	if(ctx->vkCtx.hasDebugUtilsExtension)
	{
		assign_names_to_vulkan_objects(ctx);
	}

	allocate_command_buffers(ctx, SWAPCHAIN_IMAGE_COUNT);

#if defined(DRAW_FLUID_PARAMS)
		init_imgui_context(ctx);
#endif

	int inputVelocityTextureIndex = RT_VELOCITY_FIRST;
	int inputColorTextureIndex = RT_COLOR_FIRST;
		
	std::size_t syncIndex = 0;
	while(!windowShouldClose(ctx->window.windowHandle))
	{
		updateMessageQueue();
        				
		vkWaitForFences(ctx->vkCtx.logicalDevice, 1, &ctx->swapchain.runtime.workSubmittedFences[syncIndex], VK_TRUE, UINT64_MAX);
		vkResetFences(ctx->vkCtx.logicalDevice, 1, &ctx->swapchain.runtime.workSubmittedFences[syncIndex]);
#if defined(DRAW_FLUID_PARAMS)
		begin_imgui_frame();
#endif
		// magma::log::error("status = {}",vkStrError(r));
		std::uint32_t imageIndex = {};
		vkAcquireNextImageKHR(
			ctx->vkCtx.logicalDevice,
			ctx->swapchain.swapchain,
			UINT64_MAX,
			ctx->swapchain.runtime.imageAvailableSemaphores[syncIndex],
			VK_NULL_HANDLE,
			&imageIndex
		);
			// magma::log::debug("image at index {} has been acquired", imageIndex);

		int outputVelocityTextureIndex = {};
		int outputColorTextureIndex = {};
		record_command_buffer(
			ctx,
			imageIndex,
			inputVelocityTextureIndex,
			inputColorTextureIndex,
			&outputVelocityTextureIndex,
			&outputColorTextureIndex
		);
		inputVelocityTextureIndex = outputVelocityTextureIndex;
		inputColorTextureIndex = outputColorTextureIndex;

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &ctx->swapchain.runtime.imageAvailableSemaphores[syncIndex];
		VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.pWaitDstStageMask = &waitMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &ctx->commandBuffers[imageIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &ctx->swapchain.runtime.imageMayPresentSemaphores[syncIndex];

		auto queueSubmitStatus = vkQueueSubmit(ctx->vkCtx.graphicsQueue, 1, &submitInfo, ctx->swapchain.runtime.workSubmittedFences[syncIndex]);
		// magma::log::debug("queue submit at image {} with status {}", imageIndex, vkStrError(queueSubmitStatus));
		// auto r = vkDeviceWaitIdle(ctx->logicalDevice);

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &ctx->swapchain.runtime.imageMayPresentSemaphores[syncIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &ctx->swapchain.swapchain;
		presentInfo.pImageIndices = &imageIndex;
		// presentInfo.pResults = ;

		vkQueuePresentKHR(ctx->vkCtx.graphicsQueue, &presentInfo);

		syncIndex = (syncIndex + 1) % ctx->swapchain.imageCount;
	}
}


int main(int argc, char **argv)
{

	FluidContext ctx = {};
	if(!create_fluid_context(&ctx))
	{
		return -1;
	}

	initialise_fluid_textures(&ctx);
	create_pipelines(&ctx);
	init_vertex_and_index_buffers(&ctx);
	allocate_descriptor_sets(&ctx);
	create_frame_buffers(&ctx);
	run_simulation_loop(&ctx);
	destroy_fluid_context();
	return 0;
}