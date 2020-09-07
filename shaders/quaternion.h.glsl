
vec4 quatFromAxisAndAngle(vec3 axisOfRotation, float radians)
{
	vec4 quat;
	quat.xyz = axisOfRotation * sin(radians/2.f);
	quat.w = cos(radians/2.f);
	return quat;
}

vec4 quatMultiply(vec4 left, vec4 right)
{
	vec4 outQuat;
	outQuat.xyz = left.xyz * right.w + right.xyz * left.w + cross(left.xyz, right.xyz);
	outQuat.w = left.w * right.w - dot(left.xyz, right.xyz);
	return outQuat;
}

vec4 unitQuaternion()
{
	return vec4(0.0, 0.0, 0.0, 1.0);
}

vec4 rotateFromTo(vec3 from, vec3 to)
{

	vec3 fromNormalised = normalize(from);
	vec3 toNormalised = normalize(to);

	float angleInRadians = acos(dot(fromNormalised, toNormalised));

	//if vectors are collinear
	if(abs(degrees(angleInRadians)) < 2)
	{
		return unitQuaternion();
	}
	else if(abs(degrees(angleInRadians)) > 178)
	{
		return quatFromAxisAndAngle(vec3(0.f, 1.f, 0.f), angleInRadians);
	}
	else
	{
		vec3 axisOfRotation = normalize(cross(fromNormalised, toNormalised));
		return quatFromAxisAndAngle(axisOfRotation, angleInRadians);
	}
}

mat4 loadTranslation(vec3 translationVec)
{
	mat4 translationMat = mat4(1.0);
	translationMat[3] = vec4(translationVec, 1.0);
	return translationMat;
}


vec4 axisAngleFromQuat(vec4 quat)
{
	vec4 info;
	info.xyz = quat.xyz / sqrt(1 - quat.w * quat.w);
	info.w = 2 * acos(quat.w);
	return info;
}

mat4 quatToRotationMat(vec4 quat)
{
	mat4 outputMat = mat4(1.0);

	float xx = quat.x * quat.x;
	float yy = quat.y * quat.y;
	float zz = quat.z * quat.z;
	float xy = quat.x * quat.y;
	float xz = quat.x * quat.z;
	float yz = quat.y * quat.z;
	float wz = quat.w * quat.z;
	float wy = quat.w * quat.y;
	float wx = quat.w * quat.x;

	outputMat[0].xyz = vec3(1 - 2 * yy - 2 * zz, 2 * xy + 2 * wz, 2 * xz - 2 * wy);
	outputMat[1].xyz = vec3(2 * xy - 2 * wz, 1 - 2 * xx - 2 * zz, 2 * yz + 2 * wx);
	outputMat[2].xyz = vec3(2 * xz + 2 * wy, 2 * yz - 2 * wx, 1 - 2 * xx - 2 * yy);

	return outputMat;
}

//spherical linear interpolation
vec4 slerp(vec4 first, vec4 second, float amount)
{
	float cosOmega = dot(first, second);
	vec4 tmp = second;

	//reverse one quat to get shortest arc in 4d
	if(cosOmega < 0) 
	{
		tmp = tmp * -1.f;
		cosOmega = -cosOmega;
	}

	float k0;
	float k1;
	if(cosOmega > 0.9999f) 
	{
		k0 = 1.f - amount;
		k1 = amount;
	} 
	else 
	{
		float sinOmega = sqrt(1.f - cosOmega * cosOmega);
		float omega = atan(sinOmega, cosOmega);
		float sinOmegaInverted = 1.f / sinOmega;
		k0 = sin((1 - amount) * omega) * sinOmegaInverted;
		k1 = sin(omega * amount) * sinOmegaInverted;
	}

	vec4 result;
	result.x = first.x * k0 + tmp.x * k1;
	result.y = first.y * k0 + tmp.y * k1;
	result.z = first.z * k0 + tmp.z * k1;
	result.w = first.w * k0 + tmp.w * k1;
	return result;
}