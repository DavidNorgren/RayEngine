#include "rayengine.h"

optix::Program materialClosestHitProgram, materialAnyHitProgram;

void printException(optix::Exception e) {

	cout << "OptiX error: " << e.getErrorString() << endl;
	system("pause");

}

void RayEngine::optixInit() {

	if (!OPTIX_ENABLE)
		return;

	cout << "Starting OptiX..." << endl;

	try {

		// Make context
		OptixData.context = optix::Context::create();
		OptixData.context->setRayTypeCount(2);
		OptixData.context->setEntryPointCount(1);
		OptixData.context->setStackSize(4096);

		// Generate texture
		glGenTextures(1, &OptixData.texture);
		glBindTexture(GL_TEXTURE_2D, OptixData.texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Make output buffer
#if OPTIX_USE_OUTPUT_VBO
		glGenBuffers(1, &OptixData.vbo);
		OptixData.renderBuffer = OptixData.context->createBufferFromGLBO(RT_BUFFER_OUTPUT, OptixData.vbo);
		OptixData.renderBuffer->setFormat(RT_FORMAT_FLOAT4);
		OptixData.renderBuffer->setSize(window.width, window.height);
#else
		OptixData.buffer = OptixData.context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4, window.width, window.height);
#endif

		// Make light buffer
		OptixData.lights = OptixData.context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER, curScene->lights.size());
		OptixData.lights->setElementSize(sizeof(Light));
		memcpy(OptixData.lights->map(), &curScene->lights[0], curScene->lights.size() * sizeof(Light));
		OptixData.lights->unmap();

		// Make ray generation program
		OptixData.context->setRayGenerationProgram(0, OptixData.context->createProgramFromPTXFile("ptx/camera_program.cu.ptx", "camera"));

		// Make miss program
		OptixData.context->setMissProgram(0, OptixData.context->createProgramFromPTXFile("ptx/miss_program.cu.ptx", "miss"));

		// Make material programs
		materialClosestHitProgram = OptixData.context->createProgramFromPTXFile("ptx/material_program.cu.ptx", "closestHit");
		materialAnyHitProgram = OptixData.context->createProgramFromPTXFile("ptx/material_program.cu.ptx", "anyHit");

		// Init scenes
		for (uint i = 0; i < scenes.size(); i++)
			scenes[i]->optixInit(OptixData.context);

		OptixData.context["sceneObj"]->set(curScene->OptixData.group);
		OptixData.context["sceneAmbient"]->setFloat(curScene->ambient.r(), curScene->ambient.g(), curScene->ambient.b(), curScene->ambient.a());
		OptixData.context["lights"]->set(OptixData.lights);
		OptixData.context["renderBuffer"]->set(OptixData.renderBuffer);
		OptixData.context["maxReflections"]->setInt(MAX_REFLECTIONS);

		// Compile
		OptixData.context->validate();
		OptixData.context->compile();

	} catch (optix::Exception e) {
		printException(e);
	}

}

void Scene::optixInit(optix::Context context) {

	// Group containing each object transform
	OptixData.group = context->createGroup();
	OptixData.group->setAcceleration(context->createAcceleration("Sbvh", "Bvh")); // TODO: change during runtime?

	for (uint i = 0; i < objects.size(); i++) {
		objects[i]->optixInit(context);
		OptixData.group->addChild(objects[i]->OptixData.transform);
	}

	// Sky texture sampler
#if OPTIX_USE_OPENGL_TEXTURE
	OptixData.sky = context->createTextureSamplerFromGLImage(sky->texture, RT_TARGET_GL_TEXTURE_2D);
#else
	optix::Buffer buf = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, sky->width, sky->height);
	memcpy(buf->map(), sky->pixels, sky->width * sky->height * sizeof(Color));
	buf->unmap();
	OptixData.sky = context->createTextureSampler();
	OptixData.sky->setArraySize(1);
	OptixData.sky->setMipLevelCount(1);
	OptixData.sky->setBuffer(0, 0, buf);
#endif
	OptixData.sky->setWrapMode(0, RT_WRAP_REPEAT);
	OptixData.sky->setWrapMode(1, RT_WRAP_REPEAT);
	OptixData.sky->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
	OptixData.sky->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
	OptixData.sky->setMaxAnisotropy(1.f);
	OptixData.sky->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);
	context["sky"]->setTextureSampler(OptixData.sky);


}

void Object::optixInit(optix::Context context) {

	try {
		
		// Make geometry group
		OptixData.geometryGroup = context->createGeometryGroup();
		OptixData.geometryGroup->setAcceleration(context->createAcceleration("Trbvh", "Bvh")); // TODO: change during runtime?


		// Add geometries
		for (uint i = 0; i < geometries.size(); i++) {
			geometries[i]->optixInit(context);
			OptixData.geometryGroup->addChild(((TriangleMesh*)geometries[i])->OptixData.geometryInstance);
		}

		// Make transform
		OptixData.transform = context->createTransform();
		OptixData.transform->setChild(OptixData.geometryGroup);
		OptixData.transform->setMatrix(true, matrix.e, NULL);

	} catch (optix::Exception e) {
		printException(e);
	}

}

void TriangleMesh::optixInit(optix::Context context) {

	try {

		static optix::Program intersectProgram = context->createProgramFromPTXFile("ptx/triangle_mesh_program.cu.ptx", "intersect");
		static optix::Program boundsProgram = context->createProgramFromPTXFile("ptx/triangle_mesh_program.cu.ptx", "bounds");

#if OPTIX_USE_GEOMETRY_VBO
		// Bind vertex VBO
		OptixData.posBuffer = context->createBufferFromGLBO(RT_BUFFER_INPUT, vboPos);
		OptixData.posBuffer->setFormat(RT_FORMAT_FLOAT3);
		OptixData.posBuffer->setSize(posData.size());

		// Bind normal VBO
		OptixData.normalBuffer = context->createBufferFromGLBO(RT_BUFFER_INPUT, vboNormal);
		OptixData.normalBuffer->setFormat(RT_FORMAT_FLOAT3);
		OptixData.normalBuffer->setSize(normalData.size());

		// Bind texture VBO
		OptixData.texCoordBuffer = context->createBufferFromGLBO(RT_BUFFER_INPUT, vboTexCoord);
		OptixData.texCoordBuffer->setFormat(RT_FORMAT_FLOAT2);
		OptixData.texCoordBuffer->setSize(texCoordData.size());

		// Bind index IBO
		OptixData.indexBuffer = context->createBufferFromGLBO(RT_BUFFER_INPUT, ibo);
		OptixData.indexBuffer->setFormat(RT_FORMAT_UNSIGNED_INT);
		OptixData.indexBuffer->setSize(indexData.size());
#else
		// Copy position buffer
		OptixData.posBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, posData.size());
		memcpy(OptixData.posBuffer->map(), &posData[0], posData.size() * sizeof(Vec3));
		OptixData.posBuffer->unmap();

		// Copy normal buffer
		OptixData.normalBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, normalData.size());
		memcpy(OptixData.normalBuffer->map(), &normalData[0], normalData.size() * sizeof(Vec3));
		OptixData.normalBuffer->unmap();

		// Copy texture buffer
		OptixData.texCoordBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, texCoordData.size());
		memcpy(OptixData.texCoordBuffer->map(), &texCoordData[0], texCoordData.size() * sizeof(Vec2));
		OptixData.texCoordBuffer->unmap();

		// Copy index buffer
		OptixData.indexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_INT3, indexData.size());
		memcpy(OptixData.indexBuffer->map(), &indexData[0], indexData.size() * sizeof(TrianglePrimitive));
		OptixData.indexBuffer->unmap();
#endif

		// Make geometry
		OptixData.geometry = context->createGeometry();
		OptixData.geometry->setIntersectionProgram(intersectProgram);
		OptixData.geometry->setBoundingBoxProgram(boundsProgram);
		OptixData.geometry->setPrimitiveCount(indexData.size());
		OptixData.geometry["posData"]->setBuffer(OptixData.posBuffer);
		OptixData.geometry["normalData"]->setBuffer(OptixData.normalBuffer);
		OptixData.geometry["texCoordData"]->setBuffer(OptixData.texCoordBuffer);
		OptixData.geometry["indexData"]->setBuffer(OptixData.indexBuffer);

		// Make instance
		if (!material->OptixData.material) {
			
#if OPTIX_USE_OPENGL_TEXTURE
			material->OptixData.sampler = context->createTextureSamplerFromGLImage(material->image->texture, RT_TARGET_GL_TEXTURE_2D);
#else
			optix::Buffer buf = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, material->image->width, material->image->height);
			memcpy(buf->map(), material->image->pixels, material->image->width * material->image->height * sizeof(Color));
			buf->unmap();
			material->OptixData.sampler = context->createTextureSampler();
			material->OptixData.sampler->setArraySize(1);
			material->OptixData.sampler->setMipLevelCount(1);
			material->OptixData.sampler->setBuffer(0, 0, buf);
#endif
			material->OptixData.sampler->setWrapMode(0, RT_WRAP_REPEAT);
			material->OptixData.sampler->setWrapMode(1, RT_WRAP_REPEAT);
			material->OptixData.sampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
			material->OptixData.sampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
			material->OptixData.sampler->setMaxAnisotropy(1.f);
			RTfiltermode filter = (material->image->filter == GL_NEAREST) ? RT_FILTER_NEAREST : RT_FILTER_LINEAR;
			material->OptixData.sampler->setFilteringModes(filter, filter, RT_FILTER_NONE);
			
			material->OptixData.material = context->createMaterial();
			material->OptixData.material->setClosestHitProgram(0, materialClosestHitProgram);
			material->OptixData.material->setAnyHitProgram(1, materialAnyHitProgram);
			material->OptixData.material["sampler"]->setTextureSampler(material->OptixData.sampler);
			material->OptixData.material["ambient"]->setFloat(material->ambient.r(), material->ambient.g(), material->ambient.b(), material->ambient.a());
			material->OptixData.material["specular"]->setFloat(material->specular.r(), material->specular.g(), material->specular.b(), material->specular.a());
			material->OptixData.material["diffuse"]->setFloat(material->diffuse.r(), material->diffuse.g(), material->diffuse.b(), material->diffuse.a());
			material->OptixData.material["shininess"]->setFloat(material->shininess);

		}

		OptixData.geometryInstance = context->createGeometryInstance();
		OptixData.geometryInstance->setGeometry(OptixData.geometry);
		OptixData.geometryInstance->addMaterial(material->OptixData.material);

	} catch (optix::Exception e) {
		printException(e);
	}

}

void RayEngine::optixResize() {

	if (!OPTIX_ENABLE)
		return;

	// Set dimensions
	if (rayTracingTarget == RTT_HYBRID) {
		OptixData.offset = EmbreeData.width;
		OptixData.width = window.width - OptixData.offset;
	} else {
		OptixData.offset = 0;
		OptixData.width = window.width;
	}

	// Resize buffer object
	OptixData.renderBuffer->setSize(OptixData.width, window.height);

	// Resize VBO
#if OPTIX_USE_OUTPUT_VBO
	OptixData.renderBuffer->unregisterGLBuffer();
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, OptixData.vbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(float) * 4 * OptixData.width * window.height, 0, GL_STREAM_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	OptixData.renderBuffer->registerGLBuffer();
#endif

	// Resize texture
	glBindTexture(GL_TEXTURE_2D, OptixData.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, OptixData.width, window.height, 0, GL_RGBA, GL_FLOAT, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

}