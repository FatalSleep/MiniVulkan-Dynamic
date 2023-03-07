#pragma once
#ifndef MINIVULKAN_MINIVKDYNAMICRENDERER
#define MINIVULKAN_MINIVKDYNAMICRENDERER
	#include "./MiniVK.hpp"

	namespace MINIVULKAN_NAMESPACE {
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

		VkResult vkCmdSetColorBlendEnableEKHR(VkInstance instance, VkCommandBuffer commandBuffer, uint32_t first, uint32_t attachmentCount, const std::vector<VkBool32> blendTesting) {
			auto func = (PFN_vkCmdSetColorBlendEnableEXT) vkGetInstanceProcAddr(instance, "vkCmdSetColorBlendEnableEXT");
			if (func == VK_NULL_HANDLE) throw std::runtime_error("MiniVulkan: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdSetColorBlendEnableEXT");
			func(commandBuffer, first, attachmentCount, blendTesting.data());
			return VK_SUCCESS;
		}

		#pragma endregion

		class MiniVkDynamicRenderer : public std::disposable {
		private:
			MiniVkRenderDevice& renderDevice;
			MiniVkVMAllocator& memAlloc;

		public:
			MiniVkSwapChain& swapChain;
			MiniVkDynamicPipeline& graphicsPipeline;

			/// SWAPCHAIN SYNCHRONIZATION_OBJECTS ///
			std::vector<VkSemaphore> imageAvailableSemaphores;
			std::vector<VkSemaphore> renderFinishedSemaphores;
			std::vector<VkFence> inFlightFences;

			/// INVOKABLE RENDER EVENTS: (executed in MiniVkDynamicRenderer::RenderFrame() ///
			std::invokable<uint32_t,uint32_t> onRenderEvents;

			/// COMMAND POOL FOR RENDER COMMANDS ///
			/// RENDERING DEPTH IMAGE ///
			MiniVkCommandPool& commandPool;
			std::vector<MiniVkImage*> depthImages;

			/// SWAPCHAIN FRAME MANAGEMENT ///
			size_t currentSyncFrame = 0;

			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(renderDevice.logicalDevice);

				for (MiniVkImage* depthImage : depthImages) {
					depthImage->Dispose();
					delete depthImage;
				}

				for (size_t i = 0; i < inFlightFences.size(); i++) {
					vkDestroySemaphore(renderDevice.logicalDevice, imageAvailableSemaphores [i], nullptr);
					vkDestroySemaphore(renderDevice.logicalDevice, renderFinishedSemaphores[i], nullptr);
					vkDestroyFence(renderDevice.logicalDevice, inFlightFences[i], nullptr);
				}
			}

			MiniVkDynamicRenderer(MiniVkRenderDevice& renderDevice, MiniVkVMAllocator& memAlloc, MiniVkCommandPool& commandPool, MiniVkSwapChain& swapChain, MiniVkDynamicPipeline& graphicsPipeline)
			: renderDevice(renderDevice), memAlloc(memAlloc), commandPool(commandPool), swapChain(swapChain), graphicsPipeline(graphicsPipeline) {
				onDispose += std::callback<bool>(this, &MiniVkDynamicRenderer::Disposable);

				for (int32_t i = 0; i < swapChain.images.size(); i++)
					depthImages.push_back(new MiniVkImage(renderDevice, memAlloc, swapChain.imageExtent.width*4, swapChain.imageExtent.height*4, true, QueryDepthFormat(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_ASPECT_DEPTH_BIT));

				CreateFrameSyncObjects();
			}

			void CreateFrameSyncObjects() {
				size_t count = static_cast<size_t>(swapChain.bufferingMode);
				imageAvailableSemaphores .resize(count);
				renderFinishedSemaphores.resize(count);
				inFlightFences.resize(count);

				VkSemaphoreCreateInfo semaphoreInfo{};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

				VkFenceCreateInfo fenceInfo{};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

				for (size_t i = 0; i < swapChain.images.size(); i++) {
					if (vkCreateSemaphore(renderDevice.logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores [i]) != VK_SUCCESS ||
						vkCreateSemaphore(renderDevice.logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
						vkCreateFence(renderDevice.logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
						throw std::runtime_error("MiniVulkan: Failed to create synchronization objects for a frame!");
				}
			}

			VkFormat QueryDepthFormat(VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL, VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				const std::vector<VkFormat>& candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
				for (VkFormat format : candidates) {
					VkFormatProperties props;
					vkGetPhysicalDeviceFormatProperties(renderDevice.physicalDevice, format, &props);

					if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
						return format;
					} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
						return format;
					}
				}

				throw std::runtime_error("MiniVulkan: Failed to find supported format!");
			}

			void BeginRecordCmdBuffer(uint32_t swapFrame, uint32_t syncFrame, VkExtent2D renderArea, const VkClearValue clearColor, const VkClearValue depthStencil) {
				VkCommandBuffer commandBuffer = commandPool.GetBuffers()[syncFrame];

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = nullptr; // Optional

				if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to record [begin] to command buffer!");

				const VkImageMemoryBarrier swapchain_memory_barrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.image = swapChain.images[swapFrame],
					.subresourceRange = {
					  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					  .baseMipLevel = 0,
					  .levelCount = 1,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
					},
				};
				const VkImageMemoryBarrier depth_memory_barrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					.image = depthImages[syncFrame]->image,
					.subresourceRange = {
					  .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					  .baseMipLevel = 0,
					  .levelCount = 1,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
					},
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &swapchain_memory_barrier);
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &depth_memory_barrier);

				VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
				colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
				colorAttachmentInfo.imageView = swapChain.imageViews[swapFrame];
				colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
				colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachmentInfo.clearValue = clearColor;

				VkRenderingAttachmentInfoKHR depthStencilAttachmentInfo{};
				depthStencilAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
				depthStencilAttachmentInfo.imageView = depthImages[syncFrame]->imageView;
				depthStencilAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
				depthStencilAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				depthStencilAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				depthStencilAttachmentInfo.clearValue = depthStencil;
				depthStencilAttachmentInfo.imageLayout = depthImages[syncFrame]->layout;

				VkRenderingInfoKHR dynamicRenderInfo{};
				dynamicRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;

				VkRect2D renderAreaKHR{};
				renderAreaKHR.extent = renderArea;
				renderAreaKHR.offset = { 0,0 };
				dynamicRenderInfo.renderArea = renderAreaKHR;
				dynamicRenderInfo.layerCount = 1;
				dynamicRenderInfo.colorAttachmentCount = 1;
				dynamicRenderInfo.pColorAttachments = &colorAttachmentInfo;
				dynamicRenderInfo.pDepthAttachment = &depthStencilAttachmentInfo;

				VkViewport dynamicViewportKHR{};
				dynamicViewportKHR.x = 0;
				dynamicViewportKHR.y = 0;
				dynamicViewportKHR.width = static_cast<float>(renderArea.width);
				dynamicViewportKHR.height = static_cast<float>(renderArea.height);
				dynamicViewportKHR.minDepth = 0.0f;
				dynamicViewportKHR.maxDepth = 1.0f;

				vkCmdSetViewport(commandBuffer, 0, 1, &dynamicViewportKHR);
				vkCmdSetScissor(commandBuffer, 0, 1, &renderAreaKHR);

				if (vkCmdBeginRenderingEKHR(renderDevice.instance.instance, commandBuffer, &dynamicRenderInfo) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to record [begin] to rendering!");
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.graphicsPipeline);
			}

			void EndRecordCmdBuffer(uint32_t swapFrame, uint32_t syncFrame, VkExtent2D renderArea, const VkClearValue clearColor, const VkClearValue depthStencil) {
				VkCommandBuffer commandBuffer = commandPool.GetBuffers()[syncFrame];

				if (vkCmdEndRenderingEKHR(renderDevice.instance.instance, commandBuffer) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to record [end] to rendering!");

				const VkImageMemoryBarrier image_memory_barrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					.image = swapChain.images[swapFrame],
					.subresourceRange = {
					  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					  .baseMipLevel = 0,
					  .levelCount = 1,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
					}
				};
				const VkImageMemoryBarrier depth_memory_barrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					.image = depthImages[syncFrame]->image,
					.subresourceRange = {
					  .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					  .baseMipLevel = 0,
					  .levelCount = 1,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
					},
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &image_memory_barrier);
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &depth_memory_barrier);

				VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
				colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
				colorAttachmentInfo.imageView = swapChain.imageViews[swapFrame];
				colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
				colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachmentInfo.clearValue = clearColor;

				VkRenderingAttachmentInfoKHR depthStencilAttachmentInfo{};
				depthStencilAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
				depthStencilAttachmentInfo.imageView = depthImages[syncFrame]->imageView;
				depthStencilAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
				depthStencilAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				depthStencilAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				depthStencilAttachmentInfo.clearValue = depthStencil;
				depthStencilAttachmentInfo.imageLayout = depthImages[syncFrame]->layout;

				VkRenderingInfoKHR dynamicRenderInfo{};
				dynamicRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;

				VkRect2D renderAreaKHR{};
				renderAreaKHR.extent = renderArea;
				renderAreaKHR.offset = { 0,0 };
				dynamicRenderInfo.renderArea = renderAreaKHR;
				dynamicRenderInfo.layerCount = 1;
				dynamicRenderInfo.colorAttachmentCount = 1;
				dynamicRenderInfo.pColorAttachments = &colorAttachmentInfo;
				dynamicRenderInfo.pDepthAttachment = &depthStencilAttachmentInfo;

				VkViewport dynamicViewportKHR{};
				dynamicViewportKHR.x = 0;
				dynamicViewportKHR.y = 0;
				dynamicViewportKHR.width = static_cast<float>(renderArea.width);
				dynamicViewportKHR.height = static_cast<float>(renderArea.height);
				dynamicViewportKHR.minDepth = 0.0f;
				dynamicViewportKHR.maxDepth = 1.0f;
				vkCmdSetViewport(commandBuffer, 0, 1, &dynamicViewportKHR);
				vkCmdSetScissor(commandBuffer, 0, 1, &renderAreaKHR);

				if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to record [end] to command buffer!");
			}

			void ResetCommandBuffer(uint32_t syncFrame) {
				vkResetCommandBuffer(commandPool.GetBuffers()[syncFrame], 0);
			}

			void RenderFrame() {
				std::lock_guard g(std::disposable::global_lock);

				uint32_t imageIndex;
				VkResult result;

				if (!swapChain.presentable) return;

				vkWaitForFences(renderDevice.logicalDevice, 1, &inFlightFences[currentSyncFrame], VK_TRUE, UINT64_MAX);
				result = vkAcquireNextImageKHR(renderDevice.logicalDevice, swapChain.swapChain, UINT64_MAX, imageAvailableSemaphores[currentSyncFrame], VK_NULL_HANDLE, &imageIndex);
				vkResetFences(renderDevice.logicalDevice, 1, &inFlightFences[currentSyncFrame]);

				if (result == VK_ERROR_OUT_OF_DATE_KHR) {
					swapChain.presentable = false;
					currentSyncFrame = 0;
				} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
					throw std::runtime_error("MiniVulkan: Failed to acquire swap chain image!");

				if (!swapChain.presentable) return;

				ResetCommandBuffer(currentSyncFrame);
				VkCommandBuffer cmdBuffer = commandPool.GetBuffers()[currentSyncFrame];

				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				MiniVkImage* depthImage = depthImages[currentSyncFrame];
				if (depthImage->width < swapChain.imageExtent.width || depthImage->height < swapChain.imageExtent.height) {
					depthImage->Disposable(false);
					depthImage->ReCreateImage(swapChain.imageExtent.width, swapChain.imageExtent.height, depthImage->isDepthImage, QueryDepthFormat(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_ASPECT_DEPTH_BIT);
				}
				
				onRenderEvents.invoke(imageIndex, currentSyncFrame);
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

				VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentSyncFrame] };
				VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
				submitInfo.waitSemaphoreCount = 1;
				submitInfo.pWaitSemaphores = waitSemaphores;
				submitInfo.pWaitDstStageMask = waitStages;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &cmdBuffer;

				VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentSyncFrame] };
				submitInfo.signalSemaphoreCount = 1;
				submitInfo.pSignalSemaphores = signalSemaphores;

				if (vkQueueSubmit(graphicsPipeline.graphicsQueue, 1, &submitInfo, inFlightFences[currentSyncFrame]) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to submit draw command buffer!");

				VkPresentInfoKHR presentInfo{};
				presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				presentInfo.waitSemaphoreCount = 1;
				presentInfo.pWaitSemaphores = signalSemaphores;

				VkSwapchainKHR swapChainList[]{ swapChain.swapChain };
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = swapChainList;
				presentInfo.pImageIndices = &imageIndex;

				result = vkQueuePresentKHR(graphicsPipeline.presentQueue, &presentInfo);
				currentSyncFrame = (currentSyncFrame + 1) % static_cast<size_t>(swapChain.images.size());

				if (result == VK_ERROR_OUT_OF_DATE_KHR) {
					swapChain.presentable = false;
					currentSyncFrame = 0;
				} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
					throw std::runtime_error("MiniVulkan: Failed to acquire swap chain image!");
			}
		};
	}
#endif