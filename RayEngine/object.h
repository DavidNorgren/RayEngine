#pragma once

#include "common.h"
#include "geometry.h"
#include "triangle_mesh.h"
#include "material.h"

struct Object {

	// Constructor
	Object();
	~Object();

	// Loads object(s) from a file.
	static Object* load(string file);

	// Translates the object by a vector.
	void translate(Vec3 vector);

	// Rotates the object by a vector and angle.
	void rotate(Vec3 vector, float angle);

	// Scales the object by a vector.
	void scale(Vec3 vector);

	// Variables
	vector<Geometry*> geometries;
	Mat4x4 matrix;

	// Embree
	struct Embree {
		RTCScene scene;
		map<uint, Geometry*> geomIDmap;
	} Embree;
	void embreeInit(RTCDevice device);

	// OptiX
	struct Optix{
		optix::Transform transform;
		optix::GeometryGroup geometryGroup;
	} Optix;
	void optixInit(optix::Context context);

};
