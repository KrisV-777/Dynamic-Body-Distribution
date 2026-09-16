#pragma once

#include <shared/KrisV/Singleton.h>

#include "Data/Rule.h"

namespace DBD
{
	class Database
	{
		static constexpr const char* TEXTURE_ROOT_PATH{ "Data\\Textures\\DBD" };
		static constexpr const char* RACEMENU_ROOT_PATH{ "Data\\SKSE\\Plugins\\CharGen\\Presets" };
		static constexpr const char* SLIDER_ROOT_PATH{ "Data\\CalienteTools\\BodySlide\\SliderPresets" };
		static constexpr const char* RULES_ROOT_PATH{ "Data\\SKSE\\DBD\\Rules" };

	public:
		explicit Database();
		~Database() = default;

		_NODISCARD std::shared_ptr<Data::TexturePack> SelectTexturePack(RE::Actor* a_actor) const;
		_NODISCARD std::shared_ptr<Data::BodyslidePreset> SelectBodyslidePreset(RE::Actor* a_actor) const;
		_NODISCARD std::shared_ptr<Data::RaceMenuPreset> SelectRaceMenuPreset(RE::Actor* a_actor) const;

		_NODISCARD std::shared_ptr<Data::TexturePack> FindTexturePackByName(std::string_view name) const;
		_NODISCARD std::shared_ptr<Data::BodyslidePreset> FindBodyslidePresetByName(std::string_view name) const;
		_NODISCARD std::shared_ptr<Data::RaceMenuPreset> FindRaceMenuPresetByName(std::string_view name) const;

		template <typename T>
		void ForEach(std::function<void(const T*)> a_callback) const
		{
			if constexpr (std::is_same_v<T, Data::TexturePack>) {
				for (const auto& profile : _texturePacks) {
					a_callback(profile.get());
				}
			} else if constexpr (std::is_same_v<T, Data::BodyslidePreset>) {
				for (const auto& profile : _bodyslidePresets) {
					a_callback(profile.get());
				}
			} else if constexpr (std::is_same_v<T, Data::RaceMenuPreset>) {
				for (const auto& profile : _raceMenuPresets) {
					a_callback(profile.get());
				}
			} else if constexpr (std::is_same_v<T, Data::Rule>) {
				for (const auto& rule : _rules) {
					a_callback(&rule);
				}
			} else {
				static_assert(false, "Unsupported type for ForEach");
			}
		}

	private:
		void LoadTexturePacks();
		void LoadBodyslidePresets(SKEE::IBodyMorphInterface* morphInterface);
		void LoadRaceMenuPresets(SKEE::IPresetInterface* presetInterface);
		std::vector<Data::PreloadedRule> PreloadRules();

		std::vector<Data::Rule> _rules;
		std::vector<std::shared_ptr<Data::TexturePack>> _texturePacks;
		std::vector<std::shared_ptr<Data::BodyslidePreset>> _bodyslidePresets;
		std::vector<std::shared_ptr<Data::RaceMenuPreset>> _raceMenuPresets;
	};
}  // namespace DBD
