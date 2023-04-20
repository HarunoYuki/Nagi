#define TINYOBJLOADER_IMPLEMENTATION
#include "mesh.h"
#include "tiny_obj_loader.h"
#include "bvh.h"

NAMESPACE_BEGIN(nagi)

bool Mesh::LoadMesh(std::string& filename)
{
	name = filename;
	tinyobj::attrib_t atrrib;
	std::vector<tinyobj::shape_t> shapes;
	std::string err;

	// 不需要std::vector<tinyobj::material_t> materials
	if (!tinyobj::LoadObj(&atrrib, &shapes, NULL, &err, filename.c_str(), 0, true))
	{
		printf("tinyobj::LoadObj Error: %s\n", err.c_str());
		return false;
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++)
	{
		// Loop over faces(polygon)
		size_t indexOffset = 0;

		// 遍历每个mesh的所有face
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			std::vector<vec3f> facePoints;//存储当前face的所有顶点
			for (size_t v = 0; v < 3; v++)
			{
				tinyobj::index_t idx = shapes[s].mesh.indices[indexOffset++];//indexOffset + v
				tinyobj::real_t vx = atrrib.vertices[idx.vertex_index * 3 + 0];
				tinyobj::real_t vy = atrrib.vertices[idx.vertex_index * 3 + 1];
				tinyobj::real_t vz = atrrib.vertices[idx.vertex_index * 3 + 2];
				facePoints.push_back(vec3f(vx, vy, vz));

				// 读取法线
				tinyobj::real_t nx = 0, ny = 0, nz = 0;
				if (!atrrib.normals.empty())	//部分obj模型没有顶点法线（shading normal）
				{
					nx = atrrib.normals[idx.normal_index * 3 + 0];
					ny = atrrib.normals[idx.normal_index * 3 + 1];
					nz = atrrib.normals[idx.normal_index * 3 + 2];
				}

				// 读取uv
				tinyobj::real_t tx, ty;
				if (!atrrib.texcoords.empty())
				{
					tx = atrrib.texcoords[idx.texcoord_index * 2 + 0];
					ty = 1.0f - atrrib.texcoords[idx.texcoord_index * 2 + 1];
				}
				else {	//部分obj模型没有uv
					if (v == 0) tx = ty = 0;
					else if (v == 1) tx = 0, ty = 1;
					else if (v == 2) tx = ty = 1;
				}

				verticesUVX.push_back(vec4f(vx, vy, vz, tx));
				normalsUVY.push_back(vec4f(nx, ny, nz, ty));
			}

			if (atrrib.normals.empty())	//没有法线，则手动计算面法线（geometry normal）
			{
				vec3f normal = Normalize(Cross(facePoints[2] - facePoints[0], facePoints[1] - facePoints[0]));
				for (int v = indexOffset-3; v < indexOffset; v++)
					normalsUVY[v] = vec4f(normal, normalsUVY[v].w);
			}

			//indexOffset += 3;
		}
	}

	return true;
}

void Mesh::BuildBVH()
{
	const uint32_t trianglesNum = verticesUVX.size() / 3;
	std::vector<bbox3f> bounds(trianglesNum);

#pragma omp parallel for
	for (uint32_t i = 0; i < trianglesNum; i++)
	{
		bounds[i].grow(vec3f(verticesUVX[i * 3 + 0]));
		bounds[i].grow(vec3f(verticesUVX[i * 3 + 1]));
		bounds[i].grow(vec3f(verticesUVX[i * 3 + 2]));
	}
	blasBVH = new BVHAccel(bounds);
}



NAMESPACE_END(nagi)