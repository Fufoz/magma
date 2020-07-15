#ifndef MAGMA_LOGGING_MISC_H
#define MAGMA_LOGGING_MISC_H

#include <fmt/format.h>
#include <maths.h>

template<>
struct fmt::formatter<Vec2>
{
	constexpr auto parse(format_parse_context& ctx)
	{
		return ctx.begin();
	}

	template <typename FormatContext>
  	auto format(const Vec2& vec, FormatContext& ctx) 
	{
		return format_to(ctx.out(), "[{: f};{: f}]", vec.x, vec.y);
	}
};

template<>
struct fmt::formatter<Vec3>
{
	constexpr auto parse(format_parse_context& ctx)
	{
		return ctx.begin();
	}

	template <typename FormatContext>
  	auto format(const Vec3& vec, FormatContext& ctx) 
	{
		return format_to(ctx.out(), "[{: f};{: f};{: f}]", vec.x, vec.y, vec.z);
	}
};

template<>
struct fmt::formatter<Vec4>
{
	constexpr auto parse(format_parse_context& ctx)
	{
		return ctx.begin();
	}

	template <typename FormatContext>
  	auto format(const Vec4& vec, FormatContext& ctx) 
	{
		return format_to(ctx.out(), "[{: f};{: f};{: f};{: f}]", vec.x, vec.y, vec.z, vec.w);
	}
};

template<>
struct fmt::formatter<Quat>
{
	constexpr auto parse(format_parse_context& ctx)
	{
		return ctx.begin();
	}

	template <typename FormatContext>
  	auto format(const Quat& quat, FormatContext& ctx) 
	{
		return format_to(ctx.out(), "[{: f};{: f};{: f};{: f}]", quat.x, quat.y, quat.z, quat.w);
	}
};

template<>
struct fmt::formatter<mat4x4>
{
	constexpr auto parse(format_parse_context& ctx)
	{
		return ctx.begin();
	}

	template <typename FormatContext>
  	auto format(const mat4x4& mat, FormatContext& ctx) 
	{
		return format_to(ctx.out(), "[mat4x4:\n {}\n {}\n {}\n {}]",mat.firstRow, mat.secondRow, mat.thirdRow, mat.fourthRow );
	}
};

#endif