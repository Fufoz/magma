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
		return format_to(ctx.out(), "[{};{}]", vec.x, vec.y);
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
		return format_to(ctx.out(), "[{};{};{}]", vec.x, vec.y, vec.z);
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
		return format_to(ctx.out(), "[{};{};{};{}]", vec.x, vec.y, vec.z, vec.w);
	}
};

#endif