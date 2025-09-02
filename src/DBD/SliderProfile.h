#pragma once

#include <rapidxml/rapidxml.hpp>

#include "API/SKEE.h"
#include "ProfileBase.h"

namespace DBD
{
	class SliderProfile : public ProfileBase
	{
		using SliderRange = std::pair<int32_t, int32_t>;

		constexpr static const char* MORPH_KEY = "DBD_Morph";

	public:
		static std::vector<std::shared_ptr<SliderProfile>> LoadProfiles(const std::filesystem::path& a_xmlfilePath, bool a_isMale, SKEE::IBodyMorphInterface* a_interface);
		SliderProfile(const rapidxml::xml_node<char>* a_node, bool a_isMale, bool isPrivate, SKEE::IBodyMorphInterface* a_interface);
		SliderProfile(const SliderProfile& a_other, RE::BSFixedString a_name) :
			ProfileBase(a_name, ".xml"), isMale(a_other.isMale), transformInterface(a_other.transformInterface), sliders(a_other.sliders) {}
		~SliderProfile() = default;

		void Apply(RE::Actor* a_target) const override;
		bool IsApplicable(RE::Actor* a_target) const override;

		static void DeleteMorphs(RE::Actor* a_target, SKEE::IBodyMorphInterface* a_interface);

	private:
		bool isMale;

		SKEE::IBodyMorphInterface* transformInterface;
		std::map<std::string, SliderRange, StringComparator> sliders;
	};

}  // namespace DBD
