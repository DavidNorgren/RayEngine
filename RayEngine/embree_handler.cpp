#include "rayengine.h"
#include "triangle_mesh.h"
#include "object.h"

void RayEngine::embreeInit() {

	cout << "Starting Embree..." << endl;

	// Init library
	Embree.frames = 0;
	Embree.lastTime = 0.f;
	Embree.avgTime = 0.f;
	Embree.device = rtcNewDevice(NULL);
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

	// Generate texture
	glGenTextures(1, &Embree.texture);
	glBindTexture(GL_TEXTURE_2D, Embree.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Init scenes
	for (uint i = 0; i < scenes.size(); i++)
		scenes[i]->embreeInit(Embree.device);

}

void Scene::embreeInit(RTCDevice device) {

	Embree.scene = rtcDeviceNewScene(device, EMBREE_SFLAGS_SCENE, EMBREE_AFLAGS_SCENE);

	for (uint i = 0; i < objects.size(); i++) {
		objects[i]->embreeInit(device);
		uint instID = rtcNewInstance2(Embree.scene, objects[i]->Embree.scene);
		rtcSetTransform2(Embree.scene, instID, RTC_MATRIX_COLUMN_MAJOR_ALIGNED16, objects[i]->matrix.e);
		Embree.instIDmap[instID] = objects[i];
	}

	rtcCommit(Embree.scene);

}

void TransparencyIntersectionFilter8(const void* valid, void *ptr, RayEngine::Embree::RayPacket &packet) {

	// We hit a transparent object, add contribution to the ray

	int* valids = (int*)valid;
	for (int i = 0; i < 8; i++) {

		if (!valids[i])
			continue;

		packet.instID[i] = packet.geomID[i] = packet.primID[i] = RTC_INVALID_GEOMETRY_ID;
		packet.transColor[i] = 0.5f;

	}
}

void TransparencyIntersectionFilter(void *ptr, RayEngine::Embree::Ray &ray) {

	// We hit a transparent object, add contribution to the ray

	ray.instID = ray.geomID = ray.primID = RTC_INVALID_GEOMETRY_ID;
	ray.transColor += 0.5;
}

void Object::embreeInit(RTCDevice device) {

	Embree.scene = rtcDeviceNewScene(device, EMBREE_SFLAGS_OBJECT, EMBREE_AFLAGS_OBJECT);

	// Init embree for meshes
	for (uint i = 0; i < geometries.size(); i++) { // If it crashes here, then it can't find the .mtl or the last line is not empty

		uint geomID = geometries[i]->embreeInit(Embree.scene);
		Embree.geomIDmap[geomID] = geometries[i];

		// Set filter functions
		/*rtcSetIntersectionFilterFunction(Embree.scene, geomID, (RTCFilterFunc)&TransparencyIntersectionFilter);
		rtcSetIntersectionFilterFunction8(Embree.scene, geomID, (RTCFilterFunc8)&TransparencyIntersectionFilter8);
		rtcSetOcclusionFilterFunction(Embree.scene, geomID, (RTCFilterFunc)&TransparencyIntersectionFilter);
		rtcSetOcclusionFilterFunction8(Embree.scene, geomID, (RTCFilterFunc8)&TransparencyIntersectionFilter8);*/

	}

	rtcCommit(Embree.scene);

}

uint TriangleMesh::embreeInit(RTCScene scene) {

	uint geomID = rtcNewTriangleMesh(scene, RTC_GEOMETRY_STATIC, indexData.size(), posData.size());
	rtcSetBuffer(scene, geomID, RTC_VERTEX_BUFFER, &posData[0], 0, sizeof(Vec3));
	rtcSetBuffer(scene, geomID, RTC_INDEX_BUFFER, &indexData[0], 0, sizeof(TrianglePrimitive));
	return geomID;

}

void RayEngine::embreeResize() {

	// Set dimensions
	if (renderMode == RM_HYBRID) {
		Embree.offset = 0;
		Embree.width = ceil(window.width * Hybrid.partition);
	} else {
		Embree.offset = 0;
		Embree.width = window.width;
	}

	if (Embree.width == 0)
		return;

	// Resize buffer
	Embree.buffer.resize(Embree.width * window.height);

	// Resize texture
	glBindTexture(GL_TEXTURE_2D, Embree.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Embree.width, window.height, 0, GL_RGBA, GL_FLOAT, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

}