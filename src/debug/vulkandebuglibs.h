#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Debug {
  class vulkandebuglibs {
    
    public:
      void vulkanDebugSetup(VkInstanceCreateInfo& createInfo, VkInstance& instance);
      bool checkValidationLayerSupport();
      void checkUnavailableValidationLayers();
      void setupDebugMessenger(VkInstance& vulkanInstance);
      void DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator);
  };
}