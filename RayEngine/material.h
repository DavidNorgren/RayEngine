#pragma once

#include "common.h"
#include "image.h"

struct Material {

	Material();

	Color ambient, specular, diffuse;
	float shininess;
	Image* image;

	struct {
		optix::Material material;
		optix::TextureSampler sampler;
	} OptixData;

	/*int id;

	std::string name;
	//  Type of the variables [ka], [kd], [ks], [kt], [ke] could/should be changed to [color]/[rgb].
	float ka[3];	// Ambient reflectivity // RGB format presumed
	float kd[3];	// Diffuse reflectivity // RGB format presumed
	float ks[3];	// Specular reflectivity // RGB format presumed
	float kt[3];	// Transmittance // Transmission filter	// RGB format presumed
	float ke[3];	// Emission	// RGB format presumed
	float ns;		// Shininess // Specular exponent
	float ni;		// Index of refraction // Optical density of the surface // in range [0.001, 10] // deafult: 1.0
	float dissolve;	// Dissolve factor	// in range [0.0, 1.0] -> transparent - opaque	// default: 1.0
	int sharpness;	// Sharpness of the reflections // in range [0, 1000]	// default: 60*/

};
