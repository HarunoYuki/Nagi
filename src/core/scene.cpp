#include "scene.h"
#include "camera.h"
#include "environmentMap.h"
#include "texture.h"
#include "mesh.h"
#include "material.h"
#include "light.h"

NAMESPACE_BEGIN(nagi)


Scene::Scene() :
	camera(nullptr), envMap(nullptr), renderOptions(new RenderOptions),
	initialized(false), dirty(true), instancesModified(true), envMapModified(true) {}

Scene::~Scene()
{
	if (renderOptions)
		delete renderOptions;

	if (camera)
		delete camera;

	if (envMap)
		delete envMap;

	for (size_t i = 0; i < textures.size(); i++)
		delete textures[i];
	textures.clear();

	for (size_t i = 0; i < meshes.size(); i++)
		delete meshes[i];
	meshes.clear();

	for (size_t i = 0; i < meshInstances.size(); i++)
		delete meshInstances[i];
	meshInstances.clear();

	for (size_t i = 0; i < materials.size(); i++)
		delete materials[i];
	materials.clear();

	for (size_t i = 0; i < lights.size(); i++)
		delete lights[i];
	lights.clear();
}

int Scene::AddCamera(vec3f pos, vec3f lookat, float fov)
{
	if (camera)
		delete camera;
	camera = new Camera(pos, lookat, fov);
}

int Scene::AddEnvMap(std::string& filename)
{
	if (envMap)
		delete envMap;
	//todo:
	// load envmap 
	//
	// process fail
	//return id
}

int Scene::AddTexture(std::string & filename)
{
	int id = -1;


	return id;
}

int Scene::AddMesh(std::string & filename)
{
	int id = -1;

	return id;
}

int Scene::AddMeshInstance(MeshInstance * meshInstance)
{
	int id = meshInstances.size();
	meshInstances.push_back(meshInstance);
	return id;
}

int Scene::AddMaterial(Material * mat)
{
	int id = materials.size();
	materials.push_back(mat);
	return id;
}

int Scene::AddLight(Light * light)
{
	int id = lights.size();
	lights.push_back(light);
	return id;
}


NAMESPACE_END(nagi)