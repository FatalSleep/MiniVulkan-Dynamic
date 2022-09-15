#pragma once
#ifndef MINIVK_MINIVKDYNAMICRENDERER
#define MINIVK_MINIVKDYNAMICRENDERER
#include "./MiniVk.hpp"

namespace MINIVULKAN_NS {
	#pragma region DYNAMIC RENDERING FUNCTIONS

	VkResult vkCmdBeginRenderingEKHR(VkInstance instance, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
		auto func = (PFN_vkCmdBeginRenderingKHR)vkGetInstanceProcAddr(instance, "vkCmdBeginRenderingKHR");
		if (func == VK_NULL_HANDLE) throw std::runtime_error("MiniVulkan: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdBeginRenderingKHR");
		func(commandBuffer, pRenderingInfo);
		return VK_SUCCESS;
	}

	VkResult vkCmdEndRenderingEKHR(VkInstance instance, VkCommandBuffer commandBuffer) {
		auto func = (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(instance, "vkCmdEndRenderingKHR");
		if (func == VK_NULL_HANDLE) throw std::runtime_error("MiniVulkan: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdEndRenderingKHR");
		func(commandBuffer);
		return VK_SUCCESS;
	}

	#pragma endregion

	class MiniVkRenderImage : public MiniVkObject {
	private:
		MiniVkInstanceSupportDetails mvkLayer;

		#pragma region VKIMAGE CREATION

		uint32_t QueryMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
			VkPhysicalDeviceMemoryProperties memProperties;
			vkGetPhysicalDeviceMemoryProperties(mvkLayer.physicalDevice, &memProperties);

			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
				if (typeFilter & (1 << i)) return i;

			throw std::runtime_error("MiniVulkan: Failed to find suitable memory type for vertex buffer!");
		}

		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkImageTiling tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL) {
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = usage;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateImage(mvkLayer.logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
				throw std::runtime_error("MiniVulkan: Failed to create vkimage!");

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(mvkLayer.logicalDevice, image, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = QueryMemoryType(memRequirements.memoryTypeBits, properties);

			if (vkAllocateMemory(mvkLayer.logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
				throw std::runtime_error("MiniVulkan: Failed to allocate vkimage memory!");

			vkBindImageMemory(mvkLayer.logicalDevice, image, imageMemory, 0);
		}

		void CreateSyncObjects() {
			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			if (vkCreateSemaphore(mvkLayer.logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
				vkCreateSemaphore(mvkLayer.logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
				vkCreateFence(mvkLayer.logicalDevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to create synchronization objects for a frame!");
		}

		#pragma endregion
	public:
		VkImage image;
		VkImageView imageView;
		VkDeviceMemory memory;
		VkSemaphore imageAvailableSemaphore;
		VkSemaphore renderFinishedSemaphore;
		VkFence inFlightFence;

		void Disposable() {
			vkDestroySemaphore(mvkLayer.logicalDevice, imageAvailableSemaphore, nullptr);
			vkDestroySemaphore(mvkLayer.logicalDevice, renderFinishedSemaphore, nullptr);
			vkDestroyFence(mvkLayer.logicalDevice, inFlightFence, nullptr);

			vkDestroyImage(mvkLayer.logicalDevice, image, nullptr);
			vkDestroyImageView(mvkLayer.logicalDevice, imageView, nullptr);
			vkFreeMemory(mvkLayer.logicalDevice, memory, nullptr);
		}

		MiniVkRenderImage(MiniVkInstanceSupportDetails mvkLayer, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
			VkMemoryPropertyFlags properties, VkImageTiling tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL) : mvkLayer(mvkLayer) {
			onDispose += std::callback<>(this, &MiniVkRenderImage::Disposable);

			CreateImage(width, height, format, usage, properties, image, memory, tiling);
			MiniVkSurfaceSupportDetails imageDetails;
			VkImageViewCreateInfo imageViewCreateInfo{};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = image;
			imageViewCreateInfo.format = imageDetails.dataFormat;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.pNext = VK_NULL_HANDLE;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			vkCreateImageView(mvkLayer.logicalDevice, &imageViewCreateInfo, VK_NULL_HANDLE, &imageView);
			CreateSyncObjects();
		}
	};

	template<typename VertexStruct, typename UniformStruct>
	class MiniVkDynamicRenderer : public MiniVkObject {
	private:
		MiniVkInstanceSupportDetails mvkLayer;
	public:
		MiniVkSwapChain& swapChain;
		MiniVkDynamicPipeline<VertexStruct, UniformStruct>& graphicsPipeline;

		/// SWAPCHAIN SYNCHRONIZATION_OBJECTS ///
		std::vector<VkSemaphore> swapChain_imageAvailableSemaphores;
		std::vector<VkSemaphore> swapChain_renderFinishedSemaphores;
		std::vector<VkFence> swapChain_inFlightFences;

		/// CUSTOM RENDER TARGETS
		std::vector<MiniVkRenderImage> renderTargetImages;

		/// INVOKABLE RENDER EVENTS: (executed in MiniVkDynamicRenderer::RenderFrame()
		std::invokable<VkCommandBuffer> onRenderEvents;

		/// COMMAND POOL FOR RENDER COMMANDS
		MiniVkCommandPool& commandPool;

		void Disposable() {
			vkDeviceWaitIdle(mvkLayer.logicalDevice);

			for (size_t i = 0; i < swapChain_inFlightFences.size(); i++) {
				vkDestroySemaphore(mvkLayer.logicalDevice, swapChain_imageAvailableSemaphores[i], nullptr);
				vkDestroySemaphore(mvkLayer.logicalDevice, swapChain_renderFinishedSemaphores[i], nullptr);
				vkDestroyFence(mvkLayer.logicalDevice, swapChain_inFlightFences[i], nullptr);
			}
		}

		MiniVkDynamicRenderer(MiniVkInstanceSupportDetails mvkLayer, MiniVkCommandPool& commandPool, MiniVkSwapChain& swapChain, MiniVkDynamicPipeline<VertexStruct, UniformStruct>& graphicsPipeline) :
		mvkLayer(mvkLayer), commandPool(commandPool), swapChain(swapChain), graphicsPipeline(graphicsPipeline) {
			onDispose += std::callback<>(this, &MiniVkDynamicRenderer::Disposable);

			CreateSwapChainSyncObjects();
		}

		void CreateSwapChainSyncObjects() {
			// BufferingMode -> MAX_FRAMES_IN_FLIGHT
			swapChain_imageAvailableSemaphores.resize(static_cast<size_t>(swapChain.swapChainImages.size()));
			swapChain_renderFinishedSemaphores.resize(static_cast<size_t>(swapChain.swapChainImages.size()));
			swapChain_inFlightFences.resize(static_cast<size_t>(swapChain.swapChainImages.size()));

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			for (size_t i = 0; i < swapChain.swapChainImages.size(); i++) {
				if (vkCreateSemaphore(mvkLayer.logicalDevice, &semaphoreInfo, nullptr, &swapChain_imageAvailableSemaphores[i]) != VK_SUCCESS ||
					vkCreateSemaphore(mvkLayer.logicalDevice, &semaphoreInfo, nullptr, &swapChain_renderFinishedSemaphores[i]) != VK_SUCCESS ||
					vkCreateFence(mvkLayer.logicalDevice, &fenceInfo, nullptr, &swapChain_inFlightFences[i]) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to create synchronization objects for a frame!");
			}
		}

		/// <summary>Begins recording commands into a command buffers.</summary>
		void BeginRecordCommandBuffer(VkCommandBuffer commandBuffer, const VkClearValue clearColor, VkImageView& renderTarget, VkImage& renderImageTarget, VkExtent2D renderArea) {
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0; // Optional
			beginInfo.pInheritanceInfo = nullptr; // Optional

			if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
				throw std::runtime_error("MiniVulkan: Failed to record [begin] to command buffer!");

			const VkImageMemoryBarrier image_memory_barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.image = renderImageTarget,
				.subresourceRange = {
				  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				  .baseMipLevel = 0,
				  .levelCount = 1,
				  .baseArrayLayer = 0,
				  .layerCount = 1,
				}
			};

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &image_memory_barrier);

			VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
			colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
			colorAttachmentInfo.imageView = renderTarget;
			colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
			colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachmentInfo.clearValue = clearColor;

			VkRenderingInfoKHR dynamicRenderInfo{};
			dynamicRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;

			VkRect2D renderAreaKHR{};
			renderAreaKHR.extent = renderArea;
			renderAreaKHR.offset = { 0,0 };
			dynamicRenderInfo.renderArea = renderAreaKHR;
			dynamicRenderInfo.layerCount = 1;
			dynamicRenderInfo.colorAttachmentCount = 1;
			dynamicRenderInfo.pColorAttachments = &colorAttachmentInfo;

			vkCmdBeginRenderingEKHR(mvkLayer.instance, commandBuffer, &dynamicRenderInfo);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.graphicsPipeline);

			VkViewport dynamicViewportKHR{};
			dynamicViewportKHR.x = 0;
			dynamicViewportKHR.y = 0;
			dynamicViewportKHR.width = static_cast<float>(renderArea.width);
			dynamicViewportKHR.height = static_cast<float>(renderArea.height);
			dynamicViewportKHR.minDepth = 0.0f;
			dynamicViewportKHR.maxDepth = 1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &dynamicViewportKHR);
			vkCmdSetScissor(commandBuffer, 0, 1, &renderAreaKHR);
		}

		/// <summary>Ends recording to the command buffer.</summary>
		void EndRecordCommandBuffer(VkCommandBuffer commandBuffer, VkImage& renderImageTarget) {
			vkCmdEndRenderingEKHR(mvkLayer.instance, commandBuffer);

			const VkImageMemoryBarrier image_memory_barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.image = renderImageTarget,
				.subresourceRange = {
				  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				  .baseMipLevel = 0,
				  .levelCount = 1,
				  .baseArrayLayer = 0,
				  .layerCount = 1,
				}
			};

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &image_memory_barrier);

			if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
				throw std::runtime_error("MiniVulkan: Failed to record [end] to command buffer!");
		}

		/// <summary>Resets the command buffer for new input.</summary>
		void ResetCommandBuffer(VkCommandBuffer commandBuffer) { vkResetCommandBuffer(commandBuffer, 0); }

		void RenderFrame() {
			std::vector<VkCommandBuffer>& commandBuffers = commandPool.GetBuffers();
			vkWaitForFences(mvkLayer.logicalDevice, 1, &swapChain_inFlightFences[swapChain.currentFrame], VK_TRUE, UINT64_MAX);

			uint32_t imageIndex;
			VkResult result = vkAcquireNextImageKHR(mvkLayer.logicalDevice, swapChain.swapChain, UINT64_MAX, swapChain_imageAvailableSemaphores[swapChain.currentFrame], VK_NULL_HANDLE, &imageIndex);
			VkCommandBuffer cmdBuffer = commandBuffers[imageIndex];

			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				swapChain.SetFrameBufferResized(false);
				swapChain.ReCreateSwapChain();
				return;
			} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
				throw std::runtime_error("MiniVulkan: Failed to acquire swap chain image!");

			vkResetFences(mvkLayer.logicalDevice, 1, &swapChain_inFlightFences[swapChain.currentFrame]);
			vkResetCommandBuffer(cmdBuffer, 0); //VkCommandBufferResetFlagBits

			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			onRenderEvents.invoke(cmdBuffer);

			/*
			const VkClearValue clearColor = { {1.0f, 1.0f, 1.0f, 1.0f} };
			graphicsPipeline.BeginRecordCommandBuffer(commandBuffers[swapChain.currentFrame], clearColor, swapChain.swapChainImageViews[swapChain.currentFrame], swapChain.swapChainExtent);
			vkCmdDraw(commandBuffers[swapChain.currentFrame], 3, 1, 0, 0);
			graphicsPipeline.EndRecordCommandBuffer(commandBuffers[swapChain.currentFrame]);
			*/
			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkSemaphore waitSemaphores[] = { swapChain_imageAvailableSemaphores[swapChain.currentFrame] };
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmdBuffer;

			VkSemaphore signalSemaphores[] = { swapChain_renderFinishedSemaphores[swapChain.currentFrame] };
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;

			if (vkQueueSubmit(graphicsPipeline.graphicsQueue, 1, &submitInfo, swapChain_inFlightFences[swapChain.currentFrame]) != VK_SUCCESS)
				throw std::runtime_error("MiniVulkan: Failed to submit draw command buffer!");

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;
			
			VkSwapchainKHR swapChainList[] { swapChain.swapChain };
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChainList;
			presentInfo.pImageIndices = &imageIndex;

			result = vkQueuePresentKHR(graphicsPipeline.presentQueue, &presentInfo);

			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || swapChain.framebufferResized) {
				swapChain.SetFrameBufferResized(false);
				swapChain.ReCreateSwapChain();
				return;
			} else if (result != VK_SUCCESS)
				throw std::runtime_error("MiniVulkan: Failed to present swap chain image!");
			
			swapChain.currentFrame = (swapChain.currentFrame + 1) % swapChain.swapChainImages.size();
		}
	};
}
#endif