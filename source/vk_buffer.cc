#include "vk_buffer.h"
#include "vk_dbg.h"
Buffer createBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkBufferUsageFlags usageFlags,
	VkMemoryPropertyFlagBits memoryPropertyFlags, std::size_t size, uint32_t queueFamilyIdx, const void* copyFrom)
{
	Buffer buffer = {};

	//prepare staging buffer for vertices gpu upload
	VkBufferCreateInfo stagingBufferCreateInfo = {};
	stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferCreateInfo.pNext = nullptr;
	stagingBufferCreateInfo.flags = {};
	stagingBufferCreateInfo.size = size;
	stagingBufferCreateInfo.usage = usageFlags;
	stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	stagingBufferCreateInfo.queueFamilyIndexCount = 1;
	stagingBufferCreateInfo.pQueueFamilyIndices = &queueFamilyIdx;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VK_CALL(vkCreateBuffer(logicalDevice, &stagingBufferCreateInfo, nullptr, &stagingBuffer)); 

	VkPhysicalDeviceMemoryProperties memProps = {};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
	VkMemoryPropertyFlags requiredMemProperties;


	VkMemoryRequirements memReqs = {};
	vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer, &memReqs);
	uint32_t memTypeBits = memReqs.memoryTypeBits;

	int32_t memTypeIdx = -1;
	for(uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{	
		//if required memory type for buffer is supported by device
		if(memTypeBits & (1 << i))
		{
			//if selected memory type fits our usage requirements
			if(memProps.memoryTypes[i].propertyFlags & usageFlags)
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

	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	
	VK_CALL(vkAllocateMemory(logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CALL(vkBindBufferMemory(logicalDevice, stagingBuffer, stagingMemory, 0));
	
	buffer.buffer = stagingBuffer;
	buffer.backupMemory = stagingMemory;
	
	return buffer;
}

void destroyBuffer(VkDevice logicalDevice, Buffer* buffer)
{
	vkDestroyBuffer(logicalDevice, buffer->buffer, nullptr);
	vkFreeMemory(logicalDevice, buffer->backupMemory, nullptr);
}