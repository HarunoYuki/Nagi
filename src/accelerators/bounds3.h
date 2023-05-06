#pragma once
#include <limits>
#include "vector.h"

NAMESPACE_BEGIN(nagi)

template <typename T>
class Bounds3
{
public:
	Vector3<T> pMin, pMax;
	// 0点，初始化为空volume
	Bounds3() :pMin(Vector3<T>(std::numeric_limits<float>::max())),
		pMax(Vector3<T>(std::numeric_limits<float>::lowest())) {}
	// 1点，pMin=pMax=p
	Bounds3(const Vector3<T>& p) :pMin(p), pMax(p) {}
	// 2点，两点的xyz分量最小的为pMin，最大的为pMax
	Bounds3(const Vector3<T>& p1, const Vector3<T>& p2) :pMin(Min(p1, p2)), pMax(Max(p1, p2)) {}

	// 返回中心点
	Vector3<T> Center() const { return (pMax + pMin)*0.5; }
	// 返回对角线，pMin->pMax
	Vector3<T> Diagonal() const { return pMax - pMin; }

	// 返回表面积
	T SurfaceArea() const {
		Vector3<T> d = Diagonal();
		return 2 * (d.x*d.y + d.x*d.z + d.y*d.z);
	}

	// 返回最长轴
	int MaximumExtent() const {
		Vector3<T> d = Diagonal();
		if (d.x > d.y&&d.x > d.z)
			return 0;
		else if (d.y > d.z)
			return 1;
		else
			return 2;
	}

	// this与一个点合并
	void grow(const Vector3<T>& p) {
		this->pMin = Min(this->pMin, p);
		this->pMax = Max(this->pMax, p);
	}

	// this与一个box合并
	void grow(const Bounds3<T>& box) {
		this->pMin = Min(this->pMin, box.pMin);
		this->pMax = Max(this->pMax, box.pMax);
	}

	// p是否在this中
	bool Contain(const Vector3<T>& p) {
		return (pMin.x <= p.x && p.x <= pMax.x) && 
			(pMin.y <= p.y && p.y <= pMax.y) && 
			(pMin.z <= p.z && p.z <= pMax.z);
	}

	// p点的局部归一化坐标
	Vector3<T> LocalNormalizedCoord(const Vector3<T>& p) const {
		Vector3<T> o = p - pMin;
		if (pMax.x > pMin.x) o.x /= pMax.x - pMin.x;
		if (pMax.y > pMin.y) o.y /= pMax.y - pMin.y;
		if (pMax.z > pMin.z) o.z /= pMax.z - pMin.z;
		return o;
	}
};

template <typename T>
Bounds3<T> Union(const Bounds3<T>& box, const Vector3<T>& p) {
	return Bounds3<T>(Min(box.pMin, p), Max(box.pMax, p));
}

template <typename T>
Bounds3<T> Union(const Bounds3<T>& box1, const Bounds3<T>& box2) {
	return Bounds3<T>(Min(box1.pMin, box2.pMin), Max(box1.pMax, box2.pMax));
}

template <typename T>
bool Overlaps(const Bounds3<T> &box1, const Bounds3<T> &box2) {
	return (box1.pMin <= box2.pMax) && (box2.pMin <= box1.pMax);
}

template <typename T>
Bounds3<T> Intersection(const Bounds3<T>& box1, const Bounds3<T>& box2) {
	return Bounds3<T>(Max(box1.pMin, box2.pMin), Min(box1.pMax, box2.pMax));
}

using bbox3f = Bounds3<float>;

NAMESPACE_END(nagi)
