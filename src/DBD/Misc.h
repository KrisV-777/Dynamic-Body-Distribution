#pragma once

template <class T>
constexpr inline void ToLower(T& str)
{
	std::transform(str.cbegin(), str.cend(), str.begin(), [](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });
}

inline bool IsVampire(const RE::Actor* a_target)
{
	const auto vampire = RE::TESForm::LookupByID<RE::BGSKeyword>(0xA82BB);
	return a_target->HasKeyword(vampire);
}

inline std::string GetModName(const RE::FormID a_id)
{
	auto modidx = a_id >> 24;
	if (modidx == 0xFF)
		return "";

	const RE::TESFile* file;
	if (modidx == 0xFE) {
		file = RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByIndex((a_id & 0x00FFF000) >> 12);
	} else {
		file = RE::TESDataHandler::GetSingleton()->LookupLoadedModByIndex(modidx);
	}
	return file ? std::string(file->GetFilename()) : "";
}

inline std::string GetIDHex(RE::FormID a_id)
{
	std::stringstream s;
	s << std::setfill('0') << std::setw(6) << std::hex << (a_id & 0xFFFFFF);
	return s.str();
}

inline void SetFaceTextureSet(RE::Actor* a_target, RE::BGSTextureSet* a_texture)
{
	const auto base = a_target->GetActorBase();
	if (!base)
		return;

	const auto facenode = a_target->GetFaceNode();
	// const auto facepart = base->GetHeadPartByType(RE::TESNPC::HeadPartType::kFace);
	// if (!facenode || !facepart)
	if (!facenode)
		return;

	// const auto face = facenode->GetObjectByName(facepart->formEditorID);
	// const auto facegeo = face ? face->AsGeometry() : nullptr;
	const auto facegen_geo = facenode->GetFirstGeometryOfShaderType(RE::BSShaderMaterial::Feature::kFaceGen);
	if (!facegen_geo)
		return;
	// if (!facegeo || !shader)
	// 	return;

	auto effect = facegen_geo->properties[RE::BSGeometry::States::kEffect];
	auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect.get());
	const auto material = static_cast<RE::BSLightingShaderMaterialFacegen*>(lightingShader->material);
	RE::NiPointer tex{ a_texture };
	material->SetTextureSet(tex);
}
