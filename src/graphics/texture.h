#pragma once

#include <string>
#include <vector>
#define VK_NO_PROTOTYPES
#include "volk.h"
#include <cstdint>

class Texture {
protected:
  uint32_t mipLevels;
  VkImage image;
  VkImageView imageView;
  VkSampler sampler;

  static std::vector<Texture*> instances;
public:
  Texture(const std::string& texturePath);

  static const std::vector<Texture *> &getInstances();

  VkImage& getImage();
  VkImageView& getImageView();
  VkSampler& getSampler();
  uint32_t getMipLevels();

  static void destroyTexture(Texture* texture);
  static void destroyTextures();
  
  static void createDepthImage();
  static void createColorImage();
  
  // ------------ Getters & Setters ------------ //
  struct Image {
    VkImage image;
    VkImageView imageView;
    VkDeviceMemory memory;
  };
  static Image &getColorImage();
  static Image &getDepthImage();
};
