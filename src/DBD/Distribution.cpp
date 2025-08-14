#include "Distribution.h"


namespace DBD
{
	void Distribution::Initialize()
	{
		logger::info("Loading Texture Sets");
		if (!fs::exists(TEXTURE_ROOT_PATH)) {
			logger::critical("Path to textures does not exist");
		} else {
			for (auto& folder : fs::directory_iterator{ TEXTURE_ROOT_PATH }) {
				if (!folder.is_directory())
					continue;
				try {
					TextureProfile profile{ folder };
					auto name = profile.GetName();
					skins.emplace(std::make_pair(name, profile));
					logger::info("Added Texture Set: {}", name);
				} catch (const std::exception& e) {
					logger::error("Failed to add Texture Set: {}. Error: {}", folder.path().filename().string(), e.what());
				}
			}
			logger::info("Loaded {} Texture Sets", skins.size());
		}
		logger::info("Loading Body Presets");
		// TODO: implement
	}

	bool Distribution::DistributeTexture(RE::Actor* a_target, const RE::BSFixedString& a_skinid)
	{
		auto it = skins.find(a_skinid);
		if (it != skins.end()) {
			const auto& textureProfile = it->second;
			textureProfile.ApplyTexture(a_target);
			return true;
		}
		return false;
	}

}  // namespace DBD
