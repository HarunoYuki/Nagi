#pragma once

#include <vector>
#include "matrix.h"

NAMESPACE_BEGIN(nagi)

class Camera;
class EnvironmentMap;
class Texture;
class Mesh;
class MeshInstance;
class Material;
class Light;

struct RenderOptions
{
	RenderOptions() {
		renderResolution = vec2i(600, 600);
		windowResolution = vec2i(1000, 600);
		maxSpp = 512;
		maxDepth = 3;
		enableEnvMap = false;
		envMapIntensity = 1.0f;
	}
	vec2i renderResolution;
	vec2i windowResolution;
	int maxSpp;
	int maxDepth;
	bool enableEnvMap;
	float envMapIntensity;
};

class Scene
{
public:
	Scene();
	~Scene();

	int AddCamera(vec3f pos, vec3f lookat, float fov);
	int AddEnvMap(std::string& filename);
	int AddTexture(std::string& filename);
	int AddMesh(std::string& filename);
	int AddMeshInstance(MeshInstance* meshInstance);
	int AddMaterial(Material* mat);
	int AddLight(Light* light);

public:
	// RenderOptions is responsible for indicate render options.
	RenderOptions* renderOptions;

	// only one camera for generate rays.
	Camera* camera;

	// an EnvironmentMap for Image-based Lighting.
	EnvironmentMap* envMap;

	// meshes in the scene
	std::vector<Mesh*> meshes;
	// a MeshInstance holds a meshId and relevant transform, also includes a materialId.
	// this seperation which reduces the actual number of triangle mesh in the scene can really save memory usage.
	std::vector<MeshInstance*> meshInstances;

	// store all material
	std::vector<Material*> materials;
	// store all light
	std::vector<Light*> lights;

	// Texture Data
	std::vector<Texture*> textures;
	std::vector<unsigned char> textureMapsArray;

	// scene data for all obj
	std::vector<vec3i> vertIndices;
	std::vector<vec4f> verticesUVX;// Vertex + texture Coord (u/s)
	std::vector<vec4f> normalsUVY;  // Normal + texture Coord (v/t)
	std::vector<mat4> transforms;

	// there are four varible control render state.
	// Is scene has been initialized?
	bool initialized;
	// Is scene has to rendered from scratch?
	bool dirty;
	// Is meshInstances have been modified?
	bool instancesModified;
	// Is envMap has been modified?
	bool envMapModified;
};

NAMESPACE_END(nagi)
