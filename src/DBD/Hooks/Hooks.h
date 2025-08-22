#pragma once

namespace DBD
{
	struct Hooks
	{
		Hooks() = delete;

		static void Install();

	private:
		static RE::NiAVObject* Load3D(RE::Character& a_this, bool a_arg1);
		static inline REL::Relocation<decltype(Load3D)> _Load3D;

		typedef void(WINAPI* DoReset3DType)(RE::Actor& a_this, bool a_updateWeight);
		static void DoReset3D(RE::Actor& a_this, bool a_updateWeight);
		static inline DoReset3DType _DoReset3D;
	};
}  // namespace DBD
