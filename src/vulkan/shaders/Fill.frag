// Fill.frag: a colour fill (Direct3D 9's ColorFill), no blending.
#version 450

layout(location = 0) in vec2 TexCoord;
layout(location = 1) in vec4 Color;

layout(location = 0) out vec4 Result;

void main()
{
	Result = Color;
}
