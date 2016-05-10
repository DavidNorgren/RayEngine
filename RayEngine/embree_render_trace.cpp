#include "rayengine.h"

// Stores the properties of a ray hit
struct RayHit {

	Color diffuse, specular;
	Vec3 pos, normal;
	Vec2 texCoord;
	Object* obj;
	TriangleMesh* mesh;
	Material* material;
	bool hitSky;

};

// Returns the color of missed rays
Color RayEngine::embreeRenderSky(Vec3 dir) {

	Vec3 nDir = Vec3::normalize(dir);
	float theta = atan2f(nDir.x(), nDir.z());
	float phi = M_PIf * 0.5f - acosf(nDir.y());
	float u = (theta + M_PIf) * (0.5f * M_1_PIf);
	float v = 0.5f * (1.0f + sin(phi));
	return curScene->sky->getPixel(Vec2(u, v));

}

void RayEngine::embreeRenderTraceRay(EmbreeData::Ray& ray, int depth, Color& result) {

	// No object hit, return sky color
	if (ray.geomID == RTC_INVALID_GEOMETRY_ID) {
		result = embreeRenderSky(ray.dir) + ray.transColor;
		return;
	}

	// Store hit
	RayHit hit;
	hit.diffuse = hit.specular = { 0.f };
	hit.pos = Vec3(ray.org) + Vec3(ray.dir) * ray.tfar;
	hit.obj = curScene->EmbreeData.instIDmap[ray.instID];
	hit.mesh = (TriangleMesh*)hit.obj->EmbreeData.geomIDmap[ray.geomID];
	hit.material = hit.mesh->material;
	hit.normal = Vec3::normalize(hit.obj->matrix * hit.mesh->getNormal(ray.primID, ray.u, ray.v));
	hit.texCoord = hit.mesh->getTexCoord(ray.primID, ray.u, ray.v);

	// Check lights
#if EMBREE_ONE_LIGHT
	Light& light = curScene->lights[0];
#else
	for (int l = 0; l < curScene->lights.size(); l++) {
		Light& light = curScene->lights[l];
 #endif

		float distance = Vec3::length(light.position - hit.pos);
		float attenuation = max(1.f - distance / light.range, 0.f);

		// Light is in reach
		if (attenuation > 0.f) {

			// Define light ray
			Vec3 incidence = Vec3::normalize(light.position - hit.pos);
			EmbreeData::Ray leRay;
			leRay.org[0] = hit.pos.x();
			leRay.org[1] = hit.pos.y();
			leRay.org[2] = hit.pos.z();
			leRay.dir[0] = incidence.x();
			leRay.dir[1] = incidence.y();
			leRay.dir[2] = incidence.z();
			leRay.tnear = 0.1f;
			leRay.tfar = distance;
			leRay.instID =
			leRay.geomID =
			leRay.primID = RTC_INVALID_GEOMETRY_ID;
			leRay.mask = EMBREE_RAY_VALID;
			leRay.time = 0.f;

			// Intersect
			rtcIntersect(curScene->EmbreeData.scene, leRay);

			// No object was hit, add contribution
			if (leRay.geomID == RTC_INVALID_GEOMETRY_ID) {

				// Diffuse factor
				float diffuseFactor = max(Vec3::dot(hit.normal, incidence), 0.f) * attenuation;
				hit.diffuse += diffuseFactor * light.color;

				// Specular factor
				if (hit.material->shininess > 0.0) {
					Vec3 toEye = Vec3::normalize(curCamera->position - hit.pos);
					Vec3 reflection = Vec3::reflect(incidence, hit.normal);
					float specularFactor = pow(max(Vec3::dot(reflection, toEye), 0.f), hit.material->shininess) * attenuation;
					hit.specular += specularFactor * hit.material->specular;
				}

			}

		}

#if !(EMBREE_ONE_LIGHT)
	}
#endif

	// Create color
	Color texColor = hit.material->diffuse * hit.material->image->getPixel(hit.texCoord);
	result = texColor * (curScene->ambient + hit.material->ambient + hit.diffuse) + hit.specular;

	// Add reflections
	if (depth < MAX_REFLECTIONS) { // TODO: Check material reflection

		Vec3 rayDir = Vec3::reflect(-Vec3(ray.dir), hit.normal);

		EmbreeData::Ray rRay;
		rRay.org[0] = hit.pos.x();
		rRay.org[1] = hit.pos.y();
		rRay.org[2] = hit.pos.z();
		rRay.dir[0] = rayDir.x();
		rRay.dir[1] = rayDir.y();
		rRay.dir[2] = rayDir.z();
		rRay.tnear = 0.1f;
		rRay.tfar = FLT_MAX;
		rRay.instID =
		rRay.geomID =
		rRay.primID = RTC_INVALID_GEOMETRY_ID;
		rRay.mask = EMBREE_RAY_VALID;
		rRay.time = 0.f;
		rRay.transColor = 1.f;

		rtcIntersect(curScene->EmbreeData.scene, rRay);

		Color reflectResult;
		embreeRenderTraceRay(rRay, depth + 1, reflectResult);

		result += reflectResult * 0.25f; // TODO: Use material reflectiveness

	}

}

// Stores a light ray
struct LightRay {
	Vec3 incidence;
	float distance, attenuation;
	Color lightColor;
};

void RayEngine::embreeRenderTracePacket(EmbreeData::RayPacket& packet, int* valid, int depth, Color* result) {
	
#if EMBREE_ONE_LIGHT

	EmbreeData::RayPacket lightPacket;
	LightRay lightRays[EMBREE_PACKET_SIZE];
	int lightValid[EMBREE_PACKET_SIZE];
	for (int i = 0; i < EMBREE_PACKET_SIZE; i++)
		lightValid[i] = EMBREE_RAY_INVALID;

#else

    #define MAX_LIGHTS 2

	// Create light packets (invalid by default)
	int numLights = min((int)curScene->lights.size(), MAX_LIGHTS - 1);
	EmbreeData::RayPacket lightPackets[MAX_LIGHTS];
	LightRay lightRays[MAX_LIGHTS][EMBREE_PACKET_SIZE];
	int lightValid[MAX_LIGHTS][EMBREE_PACKET_SIZE];

	for (int l = 0; l < MAX_LIGHTS; l++)
		for (int i = 0; i < EMBREE_PACKET_SIZE; i++)
			lightValid[l][i] = EMBREE_RAY_INVALID;

#endif

	// Reflection packet (invalid by default)
	bool doReflections = false;
	EmbreeData::RayPacket reflectPacket;
	int reflectValid[EMBREE_PACKET_SIZE];
	Color reflectResult[EMBREE_PACKET_SIZE];
	for (int i = 0; i < EMBREE_PACKET_SIZE; i++)
		reflectValid[i] = EMBREE_RAY_INVALID;
	
	//// Store hits ////
	
	RayHit hits[EMBREE_PACKET_SIZE];
	
	for (int i = 0; i < EMBREE_PACKET_SIZE; i++) {

		if (valid[i] == EMBREE_RAY_INVALID)
			continue;

		RayHit& hit = hits[i];
		Vec3 rayOrg = Vec3(packet.orgx[i], packet.orgy[i], packet.orgz[i]);
		Vec3 rayDir = Vec3(packet.dirx[i], packet.diry[i], packet.dirz[i]);

		if (packet.geomID[i] == RTC_INVALID_GEOMETRY_ID) {
			result[i] = embreeRenderSky(rayDir) + packet.transColor[i];
			hit.hitSky = true;
			continue;
		}

		// Store hit properties
		hit.diffuse = hit.specular = { 0.f };
		hit.pos = rayOrg + rayDir * packet.tfar[i];
		hit.obj = curScene->EmbreeData.instIDmap[packet.instID[i]];
		hit.mesh = (TriangleMesh*)hit.obj->EmbreeData.geomIDmap[packet.geomID[i]];
		hit.material = hit.mesh->material;
		hit.normal = Vec3::normalize(hit.obj->matrix * hit.mesh->getNormal(packet.primID[i], packet.u[i], packet.v[i]));
		hit.texCoord = hit.mesh->getTexCoord(packet.primID[i], packet.u[i], packet.v[i]);
		hit.hitSky = false;

		// Check lights
#if EMBREE_ONE_LIGHT
		Light& light = curScene->lights[0];
#else
		for (int l = 0; l < numLights; l++) {
			Light& light = curScene->lights[l];
#endif

			float distance = Vec3::length(light.position - hit.pos);
			float attenuation = max(1.f - distance / light.range, 0.f);

			// Light is in reach
			if (attenuation > 0.f) {

				// Define light ray
#if EMBREE_ONE_LIGHT
				LightRay& lRay = lightRays[i];
#else
				LightRay& lRay = lightRays[l][i];
#endif
				lRay.attenuation = attenuation;
				lRay.distance = distance;
				lRay.incidence = Vec3::normalize(light.position - hit.pos);
				lRay.lightColor = light.color;

#if EMBREE_ONE_LIGHT
				EMBREE_PACKET_TYPE& lPacket = lightPacket;
#else
				EMBREE_PACKET_TYPE& lPacket = lightPackets[l];
#endif
				lPacket.orgx[i] = hit.pos.x();
				lPacket.orgy[i] = hit.pos.y();
				lPacket.orgz[i] = hit.pos.z();
				lPacket.dirx[i] = lRay.incidence.x();
				lPacket.diry[i] = lRay.incidence.y();
				lPacket.dirz[i] = lRay.incidence.z();
				lPacket.tnear[i] = 0.1f;
				lPacket.tfar[i] = distance;
				lPacket.instID[i] =
				lPacket.geomID[i] =
				lPacket.primID[i] = RTC_INVALID_GEOMETRY_ID;
				lPacket.mask[i] = EMBREE_RAY_VALID;
				lPacket.time[i] = 0.f;

				// Turn on for occlusion test
#if EMBREE_ONE_LIGHT
				lightValid[i] = EMBREE_RAY_VALID;
#else
				lightValid[l][i] = EMBREE_RAY_VALID;
			}
#endif


		}

		// Create reflection rays
		if (depth < MAX_REFLECTIONS) { // TODO: Check material reflection

			Vec3 reflDir = Vec3::reflect(-rayDir, hit.normal);

			reflectPacket.orgx[i] = hit.pos.x();
			reflectPacket.orgy[i] = hit.pos.y();
			reflectPacket.orgz[i] = hit.pos.z();
			reflectPacket.dirx[i] = reflDir.x();
			reflectPacket.diry[i] = reflDir.y();
			reflectPacket.dirz[i] = reflDir.z();
			reflectPacket.tnear[i] = 0.1f;
			reflectPacket.tfar[i] = FLT_MAX;
			reflectPacket.instID[i] =
			reflectPacket.geomID[i] =
			reflectPacket.primID[i] = RTC_INVALID_GEOMETRY_ID;
			reflectPacket.mask[i] = EMBREE_RAY_VALID;
			reflectPacket.time[i] = 0.f;

			// Turn on for recursive call
			reflectValid[i] = EMBREE_RAY_VALID;
			doReflections = true;

		}
		
	}
	
	//// Light pass ////

#if EMBREE_ONE_LIGHT
	
	rtcOccluded8(lightValid, curScene->EmbreeData.scene, lightPacket);

	for (int i = 0; i < EMBREE_PACKET_SIZE; i++) {

		// Light ray was invalid or hit an object
		if (lightValid[i] == EMBREE_RAY_INVALID || lightPacket.geomID[i] != RTC_INVALID_GEOMETRY_ID)
			continue;

		LightRay& lRay = lightRays[i];

#else

	for (int l = 0; l < numLights; l++) {

		rtcOccluded8(lightValid[l], curScene->EmbreeData.scene, lightPackets[l]);

		for (int i = 0; i < EMBREE_PACKET_SIZE; i++) {

			// Light ray was invalid or hit an object
			if (lightValid[l][i] == EMBREE_RAY_INVALID || lightPackets[l].geomID[i] != RTC_INVALID_GEOMETRY_ID)
				continue;

			LightRay& lRay = lightRays[l][i];

#endif
			RayHit& hit = hits[i];

			// Diffuse factor
			float diffuseFactor = max(Vec3::dot(hit.normal, lRay.incidence), 0.f) * lRay.attenuation;
			hit.diffuse += diffuseFactor * lRay.lightColor;

			// Specular factor
			if (hit.material->shininess > 0.0) {
				Vec3 toEye = Vec3::normalize(curCamera->position - hit.pos);
				Vec3 reflection = Vec3::reflect(lRay.incidence, hit.normal);
				float specularFactor = pow(max(Vec3::dot(reflection, toEye), 0.f), hit.material->shininess) * lRay.attenuation;
				hit.specular += specularFactor * hit.material->specular;
			}

		}

#if !(EMBREE_ONE_LIGHT)
	}
#endif

	//// Reflections ////

	if (doReflections) {

		rtcIntersect8(reflectValid, curScene->EmbreeData.scene, reflectPacket);
		embreeRenderTracePacket(reflectPacket, reflectValid, depth + 1, reflectResult);

	}

	//// Calculate hit colors ////

	for (int i = 0; i < EMBREE_PACKET_SIZE; i++) {

		RayHit& hit = hits[i];

		if (valid[i] == EMBREE_RAY_INVALID || hit.hitSky)
			continue;

		// Create color
		Color texColor = hit.material->diffuse * hit.material->image->getPixel(hit.texCoord);
		result[i] = texColor * (curScene->ambient + hit.material->ambient + hit.diffuse) + hit.specular + packet.transColor[i];

		// Add reflections
		if (doReflections) // TODO: Check material reflection
			result[i] += reflectResult[i] * 0.25f; // TODO: Use material reflectiveness

	}
	
}