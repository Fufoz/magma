#ifndef MAGMA_CAMERA_H
#define	MAGMA_CAMERA_H

#include "maths.h"
#include "vk_types.h"

struct FPSCamera
{
	Vec3 position;
	Vec3 direction;
	mat4x4 viewTransform;
};

void fpsCameraUpdate(const WindowInfo& window, float deltaTime, FPSCamera* camera);

#endif