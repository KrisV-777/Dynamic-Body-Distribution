#pragma once

#include "API/SKEE.h"

namespace DBD
{
	class SliderProfile
	{
		constexpr static const char* MORPH_KEY = "DBD_Morph";

	public:
		SliderProfile(const std::filesystem::path& a_xmlfilePath);
		~SliderProfile() = default;

		RE::BSFixedString GetName() const { return name; }
		bool ApplyPreset(RE::Actor* a_target) const;
		bool IsPrivate() const { return isPrivate; }

	private:
		RE::BSFixedString name;
		bool isPrivate;

		SKEE::IBodyMorphInterface* transformInterface;
		std::map<std::string, std::pair<int32_t, int32_t>, StringComparator> sliders;
	};

}  // namespace DBD
