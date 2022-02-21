#include "camera.h"
#include "logging.h"
#include "input.h"

static constexpr float MOUSE_SENSITIVITY = 0.4f;
static constexpr float CAMERA_SPEED_VAL  = 1.5f;

void fps_camera_update(const WindowInfo& window, float deltaTime, FPSCamera* camera)
{
	static int prevMousePosX = static_cast<int>(window.windowExtent.width) / 2;
	static int prevMousePosY = static_cast<int>(window.windowExtent.height) / 2;

	static float pitch = 0.f;
	static float yaw   = 90.f;

	const float cameraSpeed = deltaTime * CAMERA_SPEED_VAL;
	
	MousePos currPos = get_mouse_position();

	MousePos delta = {prevMousePosX - currPos.x, prevMousePosY - currPos.y};
	pitch += (delta.y * MOUSE_SENSITIVITY);
	yaw   += (delta.x * MOUSE_SENSITIVITY);
	magma::log::debug("Pitch {} Yaw {}", pitch, yaw);
	
	pitch = clamp(pitch, -89.f, 89.f);

	Vec3 forward = {};
	forward.x = cos(toRad(pitch)) * cos(toRad(yaw));
	forward.y = sin(toRad(pitch));
	forward.z = -cos(toRad(pitch)) * sin(toRad(yaw));//look down to negative z axiz
	normaliseVec3InPlace(forward);
	camera->direction = forward;

	auto& position = camera->position;

	if(is_btn_pressed(KeyBoardBtn::W))
		position += cameraSpeed * camera->direction;
	if(is_btn_pressed(KeyBoardBtn::A))
		position += cameraSpeed * normaliseVec3(cross(Vec3{0.f, 1.f, 0.f}, camera->direction));
	if(is_btn_pressed(KeyBoardBtn::S))
		position -= cameraSpeed * camera->direction;
	if(is_btn_pressed(KeyBoardBtn::D))
		position -= cameraSpeed * normaliseVec3(cross(Vec3{0.f, 1.f, 0.f}, camera->direction));
	if(is_btn_pressed(KeyBoardBtn::Space))
		position += cameraSpeed * Vec3{0.f, 1.f, 0.f};
	if(is_btn_pressed(KeyBoardBtn::LeftCtrl))
		position -= cameraSpeed * Vec3{0.f, 1.f, 0.f};

	magma::log::debug("position: {}", position);

	camera->viewTransform = lookAt(camera->position, camera->position + camera->direction);
	prevMousePosX = currPos.x;
	prevMousePosY = currPos.y;

}