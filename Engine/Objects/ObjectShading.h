#pragma once

#include <cstdint>

// How the fragment shader treats an object.
enum class ObjectShading : uint32_t
{
	LIT = 0,	// sun, sky, lamps and shadows
	UNLIT = 1,	// its own colour, e.g. a glowing sign
	SKY = 2,	// the sky gradient and sun disc, for a dome around the camera
	// Lit, but only where its texture is: the texture's alpha cuts it out (dithered, so a part-covered
	// texel shows as a speckle), e.g. paint on a wall.
	CUTOUT = 3
};
