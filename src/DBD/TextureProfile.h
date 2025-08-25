#pragma once

#include "ProfileBase.h"

namespace DBD
{
	class TextureProfile : public ProfileBase
	{
		using VisitControl = RE::BSVisit::BSVisitControl;
		using Feature = RE::BSLightingShaderMaterialBase::Feature;
		using MaterialBase = RE::BSLightingShaderMaterialBase;
		using MaterialFacegen = RE::BSLightingShaderMaterialFacegen;
		using Texture = RE::BSTextureSet::Texture;
		using TextureData = std::array<std::string, Texture::kTotal>;

		static constexpr std::string_view PATH_PREFIX_CUT{ "data/textures/"sv };

	public:
		TextureProfile(const fs::directory_entry& a_textureFolder);
		~TextureProfile() = default;

		void Apply(RE::Actor* a_target) const override;
		bool IsApplicable(RE::Actor* a_target) const override;

		void ApplyHeadTexture(RE::Actor* a_target) const;
		void ApplySkinTexture(RE::Actor* a_target) const;

	private:
		static std::string GetSubfolderKey(std::string a_path);

	private:
		RE::BSFixedString textureRoot;
		std::map<std::string, std::string, StringComparator> textures;
		std::map<std::string, std::string, StringComparator> headNormals;
		std::map<std::string, std::string, StringComparator> headNormalsVampire;
	};

}  // namespace DBD
