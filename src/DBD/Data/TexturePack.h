#pragma once

namespace DBD::Data
{
	class TexturePack
	{
	public:
		/// @brief Constructs a TexturePack from a JSON string.
		/// @throws std::runtime_error if the JSON is invalid or cannot be parsed.
		TexturePack(std::string_view a_json);
		~TexturePack() noexcept;

		/// @return The name of the texture pack.
		std::string_view GetName() const noexcept { return _name; }

		/// @return The description of the texture pack.
		std::string_view GetDescription() const noexcept { return _description; }

		/// @brief Applies the texture pack to the given actor or object.
		/// @param a_actor The actor to apply the texture pack to.
		/// @param a_object The object to apply the texture pack to.
		/// @return True if any replacements occurred, false otherwise.
		bool Apply(RE::Actor* a_actor) const;
		bool Apply(RE::NiAVObject* a_object) const;

		/// @brief Checks if the texture pack has any replacements for the given actor or object.
		/// @param a_actor The actor to check for replacements.
		/// @param a_object The object to check for replacements.
		/// @return True if any replacements exist, false otherwise.
		/// @note This behaves as a dry-run of Apply. If application is intended, use Apply directly.
		_NODISCARD bool HasReplacements(RE::Actor* a_actor) const;
		_NODISCARD bool HasReplacements(RE::NiAVObject* a_object) const;

	private:
		bool OverrideGeometry(RE::BSGeometry* a_geometry) const;
		RE::BSTextureSet* BuildReplacementTextureSet(RE::BSTextureSet* a_originalTextureSet) const;
		void CopyFeatureSpecificTextures(RE::BSLightingShaderMaterialBase* a_source, RE::BSLightingShaderMaterialBase* a_target) const;

		std::string _name;
		std::string _description;
		std::map<std::string, std::string> _textureMappings;

		mutable std::unordered_map<RE::BSTextureSet*, RE::BSTextureSet*> _createdTextureSets;  ///< Existing TexSet -> Replacement
	};
}  // namespace DBD::Data
