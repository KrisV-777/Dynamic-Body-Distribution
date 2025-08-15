#pragma once

#include "SliderProfile.h"
#include "TextureProfile.h"
#include "Util/Singleton.h"

namespace DBD
{
	class Distribution :
		public Singleton<Distribution>
	{
		static constexpr const char* TEXTURE_ROOT_PATH{ "Data\\Textures\\DBD" };
		static constexpr const char* SLIDER_ROOT_PATH{ "Data\\SKSE\\DBD\\Sliders" };

	public:
		void Initialize();

		bool DistributeTexture(RE::Actor* a_target, const RE::BSFixedString& a_skinid);
		bool DistributeBody(RE::Actor* a_target, const RE::BSFixedString& a_bodyid);

	private:
		std::map<RE::BSFixedString, TextureProfile, FixedStringComparator> skins;
		std::map<RE::BSFixedString, SliderProfile, FixedStringComparator> slidersMale;
		std::map<RE::BSFixedString, SliderProfile, FixedStringComparator> slidersFemale;
	};

}  // namespace DBD
