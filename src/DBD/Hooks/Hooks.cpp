#include "Hooks.h"

#include <detours.h>

#include "DBD/Distribution.h"
#include "Util/StringUtil.h"

namespace DBD
{
	void Hooks::Install()
	{
		REL::Relocation<std::uintptr_t> char_vt{ RE::Character::VTABLE[0] };
		_Load3D = char_vt.write_vfunc(0x6A, Load3D);

		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(39181, 40255) };
		const uintptr_t addr = target.address();
		_DoReset3D = (DoReset3DType)addr;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)_DoReset3D, (PBYTE)&DoReset3D);
		if (DetourTransactionCommit() != NO_ERROR) {
			logger::error("Failed to install hook on DoReset3D");
		}
	}

	RE::NiAVObject* Hooks::Load3D(RE::Character& a_this, bool a_arg1)
	{
		std::thread([id = a_this.formID]() {
			// Im aware how ugly this is, but I couldn't find a hook in which this can be done 'cleanly'
			std::this_thread::sleep_for(2s);
			SKSE::GetTaskInterface()->AddTask([id]() {
				const auto actor = RE::TESForm::LookupByID<RE::Actor>(id);
				if (actor && actor->Is3DLoaded()) {
					actor->DoReset3D(true);
				}
			});
		}).detach();
		return _Load3D(a_this, a_arg1);
	}

	void Hooks::DoReset3D(RE::Actor& a_this, bool a_updateWeight)
	{
		_DoReset3D(a_this, a_updateWeight);

		logger::info("Resetting 3D for Actor: {}", a_this.formID);
		const auto dist = DBD::Distribution::GetSingleton();
		const auto profiles = dist->SelectProfiles(&a_this);

		for (auto&& profile : profiles) {
			if (profile) {
				profile->Apply(&a_this);
			}
		}
	}

}  // namespace DBD
