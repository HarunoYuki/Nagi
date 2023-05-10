#version 330

/*
	uniforms declaration
*/

// Renderer::InitShaders()
uniform vec2 envMapRes;
uniform float envMapTotalSum;
uniform vec2 resolution;
uniform vec2 invTilesNum;
uniform int lightsNum;
uniform int tlasBVHStartOffset;
uniform sampler2D accumTex;
uniform samplerBuffer BVHTex;
uniform isamplerBuffer vertexIndicesTex;
uniform samplerBuffer verticesTex;
uniform samplerBuffer normalsTex;
uniform sampler2D materialsTex;
uniform sampler2D transformsTex;
uniform sampler2D lightsTex;
uniform sampler2DArray textureMapsArrayTex;
uniform sampler2D envMapTex;
uniform sampler2D envMapCDFTex;

// 
uniform float envMapIntensity;
uniform float envMapRot;
uniform vec3 uniformLightCol;
uniform int maxDepth;
uniform vec2 tileOffset;
uniform int frameNum;
uniform float roughnessMollificationAmt;