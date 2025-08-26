#pragma once

#include "API/SKEE.h"
#include "ProfileBase.h"

namespace DBD
{
	class SliderProfile : public ProfileBase
	{
		constexpr static const char* MORPH_KEY = "DBD_Morph";

	public:
		SliderProfile(const std::filesystem::path& a_xmlfilePath, bool a_isMale, SKEE::IBodyMorphInterface* a_interface);
		~SliderProfile() = default;

		void Apply(RE::Actor* a_target) const override;
		bool IsApplicable(RE::Actor* a_target) const override;

		static void DeleteMorphs(RE::Actor* a_target, SKEE::IBodyMorphInterface* a_interface);

	private:
		bool isMale;

		SKEE::IBodyMorphInterface* transformInterface;
		std::map<std::string, std::pair<int32_t, int32_t>, StringComparator> sliders;
	};

}  // namespace DBD
