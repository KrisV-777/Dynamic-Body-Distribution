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
		logger::info("Resetting 3D for Actor: {}", a_this.formID);
		const auto dist = DBD::Distribution::GetSingleton();
		const auto profiles = dist->SelectProfiles(&a_this);

		using Index = Distribution::ProfileIndex;
		for (size_t i = 0; i < Index::Total; i++) {
			if (!profiles[i])
				continue;
			if (i == Index::TextureId) {
				/*	Textures have their application split, as applying body textures requires a full 3D resets AFTER the update.
					However, this reset will also undo any texture overwrites on the model itself (such as face textures). It is possible
					to update body textures on the model itself, however doing so requires the body textures to be updated every time the
					model changes, thus requiring additional hooks and updates, which I want to avoid to keep invasiveness to a minimum.
				*/
				const auto& textureProfile = static_cast<TextureProfile*>(profiles[i]);
				textureProfile->ApplySkinTexture(&a_this);
				_DoReset3D(a_this, a_updateWeight);
				textureProfile->ApplyHeadTexture(&a_this);
			} else {
				profiles[i]->Apply(&a_this);
			}
		}

		if (!profiles[Index::TextureId]) {
			// Dont reset 3d twice
			_DoReset3D(a_this, a_updateWeight);
		}
	}

}  // namespace DBD
