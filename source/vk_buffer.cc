#include "vk_buffer.h"
#include "vk_dbg.h"
#include "vk_boilerplate.h"

Buffer createBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkBufferUsageFlags usageFlags,
	VkMemoryPropertyFlagBits requiredMemProperties, std::size_t size, uint32_t queueFamilyIdx)
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
	bufferCreateInfo.queueFamilyIndexCount = 1;
	bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIdx;

	VkBuffer bufferHandle = VK_NULL_HANDLE;
	VK_CALL(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &bufferHandle)); 

	VkPhysicalDeviceMemoryProperties memProps = {};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

	VkMemoryRequirements memReqs = {};
	vkGetBufferMemoryRequirements(logicalDevice, bufferHandle, &memReqs);
	uint32_t memTypeBits = memReqs.memoryTypeBits;

	int32_t memTypeIdx = -1;
	for(uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{	
		//if required memory type for buffer is supported by device
		if(memTypeBits & (1 << i))
		{
			//if selected memory type fits our usage requirements
			if(memProps.memoryTypes[i].propertyFlags & requiredMemProperties)
			{
				memTypeIdx = i;
				break;
			}
		}
	}

	if(memTypeIdx < 0)
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
	
	VK_CALL(vkAllocateMemory(logicalDevice, &memAllocInfo, nullptr, &backupMemory));
	VK_CALL(vkBindBufferMemory(logicalDevice, bufferHandle, backupMemory, 0));
	
	buffer.buffer = bufferHandle;
	buffer.backupMemory = backupMemory;
	buffer.bufferSize = size;
	return buffer;
}

void destroyBuffer(VkDevice logicalDevice, Buffer* buffer)
{
	vkDestroyBuffer(logicalDevice, buffer->buffer, nullptr);
	vkFreeMemory(logicalDevice, buffer->backupMemory, nullptr);
	buffer->buffer = VK_NULL_HANDLE;
	buffer->backupMemory = nullptr;
	buffer->bufferSize = 0;
}

VkResult copyDataToStagingBuffer(VkDevice logicalDevice, VkDeviceSize offset, const void* copyFrom, Buffer* buffer)
{
	void* mappedArea = nullptr;
	VK_CALL_RETURN(vkMapMemory(logicalDevice, buffer->backupMemory, offset, buffer->bufferSize, VK_FLAGS_NONE, &mappedArea));
	if(!mappedArea)
		return VK_ERROR_MEMORY_MAP_FAILED;
	
	memcpy(mappedArea, copyFrom, buffer->bufferSize);

	//explicitly inform the driver that staging buffer contents were modified
	VkMappedMemoryRange mappedMemoryRange = {};
	mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedMemoryRange.pNext = nullptr;
	mappedMemoryRange.memory = buffer->backupMemory;
	mappedMemoryRange.offset = 0;
	mappedMemoryRange.size = VK_WHOLE_SIZE;

	VK_CALL_RETURN(vkFlushMappedMemoryRanges(logicalDevice, 1, &mappedMemoryRange));

	vkUnmapMemory(logicalDevice, buffer->backupMemory);

	return VK_SUCCESS;
}

VkBool32 pushDataToDeviceLocalBuffer(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, Buffer* deviceLocalBuffer)
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

	VkFence fence = createFence(vkCtx.logicalDevice);

	VK_CALL(vkQueueSubmit(vkCtx.graphicsQueue, 1, &queueSubmitInfo, fence));
	VK_CALL(vkWaitForFences(vkCtx.logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));
	
	vkDestroyFence(vkCtx.logicalDevice, fence, nullptr);

	vkFreeCommandBuffers(vkCtx.logicalDevice, commandPool, 1, &commandBuffer);
	
	return VK_TRUE;
}