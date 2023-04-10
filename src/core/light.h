#pragma once
#include "vector.h"

NAMESPACE_BEGIN(nagi)

enum LightType
{
	RectLight, SphereLight, DistantLight
};

class Light
{
public:
	// default light is DistantLight.
	Light() : position(0,0,0), emission(17, 12, 4), 
		radius(0.0f), area(0.0f), type(LightType::DistantLight) {}

	vec3f position;
	vec3f emission;
	vec3f u, v;
	float radius;
	float area;
	float type;
};

NAMESPACE_END(nagi)