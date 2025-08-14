#pragma once

#include "TextureProfile.h"
#include "Util/Singleton.h"

namespace DBD
{
	class Distribution :
		public Singleton<Distribution>
	{
		static constexpr const char* TEXTURE_ROOT_PATH{ "Data\\Textures\\DBD" };

	public:
		void Initialize();

		bool DistributeTexture(RE::Actor* a_target, const RE::BSFixedString& a_skinid);

	private:
		std::map<RE::BSFixedString, TextureProfile, FixedStringComparator> skins;
	};

}  // namespace DBD
