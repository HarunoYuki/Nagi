#include <cmath>
#include <cstring>
#include "camera.h"

NAMESPACE_BEGIN(nagi)

Camera::Camera(vec3f pos, vec3f lookat, float fov) :
	position(pos), lookat(lookat), fov(Radians(fov)), lensRadius(0.0f), focalDistance(1.0f)
{
	radius = (lookat - pos).Length();
	forward = Normalize(lookat - pos);

	pitch = Degrees(asin(forward.y));
	yaw = Degrees(atan2(forward.z, forward.x));
	UpdateCamera();
}

Camera& Camera::operator=(const Camera& other)
{
	// size不包括isMoving自身
	ptrdiff_t size = (unsigned char*)&isMoving - (unsigned char*)&position.x;
	isMoving = memcmp(&position.x, &other.position.x, size) != 0;
	memcmp(&position.x, &other.position.x, size);
	return *this;
}

void Camera::OffsetOrientation(float dx, float dy)
{
	pitch -= dy; yaw += dx;
	UpdateCamera();
}

void Camera::Strafe(float dx, float dy)
{
	// 此处平移，移动不是相机pos，而是目标点lookat的位置。
	lookat += right * -dx + up * dy;
	UpdateCamera();
}

void Camera::SetRadius(float dr)
{
	radius += dr;
	UpdateCamera();
}

void Camera::SetFov(float df)
{
	fov = Radians(df);
}

void Camera::UpdateCamera()
{
	vec3f temp;
	temp.x = cos(Radians(pitch)) * cos(Radians(yaw));
	temp.y = sin(Radians(pitch));
	temp.z = cos(Radians(pitch)) * sin(Radians(yaw));

	forward = Normalize(temp);
	position = lookat - forward * radius;

	right = Normalize(Cross(forward, worldUp));
	up = Cross(right, forward);
}

NAMESPACE_END(nagi)