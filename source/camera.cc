#include "camera.h"
#include "logging.h"
#include "input.h"

static constexpr float MOUSE_SENSITIVITY = 0.4f;
static constexpr float CAMERA_SPEED_VAL  = 1.5f;

void fpsCameraUpdate(const WindowInfo& window, float deltaTime, FPSCamera* camera)
{
	static const uint32_t centerX = window.windowExtent.width / 2;
	static const uint32_t centerY = window.windowExtent.height / 2;
	static float pitch = 0.f;
	static float yaw   = 90.f;

	const float cameraSpeed = deltaTime * CAMERA_SPEED_VAL;
	
	MousePos currPos = getMousePos();
	MousePos delta = {centerX - currPos.x, centerY - currPos.y};
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

	if(isBtnPressed(KeyBoardBtn::W))
		position += cameraSpeed * camera->direction;
	if(isBtnPressed(KeyBoardBtn::A))
		position += cameraSpeed * normaliseVec3(cross(Vec3{0.f, 1.f, 0.f}, camera->direction));
	if(isBtnPressed(KeyBoardBtn::S))
		position -= cameraSpeed * camera->direction;
	if(isBtnPressed(KeyBoardBtn::D))
		position -= cameraSpeed * normaliseVec3(cross(Vec3{0.f, 1.f, 0.f}, camera->direction));
	if(isBtnPressed(KeyBoardBtn::Space))
		position += cameraSpeed * Vec3{0.f, 1.f, 0.f};
	if(isBtnPressed(KeyBoardBtn::LeftCtrl))
		position -= cameraSpeed * Vec3{0.f, 1.f, 0.f};

	magma::log::debug("position: {}", position);

	camera->viewTransform = lookAt(camera->position, camera->position + camera->direction);

	//reset cursor back
	setCursorPos(window.windowHandle, centerX, centerY);
}