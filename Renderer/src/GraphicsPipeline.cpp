#include "RendererInternal.h"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
    vk::VertexInputBindingDescription GetVertexBindingDescription()
    {
        return
        {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };
    }

    std::array<vk::VertexInputAttributeDescription, 2> GetVertexAttributeDescriptions()
    {
        return
        {
            vk::VertexInputAttributeDescription
            {
                .location = 0,
                .binding = 0,
                .format = vk::Format::eR32G32Sfloat,
                .offset = offsetof(Vertex, position)
            },
            vk::VertexInputAttributeDescription
            {
                .location = 1,
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(Vertex, color)
            }
        };
    }

    std::filesystem::path GetExecutableDirectory()
    {
        std::vector<wchar_t> pathBuffer(32768);
        const DWORD pathLength = GetModuleFileNameW(
            nullptr,
            pathBuffer.data(),
            static_cast<DWORD>(pathBuffer.size()));
        if (pathLength == 0 || pathLength >= pathBuffer.size())
        {
            throw std::runtime_error("Failed to determine the executable directory.");
        }

        return std::filesystem::path(std::wstring(pathBuffer.data(), pathLength)).parent_path();
    }

    std::vector<uint32_t> ReadSpirvFile(const std::filesystem::path& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open shader file: " + filename.string());
        }

        const std::streamoff fileSize = file.tellg();
        if (fileSize <= 0 || fileSize % sizeof(uint32_t) != 0)
        {
            throw std::runtime_error("Shader file has an invalid SPIR-V size: " + filename.string());
        }

        std::vector<uint32_t> code(static_cast<size_t>(fileSize) / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));
        if (!file)
        {
            throw std::runtime_error("Failed to read shader file: " + filename.string());
        }

        return code;
    }
}

void Renderer::Impl::CreateGraphicsPipeline()
{
    const auto shaderDirectory = GetExecutableDirectory() / L"shaders";
    const auto vertexShaderCode = ReadSpirvFile(shaderDirectory / L"vert.spv");
    const auto fragmentShaderCode = ReadSpirvFile(shaderDirectory / L"frag.spv");

    const vk::ShaderModuleCreateInfo vertexShaderCreateInfo
    {
        .codeSize = vertexShaderCode.size() * sizeof(uint32_t),
        .pCode = vertexShaderCode.data()
    };
    const vk::ShaderModuleCreateInfo fragmentShaderCreateInfo
    {
        .codeSize = fragmentShaderCode.size() * sizeof(uint32_t),
        .pCode = fragmentShaderCode.data()
    };

    const vk::raii::ShaderModule vertexShader(device, vertexShaderCreateInfo);
    const vk::raii::ShaderModule fragmentShader(device, fragmentShaderCreateInfo);

    const std::array shaderStages
    {
        vk::PipelineShaderStageCreateInfo
        {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = *vertexShader,
            .pName = "main"
        },
        vk::PipelineShaderStageCreateInfo
        {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = *fragmentShader,
            .pName = "main"
        }
    };

    const vk::VertexInputBindingDescription bindingDescription = GetVertexBindingDescription();
    const auto attributeDescriptions = GetVertexAttributeDescriptions();
    const vk::PipelineVertexInputStateCreateInfo vertexInputInfo
    {
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()
    };
    const vk::PipelineInputAssemblyStateCreateInfo inputAssembly
    {
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = false
    };
    const vk::PipelineViewportStateCreateInfo viewportState
    {
        .viewportCount = 1,
        .scissorCount = 1
    };
    const vk::PipelineRasterizationStateCreateInfo rasterizer
    {
        .depthClampEnable = false,
        .rasterizerDiscardEnable = false,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = false,
        .lineWidth = 1.0f
    };
    const vk::PipelineMultisampleStateCreateInfo multisampling
    {
        .rasterizationSamples = vk::SampleCountFlagBits::e1
    };
    const vk::PipelineColorBlendAttachmentState colorBlendAttachment
    {
        .blendEnable = false,
        .colorWriteMask = vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
    };
    const vk::PipelineColorBlendStateCreateInfo colorBlending
    {
        .logicOpEnable = false,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };
    constexpr std::array dynamicStates
    {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    const vk::PipelineDynamicStateCreateInfo dynamicState
    {
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    const vk::PipelineLayoutCreateInfo layoutCreateInfo{};
    pipelineLayout = vk::raii::PipelineLayout(device, layoutCreateInfo);

    const vk::PipelineRenderingCreateInfo renderingInfo
    {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapChainImageFormat
    };
    const vk::GraphicsPipelineCreateInfo pipelineCreateInfo
    {
        .pNext = &renderingInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = *pipelineLayout,
        .renderPass = nullptr,
        .subpass = 0
    };

    graphicsPipeline = device.createGraphicsPipeline(nullptr, pipelineCreateInfo);
}
