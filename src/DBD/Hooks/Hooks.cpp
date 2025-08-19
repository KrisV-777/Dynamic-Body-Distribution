#include "Hooks.h"

#include "DBD/Distribution.h"
#include "Util/StringUtil.h"

namespace DBD
{
	void Hooks::Install()
	{
		REL::Relocation<std::uintptr_t> plu{ RE::PlayerCharacter::VTABLE[0] };
		_Load3DPlayer = plu.write_vfunc(0x6A, Load3DPlayer);

		REL::Relocation<std::uintptr_t> char_vt{ RE::Character::VTABLE[0] };
		_Load3D = char_vt.write_vfunc(0x6A, Load3D);
	}

	inline RE::NiAVObject* Load3DImpl(RE::Character& a_this, RE::NiAVObject* a_retVal)
	{
		if (!a_retVal) {
			return a_retVal;
		}
		std::thread([id = a_this.formID]() {
			// Im fully aware how ugly this is, but Im unable to find a hook in which this can be done 'cleanly'
			// Without the delay here, head textures would never update
			std::this_thread::sleep_for(2s);
			SKSE::GetTaskInterface()->AddTask([id]() {
				const auto actor = RE::TESForm::LookupByID<RE::Actor>(id);
				if (actor && actor->Is3DLoaded()) {
					logger::info("Applying profiles for Actor: {}", id);
				} else {
					logger::info("Actor not found or its 3D is not loaded: {}", id);
					return;
				}
				const auto dist = DBD::Distribution::GetSingleton();
				dist->ApplyProfiles(actor);
			});
		}).detach();
		return a_retVal;
	}

	RE::NiAVObject* Hooks::Load3DPlayer(RE::Character& a_this, bool a_arg1)
	{
		return Load3DImpl(a_this, _Load3DPlayer(a_this, a_arg1));
	}

	RE::NiAVObject* Hooks::Load3D(RE::Character& a_this, bool a_arg1)
	{
		return Load3DImpl(a_this, _Load3D(a_this, a_arg1));
	}

}  // namespace DBD
