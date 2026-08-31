#include "Hooks.h"

#include <detours.h>

#include "DBD/Distribution.h"
#include "shared/KrisV/Util/String.h"

namespace DBD
{
	void Hooks::Install()
	{
		logger::info("Installing Hooks");

		REL::Relocation<std::uintptr_t> target{ REL::VariantID(15535, 15712, 0x01DB9E0) };
		const uintptr_t addr = target.address();
		_UpdateBipedAnim = (decltype(_UpdateBipedAnim))addr;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)_UpdateBipedAnim, (PBYTE)&UpdateBipedAnim);
		if (DetourTransactionCommit() != NO_ERROR) {
			logger::error("Failed to install hook on UpdateBipedAnim");
		}

		REL::Relocation<std::uintptr_t> bsfacegenninode_vt{ RE::BSFaceGenNiNode::VTABLE[0] };
		_FixSkinInstances = bsfacegenninode_vt.write_vfunc(REL::Module::IsVR() ? 0x3F : 0x3E, FixSkinInstances);

		logger::debug("Hooks installed");
	}

	void Hooks::FixSkinInstances(RE::BSFaceGenNiNode& a_this, RE::NiNode* a_skeleton, bool a_arg2)
	{
		_FixSkinInstances(a_this, a_skeleton, a_arg2);

		if (!a_skeleton) {
			return;
		}
		const auto userRef = a_this.GetUserData();
		if (!userRef || !userRef->Is(RE::FormType::ActorCharacter)) {
			return;
		}
		const auto userAct = userRef->As<RE::Actor>();
		assert(userAct);

		DBD::Distribution::GetSingleton()->ApplyProfiles(userAct, a_skeleton);
	}

	RE::NiAVObject* Hooks::UpdateBipedAnim(RE::BipedAnim& a_this, RE::NiNode* a_skeleton, RE::BSFadeNode* a3, RE::BIPED_OBJECT a_biped, uint64_t a5, uint64_t a6, uint64_t a7)
	{
		const auto ret = _UpdateBipedAnim(a_this, a_skeleton, a3, a_biped, a5, a6, a7);

		const auto actorHandle = a_this.actorRef.get();
		const auto actor = actorHandle ? actorHandle->As<RE::Actor>() : nullptr;
		if (actor && ret) {
			DBD::Distribution::GetSingleton()->ApplyProfiles(actor, ret);
		}

		return ret;
	}

}  // namespace DBD
