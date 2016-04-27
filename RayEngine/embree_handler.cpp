#include "rayengine.h"
#include "triangle_mesh.h"
#include "object.h"

void RayEngine::initEmbree() {

	EmbreeData.device = rtcNewDevice(NULL);
	EmbreeData.buffer = nullptr;
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

	// Init scenes
	for (uint i = 0; i < scenes.size(); i++)
		scenes[i]->initEmbree(EmbreeData.device);

	// Generate texture
	glGenTextures(1, &EmbreeData.texture);
	glBindTexture(GL_TEXTURE_2D, EmbreeData.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

}

void Scene::initEmbree(RTCDevice device) {

	EmbreeData.scene = rtcDeviceNewScene(device,
										 RTC_SCENE_DYNAMIC | RTC_SCENE_COMPACT,
										 RTC_INTERSECT1 | RTC_INTERSECT4 | RTC_INTERSECT8);

	//TODO: Check other instance modes?
	for (uint i = 0; i < objects.size(); i++) {
		objects[i]->initEmbree(device);
		objects[i]->EmbreeData.instID = rtcNewInstance2(EmbreeData.scene, objects[i]->EmbreeData.scene);
	}

	rtcCommit(EmbreeData.scene);

}

void Object::initEmbree(RTCDevice device) {

	// Init embree for mesh and all the children
	EmbreeData.scene = rtcDeviceNewScene(device,
										 RTC_SCENE_STATIC | RTC_SCENE_COMPACT,
										 RTC_INTERSECT1 | RTC_INTERSECT4 | RTC_INTERSECT8);
	
	for (uint i = 0; i < geometries.size(); i++)
		geometries[i]->initEmbree(EmbreeData.scene);

	rtcCommit(EmbreeData.scene);

}

void TriangleMesh::initEmbree(RTCScene scene) {

	EmbreeData.geomID = rtcNewTriangleMesh(scene, RTC_GEOMETRY_STATIC, indexData.size(), posData.size());
	rtcSetBuffer(scene, EmbreeData.geomID, RTC_INDEX_BUFFER, &indexData[0], 0, sizeof(TrianglePrimitive));
	rtcSetBuffer(scene, EmbreeData.geomID, RTC_VERTEX_BUFFER, &posData[0], 0, sizeof(Vec3));

}

void RayEngine::resizeEmbree() {

	if (EmbreeData.buffer)
		delete EmbreeData.buffer;

	EmbreeData.buffer = new Color[window.width * window.height];

}