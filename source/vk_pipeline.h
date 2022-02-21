#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H
#include "vk_types.h"


VkPipelineShaderStageCreateInfo fill_shader_stage_ci(const VkDevice logicalDevice,
 	const char* shaderSource, VkShaderStageFlagBits shaderStage);

VkPipelineVertexInputStateCreateInfo fill_vertex_input_state_ci(
	const VkVertexInputBindingDescription* bindingDescr, uint32_t bindingDescrCount,
	const VkVertexInputAttributeDescription* vertexAttribDescr, uint32_t attribDescrCount);

VkPipelineLayout create_pipe_layout(VkDevice logicalDevice, const VkDescriptorSetLayout* descrSetLayouts,
	uint32_t descrSetLayoutCount, const VkPushConstantRange* pushConstantRanges, uint32_t pushConstantRangeCount);

VkPipelineRasterizationStateCreateInfo fill_raster_state_ci(VkCullModeFlags cullMode, VkFrontFace frontFaceWindingOrder);

VkViewport create_viewport(VkExtent2D windowExtent);

VkPipelineViewportStateCreateInfo fill_viewport_state_ci(const VkViewport& viewport, const VkRect2D& scissors);

VkPipelineInputAssemblyStateCreateInfo fill_input_assembly_state_ci(VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

VkPipelineMultisampleStateCreateInfo fill_multisample_state_ci();

VkPipelineDepthStencilStateCreateInfo fill_depth_stencil_state_ci(VkCompareOp depthCompareOp);

VkPipelineColorBlendStateCreateInfo fill_color_blend_state_ci(const VkPipelineColorBlendAttachmentState& blendAttachmentState);

VkPipelineDynamicStateCreateInfo fill_dynamic_state_ci(VkDynamicState* dynStates, uint32_t statesCount);


#endif