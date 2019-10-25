#include "vk_buffer.h"
#include "vk_dbg.h"

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