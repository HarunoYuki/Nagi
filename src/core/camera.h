#pragma once

#include "vector.h"

NAMESPACE_BEGIN(nagi)

class Camera
{
public:
	Camera(vec3f pos, vec3f lookat, float fov);
	
	vec3f position;
	vec3f direction;
	vec3f up;
	vec3f right;

	float fov;
	float focalDistance;
	float lensRadius;
	bool isMoving;
};

NAMESPACE_END(nagi)