#include "path.h"

Path::Path(float time) : time(time), progress(0.f) {}
LinePath::LinePath(Vec3 start, Vec3 end, float time) : start(start), end(end), Path(time) {}
CirclePath::CirclePath(Vec3 position, float radius, float angle, float time) : position(position), radius(radius), angle(angle), Path(time) {}
LookPath::LookPath(float time) : Path(time) {}

void LinePath::update(Camera& camera, bool animate) {

	static float progress = 0.f;
	if (animate)
		progress = glfwGetTime() / time;
	camera.position = start + (end - start) * min(progress, 1.f);

}

void CirclePath::update(Camera& camera, bool animate) {

	static float progress = 0.f;
	if (animate)
		progress = glfwGetTime() / time;

	float a = progress * M_PIf * 2.f;
	camera.position = {
		radius * cos(a) * cos(angle),
		radius * sin(angle),
		radius * sin(a) * cos(angle)
	};

	camera.zaxis = Vec3::normalize(position - camera.position);
	camera.xaxis = -Vec3::normalize(Vec3::cross({ 0.f, 1.f, 0.f }, camera.zaxis));
	camera.yaxis = -Vec3::cross(camera.zaxis, camera.xaxis);


}

void LookPath::update(Camera& camera, bool animate) {

	static float progress = 0.f;
	if (animate)
		progress = glfwGetTime() / time;

	float a = progress * M_PIf * 2.f;
	camera.xaxis = { cos(a), 0.f, -sin(a) };
	camera.yaxis = { 0.f,    1.f, 0.f };
	camera.zaxis = { -sin(a), 0.f, -cos(a) };

}