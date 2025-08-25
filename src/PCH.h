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

	inline bool read_string(SKSE::SerializationInterface* a_intfc, std::string& a_str)
	{
		std::size_t size = 0;
		if (!a_intfc->ReadRecordData(size)) {
			return false;
		}
		a_str.reserve(size);
		if (!a_intfc->ReadRecordData(a_str.data(), static_cast<std::uint32_t>(size))) {
			return false;
		}
		return true;
	}

	template <class S>
	inline bool write_string(SKSE::SerializationInterface* a_intfc, const S& a_str)
	{
		std::size_t size = a_str.length() + 1;
		return a_intfc->WriteRecordData(size) && a_intfc->WriteRecordData(a_str.data(), static_cast<std::uint32_t>(size));
	}
}

namespace Papyrus
{
#define REGISTERFUNC(func, classname, delay) a_vm->RegisterFunction(#func##sv, classname, func, !delay)
#define STATICARGS [[maybe_unused]] VM *a_vm, [[maybe_unused]] RE::VMStackID a_stackID, RE::StaticFunctionTag *
#define TRACESTACK(err) a_vm->TraceStack(err, a_stackID)

	using VM = RE::BSScript::IVirtualMachine;
	using StackID = RE::VMStackID;
}

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
