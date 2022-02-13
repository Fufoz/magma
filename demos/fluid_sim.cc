#include <magma.h>

#include <vector>
#include <array>

static constexpr int SWAPCHAIN_IMAGE_COUNT = 2;

struct Fluid
{
	explicit Fluid(VulkanGlobalContext* context, SwapChain* sc, WindowInfo* info)
	: ctx(context), swapchain(sc), window(info) {}


	bool initialise_fluid_textures()
	{
		VkExtent3D textureSize = {window->windowExtent.width, window->windowExtent.height, 1};

		for(std::size_t textureIndex = 0; textureIndex < simTextures.size(); textureIndex++)
		{
			simTextures[textureIndex] = createResourceImage(
				*ctx,
				textureSize,
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
			);

		}

		std::vector<uint8_t> hostBuffer = {};
		const std::size_t numc = 4;
		hostBuffer.resize(window->windowExtent.width * window->windowExtent.height * numc * sizeof(float));
		std::memset(hostBuffer.data(), 0, hostBuffer.size());

		Buffer stagingBuffer = createBuffer(
			*ctx,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			hostBuffer.size()
		);

		copyDataToHostVisibleBuffer(*ctx, 0, hostBuffer.data(), hostBuffer.size(), &stagingBuffer);

		auto tmpCmdPool = createCommandPool(*ctx);
		for(auto&& texture : simTextures)
		{
			pushTextureToDeviceLocalImage(tmpCmdPool, *ctx, stagingBuffer, textureSize, &texture);
		}
		vkDestroyCommandPool(ctx->logicalDevice, tmpCmdPool, nullptr);
		destroyBuffer(ctx->logicalDevice, &stagingBuffer);
		
		bool status = false;
		
		defaultSampler = createDefaultSampler(ctx->logicalDevice, &status);
		
		return status;
	}

	void init_vertex_and_index_buffers()
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
			*ctx,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vertexBufferSize
		);

		copyDataToHostVisibleBuffer(*ctx, 0, cubeVerts.data(), vertexBufferSize, &stagingVertexBuffer);
		
		deviceVertexBuffer = createBuffer(
			*ctx,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexBufferSize
		);

		VkCommandPoolCreateInfo commandPoolCI = {};
		commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCI.pNext = nullptr;
		commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCI.queueFamilyIndex = ctx->queueFamIdx;
		VkCommandPool cmdPool = VK_NULL_HANDLE;
		vkCreateCommandPool(ctx->logicalDevice, &commandPoolCI, nullptr, &cmdPool);
		pushDataToDeviceLocalBuffer(cmdPool, *ctx, stagingVertexBuffer, &deviceVertexBuffer, ctx->graphicsQueue);

		auto stagingIndexBuffer = createBuffer(
			*ctx,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			indexBufferSize
		);

		copyDataToHostVisibleBuffer(*ctx, 0, indicies.data(), indexBufferSize, &stagingIndexBuffer);
		
		deviceIndexBuffer = createBuffer(
			*ctx,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBufferSize
		);

		pushDataToDeviceLocalBuffer(cmdPool, *ctx, stagingIndexBuffer, &deviceIndexBuffer, ctx->graphicsQueue);

		destroyBuffer(ctx->logicalDevice, &stagingVertexBuffer);
		destroyBuffer(ctx->logicalDevice, &stagingIndexBuffer);
		destroyCommandPool(ctx->logicalDevice, cmdPool);
	}

	bool create_pipelines()
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
			static_cast<float>(window->windowExtent.width),
			static_cast<float>(window->windowExtent.height),
			0.f, 1.f
		};

		VkRect2D scissors = {};
		scissors.offset = {0, 0};
		scissors.extent = {window->windowExtent.width, window->windowExtent.height};
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

		// std::array<VkDynamicState, 1> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT};
		// auto dynStateCI = fillDynamicStateCreateInfo(dynamicStates.data(), dynamicStates.size());
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

		// creating advect pipeline
		std::array<VkGraphicsPipelineCreateInfo, PIPE_COUNT> pipesCreateInfo = {};


		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCI = {};
		shaderStageCI[0] = fillShaderStageCreateInfo(
			ctx->logicalDevice,
			"shaders/spv/fluid_cube_vert.spv",
			VK_SHADER_STAGE_VERTEX_BIT
		);
		shaderStageCI[1] = fillShaderStageCreateInfo(
			ctx->logicalDevice,
			"shaders/spv/fluid_advect_quantity.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		std::array<VkDescriptorSetLayoutBinding, 2> descrSetLayoutBinding = {};
		descrSetLayoutBinding[0].binding = 0;
		descrSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descrSetLayoutBinding[0].descriptorCount = 1;
		descrSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		descrSetLayoutBinding[0].pImmutableSamplers = &defaultSampler;

		descrSetLayoutBinding[1].binding = 1;
		descrSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descrSetLayoutBinding[1].descriptorCount = 1;
		descrSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		descrSetLayoutBinding[1].pImmutableSamplers = &defaultSampler;

		VkDescriptorSetLayoutCreateInfo descrSetCI = {};
		descrSetCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descrSetCI.bindingCount = descrSetLayoutBinding.size();
		descrSetCI.pBindings = descrSetLayoutBinding.data();

		VkDescriptorSetLayout advectDescrSetLayout = {};
		vkCreateDescriptorSetLayout(ctx->logicalDevice, &descrSetCI, nullptr, &advectDescrSetLayout);

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
		vkCreatePipelineLayout(ctx->logicalDevice, &advectPipeLayoutCI, nullptr, &advectPipeLayout);

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
		vkCreateRenderPass(ctx->logicalDevice, &advectRenderPassCI, nullptr, &advectRenderPass);
		
		pipesCreateInfo[PIPE_ADVECTION] = commonPipeStateInfo;
		pipesCreateInfo[PIPE_ADVECTION].stageCount = shaderStageCI.size();
		pipesCreateInfo[PIPE_ADVECTION].pStages = shaderStageCI.data();
		pipesCreateInfo[PIPE_ADVECTION].layout = advectPipeLayout;
		pipesCreateInfo[PIPE_ADVECTION].renderPass = advectRenderPass;
		pipesCreateInfo[PIPE_ADVECTION].subpass = 0;



		// creating jacobi solver pipelines

		std::array<VkPipelineShaderStageCreateInfo, 2> jacobiShadersVisc = {};
		jacobiShadersVisc[0] = shaderStageCI[0];
		jacobiShadersVisc[1] = fillShaderStageCreateInfo(
			ctx->logicalDevice,
			"shaders/spv/fluid_jacobi_solver.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		std::array<VkPipelineShaderStageCreateInfo, 2> jacobiShadersPressure = {};
		jacobiShadersPressure[0] = shaderStageCI[0];
		jacobiShadersPressure[1] = fillShaderStageCreateInfo(
			ctx->logicalDevice,
			"shaders/spv/fluid_jacobi_solver_pressure.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		
		VkPushConstantRange pushConstantRangeJacobi = {};
		pushConstantRangeJacobi.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRangeJacobi.offset = 0;
		pushConstantRangeJacobi.size = sizeof(SolverConstants);

		VkDescriptorSetLayout jacobiDescrSetLayout = {};
		vkCreateDescriptorSetLayout(ctx->logicalDevice, &descrSetCI, nullptr, &jacobiDescrSetLayout);

		VkPipelineLayoutCreateInfo jacobiPipeLayoutCI = {};
		jacobiPipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		jacobiPipeLayoutCI.setLayoutCount = 1;
		jacobiPipeLayoutCI.pSetLayouts = &jacobiDescrSetLayout;
		jacobiPipeLayoutCI.pushConstantRangeCount = 1;
		jacobiPipeLayoutCI.pPushConstantRanges = &pushConstantRangeJacobi;

		VkPipelineLayout jacobiPipeLayout = VK_NULL_HANDLE;
		vkCreatePipelineLayout(ctx->logicalDevice, &jacobiPipeLayoutCI, nullptr, &jacobiPipeLayout);

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
		vkCreateRenderPass(ctx->logicalDevice, &jacobiRenderPassCI, nullptr, &jacobiRenderPass);
		
		pipesCreateInfo[PIPE_JACOBI_SOLVER_VISCOCITY] = commonPipeStateInfo;
		pipesCreateInfo[PIPE_JACOBI_SOLVER_VISCOCITY].stageCount = jacobiShadersVisc.size();
		pipesCreateInfo[PIPE_JACOBI_SOLVER_VISCOCITY].pStages = jacobiShadersVisc.data();
		pipesCreateInfo[PIPE_JACOBI_SOLVER_VISCOCITY].layout = jacobiPipeLayout;
		pipesCreateInfo[PIPE_JACOBI_SOLVER_VISCOCITY].renderPass = jacobiRenderPass;

		pipesCreateInfo[PIPE_JACOBI_SOLVER_PRESSURE] = commonPipeStateInfo;
		pipesCreateInfo[PIPE_JACOBI_SOLVER_PRESSURE].stageCount = jacobiShadersPressure.size();
		pipesCreateInfo[PIPE_JACOBI_SOLVER_PRESSURE].pStages = jacobiShadersPressure.data();
		pipesCreateInfo[PIPE_JACOBI_SOLVER_PRESSURE].layout = jacobiPipeLayout;
		pipesCreateInfo[PIPE_JACOBI_SOLVER_PRESSURE].renderPass = jacobiRenderPass;
		
		// creating force pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> forceShaders = {};
		forceShaders[0] = shaderStageCI[0];
		forceShaders[1] = fillShaderStageCreateInfo(
			ctx->logicalDevice,
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
		forceDescrSetLayoutBinding.pImmutableSamplers = &defaultSampler;

		VkDescriptorSetLayoutCreateInfo forceDescrSetLayoutCI = {};
		forceDescrSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		forceDescrSetLayoutCI.bindingCount = 1;
		forceDescrSetLayoutCI.pBindings = &forceDescrSetLayoutBinding;

		VkDescriptorSetLayout forceDescrSetLayout = VK_NULL_HANDLE;
		vkCreateDescriptorSetLayout(
			ctx->logicalDevice,
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
		vkCreatePipelineLayout(ctx->logicalDevice, &forcePipeLayoutCI, nullptr, &forcePipeLayout);

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
		vkCreateRenderPass(ctx->logicalDevice, &forceRenderPassCI, nullptr, &forceRenderPass);

		pipesCreateInfo[PIPE_EXTERNAL_FORCES] = commonPipeStateInfo;
		pipesCreateInfo[PIPE_EXTERNAL_FORCES].stageCount = forceShaders.size();
		pipesCreateInfo[PIPE_EXTERNAL_FORCES].pStages = forceShaders.data();
		pipesCreateInfo[PIPE_EXTERNAL_FORCES].layout = forcePipeLayout;
		pipesCreateInfo[PIPE_EXTERNAL_FORCES].renderPass = forceRenderPass;

		// creating divergence pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> divShaders = {};
		divShaders[0] = shaderStageCI[0];
		divShaders[1] = fillShaderStageCreateInfo(
			ctx->logicalDevice,
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
		divDescrSetLayoutBinding.pImmutableSamplers = &defaultSampler;

		VkDescriptorSetLayoutCreateInfo divDescrSetLayoutCI = {};
		divDescrSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		divDescrSetLayoutCI.bindingCount = 1;
		divDescrSetLayoutCI.pBindings = &divDescrSetLayoutBinding;

		VkDescriptorSetLayout divDescrSetLayout = VK_NULL_HANDLE;
		vkCreateDescriptorSetLayout(
			ctx->logicalDevice,
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
		vkCreatePipelineLayout(ctx->logicalDevice, &divPipeLayoutCI, nullptr, &divPipeLayout);

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
		vkCreateRenderPass(ctx->logicalDevice, &divRenderPassCI, nullptr, &divRenderPass);

		pipesCreateInfo[PIPE_DIVERGENCE] = commonPipeStateInfo;
		pipesCreateInfo[PIPE_DIVERGENCE].stageCount = divShaders.size();
		pipesCreateInfo[PIPE_DIVERGENCE].pStages = divShaders.data();
		pipesCreateInfo[PIPE_DIVERGENCE].layout = divPipeLayout;
		pipesCreateInfo[PIPE_DIVERGENCE].renderPass = divRenderPass;

		// creating project pipeline

		std::array<VkPipelineShaderStageCreateInfo, 2> projectShaders = {};
		projectShaders[0] = shaderStageCI[0];
		projectShaders[1] = fillShaderStageCreateInfo(
			ctx->logicalDevice,
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
		projectDescrSetLayoutBinding[0].pImmutableSamplers = &defaultSampler;
		
		projectDescrSetLayoutBinding[1].binding = 1;
		projectDescrSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		projectDescrSetLayoutBinding[1].descriptorCount = 1;
		projectDescrSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		projectDescrSetLayoutBinding[1].pImmutableSamplers = &defaultSampler;

		VkDescriptorSetLayoutCreateInfo projectDescrSetLayoutCI = {};
		projectDescrSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		projectDescrSetLayoutCI.bindingCount = projectDescrSetLayoutBinding.size();
		projectDescrSetLayoutCI.pBindings = projectDescrSetLayoutBinding.data();

		VkDescriptorSetLayout projectDescrSetLayout = VK_NULL_HANDLE;
		vkCreateDescriptorSetLayout(
			ctx->logicalDevice,
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
		vkCreatePipelineLayout(ctx->logicalDevice, &projectPipeLayoutCI, nullptr, &projectPipeLayout);

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
		vkCreateRenderPass(ctx->logicalDevice, &projectRenderPassCI, nullptr, &projectRenderPass);

		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT] = commonPipeStateInfo;
		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT].stageCount = projectShaders.size();
		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT].pStages = projectShaders.data();
		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT].layout = projectPipeLayout;
		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT].renderPass = projectRenderPass;

		//creating present pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> presentShaders = {};
		presentShaders[0] = shaderStageCI[0];
		presentShaders[1] = fillShaderStageCreateInfo(
			ctx->logicalDevice,
			"shaders/spv/fluid_ink_present.spv",
			VK_SHADER_STAGE_FRAGMENT_BIT
		);

		VkDescriptorSetLayoutBinding presentDescrSetLayoutBinding = {};
		presentDescrSetLayoutBinding.binding = 0;
		presentDescrSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		presentDescrSetLayoutBinding.descriptorCount = 1;
		presentDescrSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		presentDescrSetLayoutBinding.pImmutableSamplers = &defaultSampler;

		VkDescriptorSetLayoutCreateInfo presentDescrSetLayoutCI = {};
		presentDescrSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		presentDescrSetLayoutCI.bindingCount = 1;
		presentDescrSetLayoutCI.pBindings = &presentDescrSetLayoutBinding;

		VkDescriptorSetLayout presentDescrSetLayout = VK_NULL_HANDLE;
		vkCreateDescriptorSetLayout(
			ctx->logicalDevice,
			&presentDescrSetLayoutCI,
			nullptr,
			&presentDescrSetLayout
		);

		VkPipelineLayoutCreateInfo presentPipeLayoutCI = {};
		presentPipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		presentPipeLayoutCI.setLayoutCount = 1;
		presentPipeLayoutCI.pSetLayouts = &presentDescrSetLayout;
		
		VkPipelineLayout presentPipeLayout = VK_NULL_HANDLE;
		vkCreatePipelineLayout(ctx->logicalDevice, &presentPipeLayoutCI, nullptr, &presentPipeLayout);

		VkAttachmentDescription presentColorAttachmentDescr = {};
		presentColorAttachmentDescr.format = swapchain->imageFormat;
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
		vkCreateRenderPass(ctx->logicalDevice, &presentRenderPassCI, nullptr, &presentRenderPass);

		pipesCreateInfo[PIPE_PRESENT] = commonPipeStateInfo;
		pipesCreateInfo[PIPE_PRESENT].stageCount = presentShaders.size();
		pipesCreateInfo[PIPE_PRESENT].pStages = presentShaders.data();
		pipesCreateInfo[PIPE_PRESENT].layout = presentPipeLayout;
		pipesCreateInfo[PIPE_PRESENT].renderPass = presentRenderPass;

		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT] = commonPipeStateInfo;
		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT].stageCount = projectShaders.size();
		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT].pStages = projectShaders.data();
		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT].layout = projectPipeLayout;
		pipesCreateInfo[PIPE_GRADIENT_SUBTRACT].renderPass = projectRenderPass;

		descrSetLayouts[PIPE_ADVECTION] = advectDescrSetLayout;
		descrSetLayouts[PIPE_JACOBI_SOLVER_VISCOCITY] = jacobiDescrSetLayout;
		descrSetLayouts[PIPE_JACOBI_SOLVER_PRESSURE] = jacobiDescrSetLayout;
		descrSetLayouts[PIPE_EXTERNAL_FORCES] = forceDescrSetLayout;
		descrSetLayouts[PIPE_DIVERGENCE] = divDescrSetLayout;
		descrSetLayouts[PIPE_GRADIENT_SUBTRACT] = projectDescrSetLayout;
		descrSetLayouts[PIPE_PRESENT] = presentDescrSetLayout;

		pipeLayouts[PIPE_ADVECTION] = advectPipeLayout;
		pipeLayouts[PIPE_JACOBI_SOLVER_VISCOCITY] = jacobiPipeLayout;
		pipeLayouts[PIPE_JACOBI_SOLVER_PRESSURE] = jacobiPipeLayout;
		pipeLayouts[PIPE_EXTERNAL_FORCES] = forcePipeLayout;
		pipeLayouts[PIPE_DIVERGENCE] = divPipeLayout;
		pipeLayouts[PIPE_GRADIENT_SUBTRACT] = projectPipeLayout;
		pipeLayouts[PIPE_PRESENT] = presentPipeLayout;

		renderPasses[PIPE_ADVECTION] = advectRenderPass;
		renderPasses[PIPE_JACOBI_SOLVER_VISCOCITY] = jacobiRenderPass;
		renderPasses[PIPE_JACOBI_SOLVER_PRESSURE] = jacobiRenderPass;
		renderPasses[PIPE_EXTERNAL_FORCES] = forceRenderPass;
		renderPasses[PIPE_DIVERGENCE] = divRenderPass;
		renderPasses[PIPE_GRADIENT_SUBTRACT] = projectRenderPass;
		renderPasses[PIPE_PRESENT] = presentRenderPass;

		auto pipesCreateError = vkCreateGraphicsPipelines(
			ctx->logicalDevice,
			VK_NULL_HANDLE,
			pipesCreateInfo.size(),
			pipesCreateInfo.data(),
			nullptr,
			pipelines.data()
		);

		return !pipesCreateError;
	}


	void allocate_descriptor_sets()
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
		vkCreateDescriptorPool(ctx->logicalDevice, &descrPoolCreateInfo, nullptr, &descriptorPool);

		VkDescriptorSetAllocateInfo descrSetAllocateInfo = {};
		descrSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descrSetAllocateInfo.descriptorPool = descriptorPool;
		descrSetAllocateInfo.descriptorSetCount = DSI_INDEX_COUNT;

		VkDescriptorSetLayout layouts[DSI_INDEX_COUNT] = {
			descrSetLayouts[PIPE_ADVECTION],//advect_vel1
			descrSetLayouts[PIPE_JACOBI_SOLVER_VISCOCITY],//visc1
			descrSetLayouts[PIPE_JACOBI_SOLVER_VISCOCITY],//visc2
			descrSetLayouts[PIPE_EXTERNAL_FORCES],//forces
			descrSetLayouts[PIPE_EXTERNAL_FORCES], //color forces
			descrSetLayouts[PIPE_DIVERGENCE],//project_div
			descrSetLayouts[PIPE_JACOBI_SOLVER_PRESSURE],//project_pressure_1
			descrSetLayouts[PIPE_JACOBI_SOLVER_PRESSURE],//project_pressure_2
			descrSetLayouts[PIPE_GRADIENT_SUBTRACT],//project_grad_sub
			descrSetLayouts[PIPE_ADVECTION],//advect_col1
			descrSetLayouts[PIPE_PRESENT] //final present
		};

		descrSetAllocateInfo.pSetLayouts = layouts;

		for(std::size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++)
		{
			auto allocateStatus = vkAllocateDescriptorSets(
				ctx->logicalDevice,
				&descrSetAllocateInfo,
				descrSetsPerFrame[i]
			);
		}
	}

	void update_pressure_descr_set(int imageIndex, int divergenceTextureIndex)
	{
		VkDescriptorImageInfo pressureImageInfo1 = {};
		pressureImageInfo1.sampler = defaultSampler;
		pressureImageInfo1.imageView = simTextures[RT_PRESSURE_FIRST].view;
		pressureImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo pressureImageInfo2 = {};
		pressureImageInfo2.sampler = defaultSampler;
		pressureImageInfo2.imageView = simTextures[RT_PRESSURE_SECOND].view;
		pressureImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo divergentVelImageInfo = {};
		divergentVelImageInfo.sampler = defaultSampler;
		divergentVelImageInfo.imageView = simTextures[divergenceTextureIndex].view;
		divergentVelImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::array<VkWriteDescriptorSet, 4> pressureWriteDescrSets = {};

		pressureWriteDescrSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		pressureWriteDescrSets[0].dstSet = descrSetsPerFrame[imageIndex][DSI_PRESSURE_1];
		pressureWriteDescrSets[0].dstBinding = 0;
		pressureWriteDescrSets[0].dstArrayElement = 0;
		pressureWriteDescrSets[0].descriptorCount = 1;
		pressureWriteDescrSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pressureWriteDescrSets[0].pImageInfo = &pressureImageInfo1;

		pressureWriteDescrSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		pressureWriteDescrSets[1].dstSet = descrSetsPerFrame[imageIndex][DSI_PRESSURE_1];
		pressureWriteDescrSets[1].dstBinding = 1;
		pressureWriteDescrSets[1].dstArrayElement = 0;
		pressureWriteDescrSets[1].descriptorCount = 1;
		pressureWriteDescrSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pressureWriteDescrSets[1].pImageInfo = &divergentVelImageInfo;

		pressureWriteDescrSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		pressureWriteDescrSets[2].dstSet = descrSetsPerFrame[imageIndex][DSI_PRESSURE_2];
		pressureWriteDescrSets[2].dstBinding = 0;
		pressureWriteDescrSets[2].dstArrayElement = 0;
		pressureWriteDescrSets[2].descriptorCount = 1;
		pressureWriteDescrSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pressureWriteDescrSets[2].pImageInfo = &pressureImageInfo2;

		pressureWriteDescrSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		pressureWriteDescrSets[3].dstSet = descrSetsPerFrame[imageIndex][DSI_PRESSURE_2];
		pressureWriteDescrSets[3].dstBinding = 1;
		pressureWriteDescrSets[3].dstArrayElement = 0;
		pressureWriteDescrSets[3].descriptorCount = 1;
		pressureWriteDescrSets[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pressureWriteDescrSets[3].pImageInfo = &divergentVelImageInfo;

		vkUpdateDescriptorSets(
			ctx->logicalDevice,
			pressureWriteDescrSets.size(),
			pressureWriteDescrSets.data(),
			0, nullptr
		);
	}

	void update_viscocity_descr_set(int imageIndex, int velocityTextureIndex)
	{
		VkDescriptorImageInfo viscImageInfo1 = {};
		viscImageInfo1.sampler = defaultSampler;
		viscImageInfo1.imageView = simTextures[RT_VELOCITY_FIRST].view;
		// viscImageInfo1.imageView = simTextures[velocityTextureIndex].view;
		viscImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo viscImageInfo2 = {};
		viscImageInfo2.sampler = defaultSampler;
		viscImageInfo2.imageView = simTextures[RT_VELOCITY_SECOND].view;
		// viscImageInfo2.imageView = simTextures[velocityTextureIndex == RT_VELOCITY_FIRST ?
		// 	RT_VELOCITY_SECOND : RT_VELOCITY_FIRST].view;
		viscImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::array<VkWriteDescriptorSet, 4> viscWriteDescrSets = {};

		viscWriteDescrSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		viscWriteDescrSets[0].dstSet = descrSetsPerFrame[imageIndex][DSI_VISCOCITY_1];
		viscWriteDescrSets[0].dstBinding = 0;
		viscWriteDescrSets[0].dstArrayElement = 0;
		viscWriteDescrSets[0].descriptorCount = 1;
		viscWriteDescrSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		viscWriteDescrSets[0].pImageInfo = &viscImageInfo1;

		viscWriteDescrSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		viscWriteDescrSets[1].dstSet = descrSetsPerFrame[imageIndex][DSI_VISCOCITY_1];
		viscWriteDescrSets[1].dstBinding = 1;
		viscWriteDescrSets[1].dstArrayElement = 0;
		viscWriteDescrSets[1].descriptorCount = 1;
		viscWriteDescrSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		viscWriteDescrSets[1].pImageInfo = &viscImageInfo1;

		viscWriteDescrSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		viscWriteDescrSets[2].dstSet = descrSetsPerFrame[imageIndex][DSI_VISCOCITY_2];
		viscWriteDescrSets[2].dstBinding = 0;
		viscWriteDescrSets[2].dstArrayElement = 0;
		viscWriteDescrSets[2].descriptorCount = 1;
		viscWriteDescrSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		viscWriteDescrSets[2].pImageInfo = &viscImageInfo2;

		viscWriteDescrSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		viscWriteDescrSets[3].dstSet = descrSetsPerFrame[imageIndex][DSI_VISCOCITY_2];
		viscWriteDescrSets[3].dstBinding = 1;
		viscWriteDescrSets[3].dstArrayElement = 0;
		viscWriteDescrSets[3].descriptorCount = 1;
		viscWriteDescrSets[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		viscWriteDescrSets[3].pImageInfo = &viscImageInfo2;

		vkUpdateDescriptorSets(
			ctx->logicalDevice,
			viscWriteDescrSets.size(),
			viscWriteDescrSets.data(),
			0, nullptr
		);
	}


	void update_advect_velocity_descriptor_set(int imageIndex, int velocityTextureIndex)
	{
		std::array<VkWriteDescriptorSet, 2> advectVelocityWriteDescrSet = {}; 

		VkDescriptorImageInfo velocityImageInfo = {};
		velocityImageInfo.sampler = defaultSampler;
		velocityImageInfo.imageView = simTextures[velocityTextureIndex].view;
		velocityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		advectVelocityWriteDescrSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		advectVelocityWriteDescrSet[0].dstSet = descrSetsPerFrame[imageIndex][DSI_ADVECT_VELOCITY];
		advectVelocityWriteDescrSet[0].dstBinding = 0;
		advectVelocityWriteDescrSet[0].dstArrayElement = 0;
		advectVelocityWriteDescrSet[0].descriptorCount = 1;
		advectVelocityWriteDescrSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		advectVelocityWriteDescrSet[0].pImageInfo = &velocityImageInfo;

		advectVelocityWriteDescrSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		advectVelocityWriteDescrSet[1].dstSet = descrSetsPerFrame[imageIndex][DSI_ADVECT_VELOCITY];
		advectVelocityWriteDescrSet[1].dstBinding = 1;
		advectVelocityWriteDescrSet[1].dstArrayElement = 0;
		advectVelocityWriteDescrSet[1].descriptorCount = 1;
		advectVelocityWriteDescrSet[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		advectVelocityWriteDescrSet[1].pImageInfo = &velocityImageInfo;

		vkUpdateDescriptorSets(
			ctx->logicalDevice,
			advectVelocityWriteDescrSet.size(),
			advectVelocityWriteDescrSet.data(),
			0, nullptr
		);
	}

	void update_advect_color_descriptor_sets(int imageIndex, int velocityTextureIndex, int colorTextureIndex)
	{
		std::array<VkWriteDescriptorSet, 2> advectColorWriteDescrSet = {}; 

		VkDescriptorImageInfo velocityImageInfo = {};
		velocityImageInfo.sampler = defaultSampler;
		velocityImageInfo.imageView = simTextures[velocityTextureIndex].view;
		velocityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo colorImageInfo = {};
		colorImageInfo.sampler = defaultSampler;
		colorImageInfo.imageView = simTextures[colorTextureIndex].view;
		colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		advectColorWriteDescrSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		advectColorWriteDescrSet[0].dstSet = descrSetsPerFrame[imageIndex][DSI_ADVECT_COLOR];
		advectColorWriteDescrSet[0].dstBinding = 0;
		advectColorWriteDescrSet[0].dstArrayElement = 0;
		advectColorWriteDescrSet[0].descriptorCount = 1;
		advectColorWriteDescrSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		advectColorWriteDescrSet[0].pImageInfo = &velocityImageInfo;

		advectColorWriteDescrSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		advectColorWriteDescrSet[1].dstSet = descrSetsPerFrame[imageIndex][DSI_ADVECT_COLOR];
		advectColorWriteDescrSet[1].dstBinding = 1;
		advectColorWriteDescrSet[1].dstArrayElement = 0;
		advectColorWriteDescrSet[1].descriptorCount = 1;
		advectColorWriteDescrSet[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		advectColorWriteDescrSet[1].pImageInfo = &colorImageInfo;

		vkUpdateDescriptorSets(
			ctx->logicalDevice,
			advectColorWriteDescrSet.size(),
			advectColorWriteDescrSet.data(),
			0, nullptr
		);
	}
	
	void update_forces_descr_set(int imageIndex, int textureIndex, int descrSetIndex)
	{
		VkWriteDescriptorSet forceWriteDescrSet = {}; 
		VkDescriptorImageInfo forceImageInfo = {};
		forceImageInfo.sampler = defaultSampler;
		forceImageInfo.imageView = simTextures[textureIndex].view;
		forceImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		forceWriteDescrSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		forceWriteDescrSet.dstSet = descrSetsPerFrame[imageIndex][descrSetIndex];
		forceWriteDescrSet.dstBinding = 0;
		forceWriteDescrSet.dstArrayElement = 0;
		forceWriteDescrSet.descriptorCount = 1;
		forceWriteDescrSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		forceWriteDescrSet.pImageInfo = &forceImageInfo;

		vkUpdateDescriptorSets(ctx->logicalDevice, 1, &forceWriteDescrSet, 0, nullptr);
	}
	
	void update_divergence_descr_set(int imageIndex, int textureIndex)
	{
		VkWriteDescriptorSet divWriteDescrSet = {}; 
		VkDescriptorImageInfo divImageInfo = {};
		divImageInfo.sampler = defaultSampler;
		divImageInfo.imageView = simTextures[textureIndex].view;
		divImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		divWriteDescrSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		divWriteDescrSet.dstSet = descrSetsPerFrame[imageIndex][DSI_DIVERGENCE];
		divWriteDescrSet.dstBinding = 0;
		divWriteDescrSet.dstArrayElement = 0;
		divWriteDescrSet.descriptorCount = 1;
		divWriteDescrSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		divWriteDescrSet.pImageInfo = &divImageInfo;

		vkUpdateDescriptorSets(ctx->logicalDevice, 1, &divWriteDescrSet, 0, nullptr);
	}

	void update_pressure_subtract_descr_set(int imageIndex, int velocityTextureIndex, int pressureTextureIndex)
	{
		std::array<VkWriteDescriptorSet, 2> subWriteDescrSet = {}; 

		VkDescriptorImageInfo subImageInfo1 = {};
		subImageInfo1.sampler = defaultSampler;
		subImageInfo1.imageView = simTextures[velocityTextureIndex].view;
		subImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo subImageInfo2 = {};
		subImageInfo2.sampler = defaultSampler;
		subImageInfo2.imageView = simTextures[pressureTextureIndex].view;
		subImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		subWriteDescrSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		subWriteDescrSet[0].dstSet = descrSetsPerFrame[imageIndex][DSI_GRADIENT_SUBTRACT];
		subWriteDescrSet[0].dstBinding = 0;
		subWriteDescrSet[0].dstArrayElement = 0;
		subWriteDescrSet[0].descriptorCount = 1;
		subWriteDescrSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		subWriteDescrSet[0].pImageInfo = &subImageInfo1;

		subWriteDescrSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		subWriteDescrSet[1].dstSet = descrSetsPerFrame[imageIndex][DSI_GRADIENT_SUBTRACT];
		subWriteDescrSet[1].dstBinding = 1;
		subWriteDescrSet[1].dstArrayElement = 0;
		subWriteDescrSet[1].descriptorCount = 1;
		subWriteDescrSet[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		subWriteDescrSet[1].pImageInfo = &subImageInfo2;

		vkUpdateDescriptorSets(
			ctx->logicalDevice,
			subWriteDescrSet.size(),
			subWriteDescrSet.data(),
			0,
			nullptr
		);
	}

	void update_present_descr_set(int imageIndex, int colorTextureIndex)
	{
		VkDescriptorImageInfo presentImageInfo = {};
		presentImageInfo.sampler = defaultSampler;
		presentImageInfo.imageView = simTextures[colorTextureIndex].view;
		presentImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet presentWriteDescrSet = {};
		presentWriteDescrSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		presentWriteDescrSet.dstSet = descrSetsPerFrame[imageIndex][DSI_PRESENT];
		presentWriteDescrSet.dstBinding = 0;
		presentWriteDescrSet.descriptorCount = 1;
		presentWriteDescrSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		presentWriteDescrSet.pImageInfo = &presentImageInfo;

		vkUpdateDescriptorSets(ctx->logicalDevice, 1, &presentWriteDescrSet, 0, nullptr);
	}

	void create_frame_buffers()
	{
		frameBuffers[RT_VELOCITY_FIRST] = create_frame_buffer(
			ctx->logicalDevice,
			simTextures[RT_VELOCITY_FIRST].view,
			renderPasses[PIPE_ADVECTION],
			window->windowExtent.width,
			window->windowExtent.height
		);

		frameBuffers[RT_VELOCITY_SECOND] = create_frame_buffer(
			ctx->logicalDevice,
			simTextures[RT_VELOCITY_SECOND].view,
			renderPasses[PIPE_ADVECTION],
			window->windowExtent.width,
			window->windowExtent.height
		);

		frameBuffers[RT_PRESSURE_FIRST] = create_frame_buffer(
			ctx->logicalDevice,
			simTextures[RT_PRESSURE_FIRST].view,
			renderPasses[PIPE_JACOBI_SOLVER_PRESSURE],
			window->windowExtent.width,
			window->windowExtent.height
		);

		frameBuffers[RT_PRESSURE_SECOND] = create_frame_buffer(
			ctx->logicalDevice,
			simTextures[RT_PRESSURE_SECOND].view,
			renderPasses[PIPE_JACOBI_SOLVER_PRESSURE],
			window->windowExtent.width,
			window->windowExtent.height
		);

		frameBuffers[RT_COLOR_FIRST] = create_frame_buffer(
			ctx->logicalDevice,
			simTextures[RT_COLOR_FIRST].view,
			renderPasses[PIPE_ADVECTION],
			window->windowExtent.width,
			window->windowExtent.height
		);

		frameBuffers[RT_COLOR_SECOND] = create_frame_buffer(
			ctx->logicalDevice,
			simTextures[RT_COLOR_SECOND].view,
			renderPasses[PIPE_ADVECTION],
			window->windowExtent.width,
			window->windowExtent.height
		);

		//present frame buffers to render to
		for(std::size_t i = 0; i < swapchain->imageCount; i++)
		{
			swapchain->runtime.frameBuffers[i] = create_frame_buffer(
				ctx->logicalDevice,
				swapchain->runtime.imageViews[i],
				renderPasses[PIPE_PRESENT],
				window->windowExtent.width,
				window->windowExtent.height
			);
		}

	}

	void insert_full_memory_barrier(VkCommandBuffer cmdBuffer)
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

	void clear_pressure_texture(VkCommandBuffer commandBuffer, int textureIndex)
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
			commandBuffer,
			simTextures[textureIndex].image,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			simTextures[textureIndex].layout,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		);

		vkCmdClearColorImage(
			commandBuffer,
			simTextures[textureIndex].image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			&clearColor,
			1,
			&range
		);

		insert_image_memory_barrier(
			commandBuffer,
			simTextures[textureIndex].image,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		simTextures[textureIndex].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	void cmd_begin_debug_label(VkCommandBuffer commandBuffer, const char* labelName, Vec4 color)
	{
		if(ctx->hasDebugUtilsExtension)
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

	void cmd_end_debug_label(VkCommandBuffer commandBuffer)
	{
		if(ctx->hasDebugUtilsExtension)
		{
			vkCmdEndDebugUtilsLabelEXT(commandBuffer);
		}
	}

	void assign_names_to_vulkan_objects()
	{
		const char* RenderTargetNames[RT_MAX_COUNT] = 
		{
			"RT_VELOCITY_FIRST",
			"RT_VELOCITY_SECOND",
			"RT_PRESSURE_FIRST",
			"RT_PRESSURE_SECOND",
			"RT_COLOR_FIRST",
			"RT_COLOR_SECOND"
		};

		const char* DescrSetNames[DSI_INDEX_COUNT] = 
		{
			"DSI_ADVECT_VELOCITY",
			"DSI_VISCOCITY_1",
			"DSI_VISCOCITY_2",
			"DSI_FORCES",
			"DSI_FORCES_COLOR",
			"DSI_DIVERGENCE",
			"DSI_PRESSURE_1",
			"DSI_PRESSURE_2",
			"DSI_GRADIENT_SUBTRACT",
			"DSI_ADVECT_COLOR",
			"DSI_PRESENT"
		};

		for(std::size_t textureIndex = 0; textureIndex < simTextures.size(); textureIndex++)
		{
				VkDebugUtilsObjectNameInfoEXT debugTextureInfo = {};
				debugTextureInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debugTextureInfo.objectType = VK_OBJECT_TYPE_IMAGE;
				debugTextureInfo.objectHandle = (uint64_t)simTextures[textureIndex].image;
				debugTextureInfo.pObjectName = RenderTargetNames[textureIndex];
								
				vkSetDebugUtilsObjectNameEXT(ctx->logicalDevice, &debugTextureInfo);
		}

		for(std::size_t imageIndex = 0; imageIndex < SWAPCHAIN_IMAGE_COUNT; imageIndex++)
		{
			for(std::size_t descrSetIndex = 0; descrSetIndex < DSI_INDEX_COUNT; descrSetIndex++)
			{
				VkDebugUtilsObjectNameInfoEXT debugTextureInfo = {};
				debugTextureInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debugTextureInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
				debugTextureInfo.objectHandle = (uint64_t)descrSetsPerFrame[imageIndex][descrSetIndex];
				debugTextureInfo.pObjectName = DescrSetNames[descrSetIndex];
				
				vkSetDebugUtilsObjectNameEXT(ctx->logicalDevice, &debugTextureInfo);
			}
		}
	}

	void insert_image_memory_barrier(
		VkCommandBuffer cmdBuffer, VkImage image,
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
		memoryBarrier.srcQueueFamilyIndex = ctx->queueFamIdx;
		memoryBarrier.dstQueueFamilyIndex = ctx->queueFamIdx;
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

	void record_command_buffer(
		int commandBufferIndex,
		int inputVelocityTextureIndex,
		int inputColorTextureIndex,
		int* outputVelocityTextureIndex,
		int* outputColorTextureIndex)
	{
		auto windowExtent = window->windowExtent;

		const float dx = 1.f / (float)std::max(windowExtent.width, windowExtent.height);
		const float timeStep = 0.005f;
		const float kv = 1.5f;//kinematic viscocity

		VkCommandBufferBeginInfo cmdBuffBeginInfo = {};
		cmdBuffBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		vkBeginCommandBuffer(commandBuffers[commandBufferIndex], &cmdBuffBeginInfo);

		update_advect_velocity_descriptor_set(commandBufferIndex, inputVelocityTextureIndex);
		int advectVelocityRenderTarget = inputVelocityTextureIndex == RT_VELOCITY_FIRST ? 
			RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;
		{

			cmd_begin_debug_label(commandBuffers[commandBufferIndex], "advect velocity pass", {0.713f, 0.921f, 0.556f, 1.f});

			VkRenderPassBeginInfo advectPassBeginInfo = {};
			advectPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			advectPassBeginInfo.renderPass = renderPasses[PIPE_ADVECTION];
			advectPassBeginInfo.framebuffer = frameBuffers[advectVelocityRenderTarget];
			advectPassBeginInfo.renderArea.offset = {0, 0};
			advectPassBeginInfo.renderArea.extent = windowExtent;
			advectPassBeginInfo.clearValueCount = 0;
			advectPassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &advectPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_ADVECTION]);
			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_ADVECTION],
				0,
				1, &descrSetsPerFrame[commandBufferIndex][DSI_ADVECT_VELOCITY],
				0, nullptr
			);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			AdvectConstants advectConstants = {};
			advectConstants.timestep = timeStep;
			advectConstants.gridScale = dx;
			vkCmdPushConstants(
				commandBuffers[commandBufferIndex], pipeLayouts[PIPE_ADVECTION],
				VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(AdvectConstants), &advectConstants
			);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);

			cmd_end_debug_label(commandBuffers[commandBufferIndex]);
		}
		
		// insert_full_memory_barrier(commandBuffers[commandBufferIndex]);
		insert_image_memory_barrier(
			commandBuffers[commandBufferIndex],
			simTextures[advectVelocityRenderTarget].image,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);


		// TODO: removing viscocity pass makes fluid to behave weird and glitchy for some reason
		//viscocity pass
		update_viscocity_descr_set(commandBufferIndex, advectVelocityRenderTarget);
		int viscPassRenderTarget = advectVelocityRenderTarget == RT_VELOCITY_FIRST ? 
			RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;
		cmd_begin_debug_label(commandBuffers[commandBufferIndex], "viscocity pass", {0.854f, 0.556f, 0.921f, 1.f});

		for(std::size_t i = 0; i < jacobiIterations; i++)
		{

			VkRenderPassBeginInfo viscPassBeginInfo = {};
			viscPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			viscPassBeginInfo.renderPass = renderPasses[PIPE_JACOBI_SOLVER_VISCOCITY];
			viscPassBeginInfo.framebuffer = frameBuffers[viscPassRenderTarget];
			viscPassBeginInfo.renderArea.offset = {0, 0};
			viscPassBeginInfo.renderArea.extent = windowExtent;
			viscPassBeginInfo.clearValueCount = 0;
			viscPassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &viscPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_JACOBI_SOLVER_VISCOCITY]);

			int descriptorSetIndex = viscPassRenderTarget == RT_VELOCITY_FIRST ? DSI_VISCOCITY_2 : DSI_VISCOCITY_1;

			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_JACOBI_SOLVER_VISCOCITY],
				0, 1, &descrSetsPerFrame[commandBufferIndex][descriptorSetIndex],
				0, nullptr
			);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			SolverConstants solverConstants = {};
			solverConstants.alpha = (dx * dx) / (kv * timeStep);
			solverConstants.beta = 4 + solverConstants.alpha;
			solverConstants.texelSize = dx;
			vkCmdPushConstants(
				commandBuffers[commandBufferIndex],
				pipeLayouts[PIPE_JACOBI_SOLVER_VISCOCITY],
				VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(SolverConstants), &solverConstants
			);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);
// insert_full_memory_barrier(commandBuffers[commandBufferIndex]);
			insert_image_memory_barrier(
				commandBuffers[commandBufferIndex],
				simTextures[viscPassRenderTarget].image,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);

			viscPassRenderTarget = viscPassRenderTarget == RT_VELOCITY_FIRST ? RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;

		}
		cmd_end_debug_label(commandBuffers[commandBufferIndex]);

		RenderTarget forcePassRenderTarget = viscPassRenderTarget == RT_VELOCITY_FIRST ? 
			RT_VELOCITY_FIRST: RT_VELOCITY_SECOND;
		update_forces_descr_set(commandBufferIndex, viscPassRenderTarget == RT_VELOCITY_FIRST ? 
			RT_VELOCITY_SECOND: RT_VELOCITY_FIRST, DSI_FORCES);

		// update_forces_descr_set(commandBufferIndex, advectVelocityRenderTarget, DSI_FORCES);
		// RenderTarget forcePassRenderTarget = advectVelocityRenderTarget == RT_VELOCITY_FIRST ? 
		// 	RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;
		
		//force velocity pass
		{
			cmd_begin_debug_label(commandBuffers[commandBufferIndex], "Force velocity pass", {0.254f, 0.329f, 0.847f, 1.f});
			VkRenderPassBeginInfo forcePassBeginInfo = {};
			forcePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			forcePassBeginInfo.renderPass = renderPasses[PIPE_EXTERNAL_FORCES];
			forcePassBeginInfo.framebuffer = frameBuffers[forcePassRenderTarget];
			forcePassBeginInfo.renderArea.offset = {0, 0};
			forcePassBeginInfo.renderArea.extent = windowExtent;
			forcePassBeginInfo.clearValueCount = 0;
			forcePassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &forcePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_EXTERNAL_FORCES]);

			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_EXTERNAL_FORCES],
				0, 1, &descrSetsPerFrame[commandBufferIndex][DSI_FORCES],
				0, nullptr
			);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			
			
			ForceConstants forceConsts = {};
			forceConsts.impulseRadius = 0.025f;

			static bool isMouseBeingDragged = false;
			static Vec2 prevMousePos = {};

			if(isMouseBtnPressed(MouseBtn::LeftBtn) && !isMouseBeingDragged)
			{
				isMouseBeingDragged = true;
				auto pos = getMousePos();
				prevMousePos.x = (float)pos.x * dx;
				prevMousePos.y = (float)pos.y * dx;
				// magma::log::error("prev = {} {}", prevMousePos.x, prevMousePos.y);
			}
			else if(!isMouseBtnPressed(MouseBtn::LeftBtn))
			{
				isMouseBeingDragged = false;
			}
			else if(isMouseBeingDragged)
			{
				auto currentMousePos = getMousePos();

				forceConsts.mousePos = {(float)currentMousePos.x * dx, (float)currentMousePos.y * dx};
				// magma::log::error("current = {} {}", forceConsts.mousePos.x, forceConsts.mousePos.y);

				forceConsts.force = {
					(forceConsts.mousePos.x - prevMousePos.x)* 15000.f, 
					(forceConsts.mousePos.y - prevMousePos.y)* 15000.f,
					0.f, 0.f
				};

				// isMouseBeingDragged = std::abs(forceConsts.force.x) > 0 || std::abs(forceConsts.force.y > 0);
				// magma::log::error("recording force... {};{}",forceConsts.force.x,forceConsts.force.y);
				prevMousePos = forceConsts.mousePos;
			}

			vkCmdPushConstants(commandBuffers[commandBufferIndex], pipeLayouts[PIPE_EXTERNAL_FORCES],
				VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ForceConstants), &forceConsts);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);

			cmd_end_debug_label(commandBuffers[commandBufferIndex]);
		}
		
		insert_image_memory_barrier(
			commandBuffers[commandBufferIndex],
			simTextures[forcePassRenderTarget].image,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		//force color pass
		update_forces_descr_set(commandBufferIndex, inputColorTextureIndex, DSI_FORCES_COLOR);
		int forceColorPassRenderTarget = inputColorTextureIndex == RT_COLOR_FIRST ?
			RT_COLOR_SECOND : RT_COLOR_FIRST;
		{
			cmd_begin_debug_label(commandBuffers[commandBufferIndex], "Force color pass", {0.2f, 0.1f, 1.f, 1.f});

			VkRenderPassBeginInfo forcePassBeginInfo = {};
			forcePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			forcePassBeginInfo.renderPass = renderPasses[PIPE_EXTERNAL_FORCES];
			forcePassBeginInfo.framebuffer = frameBuffers[forceColorPassRenderTarget];
			forcePassBeginInfo.renderArea.offset = {0, 0};
			forcePassBeginInfo.renderArea.extent = windowExtent;
			forcePassBeginInfo.clearValueCount = 0;
			forcePassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &forcePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_EXTERNAL_FORCES]);

			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_EXTERNAL_FORCES],
				0, 1, &descrSetsPerFrame[commandBufferIndex][DSI_FORCES_COLOR],
				0, nullptr
			);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			
			
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
				forceConsts.mousePos = {(float)currentMousePos.x * dx, (float)currentMousePos.y * dx};
				forceConsts.force = {0.082, 0.976, 0.901, 1.f};
			}

			vkCmdPushConstants(commandBuffers[commandBufferIndex], pipeLayouts[PIPE_EXTERNAL_FORCES],
				VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ForceConstants), &forceConsts);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);

			cmd_end_debug_label(commandBuffers[commandBufferIndex]);
		}
		
		insert_image_memory_barrier(
			commandBuffers[commandBufferIndex],
			simTextures[forceColorPassRenderTarget].image,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

// insert_full_memory_barrier(commandBuffers[commandBufferIndex]);

		int divergencePassRenderTarget = forcePassRenderTarget == RT_VELOCITY_FIRST ?
			RT_VELOCITY_SECOND : RT_VELOCITY_FIRST;

		update_divergence_descr_set(commandBufferIndex, forcePassRenderTarget);
		//divergence pass
		{
			cmd_begin_debug_label(commandBuffers[commandBufferIndex], "Divergence pass", {0.254f, 0.847f, 0.839f, 1.f});

			VkRenderPassBeginInfo divPassBeginInfo = {};
			divPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			divPassBeginInfo.renderPass = renderPasses[PIPE_DIVERGENCE];
			divPassBeginInfo.framebuffer = frameBuffers[divergencePassRenderTarget];
			divPassBeginInfo.renderArea.offset = {0, 0};
			divPassBeginInfo.renderArea.extent = windowExtent;
			divPassBeginInfo.clearValueCount = 0;
			divPassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &divPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_DIVERGENCE]);

			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_DIVERGENCE],
				0, 1, &descrSetsPerFrame[commandBufferIndex][DSI_DIVERGENCE],
				0, nullptr
			);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdPushConstants(commandBuffers[commandBufferIndex], pipeLayouts[PIPE_DIVERGENCE],
				VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &dx);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);

			cmd_end_debug_label(commandBuffers[commandBufferIndex]);
		}
// insert_full_memory_barrier(commandBuffers[commandBufferIndex]);
		insert_image_memory_barrier(
			commandBuffers[commandBufferIndex],
			simTextures[divergencePassRenderTarget].image,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		update_pressure_descr_set(commandBufferIndex, divergencePassRenderTarget);

		cmd_begin_debug_label(commandBuffers[commandBufferIndex], "Pressure pass", {0.996f, 0.933f, 0.384f, 1.f});
		//pressure pass
		clear_pressure_texture(commandBuffers[commandBufferIndex], RT_PRESSURE_FIRST);
		for(std::size_t i = 0; i < jacobiIterations; i++)
		{
			bool evenIteration = !(bool)(i % 2);
			
			VkRenderPassBeginInfo pressurePassBeginInfo = {};
			pressurePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			pressurePassBeginInfo.renderPass = renderPasses[PIPE_JACOBI_SOLVER_PRESSURE];
			pressurePassBeginInfo.framebuffer = frameBuffers[evenIteration ? RT_PRESSURE_SECOND : RT_PRESSURE_FIRST];
			pressurePassBeginInfo.renderArea.offset = {0, 0};
			pressurePassBeginInfo.renderArea.extent = windowExtent;
			pressurePassBeginInfo.clearValueCount = 0;
			pressurePassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &pressurePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_JACOBI_SOLVER_PRESSURE]);

			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_JACOBI_SOLVER_PRESSURE],
				0, 1, &descrSetsPerFrame[commandBufferIndex][evenIteration ? DSI_PRESSURE_1 : DSI_PRESSURE_2],
				0, nullptr
			);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			SolverConstants solverConstants = {};
			solverConstants.alpha = -(dx * dx) ;
			solverConstants.beta = 4;
			solverConstants.texelSize = dx;
			vkCmdPushConstants(
				commandBuffers[commandBufferIndex],
				pipeLayouts[PIPE_JACOBI_SOLVER_PRESSURE],
				VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(SolverConstants), &solverConstants
			);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);
// insert_full_memory_barrier(commandBuffers[commandBufferIndex]);
			insert_image_memory_barrier(
				commandBuffers[commandBufferIndex],
				simTextures[evenIteration ? RT_PRESSURE_SECOND : RT_PRESSURE_FIRST].image,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);

		}

		cmd_end_debug_label(commandBuffers[commandBufferIndex]);

		//pressure subtract pass
		inputVelocityTextureIndex = forcePassRenderTarget;
		int pressureTextureIndex = jacobiIterations % 2 == 0 ?
			RT_PRESSURE_SECOND : RT_PRESSURE_FIRST;
		int subtractPassRenderTarget = divergencePassRenderTarget;

		update_pressure_subtract_descr_set(commandBufferIndex, inputVelocityTextureIndex, pressureTextureIndex);
		{
			cmd_begin_debug_label(commandBuffers[commandBufferIndex], "Pressure subtract pass",{0.384f, 0.996f, 0.639f,1.f});

			VkRenderPassBeginInfo subtractPassBeginInfo = {};
			subtractPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			subtractPassBeginInfo.renderPass = renderPasses[PIPE_GRADIENT_SUBTRACT];
			subtractPassBeginInfo.framebuffer = frameBuffers[subtractPassRenderTarget];
			subtractPassBeginInfo.renderArea.offset = {0, 0};
			subtractPassBeginInfo.renderArea.extent = windowExtent;
			subtractPassBeginInfo.clearValueCount = 0;
			subtractPassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &subtractPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_GRADIENT_SUBTRACT]);

			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_GRADIENT_SUBTRACT],
				0, 1, &descrSetsPerFrame[commandBufferIndex][DSI_GRADIENT_SUBTRACT],
				0, nullptr
			);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdPushConstants(commandBuffers[commandBufferIndex], pipeLayouts[PIPE_GRADIENT_SUBTRACT],
				VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &dx);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);

			cmd_end_debug_label(commandBuffers[commandBufferIndex]);
		}
// insert_full_memory_barrier(commandBuffers[commandBufferIndex]);
		insert_image_memory_barrier(
			commandBuffers[commandBufferIndex],
			simTextures[subtractPassRenderTarget].image,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		update_advect_color_descriptor_sets(commandBufferIndex, subtractPassRenderTarget, forceColorPassRenderTarget);

		int advectColorRenderTarget = forceColorPassRenderTarget == RT_COLOR_FIRST ?
			RT_COLOR_SECOND : RT_COLOR_FIRST;
		//  advect for color
		{
			cmd_begin_debug_label(commandBuffers[commandBufferIndex], "Advect color pass", {0.556f, 0.384f, 0.996f, 1.f});

			VkRenderPassBeginInfo advectColorPassBeginInfo = {};
			advectColorPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			advectColorPassBeginInfo.renderPass = renderPasses[PIPE_ADVECTION];
			advectColorPassBeginInfo.framebuffer = frameBuffers[advectColorRenderTarget];
			advectColorPassBeginInfo.renderArea.offset = {0, 0};
			advectColorPassBeginInfo.renderArea.extent = windowExtent;
			advectColorPassBeginInfo.clearValueCount = 0;
			advectColorPassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &advectColorPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_ADVECTION]);
			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_ADVECTION],
				0,
				1, &descrSetsPerFrame[commandBufferIndex][DSI_ADVECT_COLOR],
				0, nullptr
			);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			AdvectConstants advectConstants = {};
			advectConstants.timestep = 0.005f;
			advectConstants.gridScale = dx;
			vkCmdPushConstants(
				commandBuffers[commandBufferIndex], pipeLayouts[PIPE_ADVECTION],
				VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(AdvectConstants), &advectConstants
			);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);

			cmd_end_debug_label(commandBuffers[commandBufferIndex]);
		}
// insert_full_memory_barrier(commandBuffers[commandBufferIndex]);
		insert_image_memory_barrier(
			commandBuffers[commandBufferIndex],
			simTextures[advectColorRenderTarget].image,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		//present pass
		update_present_descr_set(commandBufferIndex, advectColorRenderTarget);
		{
			cmd_begin_debug_label(commandBuffers[commandBufferIndex], "Present pass", {0.996f, 0.384f, 0.447f, 1.f});
			VkRenderPassBeginInfo presentColorPassBeginInfo = {};
			presentColorPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			presentColorPassBeginInfo.renderPass = renderPasses[PIPE_PRESENT];
			presentColorPassBeginInfo.framebuffer = swapchain->runtime.frameBuffers[commandBufferIndex];
			presentColorPassBeginInfo.renderArea.offset = {0, 0};
			presentColorPassBeginInfo.renderArea.extent = windowExtent;
			presentColorPassBeginInfo.clearValueCount = 0;
			presentColorPassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &presentColorPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPE_PRESENT]);
			vkCmdBindDescriptorSets(
				commandBuffers[commandBufferIndex],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeLayouts[PIPE_PRESENT],
				0,
				1, &descrSetsPerFrame[commandBufferIndex][DSI_PRESENT],
				0, nullptr
			);
			
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffers[commandBufferIndex], 0, 1, &deviceVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(commandBuffers[commandBufferIndex], deviceIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			const std::uint32_t indexCount = 6; 
			vkCmdDrawIndexed(commandBuffers[commandBufferIndex], indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);
			
			cmd_end_debug_label(commandBuffers[commandBufferIndex]);
		}

		vkEndCommandBuffer(commandBuffers[commandBufferIndex]);

		*outputVelocityTextureIndex = subtractPassRenderTarget;
		*outputColorTextureIndex = advectColorRenderTarget;

	}

	void allocate_command_buffers(int commandBufferCount)
	{
		commandBuffers.resize(commandBufferCount);

		VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
		cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		cmdPoolCreateInfo.queueFamilyIndex = ctx->queueFamIdx;

		vkCreateCommandPool(ctx->logicalDevice, &cmdPoolCreateInfo, nullptr, &commandPool);

		VkCommandBufferAllocateInfo cmdBufferAllocInfo = {};
		cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufferAllocInfo.commandPool = commandPool;
		cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufferAllocInfo.commandBufferCount = commandBufferCount;
		
		vkAllocateCommandBuffers(
			ctx->logicalDevice,
			&cmdBufferAllocInfo,
			commandBuffers.data()
		);
	}

	void run_simulation_loop()
	{
		if(ctx->hasDebugUtilsExtension)
		{
			assign_names_to_vulkan_objects();
		}

		allocate_command_buffers(swapchain->imageCount);

		int inputVelocityTextureIndex = RT_VELOCITY_FIRST;
		int inputColorTextureIndex = RT_COLOR_FIRST;

		std::size_t syncIndex = 0;
		while(!windowShouldClose(window->windowHandle))
		{
			updateMessageQueue();
			
			vkWaitForFences(ctx->logicalDevice, 1, &swapchain->runtime.workSubmittedFences[syncIndex], VK_TRUE, UINT64_MAX);
			vkResetFences(ctx->logicalDevice, 1, &swapchain->runtime.workSubmittedFences[syncIndex]);

			// magma::log::error("status = {}",vkStrError(r));
			std::uint32_t imageIndex = {};
			vkAcquireNextImageKHR(
				ctx->logicalDevice,
				swapchain->swapchain,
				UINT64_MAX,
				swapchain->runtime.imageAvailableSemaphores[syncIndex],
				VK_NULL_HANDLE,
				&imageIndex
			);
			// magma::log::debug("image at index {} has been acquired", imageIndex);

			int outputVelocityTextureIndex = {};
			int outputColorTextureIndex = {};
			record_command_buffer(
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
			submitInfo.pWaitSemaphores = &swapchain->runtime.imageAvailableSemaphores[syncIndex];
			VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			submitInfo.pWaitDstStageMask = &waitMask;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &swapchain->runtime.imageMayPresentSemaphores[syncIndex];

			auto queueSubmitStatus = vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, swapchain->runtime.workSubmittedFences[syncIndex]);
			// magma::log::debug("queue submit at image {} with status {}", imageIndex, vkStrError(queueSubmitStatus));
			// auto r = vkDeviceWaitIdle(ctx->logicalDevice);


			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &swapchain->runtime.imageMayPresentSemaphores[syncIndex];
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &swapchain->swapchain;
			presentInfo.pImageIndices = &imageIndex;
			// presentInfo.pResults = ;

			vkQueuePresentKHR(ctx->graphicsQueue, &presentInfo);

			syncIndex = (syncIndex + 1) % swapchain->imageCount;
		}
	}


	struct AdvectConstants
	{
		float gridScale;
		float timestep;
	};
	
	struct SolverConstants
	{
		float alpha;
		float beta;
		float texelSize;
	};
	const std::size_t jacobiIterations = 50;

	struct ForceConstants
	{
		Vec4 force;
		Vec2 mousePos;
		float impulseRadius;
	};

	enum Pipeline
	{
		PIPE_ADVECTION,//velocity & color advection
		PIPE_JACOBI_SOLVER_PRESSURE,//viscocity, pressure solve
		PIPE_JACOBI_SOLVER_VISCOCITY,
		PIPE_EXTERNAL_FORCES,
		PIPE_DIVERGENCE,//before pressure solve (vW)
		PIPE_GRADIENT_SUBTRACT,
		PIPE_PRESENT,
		PIPE_COUNT
	};

	enum RenderTarget
	{
		RT_VELOCITY_FIRST,
		RT_VELOCITY_SECOND,
		RT_PRESSURE_FIRST,
		RT_PRESSURE_SECOND,
		RT_COLOR_FIRST,
		RT_COLOR_SECOND,
		RT_MAX_COUNT
	};

	enum DescrSetIndex
	{
		DSI_ADVECT_VELOCITY,
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

	VulkanGlobalContext* ctx;
	SwapChain* swapchain;
	WindowInfo* window;	

	std::array<VkPipeline, PIPE_COUNT> pipelines;
	std::array<VkDescriptorSetLayout, PIPE_COUNT> descrSetLayouts;
	std::array<VkPipelineLayout, PIPE_COUNT> pipeLayouts;
	std::array<VkRenderPass, PIPE_COUNT> renderPasses;
	std::array<ImageResource, RT_MAX_COUNT> simTextures;
	std::array<VkFramebuffer, RT_MAX_COUNT> frameBuffers;
	VkDescriptorSet descrSetsPerFrame[SWAPCHAIN_IMAGE_COUNT][DSI_INDEX_COUNT];

	VkSampler defaultSampler = VK_NULL_HANDLE;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers;
	
	Buffer deviceVertexBuffer;
	Buffer deviceIndexBuffer;

};


int main(int argc, char **argv)
{
	magma::log::initLogging();

	const std::vector<const char *> desiredLayers = {
		{"VK_LAYER_KHRONOS_validation"}
	};

	const std::vector<const char *> desiredExtensions = {
		{VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
	};

	VulkanGlobalContext ctx = {};
	if (!initVulkanGlobalContext(desiredLayers, desiredExtensions, &ctx))
	{
		return -1;
	}

	WindowInfo windowInfo = {};
	const std::size_t width = 600;
	const std::size_t height = 600;
	if (!initPlatformWindow(ctx, width, height, "fluid_sim", &windowInfo))
	{
		return -1;
	}

	SwapChain swapChain = {};
	if (!createSwapChain(ctx, windowInfo, SWAPCHAIN_IMAGE_COUNT, &swapChain))
	{
		return -1;
	}

	// the following textures are needed for simulating fluid flow:
	// 1. Two velocity textures to store tmp results
	// 2. One pressure texture
	// 3. Two ink textures (for the same reason as in 1*)
	// 4. try to perform only advect step for now


	Fluid fluid(&ctx, &swapChain, &windowInfo);
	fluid.initialise_fluid_textures();
	fluid.create_pipelines();
	fluid.init_vertex_and_index_buffers();
	fluid.allocate_descriptor_sets();
	fluid.create_frame_buffers();
	fluid.run_simulation_loop();



	return 0;
}