#include "texture.h"
#include "stb_image.h"

NAMESPACE_BEGIN(nagi)

Texture::Texture(std::string & filename, unsigned char * data, int w, int h, int c):
	name(filename),width(w),height(h),components(c)
{
	texData.resize(width*height*components);
	std::copy(data, data + width * height * components, texData.begin());
}

bool Texture::LoadTexture(std::string & filename)
{
	name = filename;
	components = 4;
	unsigned char* data = stbi_load(filename.c_str(), &width, &height, NULL, 4);
	if (!data)
		return false;

	texData.resize(width*height*components);
	std::copy(data, data + width * height * components, texData.begin());
	stbi_image_free(data);

	return true;
}



NAMESPACE_END(nagi)