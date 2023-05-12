#include <map>
#include "parser.h"
#include "scene.h"
#include "material.h"
#include "light.h"
#include "camera.h"
#include "mesh.h"

NAMESPACE_BEGIN(nagi)

static const int kMaxLineLength = 2048;

//TODO: 对mat.alphaMode、mat.mediumType、light.type可以改用raytools中的判断枚举值的方法

bool ParseFromSceneFile(std::string filename, Scene* scene)
{
	FILE* file = fopen(filename.c_str(), "r");
	if (!file)
		Error("Fail to open \"%s\" file", filename.c_str());

	printf("Parse Scene From \"%s\" file.\n", filename.c_str());

	std::map<std::string, uint16_t> materialMap;
	std::string path = filename.substr(0, filename.find_last_of("/\\") + 1);

	char line[kMaxLineLength];

	Material defaultMat;
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
			Material mat;
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

				sscanf(line, " color %f %f %f", 		&mat.baseColor.x, &mat.baseColor.y, &mat.baseColor.z);
				sscanf(line, " anisotropic %f", 		&mat.anisotropic);
				sscanf(line, " emission %f %f %f", 		&mat.emission.x, &mat.emission.y, &mat.emission.z);
				sscanf(line, " roughness %f", 			&mat.roughness);
				sscanf(line, " metallic %f", 			&mat.metallic);
				sscanf(line, " subsurface %f", 			&mat.subsurface);
				sscanf(line, " specularTint %f", 		&mat.specularTint);
				sscanf(line, " sheen %f", 				&mat.sheen);
				sscanf(line, " sheenTint %f", 			&mat.sheenTint);
				sscanf(line, " clearcoat %f", 			&mat.clearcoat);
				sscanf(line, " clearcoatGloss %f", 		&mat.clearcoatGloss);
				sscanf(line, " specTrans %f", 			&mat.specTrans);
				sscanf(line, " ior %f", 				&mat.ior);
				sscanf(line, " mediumType %f", 			&mat.mediumType);
				sscanf(line, " mediumDensity %f", 		&mat.mediumDensity);
				sscanf(line, " mediumColor %f %f %f", 	&mat.mediumColor.x, &mat.mediumColor.y, &mat.mediumColor.z);
				sscanf(line, " mediumAnisotropy %f", 	&mat.mediumAnisotropy);
				sscanf(line, " baseColorTex %s", 		baseColorTexName);
				sscanf(line, " roughnessTex %s", 		roughnessTexName);
				sscanf(line, " metallicTex %s", 		metallicTexName);
				sscanf(line, " normalMapTex %s", 		normalMapTexName);
				sscanf(line, " emissionTex %s", 		emissionMapTexName);
				sscanf(line, " opacity %f", 			&mat.opacity);
				sscanf(line, " alphaMode %f", 			&mat.alphaMode);
				sscanf(line, " alphaCutoff %f", 		&mat.alphaCutoff);
			}

			if (strcmp(baseColorTexName, "none") != 0)
				mat.baseColorTexID = scene->AddTexture(path + baseColorTexName);

			if (strcmp(roughnessTexName, "none") != 0)
				mat.roughnessTexID = scene->AddTexture(path + roughnessTexName);

			if (strcmp(metallicTexName, "none") != 0)
				mat.metallicTexID = scene->AddTexture(path + metallicTexName);

			if (strcmp(normalMapTexName, "none") != 0)
				mat.normalMapTexID = scene->AddTexture(path + normalMapTexName);

			if (strcmp(emissionMapTexName, "none") != 0)
				mat.emissionMapTexID = scene->AddTexture(path + emissionMapTexName);

			if (strcmp(alphaMode, "opaque") == 0)
				mat.alphaMode = Material::AlphaMode::Opaque;
			else if (strcmp(alphaMode, "blend") == 0)
				mat.alphaMode = Material::AlphaMode::Blend;
			else if (strcmp(alphaMode, "mask") == 0)
				mat.alphaMode = Material::AlphaMode::Mask;

			if (strcmp(mediumType, "absorb") == 0)
				mat.mediumType = Material::MediumType::Absorb;
			else if (strcmp(mediumType, "scatter") == 0)
				mat.mediumType = Material::MediumType::Scatter;
			else if (strcmp(mediumType, "emissive") == 0)
				mat.mediumType = Material::MediumType::Emissive;

			if (materialMap.find(name) == materialMap.end())
				materialMap[name] = scene->AddMaterial(mat);
		}

		if (strstr(line, "renderer"))
		{
			RenderOptions& options = *(scene->renderOptions);
			char envMapName[200] = "none";

			while (fgets(line, kMaxLineLength, file))
			{
				if (strchr(line,'}'))
					break;

				sscanf(line, " envmapFile %s", 						envMapName);
				sscanf(line, " envMapRotation %f", 					&options.envMapRot);
				sscanf(line, " envmapIntensity %f", 				&options.envMapIntensity);
				sscanf(line, " renderRes %d %d", 					&options.renderResolution.x, &options.renderResolution.y);
				sscanf(line, " windowRes %d %d", 					&options.windowResolution.x, &options.windowResolution.y);
				sscanf(line, " tileRes %d %d", 						&options.tileResolution.x,   &options.tileResolution.y);
				sscanf(line, " maxSpp %d", 							&options.maxSpp);
				sscanf(line, " maxDepth %d", 						&options.maxDepth);
				sscanf(line, " RRDepth %d", 						&options.RRDepth);
				sscanf(line, " texArrayWidth %d", 					&options.texArrayWidth);
				sscanf(line, " texArrayHeight %d", 					&options.texArrayHeight);
				sscanf(line, " denoiserFrameCnt %d", 				&options.denoiserFrameCnt);
				sscanf(line, " enableRR %d", 						&options.enableRR);
				sscanf(line, " enableDenoiser %d", 					&options.enableDenoiser);
				sscanf(line, " enableTonemap %d", 					&options.enableTonemap);
				sscanf(line, " enableAces %d", 						&options.enableAces);
				sscanf(line, " simpleAcesFit %d", 					&options.enableSimpleAcesFit);
				sscanf(line, " openglNormalMap %d", 				&options.enableOpenglNormalMap);
				sscanf(line, " hideEmitters %d", 					&options.enableHideEmitters);
				sscanf(line, " enableBackground %d", 				&options.enableBackground);
				sscanf(line, " transparentBackground %d",			&options.enableTransparentBackground);
				sscanf(line, " independentRenderSize %d", 			&options.enableIndependentRenderSize);
				sscanf(line, " enableRoughnessMollification %d", 	&options.enableRoughnessMollification);
				sscanf(line, " enableVolumeMIS %d", 				&options.enableVolumeMIS);
			}

			if (strcmp(envMapName, "none") == 0)
				options.enableEnvMap = false;
			else if (strcmp(envMapName, "none") != 0)
			{
				scene->AddEnvMap(path + envMapName);
				options.enableEnvMap = true;
			}

			if (!options.enableIndependentRenderSize)
				options.windowResolution = options.renderResolution;
		}

		if (strstr(line, "camera"))
		{
			mat4 xform;
			bool matrixProvided = false;
			vec3f pos, lookat;
			float fov, focalDistance = 1.0f, lensRadius = 0.0f;

			while (fgets(line, kMaxLineLength, file))
			{
				if (strchr(line, '}'))
					break;

				sscanf(line, " position %f %f %f", 	&pos.x, &pos.y, &pos.z);
				sscanf(line, " lookat %f %f %f", 	&lookat.x, &lookat.y, &lookat.z);
				sscanf(line, " fov %f", 			&fov);
				sscanf(line, " focalDistance %f", 	&focalDistance);
				sscanf(line, " lensRadius %f", 		&lensRadius);

				if (sscanf(line, " matrix %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
					&xform[0][0], &xform[1][0], &xform[2][0], &xform[3][0],
					&xform[0][1], &xform[1][1], &xform[2][1], &xform[3][1],
					&xform[0][2], &xform[1][2], &xform[2][2], &xform[3][2],
					&xform[0][3], &xform[1][3], &xform[2][3], &xform[3][3]
				) != 0)
					matrixProvided = true;
			}

			if (matrixProvided)
			{
				vec3f forward = vec3f(xform[2][0], xform[2][1], xform[2][2]);
				pos = vec3f(xform[3][0], xform[3][1], xform[3][2]);
				lookat = pos + forward;
			}

			scene->AddCamera(pos, lookat, fov);
			scene->camera->focalDistance = focalDistance;
			scene->camera->lensRadius = lensRadius;
		}

		if (strstr(line, "light"))
		{
			Light light;
			vec3f v1, v2;
			char lightType[100] = "none";

			while (fgets(line,kMaxLineLength,file))
			{
				if (strchr(line, '}'))
					break;

				sscanf(line, " position %f %f %f", 	&light.position.x, &light.position.y, &light.position.z);
				sscanf(line, " emission %f %f %f", 	&light.emission.x, &light.emission.y, &light.emission.z);
				sscanf(line, " v1 %f %f %f", 		&v1.x, &v1.y, &v1.z);
				sscanf(line, " v2 %f %f %f", 		&v2.x, &v2.y, &v2.z);
				sscanf(line, " radius %f", 			&light.radius);
				sscanf(line, " type %s", 			lightType);
			}

			if (strcmp(lightType, "quad") == 0)
			{
				light.type = Light::LightType::RectLight;
				light.u = v1 - light.position;
				light.v = v2 - light.position;
				light.area = Cross(light.u, light.v).Length();
			}
			else if (strcmp(lightType, "sphere") == 0)
			{
				light.type = Light::LightType::SphereLight;
				light.area = 4.0f * PI * light.radius * light.radius;
			}
			else if (strcmp(lightType, "distant") == 0)
			{
				light.type = Light::LightType::DistantLight;
				light.area = 0.0f;
			}

			scene->AddLight(light);
		}

		if (strstr(line, "mesh"))
		{
			char meshName[100] = "none";
			char matName[100] = "none";
			mat4 xform, translate, scale, rotate;
			vec4f rotQuat;
			bool matrixProvided = false;

			while (fgets(line, kMaxLineLength, file))
			{
				if (strchr(line, '}'))
					break;

				sscanf(line, " meshName %s", 			  meshName);
				sscanf(line, " matName %s", 			  matName);
				sscanf(line, " position %f %f %f", 		  &translate.data[3][0], &translate.data[3][1], &translate.data[3][2]);
				sscanf(line, " scale %f %f %f", 		  &scale.data[0][0], &scale.data[1][1], &scale.data[2][2]);
				if (sscanf(line, " rotation %f %f %f %f", &rotQuat.x, &rotQuat.y, &rotQuat.z, &rotQuat.w) != 0)
					rotate = mat4::QuatToMatrix(rotQuat.x, rotQuat.y, rotQuat.z, rotQuat.w);
				if (sscanf(line, " matrix %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
					&xform[0][0], &xform[1][0], &xform[2][0], &xform[3][0],
					&xform[0][1], &xform[1][1], &xform[2][1], &xform[3][1],
					&xform[0][2], &xform[1][2], &xform[2][2], &xform[3][2],
					&xform[0][3], &xform[1][3], &xform[2][3], &xform[3][3]
				) != 0)
					matrixProvided = true;
			}

			if (strcmp(meshName, "none") != 0)
			{
				int meshID = scene->AddMesh(path + meshName);
				if (meshID != -1)
				{
					MeshInstance* meshInstance = new MeshInstance;

					meshInstance->meshID = meshID;
					meshInstance->name = meshName;

					if (materialMap.find(matName) != materialMap.end())
						meshInstance->materialID = materialMap[matName];
					else
						printf("Could not find material \"%s\". Using default material\n", matName);

					if (matrixProvided)
						meshInstance->transform = xform;
					else
						meshInstance->transform = scale * rotate * translate;

					scene->AddMeshInstance(meshInstance);
				}
			}
		}
	}


	return true;
}

NAMESPACE_END(nagi)