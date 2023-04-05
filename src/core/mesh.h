#pragma once

#include "matrix.h"

NAMESPACE_BEGIN(nagi)

class Mesh
{
public:

private:

};

class MeshInstance
{
public:
	MeshInstance();
	~MeshInstance();

	int meshId;
	int materialId;

	mat4 transform;
	std::string name;
};

NAMESPACE_END(nagi)
