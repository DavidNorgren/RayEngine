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
		Optix.context = optix::Context::create();
		Optix.context->setRayTypeCount(2);
		Optix.context->setEntryPointCount(1);
		Optix.context->setStackSize(4096);
		Optix.frames = 0;
		Optix.lastTime = 0.f;
		Optix.avgTime = 0.f;

		// Generate texture
		glGenTextures(1, &Optix.texture);
		glBindTexture(GL_TEXTURE_2D, Optix.texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Make output buffer
#if OPTIX_USE_OUTPUT_VBO
		glGenBuffers(1, &Optix.vbo);
		Optix.renderBuffer = Optix.context->createBufferFromGLBO(RT_BUFFER_OUTPUT, Optix.vbo);
		Optix.renderBuffer->setFormat(RT_FORMAT_FLOAT4);
		Optix.renderBuffer->setSize(window.width, window.height);
#else
		Optix.buffer = Optix.context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4, window.width, window.height);
#endif

		// Make light buffer
		Optix.lights = Optix.context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER, curScene->lights.size());
		Optix.lights->setElementSize(sizeof(Light));
		memcpy(Optix.lights->map(), &curScene->lights[0], curScene->lights.size() * sizeof(Light));
		Optix.lights->unmap();

		// Make ray generation program
		Optix.context->setRayGenerationProgram(0, Optix.context->createProgramFromPTXFile("ptx/camera_program.cu.ptx", "camera"));

		// Make miss program
		Optix.context->setMissProgram(0, Optix.context->createProgramFromPTXFile("ptx/miss_program.cu.ptx", "miss"));

		// Make material programs
		materialClosestHitProgram = Optix.context->createProgramFromPTXFile("ptx/material_program.cu.ptx", "closestHit");
		materialAnyHitProgram = Optix.context->createProgramFromPTXFile("ptx/material_program.cu.ptx", "anyHit");

		// Init scenes
		for (uint i = 0; i < scenes.size(); i++)
			scenes[i]->optixInit(Optix.context);

		Optix.context["sceneObj"]->set(curScene->Optix.group);
		Optix.context["sceneAmbient"]->setFloat(curScene->ambient.r(), curScene->ambient.g(), curScene->ambient.b(), curScene->ambient.a());
		Optix.context["lights"]->set(Optix.lights);
		Optix.context["renderBuffer"]->set(Optix.renderBuffer);

		// Compile
		Optix.context->validate();
		Optix.context->compile();

	} catch (optix::Exception e) {
		printException(e);
	}

}

void Scene::optixInit(optix::Context context) {

	// Group containing each object transform
	Optix.group = context->createGroup();
	Optix.group->setAcceleration(context->createAcceleration("Sbvh", "Bvh")); // TODO: change during runtime?

	for (uint i = 0; i < objects.size(); i++) {
		objects[i]->optixInit(context);
		Optix.group->addChild(objects[i]->Optix.transform);
	}

	// Sky texture sampler
#if OPTIX_USE_OPENGL_TEXTURE
	Optix.sky = context->createTextureSamplerFromGLImage(sky->texture, RT_TARGET_GL_TEXTURE_2D);
#else
	optix::Buffer buf = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, sky->width, sky->height);
	memcpy(buf->map(), sky->pixels, sky->width * sky->height * sizeof(Color));
	buf->unmap();
	Optix.sky = context->createTextureSampler();
	Optix.sky->setArraySize(1);
	Optix.sky->setMipLevelCount(1);
	Optix.sky->setBuffer(0, 0, buf);
#endif
	Optix.sky->setWrapMode(0, RT_WRAP_REPEAT);
	Optix.sky->setWrapMode(1, RT_WRAP_REPEAT);
	Optix.sky->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
	Optix.sky->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
	Optix.sky->setMaxAnisotropy(1.f);
	Optix.sky->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);
	context["sky"]->setTextureSampler(Optix.sky);

}

void Object::optixInit(optix::Context context) {

	try {
		
		// Make geometry group
		Optix.geometryGroup = context->createGeometryGroup();
		Optix.geometryGroup->setAcceleration(context->createAcceleration("Trbvh", "Bvh")); // TODO: change during runtime?


		// Add geometries
		for (uint i = 0; i < geometries.size(); i++) {
			geometries[i]->optixInit(context);
			Optix.geometryGroup->addChild(((TriangleMesh*)geometries[i])->Optix.geometryInstance);
		}

		// Make transform
		Optix.transform = context->createTransform();
		Optix.transform->setChild(Optix.geometryGroup);
		Optix.transform->setMatrix(true, matrix.e, NULL);

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
		Optix.posBuffer = context->createBufferFromGLBO(RT_BUFFER_INPUT, vboPos);
		Optix.posBuffer->setFormat(RT_FORMAT_FLOAT3);
		Optix.posBuffer->setSize(posData.size());

		// Bind normal VBO
		Optix.normalBuffer = context->createBufferFromGLBO(RT_BUFFER_INPUT, vboNormal);
		Optix.normalBuffer->setFormat(RT_FORMAT_FLOAT3);
		Optix.normalBuffer->setSize(normalData.size());

		// Bind texture VBO
		Optix.texCoordBuffer = context->createBufferFromGLBO(RT_BUFFER_INPUT, vboTexCoord);
		Optix.texCoordBuffer->setFormat(RT_FORMAT_FLOAT2);
		Optix.texCoordBuffer->setSize(texCoordData.size());

		// Bind index IBO
		Optix.indexBuffer = context->createBufferFromGLBO(RT_BUFFER_INPUT, ibo);
		Optix.indexBuffer->setFormat(RT_FORMAT_UNSIGNED_INT);
		Optix.indexBuffer->setSize(indexData.size());
#else
		// Copy position buffer
		Optix.posBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, posData.size());
		memcpy(Optix.posBuffer->map(), &posData[0], posData.size() * sizeof(Vec3));
		Optix.posBuffer->unmap();

		// Copy normal buffer
		Optix.normalBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, normalData.size());
		memcpy(Optix.normalBuffer->map(), &normalData[0], normalData.size() * sizeof(Vec3));
		Optix.normalBuffer->unmap();

		// Copy texture buffer
		Optix.texCoordBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, texCoordData.size());
		memcpy(Optix.texCoordBuffer->map(), &texCoordData[0], texCoordData.size() * sizeof(Vec2));
		Optix.texCoordBuffer->unmap();

		// Copy index buffer
		Optix.indexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_INT3, indexData.size());
		memcpy(Optix.indexBuffer->map(), &indexData[0], indexData.size() * sizeof(TrianglePrimitive));
		Optix.indexBuffer->unmap();
#endif

		// Make geometry
		Optix.geometry = context->createGeometry();
		Optix.geometry->setIntersectionProgram(intersectProgram);
		Optix.geometry->setBoundingBoxProgram(boundsProgram);
		Optix.geometry->setPrimitiveCount(indexData.size());
		Optix.geometry["posData"]->setBuffer(Optix.posBuffer);
		Optix.geometry["normalData"]->setBuffer(Optix.normalBuffer);
		Optix.geometry["texCoordData"]->setBuffer(Optix.texCoordBuffer);
		Optix.geometry["indexData"]->setBuffer(Optix.indexBuffer);

		// Make instance
		if (!material->Optix.material) {
			
#if OPTIX_USE_OPENGL_TEXTURE
			material->Optix.sampler = context->createTextureSamplerFromGLImage(material->image->texture, RT_TARGET_GL_TEXTURE_2D);
#else
			optix::Buffer buf = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, material->image->width, material->image->height);
			memcpy(buf->map(), material->image->pixels, material->image->width * material->image->height * sizeof(Color));
			buf->unmap();
			material->Optix.sampler = context->createTextureSampler();
			material->Optix.sampler->setArraySize(1);
			material->Optix.sampler->setMipLevelCount(1);
			material->Optix.sampler->setBuffer(0, 0, buf);
#endif
			material->Optix.sampler->setWrapMode(0, RT_WRAP_REPEAT);
			material->Optix.sampler->setWrapMode(1, RT_WRAP_REPEAT);
			material->Optix.sampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
			material->Optix.sampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
			material->Optix.sampler->setMaxAnisotropy(1.f);
			RTfiltermode filter = (material->image->filter == GL_NEAREST) ? RT_FILTER_NEAREST : RT_FILTER_LINEAR;
			material->Optix.sampler->setFilteringModes(filter, filter, RT_FILTER_NONE);
			
			material->Optix.material = context->createMaterial();
			material->Optix.material->setClosestHitProgram(0, materialClosestHitProgram);
			material->Optix.material->setAnyHitProgram(1, materialAnyHitProgram);
			material->Optix.material["sampler"]->setTextureSampler(material->Optix.sampler);
			material->Optix.material["ambient"]->setFloat(material->ambient.r(), material->ambient.g(), material->ambient.b(), material->ambient.a());
			material->Optix.material["specular"]->setFloat(material->specular.r(), material->specular.g(), material->specular.b(), material->specular.a());
			material->Optix.material["diffuse"]->setFloat(material->diffuse.r(), material->diffuse.g(), material->diffuse.b(), material->diffuse.a());
			material->Optix.material["shininess"]->setFloat(material->shininess);

		}

		Optix.geometryInstance = context->createGeometryInstance();
		Optix.geometryInstance->setGeometry(Optix.geometry);
		Optix.geometryInstance->addMaterial(material->Optix.material);

	} catch (optix::Exception e) {
		printException(e);
	}

}

void RayEngine::optixResize() {

	if (!OPTIX_ENABLE)
		return;

	// Set dimensions
	if (renderMode == RM_HYBRID) {
		Optix.offset = Embree.width;
		Optix.width = window.width - Optix.offset;
	} else {
		Optix.offset = 0;
		Optix.width = window.width;
	}

	if (Optix.width == 0)
		return;

	// Resize buffer object
	Optix.renderBuffer->setSize(Optix.width, window.height);

	// Resize VBO
#if OPTIX_USE_OUTPUT_VBO
	Optix.renderBuffer->unregisterGLBuffer();
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, Optix.vbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(float) * 4 * Optix.width * window.height, 0, GL_STREAM_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	Optix.renderBuffer->registerGLBuffer();
#endif

	// Resize texture
	glBindTexture(GL_TEXTURE_2D, Optix.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Optix.width, window.height, 0, GL_RGBA, GL_FLOAT, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

}