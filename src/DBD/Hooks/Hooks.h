#pragma once

namespace DBD
{
	struct Hooks
	{
		Hooks() = delete;

		static void Install();

	private:
		static RE::NiAVObject* UpdateBipedAnim(RE::BipedAnim& a_this, RE::NiNode* a_skeleton, RE::BSFadeNode* a3, RE::BIPED_OBJECT a_biped, uint64_t a5, uint64_t a6, uint64_t a7);
		static inline REL::Relocation<decltype(UpdateBipedAnim)> _UpdateBipedAnim;

		static void FixSkinInstances(RE::BSFaceGenNiNode& a_this, RE::NiNode* a_skeleton, bool a_arg2);
		static inline REL::Relocation<decltype(FixSkinInstances)> _FixSkinInstances;
	};
}  // namespace DBD
