#version 450
#extension GL_ARB_separate_shader_objects : enable

// just to check if stops complaining. Not being used at the moment
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout (location = 1) out vec2 tex;

vec2[] texCoord = {
     vec2(0.0, 0.0),
     vec2(0.0, 1.0),
     vec2(1.0, 1.0),
     vec2(1.0, 0.0)
};

void main() 
{
     gl_Position = vec4(inPosition, 0.0, 1.0);
     tex = texCoord[gl_VertexIndex];
}