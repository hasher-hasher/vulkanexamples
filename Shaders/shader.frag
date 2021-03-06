#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (binding = 0) uniform sampler2D samplerColor;

layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
  outFragColor = texture(samplerColor, inUV);
}