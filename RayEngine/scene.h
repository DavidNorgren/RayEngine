#pragma once

#include "common.h"
#include "camera.h"
#include "object.h"
#include "light.h"

struct Scene {

	Scene(string name, Color ambient, Color background);

	// Loads object(s) from a file and adds it to the scene.
	Object* loadObject(string file);

	// Adds a new customizable object.
	Object* addObject(Geometry* geometry = nullptr);

	// Adds a new light.
	void addLight(Vec3 position, Color color, float range);

	// Variables
	string name;
	Camera camera;
	vector<Object*> objects;
	vector<Light> lights;
	Color ambient, background;

	// Embree
	struct EmbreeData {
		RTCScene scene;
		map<uint, Object*> instIDmap;
	} EmbreeData;
	void initEmbree(RTCDevice device);

	// OptiX
	struct OptixData {
		optix::Group group;
	} OptixData;
	void initOptix(optix::Context context);

};
