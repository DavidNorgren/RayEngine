#include "common.cuh"

rtBuffer<float4, 2> renderBuffer;
rtDeclareVariable(float, offset, , );
rtDeclareVariable(float, windowWidth, , );
rtDeclareVariable(float3, eye, , );
rtDeclareVariable(float3, xaxis, , );
rtDeclareVariable(float3, yaxis, , );
rtDeclareVariable(float3, zaxis, , );
rtDeclareVariable(rtObject, sceneObj, , );

rtDeclareVariable(uint2, launchIndex, rtLaunchIndex, );
rtDeclareVariable(uint2, launchDim, rtLaunchDim, );

RT_PROGRAM void camera() {

	float2 d = (make_float2(offset + launchIndex.x, launchIndex.y) / make_float2(windowWidth, launchDim.y)) * 2.f - 1.f;
	float3 rayOrg = eye;
	float3 rayDir = d.x * xaxis + d.y * yaxis + zaxis;

	Ray ray = make_Ray(rayOrg, rayDir, 0, 0.01f, RT_DEFAULT_MAX);

	RayColorData data;
	data.depth = 0;
	rtTrace(sceneObj, ray, data);

	renderBuffer[launchIndex] = data.result;
}