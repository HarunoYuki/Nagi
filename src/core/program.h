#pragma once
#include <vector>
#include "glad.h"
#include "matrix.h"

NAMESPACE_BEGIN(nagi)

class Shader;

class Program
{
public:
	Program(const std::vector<Shader>& shaders);
	~Program() { glDeleteProgram(object); }

	void use() { glUseProgram(object); }
	void stop() { glUseProgram(0); }

	GLuint GetProgram() const { return object; }

	void setBool(const std::string &name, bool val) const { glUniform1i(glGetUniformLocation(object, name.c_str()), (int)val); }
	void setInt(const std::string &name, int val) const { glUniform1i(glGetUniformLocation(object, name.c_str()), val); }
	void setFloat(const std::string &name, float val) const { glUniform1f(glGetUniformLocation(object, name.c_str()), val); }
	void setVec2(const std::string &name, const vec2f &val) const { glUniform2fv(glGetUniformLocation(object, name.c_str()), 1, &val.x); }
	void setVec2(const std::string &name, float x, float y) const { glUniform2f(glGetUniformLocation(object, name.c_str()), x, y); }
	void setVec3(const std::string &name, const vec3f &val) const { glUniform3fv(glGetUniformLocation(object, name.c_str()), 1, &val.x); }
	void setVec3(const std::string &name, float x, float y, float z) const { glUniform3f(glGetUniformLocation(object, name.c_str()), x, y, z); }
	void setVec4(const std::string &name, const vec4f &val) const { glUniform4fv(glGetUniformLocation(object, name.c_str()), 1, &val.x); }
	void setVec4(const std::string &name, float x, float y, float z, float w) const { glUniform4f(glGetUniformLocation(object, name.c_str()), x, y, z, w); }
	//void setMat2(const std::string &name, const glm::mat2 &mat) const { glUniformMatrix2fv(glGetUniformLocation(object, name.c_str()), 1, GL_FALSE, &mat.data[0][0]); }
	//void setMat3(const std::string &name, const glm::mat3 &mat) const { glUniformMatrix3fv(glGetUniformLocation(object, name.c_str()), 1, GL_FALSE, &mat.data[0][0]); }
	void setMat4(const std::string &name, const mat4 &mat) const { glUniformMatrix4fv(glGetUniformLocation(object, name.c_str()), 1, GL_FALSE, &mat.data[0][0]); }

private:
	GLuint object;
};

NAMESPACE_END(nagi)