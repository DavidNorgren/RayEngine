#pragma once

#include "common.h"
#include "window.h"
#include "scene.h"

struct RayEngine {

	// Render modes
	enum RenderMode {
		RM_RAYTRACING, // Render using the ray tracing algorithm
		RM_OPENGL // Render using OpenGL shaders
	};

	// Ray tracing targets
	enum RayTracingTarget {
		RTT_CPU, // Use Embree for CPU ray tracing
		RTT_GPU, // Use OptiX for GPU ray tracing
		RTT_HYBRID // Use both Embree and OptiX for CPU and GPU ray tracing
	};

	RayEngine(int windowWidth = WINDOW_WIDTH,
			  int windowHeight = WINDOW_HEIGHT,
			  RenderMode renderMode = RENDER_MODE,
			  RayTracingTarget rayTracingTarget = RAY_TRACING_TARGET,
			  float hybridPartition = HYBRID_PARTITION);

	~RayEngine();

	// Adds a new, empty scene.
	Scene* createScene(string name);

	// Launches the program and starts rendering.
	void launch();

private:

	void loop();
	void resize();
	void resizeEmbree();
	void resizeOptix();
	void renderOpenGL();
	void renderEmbree();
	void renderEmbreeTexture();
	void renderOptix();
	void renderOptixTexture();
	void renderHybrid();
	void input();

	// Variables
	vector<Scene*> scenes;
	Scene* curScene;
	Camera* curCamera;
	Window window;
	Shader* shdrOGL;
	Shader* shdrTex;

	RenderMode renderMode;
	RayTracingTarget rayTracingTarget;
	float hybridPartition;
	bool showEmbreeRender, showOptixRender;

	// Embree
	struct {
		RTCDevice device;
		Color* buffer;
		GLuint texture;
		int offset, width;
	} EmbreeData;
	void initEmbree();

	// OptiX
	struct {
		optix::Context context;
		optix::Buffer buffer;
		GLuint vbo;
		GLuint texture;
		int offset, width;
	} OptixData;
	void initOptix();

};