#pragma once
#include "vector.h"

NAMESPACE_BEGIN(nagi)

//template <class T>
class Matrix44
{
public:
	float data[4][4];

	Matrix44()
	{
		data[0][0] = 1; data[0][1] = 0;  data[0][2] = 0;  data[0][3] = 0;
		data[1][0] = 0; data[1][1] = 1;  data[1][2] = 0;  data[1][3] = 0;
		data[2][0] = 0; data[2][1] = 0;  data[2][2] = 1;  data[2][3] = 0;
		data[3][0] = 0; data[3][1] = 0;  data[3][2] = 0;  data[3][3] = 1;
	}

	float(&operator [](int i))[4]{ return data[i]; };
	Matrix44 operator*(const Matrix44& b) const
	{
		Matrix44 out;

		out[0][0] = data[0][0] * b.data[0][0] + data[0][1] * b.data[1][0] + data[0][2] * b.data[2][0] + data[0][3] * b.data[3][0];
		out[0][1] = data[0][0] * b.data[0][1] + data[0][1] * b.data[1][1] + data[0][2] * b.data[2][1] + data[0][3] * b.data[3][1];
		out[0][2] = data[0][0] * b.data[0][2] + data[0][1] * b.data[1][2] + data[0][2] * b.data[2][2] + data[0][3] * b.data[3][2];
		out[0][3] = data[0][0] * b.data[0][3] + data[0][1] * b.data[1][3] + data[0][2] * b.data[2][3] + data[0][3] * b.data[3][3];

		out[1][0] = data[1][0] * b.data[0][0] + data[1][1] * b.data[1][0] + data[1][2] * b.data[2][0] + data[1][3] * b.data[3][0];
		out[1][1] = data[1][0] * b.data[0][1] + data[1][1] * b.data[1][1] + data[1][2] * b.data[2][1] + data[1][3] * b.data[3][1];
		out[1][2] = data[1][0] * b.data[0][2] + data[1][1] * b.data[1][2] + data[1][2] * b.data[2][2] + data[1][3] * b.data[3][2];
		out[1][3] = data[1][0] * b.data[0][3] + data[1][1] * b.data[1][3] + data[1][2] * b.data[2][3] + data[1][3] * b.data[3][3];

		out[2][0] = data[2][0] * b.data[0][0] + data[2][1] * b.data[1][0] + data[2][2] * b.data[2][0] + data[2][3] * b.data[3][0];
		out[2][1] = data[2][0] * b.data[0][1] + data[2][1] * b.data[1][1] + data[2][2] * b.data[2][1] + data[2][3] * b.data[3][1];
		out[2][2] = data[2][0] * b.data[0][2] + data[2][1] * b.data[1][2] + data[2][2] * b.data[2][2] + data[2][3] * b.data[3][2];
		out[2][3] = data[2][0] * b.data[0][3] + data[2][1] * b.data[1][3] + data[2][2] * b.data[2][3] + data[2][3] * b.data[3][3];

		out[3][0] = data[3][0] * b.data[0][0] + data[3][1] * b.data[1][0] + data[3][2] * b.data[2][0] + data[3][3] * b.data[3][0];
		out[3][1] = data[3][0] * b.data[0][1] + data[3][1] * b.data[1][1] + data[3][2] * b.data[2][1] + data[3][3] * b.data[3][1];
		out[3][2] = data[3][0] * b.data[0][2] + data[3][1] * b.data[1][2] + data[3][2] * b.data[2][2] + data[3][3] * b.data[3][2];
		out[3][3] = data[3][0] * b.data[0][3] + data[3][1] * b.data[1][3] + data[3][2] * b.data[2][3] + data[3][3] * b.data[3][3];

		return out;

	}

	static Matrix44 Translate(const vec3f& a)
	{
		Matrix44 out;
		out[3][0] = a.x;
		out[3][1] = a.y;
		out[3][2] = a.z;
		return out;
	}
	static Matrix44 Scale(const vec3f& a)
	{
		Matrix44 out;
		out[0][0] = a.x;
		out[1][1] = a.y;
		out[2][2] = a.z;
		return out;
	}
	static Matrix44 QuatToMatrix(float x, float y, float z, float w)
	{
		Matrix44 out;

		const float x2 = x + x;
		const float y2 = y + y;
		const float z2 = z + z;

		const float xx = x * x2;
		const float xy = x * y2;
		const float xz = x * z2;

		const float yy = y * y2;
		const float yz = y * z2;
		const float zz = z * z2;

		const float wx = w * x2;
		const float wy = w * y2;
		const float wz = w * z2;

		out.data[0][0] = 1.0f - (yy + zz);
		out.data[0][1] = xy + wz;
		out.data[0][2] = xz - wy;
		out.data[0][3] = 0.0f;

		out.data[1][0] = xy - wz;
		out.data[1][1] = 1.0f - (xx + zz);
		out.data[1][2] = yz + wx;
		out.data[1][3] = 0.0f;

		out.data[2][0] = xz + wy;
		out.data[2][1] = yz - wx;
		out.data[2][2] = 1.0f - (xx + yy);
		out.data[2][3] = 0.0f;

		out.data[3][0] = 0;
		out.data[3][1] = 0;
		out.data[3][2] = 0;
		out.data[3][3] = 1.0f;

		return out;

	}
};

typedef Matrix44 mat4;

NAMESPACE_END(nagi)