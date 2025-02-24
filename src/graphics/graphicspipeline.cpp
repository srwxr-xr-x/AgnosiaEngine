#include "../devicelibrary.h"
#include "buffers.h"
#include "graphicspipeline.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "render.h"
#include "texture.h"
#include <fstream>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "../agnosiaimgui.h"
#include <iostream>
#include <vulkan/vulkan_core.h>

float lightPos[4] = {5.0f, 5.0f, 5.0f, 0.44f};
float camPos[4] = {3.0f, 3.0f, 3.0f, 0.44f};
float centerPos[4] = {0.0f, 0.0f, 0.0f, 0.44f};
float upDir[4] = {0.0f, 0.0f, 1.0f, 0.44f};
float depthField = 45.0f;
float distanceField[2] = {0.1f, 100.0f};
float lineWidth = 1.0;

std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                             VK_DYNAMIC_STATE_SCISSOR};

VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;

static std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}
VkShaderModule createShaderModule(const std::vector<char> &code,
                                  VkDevice &device) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }
  return shaderModule;
}

void Graphics::destroyGraphicsPipeline() {
  vkDestroyPipeline(DeviceControl::getDevice(), graphicsPipeline, nullptr);
  vkDestroyPipelineLayout(DeviceControl::getDevice(), pipelineLayout, nullptr);
}

void Graphics::createGraphicsPipeline() {
  // Note to self, for some reason the working directory is not where a read
  // file is called from, but the project folder!
  auto vertShaderCode = readFile("src/shaders/vertex.spv");
  auto fragShaderCode = readFile("src/shaders/fragment.spv");
  VkShaderModule vertShaderModule =
      createShaderModule(vertShaderCode, DeviceControl::getDevice());
  VkShaderModule fragShaderModule =
      createShaderModule(fragShaderCode, DeviceControl::getDevice());
  // ------------------ STAGE 1 - INPUT ASSEMBLER ---------------- //
  // This can get a little complicated, normally, vertices are loaded in
  // sequential order, with an element buffer however, you can specify the
  // indices yourself! Using an element buffer means you can reuse vertices,
  // which can lead to optimizations. If you set PrimRestart to TRUE, you can
  // utilize the _STRIP modes with special indices
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // ------------------ STAGE 2 - VERTEX SHADER ------------------ //
  // this will be revisited, right now we are hardcoding shader data, so we tell
  // it to not load anything, but that will change.
  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  // ------------------- STAGE 5 - RASTERIZATION ----------------- //
  // Take Vertex shader vertices and fragmentize them for the frament shader.
  // The rasterizer also can perform depth testing, face culling, and scissor
  // testing. In addition, it can also be configured for wireframe rendering.
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  // Render regardless of the near and far planes, useful for shadow maps,
  // requires GPU feature *depthClamp*
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  // MODE_FILL, fill polygons, MODE_LINE, draw wireframe, MODE_POINT, draw
  // vertices. Anything other than fill requires GPU feature *fillModeNonSolid*
  if(Gui::getWireframe()) {
      rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
  } else {
      rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  }
  rasterizer.lineWidth = lineWidth;
  // How to cull the faces, right here we cull the back faces and tell the
  // rasterizer front facing vertices are ordered clockwise.
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  // Whether or not to add depth values. e.x. for shadow maps.
  rasterizer.depthBiasEnable = VK_FALSE;

  // ------------------ STAGE 6 - FRAGMENT SHADER ---------------- //
  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  // ------------------ STAGE 7 - COLOR BLENDING ----------------- //
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // ---------------------- STATE CONTROLS ----------------------  //
  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;
  // Again, this will be revisited, multisampling can be very useful for
  // anti-aliasing, since it is fast, but we won't implement it for now.
  // Requires GPU feature UNKNOWN eanbled.
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_TRUE;
  multisampling.rasterizationSamples = DeviceControl::getPerPixelSampleCount();
  // TODO: Document!
  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;
  // Most of the graphics pipeline is set in stone, some of the pipeline state
  // can be modified without recreating it at runtime though! There are TONS of
  // settings, this would be another TODO to see what else we can mess with
  // dynamically, but right now we just allow dynamic size of the viewport and
  // dynamic scissor states. Scissors are pretty straightforward, they are
  // basically pixel masks for the rasterizer. Scissors describe what regions
  // pixels will be stored, it doesnt cut them after being rendered, it stops
  // them from ever being rendered in that area in the first place.
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPushConstantRange pushConstant{
      .stageFlags = VK_SHADER_STAGE_ALL,
      .offset = 0,
      .size = sizeof(Agnosia_T::GPUPushConstants),
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &Buffers::getDescriptorSetLayout();
  pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
  pipelineLayoutInfo.pushConstantRangeCount = 1;

  if (vkCreatePipelineLayout(DeviceControl::getDevice(), &pipelineLayoutInfo,
                             nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &DeviceControl::getImageFormat(),
      .depthAttachmentFormat = Texture::findDepthFormat(),
  };

  // Here we combine all of the structures we created to make the final
  // pipeline!
  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &pipelineRenderingCreateInfo,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depthStencil,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = pipelineLayout,
      .renderPass = nullptr,
      .subpass = 0,
  };

  if (vkCreateGraphicsPipelines(DeviceControl::getDevice(), VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr,
                                &graphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
  vkDestroyShaderModule(DeviceControl::getDevice(), fragShaderModule, nullptr);
  vkDestroyShaderModule(DeviceControl::getDevice(), vertShaderModule, nullptr);
}

void Graphics::createCommandPool() {
  // Commands in Vulkan are not executed using function calls, you have to
  // record the ops you wish to perform to command buffers, pools manage the
  // memory used by the buffer!
  DeviceControl::QueueFamilyIndices queueFamilyIndices =
      DeviceControl::findQueueFamilies(DeviceControl::getPhysicalDevice());

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

  if (vkCreateCommandPool(DeviceControl::getDevice(), &poolInfo, nullptr,
                          &Buffers::getCommandPool()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool!");
  }
}
void Graphics::destroyCommandPool() {
  vkDestroyCommandPool(DeviceControl::getDevice(), Buffers::getCommandPool(),
                       nullptr);
}
void Graphics::createCommandBuffer() {
  Buffers::getCommandBuffers().resize(Buffers::getMaxFramesInFlight());

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = Buffers::getCommandPool();
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = (uint32_t)Buffers::getCommandBuffers().size();

  if (vkAllocateCommandBuffers(DeviceControl::getDevice(), &allocInfo,
                               Buffers::getCommandBuffers().data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate command buffers");
  }
}
void Graphics::recordCommandBuffer(VkCommandBuffer commandBuffer,
                                   uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }
  const VkImageMemoryBarrier2 imageMemoryBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = DeviceControl::getSwapChainImages()[imageIndex],
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };
  const VkDependencyInfo dependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &imageMemoryBarrier,
  };
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

  // ------------------- DYNAMIC RENDER INFO ---------------------- //

  const VkRenderingAttachmentInfo colorAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = Texture::getColorImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
      .resolveImageView = DeviceControl::getSwapChainImageViews()[imageIndex],
      .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {.color = {0.0f, 0.0f, 0.0f, 1.0f}},
  };
  const VkRenderingAttachmentInfo depthAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = Texture::getDepthImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .clearValue = {.depthStencil = {1.0f, 0}},
  };

  const VkRenderingInfo renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0},
                     .extent = DeviceControl::getSwapChainExtent()},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentInfo,
      .pDepthAttachment = &depthAttachmentInfo,
  };

  vkCmdBeginRendering(commandBuffer, &renderInfo);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    graphicsPipeline);
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)DeviceControl::getSwapChainExtent().width;
  viewport.height = (float)DeviceControl::getSwapChainExtent().height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = DeviceControl::getSwapChainExtent();
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  int texID = 0;
  for (Model *model : Model::getInstances()) {

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &Buffers::getDescriptorSet(),
                            0, nullptr);

    Agnosia_T::GPUPushConstants pushConsts;
    pushConsts.vertexBuffer = model->getBuffers().vertexBufferAddress;
    pushConsts.objPosition = model->getPos();
    pushConsts.lightPos = glm::vec3(lightPos[0], lightPos[1], lightPos[2]);
    pushConsts.camPos = glm::vec3(camPos[0], camPos[1], camPos[2]);
    pushConsts.textureID = texID;
    pushConsts.ambient = model->getMaterial().getAmbient();
    pushConsts.spec = model->getMaterial().getSpecular();
    pushConsts.shine = model->getMaterial().getShininess();

    pushConsts.model =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));

    pushConsts.view =
        glm::lookAt(glm::vec3(camPos[0], camPos[1], camPos[2]),
                    glm::vec3(centerPos[0], centerPos[1], centerPos[2]),
                    glm::vec3(upDir[0], upDir[1], upDir[2]));

    pushConsts.proj =
        glm::perspective(glm::radians(depthField),
                         DeviceControl::getSwapChainExtent().width /
                             (float)DeviceControl::getSwapChainExtent().height,
                         distanceField[0], distanceField[1]);
    // GLM was created for OpenGL, where the Y coordinate was inverted. This
    // simply flips the sign.
    pushConsts.proj[1][1] *= -1;

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(Agnosia_T::GPUPushConstants), &pushConsts);

    vkCmdBindIndexBuffer(commandBuffer, model->getBuffers().indexBuffer.buffer,
                         0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(model->getIndices()),
                     1, 0, 0, 0);
    texID++;
  }
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

  vkCmdEndRendering(commandBuffer);

  const VkImageMemoryBarrier2 prePresentImageBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
      .dstAccessMask = 0,
      .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = DeviceControl::getSwapChainImages()[imageIndex],
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };
  const VkDependencyInfo depInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &prePresentImageBarrier,
  };

  vkCmdPipelineBarrier2(Buffers::getCommandBuffers()[Render::getCurrentFrame()],
                        &depInfo);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

float *Graphics::getCamPos() { return camPos; }
float *Graphics::getLightPos() { return lightPos; }
float *Graphics::getCenterPos() { return centerPos; }
float *Graphics::getUpDir() { return upDir; }
float &Graphics::getDepthField() { return depthField; }
float *Graphics::getDistanceField() { return distanceField; }
float &Graphics::getLineWidth() { return lineWidth; }
