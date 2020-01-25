#include "vk_pipeline.h"
#include "maths.h"
#include "vk_dbg.h"

VkBool32 configureGraphicsPipe(SwapChain& swapChain, VulkanGlobalContext& vkCtx,
	VertexBuffer& vertexInfo, VkExtent2D windowExtent, PipelineState* state)
{
	
	//assign shaders to a specific pipeline stage
	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {};
	shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfos[0].pNext = nullptr;
	shaderStageCreateInfos[0].stage = state->shaders[0].shaderType;
	shaderStageCreateInfos[0].module = state->shaders[0].handle;
	shaderStageCreateInfos[0].pName = "main";
	shaderStageCreateInfos[0].pSpecializationInfo = nullptr;

	shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfos[1].pNext = nullptr;
	shaderStageCreateInfos[1].stage = state->shaders[1].shaderType;
	shaderStageCreateInfos[1].module = state->shaders[1].handle;
	shaderStageCreateInfos[1].pName = "main";
	shaderStageCreateInfos[1].pSpecializationInfo = nullptr;
	
	VkPipelineVertexInputStateCreateInfo pipeVertexInputStateCreateInfo = {};
	pipeVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipeVertexInputStateCreateInfo.pNext = nullptr;
	pipeVertexInputStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	pipeVertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInfo.bindingDescr;
	pipeVertexInputStateCreateInfo.vertexAttributeDescriptionCount = vertexInfo.attribCount;
	pipeVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInfo.attrDescr;

	VkPipelineInputAssemblyStateCreateInfo pipeInputAssemblyCreateInfo = {};
	pipeInputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipeInputAssemblyCreateInfo.pNext = nullptr;
	pipeInputAssemblyCreateInfo.flags = VK_FLAGS_NONE;
	pipeInputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipeInputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.f;
	viewport.y = int(windowExtent.height);
	viewport.width = int(windowExtent.width);
	viewport.height = -int(windowExtent.height);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	VkRect2D scissors = {};
	scissors.extent = windowExtent;

	VkPipelineViewportStateCreateInfo pipeViewPortStateCreateInfo = {};
	pipeViewPortStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipeViewPortStateCreateInfo.pNext = nullptr;
	pipeViewPortStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeViewPortStateCreateInfo.viewportCount = 1;
	pipeViewPortStateCreateInfo.pViewports = &viewport;
	pipeViewPortStateCreateInfo.scissorCount = 1;
	pipeViewPortStateCreateInfo.pScissors = &scissors;

	VkPipelineRasterizationStateCreateInfo pipeRastStateCreateInfo = {};
	pipeRastStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipeRastStateCreateInfo.pNext = nullptr;
	pipeRastStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeRastStateCreateInfo.depthClampEnable = VK_FALSE;
	pipeRastStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	pipeRastStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipeRastStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;//backface culling (i.e back triangles are discarded)
	pipeRastStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipeRastStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipeRastStateCreateInfo.depthBiasConstantFactor = 0.f;
	pipeRastStateCreateInfo.depthBiasClamp = 0.f;
	pipeRastStateCreateInfo.depthBiasSlopeFactor = 0.f;
	pipeRastStateCreateInfo.lineWidth = 1.f;

	//disable msaa for now
	VkPipelineMultisampleStateCreateInfo pipeMultisampleCreateInfo = {};
	pipeMultisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipeMultisampleCreateInfo.pNext = nullptr;
	pipeMultisampleCreateInfo.flags = VK_FLAGS_NONE;
	pipeMultisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipeMultisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	pipeMultisampleCreateInfo.minSampleShading = 1.f;
	pipeMultisampleCreateInfo.pSampleMask = nullptr;
	pipeMultisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
	pipeMultisampleCreateInfo.alphaToOneEnable = VK_FALSE;


	//disable depth/stencil pipe
	VkPipelineDepthStencilStateCreateInfo pipeDepthStencilCreateInfo = {};
	pipeDepthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipeDepthStencilCreateInfo.pNext = nullptr;
	pipeDepthStencilCreateInfo.flags = VK_FLAGS_NONE;
	//pipeDepthStencilCreateInfo.depthTestEnable = VK_TRUE;
	//pipeDepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
	//pipeDepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	pipeDepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	pipeDepthStencilCreateInfo.stencilTestEnable = VK_FALSE;
	//pipeDepthStencilCreateInfo.front = ;
	//pipeDepthStencilCreateInfo.back = ;
	pipeDepthStencilCreateInfo.minDepthBounds = 0.f;
	pipeDepthStencilCreateInfo.maxDepthBounds = 1.f;

	//color blending state:

	//1.specify per-target color blend settings per each color attachment
	//alpha-blend testing(blend fragment color based on it's opacity)
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	//colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	//colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	//colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	//colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	//colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	//colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|
		VK_COLOR_COMPONENT_G_BIT|
		VK_COLOR_COMPONENT_B_BIT|
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo pipeColorBlendCreateInfo = {};
	pipeColorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipeColorBlendCreateInfo.pNext = nullptr;
	pipeColorBlendCreateInfo.flags = VK_FLAGS_NONE;
	pipeColorBlendCreateInfo.logicOpEnable = VK_FALSE;
	pipeColorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	pipeColorBlendCreateInfo.attachmentCount = 1;
	pipeColorBlendCreateInfo.pAttachments = &colorBlendAttachmentState;
	pipeColorBlendCreateInfo.blendConstants[0] = 0.f;
	pipeColorBlendCreateInfo.blendConstants[1] = 0.f;
	pipeColorBlendCreateInfo.blendConstants[2] = 0.f;
	pipeColorBlendCreateInfo.blendConstants[3] = 0.f;

	//amount of settings that can be changed without recreating the whole pipeline
	VkDynamicState dynamicStates = VK_DYNAMIC_STATE_VIEWPORT;

	VkPipelineDynamicStateCreateInfo pipeDynamicStateCreateInfo = {};
	pipeDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipeDynamicStateCreateInfo.pNext = nullptr;
	pipeDynamicStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeDynamicStateCreateInfo.dynamicStateCount = 1;
	pipeDynamicStateCreateInfo.pDynamicStates = &dynamicStates;

	VkPipelineLayout pipelineLayout = state->pipelineLayout;
	if(pipelineLayout == VK_NULL_HANDLE) {
		VkPipelineLayoutCreateInfo pipeLayoutCreateInfo = {};
		pipeLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeLayoutCreateInfo.pNext =  nullptr;
		pipeLayoutCreateInfo.flags = VK_FLAGS_NONE;
		pipeLayoutCreateInfo.setLayoutCount = 0;
		pipeLayoutCreateInfo.pSetLayouts = nullptr;
		pipeLayoutCreateInfo.pushConstantRangeCount = 0;
		pipeLayoutCreateInfo.pPushConstantRanges = nullptr;

		VK_CALL(vkCreatePipelineLayout(vkCtx.logicalDevice, &pipeLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	VkAttachmentDescription attachmentDescr = {};
	//color attachment
	attachmentDescr.flags = {};
	attachmentDescr.format = swapChain.imageFormat;
	attachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescr.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescr.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;//index to a vkAttachmentDescription array
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//???? WHY

	VkSubpassDescription subpassDescr = {};
//    VkSubpassDescriptionFlags       flags;
	subpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  //  uint32_t                        inputAttachmentCount;
  //  const VkAttachmentReference*    pInputAttachments;
	subpassDescr.colorAttachmentCount = 1;
	subpassDescr.pColorAttachments = &colorAttachmentRef;
	//const VkAttachmentReference*    pResolveAttachments;
	//const VkAttachmentReference*    pDepthStencilAttachment;
	//uint32_t                        preserveAttachmentCount;
	//const uint32_t*                 pPreserveAttachments;

	VkSubpassDependency subpassDep = {};
	subpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDep.dstSubpass = 0;
	subpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDep.srcAccessMask = 0;
	subpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDep.dependencyFlags = VK_FLAGS_NONE;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.flags = VK_FLAGS_NONE;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachmentDescr;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescr;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &subpassDep;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VK_CALL(vkCreateRenderPass(vkCtx.logicalDevice, &renderPassInfo, nullptr, &renderPass));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = nullptr;
	graphicsPipelineCreateInfo.flags = VK_FLAGS_NONE;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos;
	graphicsPipelineCreateInfo.pVertexInputState = &pipeVertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &pipeInputAssemblyCreateInfo;
//	graphicsPipelineCreateInfo.pTessellationState =;
	graphicsPipelineCreateInfo.pViewportState = &pipeViewPortStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &pipeRastStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &pipeMultisampleCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &pipeDepthStencilCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &pipeColorBlendCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &pipeDynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = renderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	VkPipeline graphicsPipe = VK_NULL_HANDLE;
	VK_CALL(vkCreateGraphicsPipelines(vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipe));
	
	state->pipeline = graphicsPipe;
	state->renderPass = renderPass;
	state->viewport = viewport;
	state->pipelineLayout = pipelineLayout;

	return VK_TRUE;
}