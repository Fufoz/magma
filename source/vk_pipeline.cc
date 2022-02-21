#include "vk_pipeline.h"
#include "maths.h"
#include "vk_shader.h"
#include "vk_dbg.h"


VkPipelineShaderStageCreateInfo fill_shader_stage_ci(
	const VkDevice 			logicalDevice,
 	const char* 			shaderSource,
	VkShaderStageFlagBits 	shaderStage)
{
	assert(shaderSource);

	Shader shader = {};
	VK_CHECK(load_shader(logicalDevice, shaderSource, shaderStage, &shader));
	
	VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
	shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo.stage = shaderStage;
	shaderStageCreateInfo.module = shader.handle;
	shaderStageCreateInfo.pName = "main";
	shaderStageCreateInfo.pSpecializationInfo = nullptr;

	return shaderStageCreateInfo;
}

VkPipelineVertexInputStateCreateInfo fill_vertex_input_state_ci(
	const VkVertexInputBindingDescription* 		bindingDescr,
	uint32_t 									bindingDescrCount,
	const VkVertexInputAttributeDescription* 	vertexAttribDescr,
	uint32_t 									attribDescrCount)
{
	VkPipelineVertexInputStateCreateInfo vertexInpuStateCreateInfo = {};
	vertexInpuStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInpuStateCreateInfo.vertexBindingDescriptionCount = bindingDescrCount;
	vertexInpuStateCreateInfo.pVertexBindingDescriptions = bindingDescr;
	vertexInpuStateCreateInfo.vertexAttributeDescriptionCount = attribDescrCount;
	vertexInpuStateCreateInfo.pVertexAttributeDescriptions = vertexAttribDescr;
	return vertexInpuStateCreateInfo;
}

VkViewport create_viewport(VkExtent2D windowExtent)
{
	VkViewport viewport = {};
	viewport.x = 0.f;
	viewport.y = windowExtent.height;
	viewport.width = (float)windowExtent.width;
	viewport.height = -(float)windowExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;
	return viewport;
}

VkPipelineViewportStateCreateInfo fill_viewport_state_ci(const VkViewport& viewport, const VkRect2D& scissors)
{
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissors;

	return viewportStateCreateInfo;
}

VkPipelineRasterizationStateCreateInfo fill_raster_state_ci(
	VkCullModeFlags cullMode, VkFrontFace frontFaceWindingOrder)
{
	VkPipelineRasterizationStateCreateInfo rasterStateCreateInfo = {};	
	rasterStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterStateCreateInfo.pNext = nullptr;
	rasterStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterStateCreateInfo.cullMode = cullMode;
	rasterStateCreateInfo.frontFace = frontFaceWindingOrder;
	rasterStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterStateCreateInfo.depthBiasConstantFactor = 0.f;
	rasterStateCreateInfo.depthBiasClamp = 0.f;
	rasterStateCreateInfo.depthBiasSlopeFactor = 0.f;
	rasterStateCreateInfo.lineWidth = 1.f;
	
	return rasterStateCreateInfo;
}

VkPipelineInputAssemblyStateCreateInfo fill_input_assembly_state_ci(VkPrimitiveTopology primitiveTopology)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = primitiveTopology;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
	return inputAssemblyCreateInfo;
}

VkPipelineMultisampleStateCreateInfo fill_multisample_state_ci()
{	
	VkPipelineMultisampleStateCreateInfo msStateCreateInfo = {};
	msStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msStateCreateInfo.pNext = nullptr;
	msStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	msStateCreateInfo.sampleShadingEnable = VK_FALSE;
	msStateCreateInfo.pSampleMask = nullptr;
	msStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	msStateCreateInfo.alphaToOneEnable = VK_FALSE;
	return msStateCreateInfo;
}

VkPipelineDepthStencilStateCreateInfo fill_depth_stencil_state_ci(VkCompareOp depthCompareOp)
{
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	//override depth buffer if frame sample's depth is greater or equal to the one stored in depth buffer.
	depthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	// depthStencilStateCreateInfo.front = ;
	// depthStencilStateCreateInfo.back = ;
	// depthStencilStateCreateInfo.minDepthBounds = 0.f;
	// depthStencilStateCreateInfo.maxDepthBounds = -10.f;
	return depthStencilStateCreateInfo;
}

VkPipelineColorBlendStateCreateInfo fill_color_blend_state_ci(const VkPipelineColorBlendAttachmentState& blendAttachmentState)
{
	// Color blend state describes how blend factors are calculated (if used)
	// We need one blend attachment state per color attachment (even if blending is not used)
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &blendAttachmentState;
	return colorBlendStateCreateInfo;
}

VkPipelineDynamicStateCreateInfo fill_dynamic_state_ci(VkDynamicState* dynStates, uint32_t statesCount)
{
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = statesCount;
	dynamicStateCreateInfo.pDynamicStates = dynStates;
	
	return dynamicStateCreateInfo;
}

VkPipelineLayout create_pipe_layout(
	VkDevice						logicalDevice,
	const VkDescriptorSetLayout* 	descrSetLayouts,
	uint32_t 						descrSetLayoutCount,
	const VkPushConstantRange* 		pushConstantRanges,
	uint32_t 						pushConstantRangeCount)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = descrSetLayoutCount;
	pipelineLayoutCreateInfo.pSetLayouts = descrSetLayouts;
	pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRangeCount;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	return pipelineLayout;
}
