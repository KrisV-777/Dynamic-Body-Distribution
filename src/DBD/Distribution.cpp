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

		logger::info("Loading Slider Sets");
		for (auto& type : std::vector{ "male", "female" }) {
			const auto rootFolder = std::format("{}/{}", SLIDER_ROOT_PATH, type);
			if (!fs::exists(rootFolder)) {
				logger::critical("Path to slider '{}' does not exist", type);
				continue;
			}
			auto& sliderMap = (type == "male"s) ? slidersMale : slidersFemale;
			for (auto& file : fs::directory_iterator{ rootFolder }) {
				if (!file.is_regular_file())
					continue;
				const auto fileName = file.path().filename().string();
				if (!fileName.ends_with(".xml")) {
					logger::warn("Skipping non-XML file: {}", fileName);
					continue;
				}
				try {
					SliderProfile profile{ file.path().string() };
					auto name = profile.GetName();
					sliderMap.emplace(std::make_pair(name, profile));
					logger::info("Added Slider Set: {}", name);
				} catch (const std::exception& e) {
					logger::error("Failed to add Slider Set: {}. Error: {}", fileName, e.what());
				}
			}
			logger::info("Loaded {} Slider Sets ({})", sliderMap.size(), type);
		}
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

	bool Distribution::DistributeBody(RE::Actor* a_target, const RE::BSFixedString& a_bodyid)
	{
		auto it = slidersFemale.find(a_bodyid);
		if (it != slidersFemale.end()) {
			const auto& sliderProfile = it->second;
			sliderProfile.ApplyPreset(a_target);
			return true;
		}
		return false;
	}

}  // namespace DBD
