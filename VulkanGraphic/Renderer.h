#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "vulkan/vulkan_raii.hpp"

class Renderer
{
public:
    void CreateInstance();
    
private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
};
