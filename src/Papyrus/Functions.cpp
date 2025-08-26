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
		DBD::Distribution::GetSingleton()->ForEachTextureProfile([&](const DBD::TextureProfile* a_profile) {
			if (!a_target || a_profile->IsApplicable(a_target)) {
				profiles.emplace_back(a_profile->GetName());
			}
		});
		return profiles;
	}

	std::vector<RE::BSFixedString> GetSliderProfiles(STATICARGS, RE::Actor* a_target)
	{
		std::vector<RE::BSFixedString> profiles;
		DBD::Distribution::GetSingleton()->ForEachSliderProfile([&](const DBD::SliderProfile* a_profile) {
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
		const auto retVal = DBD::Distribution::GetSingleton()->GetProfiles(a_target);
		std::vector<RE::BSFixedString> profiles;
		profiles.reserve(retVal.size());
		std::transform(retVal.begin(), retVal.end(), std::back_inserter(profiles),
			[](const auto* profile) { return profile ? profile->GetName() : ""; });
		return profiles;
	}

	void ClearProfiles(STATICARGS, RE::Actor* a_target, bool a_exclude)
	{
		if (!a_target) {
			TRACESTACK("Papyrus::ClearProfiles - target is none");
			return;
		}
		DBD::Distribution::GetSingleton()->ClearProfiles(a_target, a_exclude);
	}

}  // namespace Papyrus
