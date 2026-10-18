#pragma once

namespace DBD::Data
{
	class TexturePack
	{
		constexpr static inline std::string_view DATA_PREFIX { "data"sv };
		constexpr static inline std::string_view TEXTURE_PREFIX { "textures"sv };

	public:
		using TextureMap = std::map<std::string_view, std::string_view>;

	public:
		/// @brief Constructs a TexturePack from a JSON file path.
		/// @throws std::runtime_error if the JSON is invalid or cannot be parsed.
		TexturePack(const fs::path& a_jsonFilePath);
		~TexturePack() = default;

		/// @return The name of the texture pack.
		std::string_view GetName() const noexcept { return _name; }

		/// @return The description of the texture pack.
		std::string_view GetDescription() const noexcept { return _description; }

		/// @brief Gathers the texture overrides that would be applied to the given actor or object.
		/// @param a_actor The actor to apply the texture pack to.
		/// @param a_object The object to apply the texture pack to.
		/// @param [out] a_textureOverrides A map to store the texture overrides to be applied.
		/// @return True if any texture overrides were found, false otherwise.
		bool WriteMap(RE::Actor* a_actor, TextureMap& a_textureOverrides) const;
		bool WriteMap(RE::NiAVObject* a_object, TextureMap& a_textureOverrides) const;

		/// @brief Checks if the texture pack has any replacements for the given actor or object.
		/// @param a_actor The actor to check for replacements.
		/// @param a_object The object to check for replacements.
		/// @return True if any replacements exist, false otherwise.
		/// @note This behaves as a dry-run of Apply. If application is intended, use Apply directly.
		_NODISCARD bool HasReplacements(RE::Actor* a_actor) const;
		_NODISCARD bool HasReplacements(RE::NiAVObject* a_object) const;

	private:
		std::string _name;
		std::string _description;
		std::map<std::string, std::string> _textureMappings;
	};
}  // namespace DBD::Data
