#pragma once

#include "vector.h"

NAMESPACE_BEGIN(nagi)

class Camera
{
public:
	Camera(vec3f pos, vec3f lookat, float fov);
	Camera(const Camera& other) { *this = other; }
	Camera& operator=(const Camera& other);

	void OffsetOrientation(float dx, float dy);
	void Strafe(float dx, float dy);
	void SetRadius(float dr);
	void SetFov(float df);
	
	vec3f position;
	vec3f forward;
	vec3f up;
	vec3f right;

	float fov;
	float focalDistance;
	float lensRadius;
	bool isMoving;

private:
	void UpdateCamera();

	vec3f worldUp = vec3f(0.0f, 1.0f, 0.0f);
	vec3f lookat;

	float pitch;
	float radius;
	float yaw;
};

NAMESPACE_END(nagi)