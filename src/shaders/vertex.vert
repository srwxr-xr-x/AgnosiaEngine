#version 450

layout(binding = 0) uniform UniformBufferObject {
    float time;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// inPosition and inColor are vertex attributes, per-vertex properties defined in the vertex buffer!
// Layout assigns indices to access these inputs, dvec3 takes 2 slots so we must index it at 2. https://www.khronos.org/opengl/wiki/Layout_Qualifier_(GLSL)

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
