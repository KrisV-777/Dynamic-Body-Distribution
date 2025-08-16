#pragma once

namespace DBD
{
	struct Hooks
	{
		Hooks() = delete;

		static void Install();

	private:
		static RE::NiAVObject* Load3DPlayer(RE::Character& a_this, bool a_arg1);
		static RE::NiAVObject* Load3D(RE::Character& a_this, bool a_arg1);

		static inline REL::Relocation<decltype(Load3DPlayer)> _Load3DPlayer;
		static inline REL::Relocation<decltype(Load3D)> _Load3D;
	};
}  // namespace DBD
