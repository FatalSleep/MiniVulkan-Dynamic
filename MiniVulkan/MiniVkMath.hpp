#pragma once
#ifndef MINIVK_MINIVKMATH
#define MINIVK_MINIVKMATH
#include "./MiniVk.hpp"

namespace MINIVULKAN_NS {
	class MiniVkMath {
	public:
        static glm::mat4 Project2D(double width, double height, double znear, double zfar) {
            //return glm::transpose(glm::mat4(
            //    2.0/(width - 1), 0.0, 0.0, -1.0,
            //    0.0, 2.0/(height-1), 0.0, -1.0,
            //    0.0, 0.0, -2.0/(zfar-znear), -((zfar+znear)/(znear-zfar)),
            //    0.0, 0.0, 0.0, 1.0));
            // Defining the TOP and BOTTOM upside down will provide the proper translation transform for scaling
            // with Vulkan due to Vulkan's inverted Y-Axis without having to transpose the matrix.
            return glm::ortho(0.0, width, 0.0, height, znear, zfar);    
        }
	};

    struct MiniVkVertex {
        glm::vec2 texcoord;
        glm::vec2 position;
        glm::vec4 color;

        MiniVkVertex(glm::vec2 tex, glm::vec2 pos, glm::vec4 col) : texcoord(tex), position(pos), color(col) {}

        static VkVertexInputBindingDescription GetBindingDescription() {
            VkVertexInputBindingDescription bindingDescription(1);
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(MiniVkVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(MiniVkVertex, texcoord);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(MiniVkVertex, position);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(MiniVkVertex, color);
            return attributeDescriptions;
        }
    };
}
#endif