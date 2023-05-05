#include "environmentMap.h"
#include "stb_image.h"

NAMESPACE_BEGIN(nagi)

EnvironmentMap::~EnvironmentMap()
{
	stbi_image_free(img); 
	delete[] cdf;
}

bool EnvironmentMap::LoadEnvMap(std::string & filename)
{
	img = stbi_loadf(filename.c_str(), &width, &height, NULL, 3);

	if (!img)
		return false;

	BuildCDF();

	return true;
}

// PBRT-V3 325\328 pages. RGBToXYZ()::xyz[1]
float Luminance(float r, float g, float b)
{
	return 0.212671f * r + 0.715160f * g + 0.072169f * b;
}

// https://pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Light_Sources#InfiniteAreaLights
void EnvironmentMap::BuildCDF()
{
	size_t pixels = width * height;
	float* weights = new float[pixels];
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; j++)
		{
			size_t imgIdx = 3 * (i*width + j);
			weights[i*width + j] = Luminance(img[imgIdx + 0], img[imgIdx + 1], img[imgIdx + 2]);
		}

	cdf = new float[pixels];
	cdf[0] = weights[0];
	for (size_t i = 1; i < pixels; i++)
		cdf[i] = cdf[i - 1] + weights[i];

	totalSum = cdf[pixels - 1];
	delete[] weights;
}


NAMESPACE_END(nagi)