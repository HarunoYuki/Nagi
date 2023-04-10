#pragma once
#include "logger.h"

NAMESPACE_BEGIN(nagi)

#define PI 3.14159265358979323846f

template <class T>
class Vector2
{
public:
	T x, y;
	Vector2() { x = y = 0; }
	Vector2(T xx, T yy) :x(xx), y(yy) {}

	bool operator==(const Vector2<T>& v) const { return x == v.x && y == v.y; }
	bool operator!=(const Vector2<T>& v) const { return x != v.x || y != v.y; }

	Vector2<T> operator+(const Vector2<T>& v) const {
		return Vector2(x + v.x, y + v.y);
	}
	Vector2<T>& operator+=(const Vector2<T>& v) {
		x += v.x; y += v.y;
		return *this;
	}

	Vector2<T> operator-(const Vector2<T>& v) const {
		return Vector2(x - v.x, y - v.y);
	}
	Vector2<T>& operator-=(const Vector2<T>& v) {
		x -= v.x; y -= v.y;
		return *this;
	}

	template <typename U>
	Vector2<T> operator*(const U& f) const {
		return Vector2<T>(x*f, y*f);
	}
	template <typename U>
	Vector2<T>& operator*=(const U& f) {
		x *= f; y *= f;
		return *this;
	}

	template <typename U>
	Vector2<T> operator/(const U& f) const {
		if ((float)f == 0.0) Error("Denominator 0 is illegal!");
		float invF = (float)1 / f;
		return Vector2<T>(x*invF, y*invF);
	}
	template <typename U>
	Vector2<T>& operator/=(const U& f) {
		if ((float)f == 0.0) Error("Denominator 0 is illegal!");
		float invF = (float)1 / f;
		x *= invF; y *= invF;
		return *this;
	}

	Vector2<T> operator-() const { return Vector2<T>(-x, -y); }
	T operator[](int i) const {
		if (i < 0 || i > 1) Error("Index is out of range!");
		if (i == 0) return x;
		return y;
	}
	T& operator[](int i) {
		if (i < 0 || i > 1) Error("Index is out of range!");
		if (i == 0) return x;
		return y;
	}

	float LengthSquared() const { return x * x + y * y; }
	float Length() const { return std::sqrt(LengthSquared()); }
};

template <class T>
class Vector3
{
public:
	T x, y, z;
	Vector3() { x = y = z = 0; }
	Vector3(T xx, T yy, T zz) :x(xx), y(yy), z(zz) {}

	bool operator==(const Vector3<T>& v) const { return x == v.x && y == v.y && z == v.z; }
	bool operator!=(const Vector3<T>& v) const { return x != v.x || y != v.y || z != v.z; }

	Vector3<T> operator+(const Vector3<T>& v) const {
		return Vector3(x + v.x, y + v.y, z + v.z);
	}
	Vector3<T>& operator+=(const Vector3<T>& v) {
		x += v.x; y += v.y; z += v.z;
		return *this;
	}

	Vector3<T> operator-(const Vector3<T>& v) const {
		return Vector3(x - v.x, y - v.y, z - v.z);
	}
	Vector3<T>& operator-=(const Vector3<T>& v) {
		x -= v.x; y -= v.y; z -= v.z;
		return *this;
	}

	template <typename U>
	Vector3<T> operator*(const U& f) const {
		return Vector3<T>(x*f, y*f, z*f);
	}
	template <typename U>
	Vector3<T>& operator*=(const U& f) {
		x *= f; y *= f; z *= f;
		return *this;
	}

	template <typename U>
	Vector3<T> operator/(const U& f) const {
		if ((float)f == 0.0) Error("Denominator 0 is illegal!");
		float invF = (float)1 / f;
		return Vector3<T>(x*invF, y*invF, z*invF);
	}
	template <typename U>
	Vector3<T>& operator/=(const U& f) {
		if ((float)f == 0.0) Error("Denominator 0 is illegal!");
		float invF = (float)1 / f;
		x *= invF; y *= invF; z *= invF;
		return *this;
	}

	Vector3<T> operator-() const { return Vector3<T>(-x, -y, -z); }
	T operator[](int i) const {
		if (i < 0 || i > 2) Error("Index is out of range!");
		if (i == 0) return x;
		if (i == 1) return y;
		return z;
	}
	T& operator[](int i) {
		if (i < 0 || i > 2) Error("Index is out of range!");
		if (i == 0) return x;
		if (i == 1) return y;
		return z;
	}

	float LengthSquared() const { return x * x + y * y + z * z; }
	float Length() const { return std::sqrtf(LengthSquared()); }
};

template <class T>
class Vector4
{
public:
	T x, y, z, w;
	Vector4() { x = y = z = w = 0; }
	Vector4(T xx, T yy, T zz, T ww) :x(xx), y(yy), z(zz), w(ww) {}
	Vector4(Vector3<T> n, T ww) :x(n.x), y(n.y), z(n.z), w(ww) {}

	bool operator==(const Vector4<T>& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
	bool operator!=(const Vector4<T>& v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }

	Vector4<T> operator+(const Vector4<T>& v) const {
		return Vector4(x + v.x, y + v.y, z + v.z, w + v.w);
	}
	Vector4<T>& operator+=(const Vector4<T>& v) {
		x += v.x; y += v.y; z += v.z; w += v.w;
		return *this;
	}

	Vector4<T> operator-(const Vector4<T>& v) const {
		return Vector4(x - v.x, y - v.y, z - v.z, w - v.w);
	}
	Vector4<T>& operator-=(const Vector4<T>& v) {
		x -= v.x; y -= v.y; z -= v.z; w -= v.w;
		return *this;
	}

	Vector4<T> operator-() const { return Vector4<T>(-x, -y, -z,-w); }
	T operator[](int i) const {
		if (i < 0 || i > 3) Error("Index is out of range!");
		if (i == 0) return x;
		if (i == 1) return y;
		if (i == 2) return z;
		return w;
	}
	T& operator[](int i) {
		if (i < 0 || i > 3) Error("Index is out of range!");
		if (i == 0) return x;
		if (i == 1) return y;
		if (i == 2) return z;
		return w;
	}

	float LengthSquared() const { return x * x + y * y + z * z + w * w; }
	float Length() const { return std::sqrt(LengthSquared()); }
};

typedef Vector2<int> vec2i;
typedef Vector2<float> vec2f;
typedef Vector3<int> vec3i;
typedef Vector3<float> vec3f;
typedef Vector4<int> vec4i;
typedef Vector4<float> vec4f;

template <typename T>
float Dot(const Vector3<T>& a, const Vector3<T>& b)
{
	return (a.x * b.x + a.y * b.y + a.z * b.z);
}

template <typename T>
Vector3<T> Cross(const Vector3<T>& a, const Vector3<T>& b)
{
	return Vector3<T>(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
		);
}

template <typename T>
Vector3<T> Normalize(const Vector3<T>& d)
{
	float denominator = 1.0f / d.Length();
	return d * denominator;
}

template <typename T>
T Degrees(T r)
{
	return r * 180.0f / PI;
}

template <typename T>
T Radians(T d)
{
	return d * PI / 180.0f;
}

NAMESPACE_END(nagi)