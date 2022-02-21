#include "vk_resource.h"
#include "vk_dbg.h"
#include "vk_boilerplate.h"
#include "vk_commands.h"

#include <array>

static VkImageAspectFlags get_image_aspect_from_usage(VkImageUsageFlags usageFlags)
{
	VkImageAspectFlags aspectFlags = {};
	if(usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	if(usageFlags & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT))
	{
		aspectFlags |= VK_IMAGE_ASPECT_COLOR_BIT;
	}
	return aspectFlags;
}

ImageResource create_image_resource(const VulkanGlobalContext& vkCtx, VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags usageFlags, VkImageLayout initialLayout)
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.flags = VK_FLAGS_NONE;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = imageFormat;
	imageCreateInfo.extent = imageExtent;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = usageFlags;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 1;
	imageCreateInfo.pQueueFamilyIndices = &vkCtx.queueFamIdx;
	imageCreateInfo.initialLayout = initialLayout;
	
	VkFormatProperties formatProps = {};
	vkGetPhysicalDeviceFormatProperties(vkCtx.physicalDevice, imageFormat, &formatProps);
	VkImage image = VK_NULL_HANDLE;
	VK_CALL(vkCreateImage(vkCtx.logicalDevice, &imageCreateInfo, nullptr, &image));

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(vkCtx.logicalDevice, image, &memRequirements);
	VkPhysicalDeviceMemoryProperties deviceMemProps = {};
	vkGetPhysicalDeviceMemoryProperties(vkCtx.physicalDevice, &deviceMemProps);

	uint32_t preferredMemTypeIndex = -1;
	find_required_memtype_index(vkCtx.physicalDevice, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &preferredMemTypeIndex);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = nullptr;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = preferredMemTypeIndex;

	VkDeviceMemory imageMemory = {};
	VK_CALL(vkAllocateMemory(vkCtx.logicalDevice, &memAllocInfo, nullptr, &imageMemory));

	VK_CALL(vkBindImageMemory(vkCtx.logicalDevice, image, imageMemory, 0));

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;
	imageViewCreateInfo.flags = VK_FLAGS_NONE;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = imageFormat;
	imageViewCreateInfo.subresourceRange.aspectMask = get_image_aspect_from_usage(usageFlags);
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView imageView = VK_NULL_HANDLE;
	VK_CALL(vkCreateImageView(vkCtx.logicalDevice, &imageViewCreateInfo, nullptr, &imageView));
	
	ImageResource imageResource = {};
	imageResource.image = image;
	imageResource.view = imageView;
	imageResource.layout = initialLayout;
	imageResource.imageSize = memAllocInfo.allocationSize;
	imageResource.backupMemory = imageMemory;
	imageResource.format = imageFormat;
	return imageResource;
}


VkSampler create_default_sampler(VkDevice logicalDevice, bool* status)
{
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.f;
	samplerCreateInfo.maxLod = 1.f;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	
	VkSampler sampler = VK_NULL_HANDLE;
	auto result = vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &sampler);
	if(status)
	{
		*status = !result;
	}
	return sampler;
}

bool find_required_memtype_index(
	VkPhysicalDevice 		physicalDevice,
	VkMemoryRequirements	resourceMemoryRequirements,
	VkMemoryPropertyFlags 	desiredMemoryFlags,//DEVICE_LOCAL etc
	uint32_t* 				memoryTypeIndex)
{
	assert(memoryTypeIndex);
	
	VkPhysicalDeviceMemoryProperties deviceMemProps = {};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemProps);

	uint32_t resourceMemTypeBits = resourceMemoryRequirements.memoryTypeBits;
	for(uint32_t i = 0; i < deviceMemProps.memoryTypeCount; i++)
	{
		//check if device supports resource memory type
		if(resourceMemTypeBits & (1 << i))
		{
			//check if user desired memory properties are supported(eg DEVICE_LOCAL)
			if(deviceMemProps.memoryTypes[i].propertyFlags & desiredMemoryFlags)
			{
				*memoryTypeIndex = i;
				return true;
			}
		}
	}
	
	magma::log::error("Failed to find memory type index!");
	return false;
}

Buffer create_buffer(
	const VulkanGlobalContext&	vkCtx,
	VkBufferUsageFlags 			usageFlags,
	VkMemoryPropertyFlags		requiredMemProperties,
	std::size_t					size)
{
	Buffer buffer = {};

	//prepare staging buffer for vertices gpu upload
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.pNext = nullptr;
	bufferCreateInfo.flags = {};
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usageFlags;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	// bufferCreateInfo.queueFamilyIndexCount = 1;
	// bufferCreateInfo.pQueueFamilyIndices = &vkCtx.queueFamIdx;

	VkBuffer bufferHandle = VK_NULL_HANDLE;
	VK_CALL(vkCreateBuffer(vkCtx.logicalDevice, &bufferCreateInfo, nullptr, &bufferHandle)); 

	VkPhysicalDeviceMemoryProperties memProps = {};
	vkGetPhysicalDeviceMemoryProperties(vkCtx.physicalDevice, &memProps);

	VkMemoryRequirements memReqs = {};
	vkGetBufferMemoryRequirements(vkCtx.logicalDevice, bufferHandle, &memReqs);
	uint32_t memTypeBits = memReqs.memoryTypeBits;

	uint32_t memTypeIdx = {};
	if(!find_required_memtype_index(vkCtx.physicalDevice, memReqs, requiredMemProperties, &memTypeIdx))
	{
		magma::log::error("Failed to find memory type index for required buffer usage!");
		return buffer;
	}

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = nullptr;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = memTypeIdx;

	VkDeviceMemory backupMemory = VK_NULL_HANDLE;
	
	VK_CALL(vkAllocateMemory(vkCtx.logicalDevice, &memAllocInfo, nullptr, &backupMemory));
	VK_CALL(vkBindBufferMemory(vkCtx.logicalDevice, bufferHandle, backupMemory, 0));
	
	buffer.buffer = bufferHandle;
	buffer.backupMemory = backupMemory;
	buffer.bufferSize = size;
	buffer.alignedSize = memReqs.size;
	return buffer;
}

void destroy_buffer(VkDevice logicalDevice, Buffer* buffer)
{
	vkDestroyBuffer(logicalDevice, buffer->buffer, nullptr);
	vkFreeMemory(logicalDevice, buffer->backupMemory, nullptr);
	buffer->buffer = VK_NULL_HANDLE;
	buffer->backupMemory = nullptr;
	buffer->bufferSize = 0;
}

std::size_t alignUp(std::size_t val, std::size_t alignment)
{
	return (val + alignment - 1)& ~(alignment - 1);
}

std::size_t alignDown(std::size_t val, std::size_t alignment)
{
	return val& ~(alignment - 1);
}

bool isPowerOfTwo(std::size_t val)
{
	return (val & (val - 1)) == 0;
}

//TODO: incoherent allocations!
VkResult copy_data_to_host_visible_buffer(const VulkanGlobalContext& vkCtx, VkDeviceSize offset, const void* copyFrom, std::size_t copyByteSize, Buffer* buffer)
{
	void* mappedArea = nullptr;

	VK_CALL_RETURN(vkMapMemory(vkCtx.logicalDevice, buffer->backupMemory, offset, copyByteSize, VK_FLAGS_NONE, &mappedArea));
	if(!mappedArea)
		return VK_ERROR_MEMORY_MAP_FAILED;

	memcpy(mappedArea, copyFrom, copyByteSize);
	
	// explicitly inform the driver that staging buffer contents were modified
	// VkMappedMemoryRange mappedMemoryRange = {};
	// mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	// mappedMemoryRange.pNext = nullptr;
	// mappedMemoryRange.memory = buffer->backupMemory;
	// mappedMemoryRange.offset = offset;
	// mappedMemoryRange.size = copyByteSize;
// 
	// VK_CALL_RETURN(vkFlushMappedMemoryRanges(vkCtx.logicalDevice, 1, &mappedMemoryRange));

	vkUnmapMemory(vkCtx.logicalDevice, buffer->backupMemory);

	return VK_SUCCESS;
}

VkBool32 push_data_to_device_local_buffer(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, Buffer* deviceLocalBuffer, VkQueue queue)
{

	VkCommandBufferAllocateInfo cmdBuffAllocInfo = {};
	cmdBuffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBuffAllocInfo.pNext = nullptr;
	cmdBuffAllocInfo.commandPool = commandPool;
	cmdBuffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBuffAllocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CALL(vkAllocateCommandBuffers(vkCtx.logicalDevice, &cmdBuffAllocInfo, &commandBuffer));

	//begin recording transfer commands
	VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = nullptr;
	cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	VK_CALL(vkBeginCommandBuffer(commandBuffer, &cmdBufferBeginInfo));

	VkBufferCopy bufferCopy = {};
	bufferCopy.size = stagingBuffer.bufferSize;

	vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, deviceLocalBuffer->buffer, 1, &bufferCopy);
	
	VK_CALL(vkEndCommandBuffer(commandBuffer));
	
	VkSubmitInfo queueSubmitInfo = {};
	queueSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext = nullptr;
	queueSubmitInfo.waitSemaphoreCount = 0;
	queueSubmitInfo.pWaitSemaphores = nullptr;
	queueSubmitInfo.pWaitDstStageMask = nullptr;
	queueSubmitInfo.commandBufferCount = 1;
	queueSubmitInfo.pCommandBuffers = &commandBuffer;
	queueSubmitInfo.signalSemaphoreCount = 0;
	queueSubmitInfo.pSignalSemaphores = nullptr;

	VkFence fence = create_fence(vkCtx.logicalDevice);
	VkQueue submitQueue = queue == VK_NULL_HANDLE ?  vkCtx.graphicsQueue : queue;
	VK_CALL(vkQueueSubmit(submitQueue, 1, &queueSubmitInfo, fence));
	VK_CALL(vkWaitForFences(vkCtx.logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));
	
	vkDestroyFence(vkCtx.logicalDevice, fence, nullptr);

	vkFreeCommandBuffers(vkCtx.logicalDevice, commandPool, 1, &commandBuffer);
	
	return VK_TRUE;
}

VkBool32 push_texture_to_device_local_image(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, VkExtent3D imageExtent, ImageResource* textureResource)
{
	assert(textureResource);

	VkCommandBuffer cmdBuff = VK_NULL_HANDLE;
	//1.recording transfer commands into command buffer
	create_command_buffer(vkCtx.logicalDevice, commandPool, &cmdBuff);

	VkCommandBufferBeginInfo cmdBuffBegInfo = {};
	cmdBuffBegInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBuffBegInfo.pNext = nullptr;
	cmdBuffBegInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBuffBegInfo.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(cmdBuff, &cmdBuffBegInfo);

	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferOffset = 0;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageOffset = {0, 0, 0};
	copyRegion.imageExtent = imageExtent;

	//issue a barrier to transition our newly created texture from undefined layout to DST_OPTIMAL
	VkImageMemoryBarrier imageMemBarrier = {};
	imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemBarrier.pNext = nullptr;
	imageMemBarrier.srcAccessMask = 0;
	imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemBarrier.srcQueueFamilyIndex = vkCtx.queueFamIdx;
	imageMemBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
	imageMemBarrier.image = textureResource->image;
	imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemBarrier.subresourceRange.baseMipLevel = 0;
	imageMemBarrier.subresourceRange.levelCount = 1;
	imageMemBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemBarrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(
		cmdBuff, 
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemBarrier
	);

	vkCmdCopyBufferToImage(cmdBuff, stagingBuffer.buffer, textureResource->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	//issue another barrier
	//transfer texture layout to the shader layout so it can be sampled
	imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	vkCmdPipelineBarrier(
		cmdBuff, 
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemBarrier
	);
	
	vkEndCommandBuffer(cmdBuff);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuff;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	//submit transfer command into queue
	VkFence textureTransferredFence = create_fence(vkCtx.logicalDevice);
	VK_CALL(vkQueueSubmit(vkCtx.graphicsQueue, 1, &submitInfo, textureTransferredFence));
	VK_CALL(vkWaitForFences(vkCtx.logicalDevice, 1, &textureTransferredFence, VK_TRUE, UINT64_MAX));
	
	//cleanup
	vkDestroyFence(vkCtx.logicalDevice, textureTransferredFence, nullptr);
	vkFreeCommandBuffers(vkCtx.logicalDevice, commandPool, 1, &cmdBuff);
	
	textureResource->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	return VK_TRUE;
}

ImageResource create_cubemap_image(const VulkanGlobalContext& vkCtx, VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags usageFlags, VkImageLayout initialLayout)
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = imageFormat;
	imageCreateInfo.extent = imageExtent;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 6;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = usageFlags;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 1;
	imageCreateInfo.pQueueFamilyIndices = &vkCtx.queueFamIdx;
	imageCreateInfo.initialLayout = initialLayout;

	VkImage image = VK_NULL_HANDLE;
	VK_CALL(vkCreateImage(vkCtx.logicalDevice, &imageCreateInfo, nullptr, &image));

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(vkCtx.logicalDevice, image, &memRequirements);
	VkPhysicalDeviceMemoryProperties deviceMemProps = {};
	vkGetPhysicalDeviceMemoryProperties(vkCtx.physicalDevice, &deviceMemProps);

	uint32_t preferredMemTypeIndex = -1;
	find_required_memtype_index(vkCtx.physicalDevice, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &preferredMemTypeIndex);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = nullptr;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = preferredMemTypeIndex;

	VkDeviceMemory imageMemory = {};
	VK_CALL(vkAllocateMemory(vkCtx.logicalDevice, &memAllocInfo, nullptr, &imageMemory));

	VK_CALL(vkBindImageMemory(vkCtx.logicalDevice, image, imageMemory, 0));

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;
	imageViewCreateInfo.flags = VK_FLAGS_NONE;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	imageViewCreateInfo.format = imageFormat;
	imageViewCreateInfo.subresourceRange.aspectMask = get_image_aspect_from_usage(usageFlags);
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 6;

	VkImageView imageView = VK_NULL_HANDLE;
	VK_CALL(vkCreateImageView(vkCtx.logicalDevice, &imageViewCreateInfo, nullptr, &imageView));
	
	ImageResource imageResource = {};
	imageResource.image = image;
	imageResource.view = imageView;
	imageResource.layout = initialLayout;
	imageResource.imageSize = memAllocInfo.allocationSize;
	imageResource.backupMemory = imageMemory;

	return imageResource;
}

VkBool32 push_cubemap_to_device_local_image(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, VkExtent3D imageExtent, std::size_t planeStride, ImageResource* textureResource)
{
	assert(textureResource);

	VkCommandBuffer cmdBuff = VK_NULL_HANDLE;
	//1.recording transfer commands into command buffer
	create_command_buffer(vkCtx.logicalDevice, commandPool, &cmdBuff);

	VkCommandBufferBeginInfo cmdBuffBegInfo = {};
	cmdBuffBegInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBuffBegInfo.pNext = nullptr;
	cmdBuffBegInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBuffBegInfo.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(cmdBuff, &cmdBuffBegInfo);

	std::array<VkBufferImageCopy, 6> copyRegions = {};
	
	std::size_t bufferOffset = 0; 
	for(std::size_t i = 0; i < copyRegions.size(); i++)
	{
		copyRegions[i].bufferOffset = bufferOffset;
		copyRegions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegions[i].imageSubresource.mipLevel = 0;
		copyRegions[i].imageSubresource.baseArrayLayer = i;
		copyRegions[i].imageSubresource.layerCount = 1;
		copyRegions[i].imageOffset = {0, 0, 0};
		copyRegions[i].imageExtent = imageExtent;
		bufferOffset += planeStride;
	}

	//issue a barrier to transition our newly created texture from undefined layout to DST_OPTIMAL
	VkImageMemoryBarrier imageMemBarrier = {};
	imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemBarrier.pNext = nullptr;
	imageMemBarrier.srcAccessMask = 0;
	imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemBarrier.srcQueueFamilyIndex = vkCtx.queueFamIdx;
	imageMemBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
	imageMemBarrier.image = textureResource->image;
	imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemBarrier.subresourceRange.baseMipLevel = 0;
	imageMemBarrier.subresourceRange.levelCount = 1;
	imageMemBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemBarrier.subresourceRange.layerCount = 6;

	vkCmdPipelineBarrier(
		cmdBuff, 
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemBarrier
	);

	vkCmdCopyBufferToImage(cmdBuff, stagingBuffer.buffer, textureResource->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegions.size(), copyRegions.data());

	//issue another barrier
	//transfer texture layout to the shader layout so it can be sampled
	imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	vkCmdPipelineBarrier(
		cmdBuff, 
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemBarrier
	);
	
	vkEndCommandBuffer(cmdBuff);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuff;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	//submit transfer command into queue
	VkFence textureTransferredFence = create_fence(vkCtx.logicalDevice);
	VK_CALL(vkQueueSubmit(vkCtx.graphicsQueue, 1, &submitInfo, textureTransferredFence));
	VK_CALL(vkWaitForFences(vkCtx.logicalDevice, 1, &textureTransferredFence, VK_TRUE, UINT64_MAX));
	
	//cleanup
	vkDestroyFence(vkCtx.logicalDevice, textureTransferredFence, nullptr);
	vkFreeCommandBuffers(vkCtx.logicalDevice, commandPool, 1, &cmdBuff);
	
	return VK_TRUE;	
}

void destroy_image_resource(VkDevice logicalDevice, ImageResource* image)
{
	vkDestroyImageView(logicalDevice, image->view, nullptr);
	vkDestroyImage(logicalDevice, image->image, nullptr);
	vkFreeMemory(logicalDevice, image->backupMemory, nullptr);
	
	image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	image->imageSize = 0;
	image->backupMemory = VK_NULL_HANDLE;
}