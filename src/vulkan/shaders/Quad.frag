// Quad.frag: a texture copy, multiplied by the vertex colour (Direct3D 9's D3DTOP_MODULATE: white for a
// copy, the corner colours for the light). A texel with alpha 0 (a colour key) is not drawn: blending
// it would leave the target as it is, so discarding it gives the same pixels without a blend.
#version 450

layout(set = 0, binding = 0) uniform sampler2D Texture;

layout(location = 0) in vec2 TexCoord;
layout(location = 1) in vec4 Color;

layout(location = 0) out vec4 Result;

void main()
{
	vec4 Texel = texture(Texture, TexCoord) * Color;
	if (Texel.a == 0.0)
		discard;
	Result = Texel;
}
