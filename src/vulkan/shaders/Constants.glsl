// The push constants of every pipeline (Renderer.cpp TPushConstants).
layout(push_constant) uniform Constants {
	vec2 Scale;       // 2 / the target's size
	ivec2 Offset;     // the colour key pass: the rectangle's corner in the target
	uvec4 Key;        // the colour key pass: the key (R, G, B, A, 0-255)
	vec4 Replacement; // the colour key pass: what replaces it
} PC;
