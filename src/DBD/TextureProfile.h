#pragma once

#include "ProfileBase.h"

namespace DBD
{
	class TextureProfile : public ProfileBase
	{
		using Feature = RE::BSLightingShaderMaterialBase::Feature;
		using MaterialBase = RE::BSLightingShaderMaterialBase;
		using Texture = RE::BSTextureSet::Texture;
		using TextureData = std::array<std::string, Texture::kTotal>;

		static constexpr std::string_view PATH_PREFIX_CUT{ "data/textures/"sv };

	public:
		TextureProfile(const fs::directory_entry& a_textureFolder);
		~TextureProfile() = default;

		void Apply(RE::Actor* a_target) const override;
		bool IsApplicable(RE::Actor* a_target) const override;

	private:
		void ReadTextureFiles(const fs::directory_entry& a_textureFolder);
		void ReadTextureFilesExtra(const fs::directory_entry& a_textureFolder);

		static std::string GetSubfolderKey(std::string a_path);
		static bool HasBodyTextures(const fs::directory_entry& a_textureFolder);
		static std::optional<Texture> GetTextureType(const std::string_view a_file);

		void ApplyHeadTexture(RE::Actor* a_target) const;
		void ApplySkinTexture(RE::Actor* a_target) const;
		void ApplyTextureImpl(RE::NiAVObject* a_object, const TextureData& a_texture, const std::string& a_normal = ""s) const;

	private:
		std::string bodyTexturePath;
		TextureData textureSetHead;
		TextureData textureSetHeadVampire;
		TextureData textureSetBody;
		TextureData textureSetHands;
		std::map<std::string, std::string, StringComparator> headNormals;
		std::map<std::string, std::string, StringComparator> headNormalsVampire;
	};

}  // namespace DBD
