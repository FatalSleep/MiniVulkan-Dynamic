#ifndef MINIVK_MINIVKIMAGE
#define MINIVK_MINIVKIMAGE
	#include "./MiniVk.hpp"

	namespace MINIVULKAN_NS {
		enum class MiniVkBufferType {
			VKVMA_BUFFER_TYPE_VERTEX, VKVMA_BUFFER_TYPE_INDEX, VKVMA_BUFFER_TYPE_UNIFORM, VKVMA_BUFFER_TYPE_STAGING
		};
		/*
			ABVOUT BUFFERS & IMAGES:
				When Creating Buffers:
					Buffers must be initialized with a VkDviceSize, which is the size of the data in BYTES, not the
					size of the data container (number of items). This same principle applies to stagging buffer data.

				There are 3 types of GPU dedicated memory buffers:
					Vertex:		Allows you to send mesh triangle data to the GPU.
					Index:		Allws you to send mapped indices for vertex buffers to the GPU.
					Uniform:	Allows you to send data to shaders using uniforms.
						* Push Constants are an alternative that do not require buffers, simply use: vkCmdPushConstants(...).

				The last buffer type is a CPU memory buffer for transfering data from the CPU to the GPU:
					Staging:	Staging CPU data for transfer to the GPU.

				Render images are for rendering sprites or textures on the GPU (similar to the swap chain, but handled manually).
					The default image layout is: VK_IMAGE_LAYOUT_UNDEFINED
					To render to shaders you must change/transition the layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
					Once the layout is set for transfering you can write data to the image from CPU memory to GPU memory.
					Finally for use in shaders you need to change the layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
		*/

		class MiniVkBuffer : public MiniVkObject {
		private:
			void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags) {
				VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
				bufCreateInfo.size = size;
				bufCreateInfo.usage = usage;

				VmaAllocationCreateInfo allocCreateInfo = {};
				allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
				allocCreateInfo.flags = flags;
				
				if (vmaCreateBuffer(vmAlloc.GetAllocator(), &bufCreateInfo, &allocCreateInfo, &buffer, &memory, &description) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Could not allocate memory for MiniVkBuffer!");
			}
		public:
			MiniVkInstance& mvkLayer;
			MiniVkMemAlloc& vmAlloc;

			VkBuffer buffer = VK_NULL_HANDLE;
			VmaAllocation memory = VK_NULL_HANDLE;
			VmaAllocationInfo description;
			VkDeviceSize size;

			void Disposable() {
				vmaDestroyBuffer(vmAlloc.GetAllocator(), buffer, memory);
			}

			MiniVkBuffer(MiniVkInstance& mvkLayer, MiniVkMemAlloc& vmAlloc, VkDeviceSize dataSize, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags)
			: mvkLayer(mvkLayer), vmAlloc(vmAlloc), size(dataSize) {
				onDispose += std::callback<>(this, &MiniVkBuffer::Disposable);
				CreateBuffer(size, usage, flags);
			}

			MiniVkBuffer(MiniVkInstance& mvkLayer, MiniVkMemAlloc& vmAlloc, VkDeviceSize dataSize, MiniVkBufferType type)
			: mvkLayer(mvkLayer), vmAlloc(vmAlloc), size(dataSize) {
				onDispose += std::callback<>(this, &MiniVkBuffer::Disposable);

				switch (type) {
					case MiniVkBufferType::VKVMA_BUFFER_TYPE_VERTEX:
					CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case MiniVkBufferType::VKVMA_BUFFER_TYPE_INDEX:
					CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case MiniVkBufferType::VKVMA_BUFFER_TYPE_UNIFORM:
					CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case MiniVkBufferType::VKVMA_BUFFER_TYPE_STAGING:
					default:
					CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
					break;
				}
			}

			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			void StageBufferData(VkQueue graphicsQueue, VkCommandPool commandPool, void* data, VkDeviceSize dataSize, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
				MiniVkBuffer stagingBuffer = MiniVkBuffer(mvkLayer, vmAlloc, dataSize, MiniVkBufferType::VKVMA_BUFFER_TYPE_STAGING);
				memcpy(stagingBuffer.description.pMappedData, data, (size_t)dataSize);
				TransferBufferCmd(graphicsQueue, commandPool, stagingBuffer, size, srcOffset, dstOffset);
				stagingBuffer.Dispose();
			}

			void TransferBufferCmd(VkQueue graphicsQueue, VkCommandPool commandPool, MiniVkBuffer& srcBuffer, VkDeviceSize dataSize, VkDeviceSize srceOffset = 0, VkDeviceSize destOffset = 0) {
				VkCommandBuffer commandBuffer = BeginTransferCmd(graphicsQueue, commandPool);

				VkBufferCopy copyRegion{};
				copyRegion.srcOffset = srceOffset;
				copyRegion.dstOffset = destOffset;
				copyRegion.size = dataSize;
				vkCmdCopyBuffer(commandBuffer, srcBuffer.buffer, buffer, 1, &copyRegion);

				EndTransferCmd(graphicsQueue, commandPool, commandBuffer);
			}

			VkCommandBuffer BeginTransferCmd(VkQueue graphicsQueue, VkCommandPool commandPool) {
				VkCommandBufferAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandPool = commandPool;
				allocInfo.commandBufferCount = 1;

				VkCommandBuffer commandBuffer;
				vkAllocateCommandBuffers(mvkLayer.logicalDevice, &allocInfo, &commandBuffer);

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				vkBeginCommandBuffer(commandBuffer, &beginInfo);
				return commandBuffer;
			}

			void EndTransferCmd(VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
				vkEndCommandBuffer(commandBuffer);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(graphicsQueue);
				vkFreeCommandBuffers(mvkLayer.logicalDevice, commandPool, 1, &commandBuffer);
			}

			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		};

		class MiniVkImage : public MiniVkObject {
		private:
			MiniVkInstance& mvkLayer;
			MiniVkMemAlloc& vmAlloc;
						
			VkSemaphore availableSmphr = VK_NULL_HANDLE;
			VkSemaphore finishedSmphr = VK_NULL_HANDLE;
			VkFence inFlightFence = VK_NULL_HANDLE;

			void CreateImageView() {
				VkImageViewCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = image;
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = format;

				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				if (vkCreateImageView(mvkLayer.logicalDevice, &createInfo, nullptr, &imageView) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to create MiniVkImage view!");
			}

			void CreateTextureSampler() {
				VkSamplerCreateInfo samplerInfo {};
				samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerInfo.magFilter = VK_FILTER_LINEAR;
				samplerInfo.minFilter = VK_FILTER_LINEAR;
				samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.anisotropyEnable = VK_FALSE;
				
				VkPhysicalDeviceProperties properties{};
				vkGetPhysicalDeviceProperties(mvkLayer.physicalDevice, &properties);
				samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

				samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				samplerInfo.unnormalizedCoordinates = VK_FALSE;
				samplerInfo.compareEnable = VK_FALSE;
				samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				samplerInfo.mipLodBias = 0.0f;
				samplerInfo.minLod = 0.0f;
				samplerInfo.maxLod = 0.0f;

				if (vkCreateSampler(mvkLayer.logicalDevice, &samplerInfo, nullptr, &imageSampler) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to create image texture sampler!");
			}

			void CreateSyncObjects() {
				VkSemaphoreCreateInfo semaphoreInfo{};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

				VkFenceCreateInfo fenceInfo{};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

				if (vkCreateSemaphore(mvkLayer.logicalDevice, &semaphoreInfo, nullptr, &availableSmphr) != VK_SUCCESS ||
				vkCreateSemaphore(mvkLayer.logicalDevice, &semaphoreInfo, nullptr, &finishedSmphr) != VK_SUCCESS ||
				vkCreateFence(mvkLayer.logicalDevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to create synchronization objects for a frame!");
			}
		public:
			void Disposable() {
				vkDestroySemaphore(mvkLayer.logicalDevice, availableSmphr, nullptr);
				vkDestroySemaphore(mvkLayer.logicalDevice, finishedSmphr, nullptr);
				vkDestroyFence(mvkLayer.logicalDevice, inFlightFence, nullptr);

				vkDestroySampler(mvkLayer.logicalDevice, imageSampler, nullptr);
				vkDestroyImageView(mvkLayer.logicalDevice, imageView, nullptr);
				vmaDestroyImage(vmAlloc.GetAllocator(), image, memory);
			}

			VmaAllocation memory = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView imageView = VK_NULL_HANDLE;
			VkSampler imageSampler = VK_NULL_HANDLE;
			VkImageLayout layout;

			VkDeviceSize width, height;
			VkFormat format;

			MiniVkImage(MiniVkInstance& mvkLayer, MiniVkMemAlloc& vmAlloc, VkDeviceSize width, VkDeviceSize height, VkFormat format = VK_FORMAT_B8G8R8A8_SRGB, VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED)
			: mvkLayer(mvkLayer), vmAlloc(vmAlloc), width(width), height(height), format(format), layout(layout) {
				onDispose += std::callback<>(this, &MiniVkImage::Disposable);

				VkImageCreateInfo imgCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
				imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imgCreateInfo.extent.width = static_cast<uint32_t>(width);
				imgCreateInfo.extent.height = static_cast<uint32_t>(height);
				imgCreateInfo.extent.depth = 1;
				imgCreateInfo.mipLevels = 1;
				imgCreateInfo.arrayLayers = 1;
				imgCreateInfo.format = format;
				imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

				VmaAllocationCreateInfo allocCreateInfo = {};
				allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
				allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
				allocCreateInfo.priority = 1.0f;

				if (vmaCreateImage(vmAlloc.GetAllocator(), &imgCreateInfo, &allocCreateInfo, &image, &memory, nullptr) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Could not allocate GPU image data for MiniVkImage!");

				CreateSyncObjects();
				CreateTextureSampler();
				CreateImageView();
			}

			MiniVkImage operator=(const MiniVkImage& mvkImage) = delete;

			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			void TransitionLayoutCmd(VkQueue graphicsQueue, VkCommandPool commandPool, VkImageLayout newLayout) {
				VkCommandBuffer commandBuffer = BeginTransferCmd(graphicsQueue, commandPool);

				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = layout;
				barrier.newLayout = newLayout;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;

				VkPipelineStageFlags sourceStage;
				VkPipelineStageFlags destinationStage;
				if (layout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

					sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				} else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				} else {
					throw std::invalid_argument("unsupported layout transition!");
				}

				layout = newLayout;
				vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				EndTransferCmd(graphicsQueue, commandPool, commandBuffer);
			}

			void TransferFromBufferCmd(VkQueue graphicsQueue, VkCommandPool commandPool, MiniVkBuffer& srcBuffer) {
				VkCommandBuffer commandBuffer = BeginTransferCmd(graphicsQueue, commandPool);

				VkBufferImageCopy region{};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
				vkCmdCopyBufferToImage(commandBuffer, srcBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				EndTransferCmd(graphicsQueue, commandPool, commandBuffer);
			}

			void StageImageData(VkQueue graphicsQueue, VkCommandPool commandPool, void* data, VkDeviceSize dataSize) {
				MiniVkBuffer stagingBuffer = MiniVkBuffer(mvkLayer, vmAlloc, dataSize, MiniVkBufferType::VKVMA_BUFFER_TYPE_STAGING);
				memcpy(stagingBuffer.description.pMappedData, data, (size_t)dataSize);

				TransitionLayoutCmd(graphicsQueue, commandPool, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				TransferFromBufferCmd(graphicsQueue, commandPool, stagingBuffer);
				TransitionLayoutCmd(graphicsQueue, commandPool, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

				stagingBuffer.Dispose();
			}

			VkCommandBuffer BeginTransferCmd(VkQueue graphicsQueue, VkCommandPool commandPool) {
				VkCommandBufferAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandPool = commandPool;
				allocInfo.commandBufferCount = 1;

				VkCommandBuffer commandBuffer;
				vkAllocateCommandBuffers(mvkLayer.logicalDevice, &allocInfo, &commandBuffer);

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				vkBeginCommandBuffer(commandBuffer, &beginInfo);
				return commandBuffer;
			}

			void EndTransferCmd(VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
				vkEndCommandBuffer(commandBuffer);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(graphicsQueue);
				vkFreeCommandBuffers(mvkLayer.logicalDevice, commandPool, 1, &commandBuffer);
			}

			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		};
	}
#endif