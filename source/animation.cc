#include "animation.h"

#include <cassert>
#include <algorithm>

static mat4x4 generate_mat_from_transform(const JointTransform& input)
{
	mat4x4 out = loadIdentity();
	if(input.channelBits & CHANNEL_SCALE_BIT)
	{
		out *= loadScale(input.scale);
	}
	if(input.channelBits & CHANNEL_ROTATE_BIT)
	{
		out *= quatToRotationMat(input.rotation);
	}
	if(input.channelBits & CHANNEL_TRANSLATE_BIT)
	{
		out *= loadTranslation(input.translation);
	}

	return out;
}

void generate_global_joint_transforms(const Animation& animation, KeyFrame* keyFrame)
{
	assert(keyFrame);
	assert(!keyFrame->currentJointLocalTransforms.empty());

	for(uint32_t i = 0; i < keyFrame->currentJointLocalTransforms.size(); i++)
	{
		mat4x4 globalJointTransform = generate_mat_from_transform(keyFrame->currentJointLocalTransforms[i]);
		int parentId = animation.bindPose[i].parentId;
		while(parentId != -1)
		{
			globalJointTransform *= generate_mat_from_transform(keyFrame->currentJointLocalTransforms[parentId]);
			parentId = animation.bindPose[parentId].parentId;
		}

		keyFrame->currentJointGlobalTransforms[i] = globalJointTransform;
	}
}

void update_animation(Animation& animation, float frameTime, std::vector<mat4x4>& jointMatrices)
{
	animation.currentAnimTime += frameTime * animation.playbackRate;
	animation.currentAnimTime = fmod(animation.currentAnimTime, animation.keyFrames[animation.keyFrames.size() - 1].frameTime);

	//perform binary search to find keyFrame that is less or equal to required frametime
	int leftBorder = 0;
	int rightBorder = animation.keyFrames.size() - 1;
	
	while(leftBorder < rightBorder)
	{
		int split = (leftBorder + rightBorder) / 2;

		if(animation.keyFrames[split].frameTime > animation.currentAnimTime)
		{
			rightBorder = split;
		}
		else
		{
			leftBorder = split + 1;
		}
	}

	const int keyFrameIndex = leftBorder - 1;
	const int keyFrameIndexNext = keyFrameIndex + 1;
	const float epsilon = 0.001f;

	auto generateJointMatrices = [&](const std::vector<mat4x4>& currentJointGlobalTransforms)
	{
		for(uint32_t i = 0; i < jointMatrices.size(); i++)
		{
			jointMatrices[i] = animation.bindPose[i].invBindTransform * currentJointGlobalTransforms[i];
		}
	};


	//if keyframe time approximately matches with the current time 
	if(std::abs(animation.keyFrames[keyFrameIndex].frameTime - animation.currentAnimTime) < epsilon)
	{
		return generateJointMatrices(animation.keyFrames[keyFrameIndex].currentJointGlobalTransforms);
	}

	float duration = animation.keyFrames[keyFrameIndexNext].frameTime - animation.keyFrames[keyFrameIndex].frameTime;
	float amount = (animation.currentAnimTime - animation.keyFrames[keyFrameIndex].frameTime) / duration;
	const std::size_t jointsSize = animation.bindPose.size();

	KeyFrame interpolatedFrame = {};
	interpolatedFrame.currentJointLocalTransforms.resize(jointsSize);
	interpolatedFrame.currentJointGlobalTransforms.resize(jointsSize);

	for(uint32_t i = 0; i < jointsSize; i++)
	{
		JointTransform interpolatedTransform = {};
		auto&& firstJointTransform = animation.keyFrames[keyFrameIndex].currentJointLocalTransforms[i];
		auto&& secontJointTransform = animation.keyFrames[keyFrameIndexNext].currentJointLocalTransforms[i];

		auto channelBits = firstJointTransform.channelBits;
		if(channelBits & CHANNEL_ROTATE_BIT)
		{
			Quat first = firstJointTransform.rotation;
			Quat second = secontJointTransform.rotation;
			interpolatedTransform.rotation = sLerp(first, second, amount);
			interpolatedTransform.channelBits |= CHANNEL_ROTATE_BIT;
		}
		if(channelBits & CHANNEL_SCALE_BIT)
		{
			Vec3 first = firstJointTransform.scale;
			Vec3 second = secontJointTransform.scale;
			interpolatedTransform.scale = lerp(first, second, amount);
			interpolatedTransform.channelBits |= CHANNEL_SCALE_BIT;
		}
		if(channelBits & CHANNEL_TRANSLATE_BIT)
		{
			Vec3 first = firstJointTransform.translation;
			Vec3 second = secontJointTransform.translation;
			interpolatedTransform.translation = lerp(first, second, amount);
			interpolatedTransform.channelBits |= CHANNEL_TRANSLATE_BIT;
		}

		interpolatedFrame.currentJointLocalTransforms[i] = interpolatedTransform;
	}

	generate_global_joint_transforms(animation, &interpolatedFrame);
	generateJointMatrices(interpolatedFrame.currentJointGlobalTransforms);
	
}