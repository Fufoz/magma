#ifndef MAGMA_ANIMATION_H
#define MAGMA_ANIMATION_H

#include "maths.h"

#include <vector>

struct Joint
{
	mat4x4 invBindTransform;
	int parentId = -1;
};

enum AnimChannel
{
	CHANNEL_ROTATE_BIT    = 0b001,
	CHANNEL_SCALE_BIT     = 0b010,
	CHANNEL_TRANSLATE_BIT = 0b100
};
typedef uint32_t AnimChannelBits;

struct JointTransform
{
	Quat rotation;
	Vec3 translation;
	Vec3 scale;
	AnimChannelBits channelBits;
};

struct AnimationSampler
{
	uint32_t jointId;
	AnimChannel channel;
	std::vector<float> timeCodes;
	std::vector<uint8_t> outputs;
};

struct KeyFrame
{
	float frameTime;
	std::vector<JointTransform> currentJointLocalTransforms;//from samplers
	std::vector<mat4x4> currentJointGlobalTransforms;
};

struct Animation
{
	float currentAnimTime;
	std::vector<Joint> bindPose;
	std::vector<KeyFrame> keyFrames;
};

void updateAnimation(Animation& animation, float time, std::vector<mat4x4>& jointMatrices);
void generateGlobalJointTransforms(const Animation& animation, KeyFrame* keyFrame);

#endif