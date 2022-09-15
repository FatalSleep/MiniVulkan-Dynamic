#pragma once
#ifndef MINIVK_MINIVKDYNAMICPIPELINE
#define MINIVK_MINIVKDYNAMICPIPELINE
	#include "./MiniVk.hpp"

	namespace MINIVULKAN_NS {
		template<typename VertexStruct, typename UniformStruct>
		class MiniVkDynamicPipeline : public MiniVkObject {
		private:
			MiniVkInstanceSupportDetails mvkLayer;
		public:
			/// GRAPHICS_PIPELINE ///
			uint32_t pushConstantRangeSize;
			MiniVkShaderStages& shaderStages;
			VkDescriptorSetLayout descriptorSetLayout;
			VkPipelineDynamicStateCreateInfo dynamicState;
			VkPipelineLayout pipelineLayout;
			VkPipeline graphicsPipeline;
			VkFormat imageFormat;
			VkColorComponentFlags colorComponentFlags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			VkPrimitiveTopology vertexTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			/// QUEUE_FAMILIES ///
			VkQueue graphicsQueue;
			VkQueue presentQueue;

			void Disposable() {
				vkDeviceWaitIdle(mvkLayer.logicalDevice);
				vkDestroyPipeline(mvkLayer.logicalDevice, graphicsPipeline, nullptr);
				vkDestroyPipelineLayout(mvkLayer.logicalDevice, pipelineLayout, nullptr);
				vkDestroyDescriptorSetLayout(mvkLayer.logicalDevice, descriptorSetLayout, nullptr);
			}

			MiniVkDynamicPipeline(MiniVkInstanceSupportDetails mvkLayer, MiniVkShaderStages& shaderStages, VkFormat imageFormat, uint32_t pushConstantRangeSize = 0,
			VkColorComponentFlags colorComponentFlags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			VkPrimitiveTopology vertexTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) : mvkLayer(mvkLayer), imageFormat(imageFormat),
			colorComponentFlags(colorComponentFlags), vertexTopology(vertexTopology), shaderStages(shaderStages), pushConstantRangeSize(pushConstantRangeSize) {
				onDispose += std::callback<>(this, &MiniVkDynamicPipeline::Disposable);

				MiniVkQueueFamily indices = MiniVkQueueFamily::FindQueueFamilies(mvkLayer.physicalDevice, mvkLayer.window->GetWindowSurface());
				vkGetDeviceQueue(mvkLayer.logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
				vkGetDeviceQueue(mvkLayer.logicalDevice, indices.presentFamily.value(), 0, &presentQueue);

				static_assert(std::is_base_of<MiniVkVertex, VertexStruct>::value, "VertexStruct (T) of template must derive MiniVkVertex.");
				static_assert(std::is_base_of<MiniVkUniform, UniformStruct>::value, "UniformStruct (T) of template must derivce MiniVkUniform.");

				CreateGraphicsPipeline();
			}

			MiniVkDynamicPipeline operator=(const MiniVkDynamicPipeline& pipeline) = delete;

			/// <summary>Creates the graphics rendering pipeline: Automatically called on constructor call.</summary>
			void CreateGraphicsPipeline() {
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				/////////// This section specifies that MiniVkVertex provides the vertex layout description ///////////
				auto bindingDescription = VertexStruct::GetBindingDescription();
				auto attributeDescriptions = VertexStruct::GetAttributeDescriptions();

				VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
				vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
				vertexInputInfo.vertexBindingDescriptionCount = 1;
				vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
				vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
				vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				VkDescriptorSetLayoutBinding uboLayoutBinding{};
				uboLayoutBinding.binding = 0;
				uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				uboLayoutBinding.descriptorCount = 1;
				uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
				uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

				VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
				descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				descriptorLayoutInfo.bindingCount = 1;
				descriptorLayoutInfo.pBindings = &uboLayoutBinding;

				if (vkCreateDescriptorSetLayout(mvkLayer.logicalDevice, &descriptorLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to create Uniform Buffer descriptor set layout!");
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
				pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				pipelineLayoutInfo.setLayoutCount = 1;
				pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
				pipelineLayoutInfo.pushConstantRangeCount = 0;

				if (pushConstantRangeSize > 0) {
					VkPushConstantRange pushConstantRange{};
					pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
					pushConstantRange.offset = 0;
					pushConstantRange.size = pushConstantRangeSize;
					
					pipelineLayoutInfo.pushConstantRangeCount = 1;
					pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
				}

				if (vkCreatePipelineLayout(mvkLayer.logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to create graphics pipeline layout!");
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
				inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
				inputAssembly.topology = vertexTopology;
				inputAssembly.primitiveRestartEnable = VK_FALSE;

				VkPipelineViewportStateCreateInfo viewportState{};
				viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
				viewportState.viewportCount = 1;
				viewportState.scissorCount = 1;
				viewportState.flags = 0;

				VkPipelineRasterizationStateCreateInfo rasterizer{};
				rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
				rasterizer.depthClampEnable = VK_FALSE;
				rasterizer.rasterizerDiscardEnable = VK_FALSE;
				rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
				rasterizer.lineWidth = 1.0f;
				rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
				rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
				rasterizer.depthBiasEnable = VK_FALSE;

				VkPipelineMultisampleStateCreateInfo multisampling{};
				multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
				multisampling.sampleShadingEnable = VK_FALSE;
				multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

				VkPipelineColorBlendAttachmentState colorBlendAttachment{};
				colorBlendAttachment.colorWriteMask = colorComponentFlags;
				colorBlendAttachment.blendEnable = VK_FALSE;

				VkPipelineColorBlendStateCreateInfo colorBlending{};
				colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
				colorBlending.logicOpEnable = VK_FALSE;
				colorBlending.logicOp = VK_LOGIC_OP_COPY;
				colorBlending.attachmentCount = 1;
				colorBlending.pAttachments = &colorBlendAttachment;
				colorBlending.blendConstants[0] = 0.0f;
				colorBlending.blendConstants[1] = 0.0f;
				colorBlending.blendConstants[2] = 0.0f;
				colorBlending.blendConstants[3] = 0.0f;

				std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
				dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
				dynamicState.flags = 0;
				dynamicState.pNext = VK_NULL_HANDLE;
				dynamicState.pDynamicStates = dynamicStateEnables.data();
				dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

				VkPipelineRenderingCreateInfoKHR renderingCreateInfo{};
				renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
				renderingCreateInfo.colorAttachmentCount = 1;
				renderingCreateInfo.pColorAttachmentFormats = &imageFormat;
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				VkGraphicsPipelineCreateInfo pipelineInfo{};
				pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
				pipelineInfo.stageCount = 2;
				pipelineInfo.pStages = shaderStages.shaderStages.data();
				pipelineInfo.pVertexInputState = &vertexInputInfo;
				pipelineInfo.pInputAssemblyState = &inputAssembly;
				pipelineInfo.pViewportState = &viewportState;
				pipelineInfo.pRasterizationState = &rasterizer;
				pipelineInfo.pMultisampleState = &multisampling;
				pipelineInfo.pColorBlendState = &colorBlending;

				pipelineInfo.pDynamicState = &dynamicState;
				pipelineInfo.pNext = &renderingCreateInfo;

				pipelineInfo.layout = pipelineLayout;
				pipelineInfo.renderPass = nullptr;
				pipelineInfo.subpass = 0;
				pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
				pipelineInfo.basePipelineIndex = -1; // Optional

				if (vkCreateGraphicsPipelines(mvkLayer.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
					throw std::runtime_error("MiniVulkan: Failed to create graphics pipeline!");
				///////////////////////////////////////////////////////////////////////////////////////////////////////
			}
		};
	}
#endif