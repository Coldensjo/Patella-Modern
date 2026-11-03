// ColorKey.frag: surfaceDX9.cpp's ApplyColorKey on the GPU. The rectangle was copied to the texture
// (from its corner on); a pixel that is exactly the key (alpha included) becomes the replacement, the
// others stay as they are. No blending.
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Constants.glsl"

layout(set = 0, binding = 0) uniform sampler2D Texture;

layout(location = 0) out vec4 Result;

void main()
{
	vec4 Texel = texelFetch(Texture, ivec2(gl_FragCoord.xy) - PC.Offset, 0);
	uvec4 Value = uvec4(round(Texel * 255.0));
	Result = Value == PC.Key ? PC.Replacement : Texel;
}
