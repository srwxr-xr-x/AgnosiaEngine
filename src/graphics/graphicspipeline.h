#pragma once
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "../types.h"
class Graphics {
public:
  static void destroyPipelines();
  static void createFramebuffers();
  static void destroyFramebuffers();
  static void createCommandPool();
  static void destroyCommandPool();
  static void createCommandBuffer();
  static void recordCommandBuffer(VkCommandBuffer cmndBuffer,
                                  uint32_t imageIndex);

  static void addGraphicsPipeline(Agnosia_T::Pipeline pipeline);
  static void addFullscreenPipeline(Agnosia_T::Pipeline pipeline);
  
  static float *getCamPos();
  static float *getLightPos();
  static float *getCenterPos();
  static float *getUpDir();
  static float &getDepthField();
  static float *getDistanceField();
  
};
