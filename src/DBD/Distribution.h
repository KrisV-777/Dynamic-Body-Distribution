#pragma once

#include "SkinData.h"

namespace DBD
{
	class Distribution :
		public Singleton<Distribution>
	{
	public:
		void Initialize();

		bool DistributeTexture(RE::Actor* a_target, const std::string& a_skinid);

	private:
		

	private:
		std::map<std::string, SkinData*> skins;
	};

}  // namespace DBD
