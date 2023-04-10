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

void Scene::AddCamera(vec3f pos, vec3f lookat, float fov)
{
	if (camera)
		delete camera;
	camera = new Camera(pos, lookat, fov);
}

void Scene::AddEnvMap(std::string& filename)
{
	if (envMap)
		delete envMap;

	envMap = new EnvironmentMap;
	if (envMap->LoadEnvMap(filename))
		printf("HDR \"%s\" loaded\n", filename.c_str());
	else {
		printf("Unable to load \"%s\" HDR\n", filename.c_str());
		delete envMap;
		envMap = nullptr;
	}
}

int Scene::AddTexture(std::string & filename)
{
	// Check if texture was already loaded
	for (size_t i = 0; i < textures.size(); i++)
		if (textures[i]->name == filename)
			return i;

	int id = textures.size();
	Texture* tex = new Texture;

	printf("Loading texture \"%s\"\n", filename.c_str());
	if (tex->LoadTexture(filename))
		textures.push_back(tex);
	else {
		delete tex;
		id = -1;
		printf("Fail to load \"%s\" texture\n", filename.c_str());
	}

	return id;
}

int Scene::AddMesh(std::string & filename)
{
	// Check if mesh was already loaded
	for (size_t i = 0; i < meshes.size(); i++)
		if (meshes[i]->name == filename)
			return i;

	int id = meshes.size();
	Mesh* mesh = new Mesh;

	printf("Loading mesh \"%s\"\n", filename.c_str());
	if (mesh->LoadMesh(filename))
		meshes.push_back(mesh);
	else {
		delete mesh;
		id = -1;
		printf("Fail to load \"%s\" mesh\n", filename.c_str());
	}

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