// Quad.vert: the vertex shader of every pipeline (notes/vulkan.md). Positions are pixels of the
// target (Quads.h); Vulkan's y axis points down, as the client's.
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Constants.glsl"

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec4 Color;

layout(location = 0) out vec2 OutTexCoord;
layout(location = 1) out vec4 OutColor;

void main()
{
	gl_Position = vec4(Position * PC.Scale - 1.0, 0.0, 1.0);
	OutTexCoord = TexCoord;
	OutColor = Color;
}
