#pragma once

namespace Papyrus
{
	void Reset3D(STATICARGS, RE::Actor* a_target);

	std::vector<RE::BSFixedString> GetTextureProfiles(STATICARGS, RE::Actor* a_target);
	std::vector<RE::BSFixedString> GetSliderProfiles(STATICARGS, RE::Actor* a_target);

	bool ApplyTextureProfile(STATICARGS, RE::Actor* a_target, RE::BSFixedString a_profile);
	bool ApplySliderProfile(STATICARGS, RE::Actor* a_target, RE::BSFixedString a_profile);

	std::vector<RE::BSFixedString> GetProfiles(STATICARGS, RE::Actor* a_target);
	void ClearProfiles(STATICARGS, RE::Actor* a_target, bool a_exclude);

	inline bool RegisterFunctions(VM* a_vm)
	{
		REGISTERFUNC(Reset3D, "DynamicBodyDistribution", true);

		REGISTERFUNC(GetTextureProfiles, "DynamicBodyDistribution", true);
		REGISTERFUNC(GetSliderProfiles, "DynamicBodyDistribution", true);

		REGISTERFUNC(ApplyTextureProfile, "DynamicBodyDistribution", true);
		REGISTERFUNC(ApplySliderProfile, "DynamicBodyDistribution", true);

		REGISTERFUNC(GetProfiles, "DynamicBodyDistribution", true);
		REGISTERFUNC(ClearProfiles, "DynamicBodyDistribution", true);

		return true;
	}
}  // namespace Papyrus
