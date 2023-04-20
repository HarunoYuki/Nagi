#pragma once
#include "vector.h"

NAMESPACE_BEGIN(nagi)

class Material
{
public:
	enum AlphaMode { Opaque, Blend, Mask };
	enum MediumType { None, Absorb, Scatter, Emissive };

	Material() {
		baseColor = vec3f(1.0f, 1.0f, 1.0f);
		anisotropic = 0.0f;

		emission = vec3f(0.0f, 0.0f, 0.0f);
		// padding1

		metallic = 0.0f;
		roughness = 0.5f;
		subsurface = 0.0f;
		specularTint = 0.0f;

		sheen = 0.0f;
		sheenTint = 0.0f;
		clearcoat = 0.0f;
		clearcoatGloss = 0.0f;

		specTrans = 0.0f;
		ior = 1.5f;
		mediumType = MediumType::None;
		mediumDensity = 0.0f;

		mediumColor = vec3f(1.0f, 1.0f, 1.0f);
		mediumAnisotropy = 0.0f;

		baseColorTexID = -1.0f;
		roughnessTexID = -1.0f;
		metallicTexID = -1.0f;
		normalMapTexID = -1.0f;
		emissionMapTexID = -1.0f;
		opacity = 1.0f;
		alphaMode = AlphaMode::Opaque;
		alphaCutoff = 0.0f;
	}

	vec3f baseColor;
	float anisotropic;

	vec3f emission;
	float padding1;

	float metallic;
	float roughness;
	float subsurface;
	float specularTint;

	float sheen;
	float sheenTint;
	float clearcoat;
	float clearcoatGloss;

	float specTrans;
	float ior;
	float mediumType;
	float mediumDensity;

	vec3f mediumColor;
	float mediumAnisotropy;

	float baseColorTexID;
	float roughnessTexID;
	float metallicTexID;
	float normalMapTexID;

	float emissionMapTexID;
	float opacity;
	float alphaMode;
	float alphaCutoff;
};


NAMESPACE_END(nagi)