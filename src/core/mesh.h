#pragma once
#include <string>
#include <vector>
#include "matrix.h"

NAMESPACE_BEGIN(nagi)

class BVHAccel;

class Mesh
{
public:
	Mesh();
	~Mesh();

	bool LoadMesh(std::string& filename);
	void BuildBVH();

	BVHAccel* blasBVH;
	std::string name;
	std::vector<vec4f> verticesUVX;// Vertex + texture Coord (u/s)
	std::vector<vec4f> normalsUVY;  // Normal + texture Coord (v/t)
};


class MeshInstance
{
public:
	MeshInstance() :meshID(-1), materialID(0), name("none") {}
	~MeshInstance() {}

	int meshID;
	int materialID;

	mat4 transform;
	std::string name;
};

NAMESPACE_END(nagi)
