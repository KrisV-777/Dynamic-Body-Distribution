#include "Functions.h"

#include "DBD/Distribution.h"

namespace Papyrus
{
	void Reset3D(STATICARGS, RE::Actor* a_target)
	{
		if (!a_target) {
			TRACESTACK("Papyrus::Reset3D - target is none");
			return;
		}

		a_target->DoReset3D(true);
	}

	std::vector<RE::BSFixedString> GetTextureProfiles(STATICARGS, RE::Actor* a_target)
	{
		std::vector<RE::BSFixedString> profiles;
		DBD::Distribution::GetSingleton()->GetDatabase().ForEach<DBD::Data::TexturePack>([&](const DBD::Data::TexturePack* a_profile) {
			if (!a_target || a_profile->HasReplacements(a_target)) {
				profiles.emplace_back(a_profile->GetName());
			}
		});
		return profiles;
	}

	std::vector<RE::BSFixedString> GetSliderProfiles(STATICARGS, RE::Actor* a_target)
	{
		std::vector<RE::BSFixedString> profiles;
		DBD::Distribution::GetSingleton()->GetDatabase().ForEach<DBD::Data::BodyslidePreset>([&](const DBD::Data::BodyslidePreset* a_profile) {
			if (!a_target || a_profile->IsApplicable(a_target)) {
				profiles.emplace_back(a_profile->GetName());
			}
		});
		return profiles;
	}

	bool ApplyTextureProfile(STATICARGS, RE::Actor* a_target, RE::BSFixedString a_profile)
	{
		if (!a_target) {
			TRACESTACK("Papyrus::ApplyTextureProfile - target is none");
			return false;
		}
		return DBD::Distribution::GetSingleton()->ApplyTextureProfile(a_target, a_profile.data());
	}

	bool ApplySliderProfile(STATICARGS, RE::Actor* a_target, RE::BSFixedString a_profile)
	{
		if (!a_target) {
			TRACESTACK("Papyrus::ApplySliderProfile - target is none");
			return false;
		}
		return DBD::Distribution::GetSingleton()->ApplySliderProfile(a_target, a_profile.data());
	}

	std::vector<RE::BSFixedString> GetProfiles(STATICARGS, RE::Actor* a_target)
	{
		if (!a_target) {
			TRACESTACK("Papyrus::GetProfiles - target is none");
			return { "", "" };
		}
		const auto profileNames = DBD::Distribution::GetSingleton()->GetProfileNames(a_target);
		std::vector<RE::BSFixedString> profiles;
		profiles.reserve(profileNames.size());
		std::ranges::transform(profileNames, std::back_inserter(profiles),
			[](std::string_view a_name) { return RE::BSFixedString(a_name); });
		return profiles;
	}

	void ClearProfiles(STATICARGS, RE::Actor* a_target, bool a_exclude)
	{
		if (!a_target) {
			TRACESTACK("Papyrus::ClearProfiles - target is none");
			return;
		}
		DBD::Distribution::GetSingleton()->ClearCache(a_target);
		if (!a_exclude) {
			a_target->DoReset3D(false);
		}
	}

}  // namespace Papyrus
