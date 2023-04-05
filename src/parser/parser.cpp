#include <map>
#include "parser.h"
#include "scene.h"
#include "material.h"
#include "light.h"

NAMESPACE_BEGIN(nagi)

static const int kMaxLineLength = 2048;

bool ParseFromSceneFile(std::string filename, Scene* scene)
{
	FILE* file = fopen(filename.c_str(), "r");
	if (!file)
		Error("Fail to open %s file", filename.c_str());

	printf("Parse Scene From %s file \n", filename.c_str());

	std::map<std::string, uint16_t> materialMap;
	std::string path = filename.substr(0, filename.find_last_of("/\\")) + "/";

	char line[kMaxLineLength];

	Material* defaultMat = new Material;
	scene->AddMaterial(defaultMat);

	while(fgets(line, kMaxLineLength, file))
	{
		// skip comments
		if (line[0] == '#')
			continue;

		// name used for materials and meshes
		char name[kMaxLineLength] = { 0 };

		// Material
		if (sscanf(line, " material %s", name) == 1)
		{
			Material* mat = new Material;
			char baseColorTexName[100] = "none";
			char roughnessTexName[100] = "none";
			char metallicTexName[100] = "none";
			char normalMapTexName[100] = "none";
			char emissionMapTexName[100] = "none";
			char alphaMode[100] = "none";
			char mediumType[100] = "none";

			while(fgets(line, kMaxLineLength, file))
			{
				if (strchr(line, '}'))
					break;

				sscanf(line, " color %f %f %f", &mat->baseColor.x, &mat->baseColor.y, &mat->baseColor.z);
				sscanf(line, " anisotropic %f", &mat->anisotropic);
				sscanf(line, " emission %f %f %f", &mat->emission.x, &mat->emission.y, &mat->emission.z);
				sscanf(line, " roughness %f", &mat->roughness);
				sscanf(line, " metallic %f", &mat->metallic);
				sscanf(line, " subsurface %f", &mat->subsurface);
				sscanf(line, " specularTint %f", &mat->specularTint);
				sscanf(line, " sheen %f", &mat->sheen);
				sscanf(line, " sheenTint %f", &mat->sheenTint);
				sscanf(line, " clearcoat %f", &mat->clearcoat);
				sscanf(line, " clearcoatGloss %f", &mat->clearcoatGloss);
				sscanf(line, " specTrans %f", &mat->specTrans);
				sscanf(line, " ior %f", &mat->ior);
				sscanf(line, " mediumType %f", &mat->mediumType);
				sscanf(line, " mediumDensity %f", &mat->mediumDensity);
				sscanf(line, " mediumColor %f %f %f", &mat->mediumColor.x, &mat->mediumColor.y, &mat->mediumColor.z);
				sscanf(line, " mediumAnisotropy %f", &mat->mediumAnisotropy);
				sscanf(line, " baseColorTex %s", baseColorTexName);
				sscanf(line, " roughnessTex %s", roughnessTexName);
				sscanf(line, " metallicTex %s", metallicTexName);
				sscanf(line, " normalMapTex %s", normalMapTexName);
				sscanf(line, " emissionTex %s", emissionMapTexName);
				sscanf(line, " opacity %f", &mat->opacity);
				sscanf(line, " alphaMode %f", &mat->alphaMode);
				sscanf(line, " alphaCutoff %f", &mat->alphaCutoff);
			}

			if (baseColorTexName != "none")
				mat->baseColorTexID = scene->AddTexture(path + baseColorTexName);

			if (roughnessTexName != "none")
				mat->roughnessTexID = scene->AddTexture(path + roughnessTexName);

			if (metallicTexName != "none")
				mat->metallicTexID = scene->AddTexture(path + metallicTexName);

			if (normalMapTexName != "none")
				mat->normalMapTexID = scene->AddTexture(path + normalMapTexName);

			if (emissionMapTexName != "none")
				mat->emissionMapTexID = scene->AddTexture(path + emissionMapTexName);

			if (strcmp(alphaMode, "opaque"))
				mat->alphaMode = AlphaMode::Opaque;
			else if (strcmp(alphaMode, "blend"))
				mat->alphaMode = AlphaMode::Blend;
			else if (strcmp(alphaMode, "mask"))
				mat->alphaMode = AlphaMode::Mask;

			if (strcmp(mediumType, "absorb"))
				mat->mediumType = MediumType::Absorb;
			else if (strcmp(mediumType, "scatter"))
				mat->mediumType = MediumType::Scatter;
			else if (strcmp(mediumType, "emissive"))
				mat->mediumType = MediumType::Emissive;

			if (materialMap.find(name) == materialMap.end())
				materialMap[name] = scene->AddMaterial(mat);
		}

		if (strstr(line, "renderer"))
		{
			RenderOptions& options = *(scene->renderOptions);
			while (fgets(line, kMaxLineLength, file))
			{
				if (strchr(line,'}'))
					break;

				sscanf(line, " renderRes %d %d", &options.renderResolution.x, &options.renderResolution.y);
				sscanf(line, " windowRes %d %d", &options.windowResolution.x, &options.windowResolution.y);
			}
		}

		if (strstr(line, "camera"))
		{
			while (fgets(line, kMaxLineLength, file))
			{
				if (strchr(line, '}'))
					break;
			}
		}

		if (strstr(line, "light"))
		{
			Light* light = new Light;
			vec3f v1, v2;
			char lightType[100] = "none";
			while (fgets(line,kMaxLineLength,file))
			{
				if (strchr(line, '}'))
					break;

				sscanf(line, " position %f %f %f", &light->position.x, &light->position.y, &light->position.z);
				sscanf(line, " emission %f %f %f", &light->emission.x, &light->emission.y, &light->emission.z);
				sscanf(line, " v1 %f %f %f", &v1.x, &v1.y, &v1.z);
				sscanf(line, " v2 %f %f %f", &v2.x, &v2.y, &v2.z);
				sscanf(line, " radius %f", &light->radius);
				sscanf(line, " type %s", lightType);
			}

			if (lightType == "quad")
			{
				light->type = LightType::RectLight;
				light->u = v1 - light->position;
				light->v = v2 - light->position;
				light->area = Cross(light->u, light->v).Length();
			}
			else if (lightType == "sphere")
			{
				light->type = LightType::SphereLight;
				light->area = 4.0f * PI * light->radius * light->radius;
			}
			else if (lightType == "distant")
			{
				light->type = LightType::DistantLight;
				light->area = 0.0f;
			}

			scene->AddLight(light);
		}

		if (strstr(line, "mesh"))
		{

			while (fgets(line, kMaxLineLength, file))
			{
				if (strchr(line, '}'))
					break;

			}
		}
	}


	return true;
}

NAMESPACE_END(nagi)