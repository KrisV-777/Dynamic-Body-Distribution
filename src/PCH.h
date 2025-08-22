#pragma once

#pragma warning(push)
#pragma warning(disable : 4200)
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#pragma warning(pop)

#include <atomic>
#include <unordered_map>

#include "magic_enum.hpp"

#pragma warning(push)
#ifdef NDEBUG
#include <spdlog/sinks/basic_file_sink.h>
#else
#include <spdlog/sinks/msvc_sink.h>
#endif
#pragma warning(pop)

namespace logger = SKSE::log;
namespace fs = std::filesystem;
using namespace std::literals;

namespace stl
{
	using namespace SKSE::stl;

#ifdef IS_PRE_AE
	constexpr std::uint32_t version_pack(REL::Version a_version) noexcept
	{
		return static_cast<std::uint32_t>(
			(a_version[0] & 0x0FF) << 24u |
			(a_version[1] & 0x0FF) << 16u |
			(a_version[2] & 0xFFF) << 4u |
			(a_version[3] & 0x00F) << 0u);
	}
#endif
}

struct FixedStringComparator
{
	bool operator()(const RE::BSFixedString& lhs, const RE::BSFixedString& rhs) const
	{
		return _stricmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};
struct StringComparator
{
	bool operator()(const std::string& lhs, const std::string& rhs) const
	{
		return _stricmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};

template <>
struct std::formatter<RE::BSFixedString> : std::formatter<const char*>
{
	template <typename FormatContext>
	auto format(const RE::BSFixedString& myStr, FormatContext& ctx) const
	{
		return std::formatter<const char*>::format(myStr.data(), ctx);
	}
};

#define DLLEXPORT __declspec(dllexport)
