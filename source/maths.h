#ifndef MATHS_H
#define MATHS_H

#include <cstdint>
#include <stdio.h>
#include <cmath>

#define PI 3.14159265359f

union Vec2
{
	struct {
		float x;
		float y;
	};

	struct {
		float u;
		float v;
	};
	
	struct {
		float data[2];
	};

	inline float& operator[] (int idx) { return data[idx];}
	inline const float& operator[] (int idx) const { return data[idx];} 
};

union Vec3
{
	struct {
		Vec2 xy;
		float _z;
	};

	struct {
		float x;
		float y;
		float z;
	};
	
	struct {
		float u;
		float v;
		float _;
	};

	struct {
		float R;
		float G;
		float B;
	};

	struct {
		float data[3];
	};

	inline float& operator[] (int idx) { return data[idx];}
	inline const float& operator[] (int idx) const { return data[idx];} 
};

union Vec4
{
	struct {
		union {
			struct {
				float x;
				float y;
				float z;
			};
			Vec3 xyz;
		};

		float w;
	};

	struct {
		float R;
		float G;
		float B;
		float A;
	};

	struct {
		float data[4];
	};

	inline float& operator[] (int idx) { return data[idx];} 
	inline const float& operator[] (int idx) const { return data[idx];} 
};

union mat4x4
{
	struct {
		float p[16];
	};

	struct {
		Vec4 rows[4];
	};
	
	struct {
		Vec4 firstRow;
		Vec4 secondRow;
		Vec4 thirdRow;
		Vec4 fourthRow;
	};
};

union mat3x3
{
	struct {
		float p[9];
	};

	struct {
		Vec3 rows[3];
	};
	
	struct {
		Vec3 firstRow;
		Vec3 secondRow;
		Vec3 thirdRow;
	};
};

union Quat
{
	struct {
		float x;
		float y;
		float z;
		float w;
	};

	struct {
		Vec3 complex;
		float scalar;
	};
	
	Vec4 xyzw;
};

struct Vertex
{
	Vec4 pos;
	Vec3 texCoords;
	Vec3 normal;
	Vec3 color;
	Vec3 tangent;
};

struct Triangle
{
	Vertex v1;
	Vertex v2;
	Vertex v3;
};

inline float toRad(float degree)
{
	return degree * PI / 180.f;
}

inline Vec2 operator+(const Vec2& left, const Vec2& right)
{
	return Vec2{left.x + right.x, left.y + right.y};
}

inline Vec2 operator-(const Vec2& left, const Vec2& right)
{
	return Vec2{left.x - right.x, left.y - right.y};
}

inline Vec2& operator+=(Vec2& self, const Vec2& other)
{
	self = self + other;
	return self;
}

inline Vec2 operator+(const Vec3& left, const Vec2& right)
{
	return Vec2{left.x + right.x, left.y + right.y};
}

inline Vec2 operator+(const Vec2& left, const Vec3& right)
{
	return Vec2{left.x + right.x, left.y + right.y};
}

inline Vec2& operator-=(Vec2& self, const Vec2& other)
{
	self = self - other;
	return self;
}

inline Vec2 operator*(const Vec2& left, float value)
{
	return Vec2{left.x * value, left.y * value};
}

inline Vec2 operator*(float value, const Vec2& left)
{
	return left * value;
}

inline Vec2 operator^(const Vec2& left, const Vec2& right)
{
	return Vec2 {left.x * right.x, left.y * right.y}; 
}

inline Vec2 operator/(const Vec2& left, float value)
{
	return Vec2{left.x / value, left.y / value};
}

inline Vec2 operator/(float value, const Vec2& left)
{
	return Vec2{value / left.x, value/left.y};
}

inline float dotVec2(const Vec2& left, const Vec2& right)
{
	return left.x * right.x + left.y * right.y; 
}

inline float lengthVec2(const Vec2& in)
{
	return sqrt(dotVec2(in, in));
}

inline Vec2 normaliseVec2(const Vec2& in)
{
	float length = lengthVec2(in);
	if(length > 0) {
		float invLength = 1 / length;
		return Vec2{in.x * invLength, in.y * invLength};
	}
	return Vec2{0.f, 0.f};
}

inline Vec3 operator+(const Vec3& left, const Vec3& right)
{
	return Vec3{left.x + right.x, left.y + right.y, left.z + right.z};
}

inline Vec3 operator-(const Vec3& left, const Vec3& right)
{
	return Vec3{left.x - right.x, left.y - right.y, left.z - right.z};
}

inline Vec3 operator-(const Vec3& left, float val)
{
	return Vec3{left.x - val, left.y - val, left.z - val};
}

inline Vec3& operator+=(Vec3& self, const Vec3& other)
{
	self = self + other;
	return self;
}

inline Vec3& operator-=(Vec3& self, const Vec3& other)
{
	self = self - other;
	return self;
}
inline bool operator==(Vec3& self, const Vec3& other)
{
	return self.x == other.x && self.y == other.y && self.z == other.z;
}
inline Vec3 operator*(float scalar,const Vec3& other)
{
	return Vec3{scalar * other.x, scalar * other.y, scalar * other.z};
}

inline Vec3 operator*(const Vec3& other, float scalar)
{
	return scalar * other;
}

//component-wise vector multiplication
inline Vec3 operator^(const Vec3& left, const Vec3& right)
{
	return Vec3 {left.x * right.x, left.y * right.y, left.z * right.z};
}

inline Vec3 operator/(float scalar,const Vec3& other)
{
	return Vec3{scalar / other.x, scalar / other.y, scalar / other.z};
}

inline Vec3 operator/(const Vec3& other, float scalar)
{
	return Vec3{other.x / scalar, other.y / scalar, other.z / scalar};
}

inline float dotVec3(const Vec3& left, const Vec3& right)
{
	return left.x * right.x + left.y * right.y + left.z * right.z; 
}

inline float lengthVec3(const Vec3& in)
{
	return sqrt(dotVec3(in, in));
}

inline Vec3 normaliseVec3(const Vec3& in)
{
	float length = lengthVec3(in);
	if(length > 0) {
		float invLength = 1 / length;
		return Vec3{in.x * invLength, in.y * invLength, in.z * invLength};
	}
	return Vec3{0.f, 0.f, 0.f};
}

inline void normaliseVec3InPlace(Vec3& in)
{
	float length = lengthVec3(in);
	if(length > 0) {
		float invLength = 1 / length;
		in.x = in.x * invLength;
		in.y = in.y * invLength;
		in.z = in.z * invLength;
	}
}

inline Vec3 normaliseVec4(const Vec3& in)
{
	float length = lengthVec3(in);
	if(length > 0) {
		float invLength = 1 / length;
		return Vec3{in.x * invLength, in.y * invLength, in.z * invLength};
	}
	return Vec3{0.f, 0.f, 0.f};
}

inline Vec3 cross(const Vec3& first, const Vec3& second)
{
	return Vec3{
		first.y * second.z - first.z * second.y,
		first.z * second.x - first.x * second.z,
		first.x * second.y - first.y * second.x
	};
}

inline Vec4 homogenize(const Vec3& in)
{
	return Vec4{in.x, in.y, in.z, 1.f};
}

inline Vec4 perspectiveDivide(const Vec4& in)
{
	return Vec4 {in.x/in.w, in.y/in.w, in.z/in.w, 1.f};
}

inline float dotVec4(const Vec4& left, const Vec4& right)
{
	return left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w; 
}

inline Vec4 operator*(float scalar, const Vec4& other)
{
	return Vec4{scalar * other.x, scalar * other.y, scalar * other.z, scalar * other.w};
}

inline Vec4 operator*(const Vec4& other, float scalar)
{
	return scalar * other;
}

inline Vec4 operator/(float scalar, const Vec4& other)
{
	return Vec4{scalar / other.x, scalar / other.y, scalar / other.z, scalar / other.w};
}

inline Vec4 operator/(const Vec4& other, float scalar)
{
	return scalar / other;
}

inline Vec4 operator+(const Vec4& left, const Vec4& right)
{
	return Vec4{left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w};
}

inline Vec4 operator-(const Vec4& left, const Vec4& right)
{
	return Vec4{left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w};
}

inline Vec4& operator+=(Vec4& self, const Vec4& other)
{
	self = self + other;
	return self;
}

inline Vec4& operator+=(Vec4& self, float value)
{
	self = {self.x + value, self.y + value, self.z + value, self.w + value};
	return self;
}

inline Vec4& operator-=(Vec4& self, const Vec4& other)
{
	self = self - other;
	return self;
}

inline Vec4& operator-=(Vec4& self, float value)
{
	self = {self.x - value, self.y - value, self.z - value, self.w - value};
	return self;
}

inline float clamp(float val, float min, float max)
{
	return val < min ? min : (val > max ? max : val); 
}

inline Vec2 clamp(const Vec2& val, const Vec2& min, const Vec2& max)
{
	return Vec2 {
		clamp(val.x, min.x, max.x),
		clamp(val.y, min.y, max.y)
	};
}

inline Vec3 clamp(const Vec3& val, const Vec3& min, const Vec3& max)
{
	return Vec3 {
		clamp(val.x, min.x, max.x),
		clamp(val.y, min.y, max.y),
		clamp(val.z, min.z, max.z)
	};
}

static Vec3 RGB_BLACK = {0.f, 0.f, 0.f};
static Vec3 RGB_WHITE = {255.f, 255.f, 255.f};

inline mat4x4 loadIdentity()
{
	return mat4x4 {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
}

inline mat4x4 loadTranslation(const Vec3& vec)
{
	return mat4x4 {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		vec.x, vec.y, vec.z, 1
	};
}

inline mat4x4 loadScale(const Vec3& vec)
{
	return mat4x4 {
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1
	};
}

inline mat4x4 simplePerspective(Vec3 v)
{
	return mat4x4 {
		1.f/(1.f-v.z/4.f), 0, 0, 0,
		0, 1.f/(1.f-v.z/4.f), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
}

inline mat4x4 viewport(float screenWidth, float screenHeight)
{
	return mat4x4 {
		screenWidth/2.f,        0,                   0, 0,
		0,                    screenHeight/2.f,      0, 0,
		0,                    0,                     1, 0,
		screenWidth/2.f - 0.5f, screenHeight/2.f - 0.5f, 0, 1
	};

}

inline mat4x4 frustum(float left, float right, float bottom, float top, float near, float far)
{
	return mat4x4 {
		(2.f*near)/(right - left),   0,                          0,                          0,
		0,                         2.f*near/(top-bottom),        0,                          0,
		(right+left)/(right-left), (top+bottom)/(top-bottom),  -(far + near)/(far-near),    -1.f,
		0,                         0,                          (-2.f*far*near)/(far-near),      0
	};
}

inline mat4x4 perspectiveProjection(float FOV, float aspect, float near, float far)
{
	float h;
	float w;

	h = tan(FOV * 0.5f * PI / 180.f) * near;
	w = h * aspect;
	return frustum(-w, w, -h, h, near, far);
}

inline float determinant(const mat4x4& in)
{
	float det2x2Mul1 = (in.p[0] * in.p[5] - in.p[4] * in.p[1]) *
		(in.p[10] * in.p[15] - in.p[14] * in.p[11]);
		
	float det2x2Mul2 = (in.p[1] * in.p[6] - in.p[5] * in.p[2]) *
		(in.p[8] * in.p[15] - in.p[12] * in.p[11]);
		
	float det2x2Mul3 = (in.p[2] * in.p[7] - in.p[6] * in.p[3]) *
		(in.p[8] * in.p[13] - in.p[12] * in.p[9]);

	float det2x2Mul4 = (in.p[1] * in.p[7] - in.p[5] * in.p[3]) *
		(in.p[8] * in.p[14] - in.p[12] * in.p[10]);
		
	float det2x2Mul5 = (in.p[0] * in.p[6] - in.p[4] * in.p[2]) *
		(in.p[9] * in.p[15] - in.p[13] * in.p[11]);
		
	float det2x2Mul6 = (in.p[0] * in.p[7] - in.p[4] * in.p[3]) *
		(in.p[9] * in.p[14] - in.p[13] * in.p[10]);

	return det2x2Mul1 + det2x2Mul2 + det2x2Mul3 - det2x2Mul4 - det2x2Mul5 + det2x2Mul6;
}

//taken from : https://www.geometrictools.com/Documentation/LaplaceExpansionTheorem.pdf
inline mat4x4 inverse(const mat4x4& in)
{
	float s0 = in.p[0] * in.p[5] - in.p[4] * in.p[1];
	float s1 = in.p[0] * in.p[6] - in.p[4] * in.p[2];
	float s2 = in.p[0] * in.p[7] - in.p[4] * in.p[3];
	float s3 = in.p[1] * in.p[6] - in.p[5] * in.p[2];
	float s4 = in.p[1] * in.p[7] - in.p[5] * in.p[3];
	float s5 = in.p[2] * in.p[7] - in.p[6] * in.p[3];

	float c5 = in.p[10] * in.p[15] - in.p[14] * in.p[11];
	float c4 = in.p[9] * in.p[15] - in.p[13] * in.p[11];
	float c3 = in.p[9] * in.p[14] - in.p[13] * in.p[10];
	float c2 = in.p[8] * in.p[15] - in.p[12] * in.p[11];
	float c1 = in.p[8] * in.p[14] - in.p[12] * in.p[10];
	float c0 = in.p[8] * in.p[13] - in.p[12] * in.p[9];

	float invDet = 1.f / (s0*c5 - s1*c4 + s2*c3 +s3*c2 - s4*c1 + s5*c0);

	mat4x4 out = {};
	
	out.p[0] = (in.p[5] * c5 - in.p[6] * c4 + in.p[7] * c3)*invDet;
	out.p[1] = (-in.p[1] * c5 + in.p[2] * c4 - in.p[3] * c3)*invDet;
	out.p[2] = (in.p[13] * s5 - in.p[14] * s4 + in.p[15] * s3)*invDet;
	out.p[3] = (-in.p[9] * s5 + in.p[10] * s4 - in.p[11] * s3)*invDet;

	out.p[4] = (-in.p[4] * c5 + in.p[6] * c2 - in.p[7] * c1)*invDet;
	out.p[5] = (in.p[0] * c5 - in.p[2] * c2 + in.p[3] * c1)*invDet;
	out.p[6] = (-in.p[12] * s5 + in.p[14] * s2 - in.p[15] * s1)*invDet;
	out.p[7] = (in.p[8] * s5 - in.p[10] * s2 + in.p[11] * s1)*invDet;

	out.p[8] = (in.p[4] * c4 - in.p[5] * c2 + in.p[7] * c0)*invDet;
	out.p[9] = (-in.p[0] * c4 + in.p[1] * c2 - in.p[3] * c0)*invDet;
	out.p[10] =( in.p[12] * s4 - in.p[13] * s2 + in.p[15] * s0)*invDet;
	out.p[11] =( -in.p[8] * s4 + in.p[9] * s2 - in.p[11] * s0)*invDet;

	out.p[12] = (-in.p[4] * c3 + in.p[5] * c1 - in.p[6] * c0)*invDet;
	out.p[13] = (in.p[0] * c3 - in.p[1] * c1 + in.p[2] * c0)*invDet;
	out.p[14] = (-in.p[12] * s3 + in.p[13] * s1 - in.p[14] * s0)*invDet;
	out.p[15] = (in.p[8] * s3 - in.p[9] * s1 + in.p[10] * s0)*invDet;

	return out;
}

inline mat4x4 transpose(const mat4x4& in)
{
	mat4x4 out = {};

	for(uint8_t i = 0; i < 4; i++) {
		for(uint8_t j = 0; j < 4; j++) {
			out.p[j + i * 4] = in.p[j * 4 + i];
		}
	}
	return out;
}

inline mat3x3 transpose(const mat3x3& in)
{
	mat3x3 out = {};

	for(uint8_t i = 0; i < 3; i++) {
		for(uint8_t j = 0; j < 3; j++) {
			out.p[j + i * 3] = in.p[j * 3 + i];
		}
	}
	return out;
}

inline void transposeInplace(mat4x4& in)
{
	mat4x4 tmp = in;

	for(uint8_t i = 0; i < 4; i++) {
		for(uint8_t j = 0; j < 4; j++) {
			in.p[j + i * 4] = tmp.p[j * 4 + i];
		}
	}
}

inline mat4x4 operator*(const mat4x4& left, const mat4x4& right)
{
	mat4x4 result = {};
	const uint8_t stride = 4;

	for(uint8_t i = 0; i < 4; i++) {
		for(uint8_t j = 0; j < 4; j++) {
			for(uint8_t k = 0; k < 4; k++) {
				result.p[j + stride * i] += left.p[k + stride * i] * right.p[k * stride + j];
			}
		}
	}
	return result;
}

inline Vec4 operator*(const Vec4& left, const mat4x4& right)
{
	Vec4 out = {};
	out.x = left.x * right.p[0] + left.y * right.p[4] + left.z * right.p[8] +  left.w * right.p[12];
	out.y = left.x * right.p[1] + left.y * right.p[5] + left.z * right.p[9] +  left.w * right.p[13];
	out.z = left.x * right.p[2] + left.y * right.p[6] + left.z * right.p[10] + left.w * right.p[14];
	out.w = left.x * right.p[3] + left.y * right.p[7] + left.z * right.p[11] + left.w * right.p[15];
	return out;
}

inline Vec4& operator*=(Vec4& left, const mat4x4& right)
{
	left = left * right;
	return left;
}

inline Vec4& operator*=(Vec4& left, float val)
{
	left = left * val;
	return left;
}

inline Vec3& operator*=(Vec3& left, float val)
{
	left = left * val;
	return left;
}

inline Vec3 operator*(const Vec3& left, const mat4x4& right)
{
	Vec4 out = {left.x, left.y, left.z, 0.f};
	out = out * right;
	return out.xyz;
}


inline Vec3 operator*(const Vec3& left, const mat3x3& right)
{
	Vec3 out = {};
	out.x = left.x * right.p[0] + left.y * right.p[3] + left.z * right.p[6];
	out.y = left.x * right.p[1] + left.y * right.p[4] + left.z * right.p[7];
	out.z = left.x * right.p[2] + left.y * right.p[5] + left.z * right.p[8];
	return out;
}

inline void logMat4x4(const char* tag, const mat4x4& in)
{
	printf("-------------------%s--------------------\n",tag);
	for(uint8_t i = 0; i < 4; i++) {
		for(uint8_t j = 0; j < 4; j++) {
			printf("%f ",in.p[j + 4 * i]);
		}
		printf("\n");
	}
	printf("---------------------------------------\n");
}

inline mat4x4 lookAt(Vec3 cameraPos, Vec3 thing, Vec3 UpDir = Vec3{0.f, 1.f, 0.f})
{
	Vec3 zAxis  = normaliseVec3(cameraPos - thing);
	Vec3 xAxis = normaliseVec3(cross(UpDir, zAxis));
	Vec3 yAxis = cross(zAxis, xAxis);
	
	mat4x4 viewMat = {
		xAxis.x, yAxis.x, zAxis.x, 0,
		xAxis.y, yAxis.y, zAxis.y, 0,
		xAxis.z, yAxis.z, zAxis.z, 0,
		-dotVec3(thing, xAxis), -dotVec3(thing, yAxis), -dotVec3(thing, zAxis), 1
	};
	return viewMat;
}

inline mat4x4 rotateZ(float degrees)
{
	float rad = degrees * PI / 180.f;

	return mat4x4 {
		cosf(rad),  sinf(rad), 0, 0,
		-sinf(rad), cosf(rad), 0, 0,
		0,          0,         1, 0,
		0,          0,         0, 1
	};
}

inline mat4x4 rotateY(float degrees)
{
	float rad = degrees * PI / 180.f;
	return mat4x4 {
		cosf(rad), 0, -sinf(rad), 0,
		0,         1, 0,          0,
		sinf(rad), 0, cosf(rad),  0,
		0,         0, 0,          1
	};
}

inline mat4x4 rotateX(float degrees)
{
	float rad = degrees * PI / 180.f;
	return mat4x4 {
		1, 0,          0,          0,
		0, cosf(rad),  sinf(rad),  0,
		0, -sinf(rad), cosf(rad),  0,
		0, 0,          0,          1
	};
}

template<typename T>
inline T max(T a, T b)
{
	return a > b ? a : b;
}

template<typename T>
inline T min(T a, T b)
{
	return a > b ? b : a;
}

inline float computeArea(Vec3 v0, Vec3 v1, Vec3 v2)
{
	return (v1.x - v0.x) * (v2.y - v0.y) - (v2.x  - v0.x) * (v1.y - v0.y); 
}

inline float lerp(float start, float end, float amount)
{
	return start + amount * (end - start);
}

inline Vec2 lerp(const Vec2& start, const Vec2& end, float amount)
{
	return Vec2 { 
		lerp(start.x, end.x, amount),
		lerp(start.y, end.y, amount)
	};
}

inline Vec3 lerp(const Vec3& start, const Vec3& end, float amount)
{
	return Vec3 { 
		lerp(start.x, end.x, amount),
		lerp(start.y, end.y, amount),
		lerp(start.z, end.z, amount)
	};
}

inline Vec4 lerp(const Vec4& start, const Vec4& end, float amount)
{
	return Vec4 {
		lerp(start.x, end.x, amount),
		lerp(start.y, end.y, amount),
		lerp(start.z, end.z, amount),
		lerp(start.w, end.w, amount)
	};
}

//For unit quaternions conjugate and inverse are identical 
//since magnitude of rotation quat is 1. (q^-1 = q*/||q||)
//for pure rotation quats conjugate quat rotates in the 
//direction opposite to the original quaternion
inline Quat conjugate(const Quat& in)
{
	return Quat{-in.x, -in.y, -in.z, in.w};
}

//A.K.A Hamilton product
inline Quat operator*(const Quat& left, const Quat& right)
{
	Quat out = {};
	out.complex = left.complex * right.w + right.complex * left.w + cross(left.complex, right.complex);
	out.w = left.w * right.w - dotVec3(left.complex, right.complex);
	return out;
}

inline float lengthQuat(const Quat& q)
{
	return sqrt(dotVec4(q.xyzw, q.xyzw));
}

inline Quat normalise(const Quat& q)
{
	Quat out = {};
	float length = lengthQuat(q);
	if(length > 0) {
		out.x /= length;
		out.y /= length;
		out.z /= length;
		out.w /= length;
	}
	return out;
}

inline Quat operator*(const Quat& left, float scalar)
{    
	return Quat { left.x * scalar, left.y * scalar, left.z * scalar, left.w * scalar};
}

inline Quat operator*(float scalar, const Quat& right)
{
	return right * scalar;
}

inline Quat quatFromAxisAndAngle(const Vec3& axis, float angle)
{
	Quat out = {};
	float radians = angle * PI / 180.f;
	out.complex = axis * sinf(radians/2.f);
	out.scalar = cosf(radians/2.f);
	return out;
}

//spherical linear interpolation
inline Quat sLerp(const Quat& first, const Quat& second, float amount)
{
	float cosOmega = dotVec4(first.xyzw, second.xyzw);
	Quat tmp = second;

	//reverse one quat to get shortest arc in 4d
	if(cosOmega < 0) {
		tmp = tmp * -1.f;
		cosOmega = -cosOmega;
	}

	float k0;
	float k1;
	if(cosOmega > 0.9999f) {
		k0 = 1.f - amount;
		k1 = amount;
	} else {
		float sinOmega = sqrt(1.f - cosOmega * cosOmega);
		float omega = atan2f(sinOmega, cosOmega);
		float sinOmegaInverted = 1.f / sinOmega;
		k0 = sinf((1 - amount) * omega) * sinOmegaInverted;
		k1 = sinf(omega * amount) * sinOmegaInverted;
	}

	Quat out = {};
	out.x = first.x * k0 + tmp.x * k1;
	out.y = first.y * k0 + tmp.y * k1;
	out.z = first.z * k0 + tmp.z * k1;
	out.w = first.w * k0 + tmp.w * k1;
	return out;
}

inline mat4x4 quatToRotationMat(const Quat& quat)
{
	float xx = quat.x * quat.x;
	float yy = quat.y * quat.y;
	float zz = quat.z * quat.z;
	float xy = quat.x * quat.y;
	float xz = quat.x * quat.z;
	float yz = quat.y * quat.z;
	float wz = quat.w * quat.z;
	float wy = quat.w * quat.y;
	float wx = quat.w * quat.x;

	mat4x4 out = loadIdentity();
	out.p[0] = 1 - 2 * yy - 2 * zz;
	out.p[5] = 1 - 2 * xx - 2 * zz;
	out.p[10] = 1 - 2 * xx - 2 * yy;

	out.p[1] = 2 * xy + 2 * wz;
	out.p[2] = 2 * xz - 2 * wy;
	out.p[4] = 2 * xy - 2 * wz;
	out.p[6] = 2 * yz + 2 * wx;
	out.p[8] = 2 * xz + 2 * wy;
	out.p[9] = 2 * yz - 2 * wz;

	return out;
}

#endif