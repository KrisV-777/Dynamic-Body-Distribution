#include "ReferenceConfiguration.h"

#include "shared/KrisV/Util/String.h"

namespace DBD::Data
{
	ReferenceConfiguration::ReferenceConfiguration(RE::Actor* a_target, const Database& a_database) :
		formID(a_target ? a_target->formID : 0),
		handle(a_target ? a_target->CreateRefHandle() : RE::ActorHandle{}),
		texturePreset(a_target ? a_database.BuildTexturePreset(a_target) : nullptr),
		bodyslidePreset(a_target ? a_database.SelectBodyslidePreset(a_target) : nullptr),
		raceMenuPreset(a_target ? a_database.SelectRaceMenuPreset(a_target) : nullptr)
	{}

	ReferenceConfiguration::ReferenceConfiguration(ReferenceConfiguration&& a_rhs) noexcept
	{
		std::unique_lock lock{ a_rhs._mutex };
		formID = a_rhs.formID;
		handle = std::move(a_rhs.handle);
		bodyslidePreset = std::move(a_rhs.bodyslidePreset);
		raceMenuPreset = std::move(a_rhs.raceMenuPreset);
		texturePreset = std::move(a_rhs.texturePreset);
	}

	ReferenceConfiguration& ReferenceConfiguration::operator=(ReferenceConfiguration&& a_rhs) noexcept
	{
		if (this == std::addressof(a_rhs)) {
			return *this;
		}
		std::scoped_lock lock{ _mutex, a_rhs._mutex };
		formID = a_rhs.formID;
		handle = std::move(a_rhs.handle);
		bodyslidePreset = std::move(a_rhs.bodyslidePreset);
		raceMenuPreset = std::move(a_rhs.raceMenuPreset);
		texturePreset = std::move(a_rhs.texturePreset);
		return *this;
	}

	bool ReferenceConfiguration::AddTexturePack(std::shared_ptr<TexturePack> a_texturePack, bool a_update)
	{
		std::unique_lock lock{ _mutex };
		const auto actor = handle.get();
		if (!actor) {
			return false;
		}
		if (texturePreset) {
			a_texturePack->WriteMap(actor.get(), texturePreset->data);
			texturePreset->packs.emplace(a_texturePack);
			if (a_update) {
				texturePreset->Apply(actor.get());
			}
			return true;
		}
		return false;
	}

	bool ReferenceConfiguration::SetBodyslidePreset(std::shared_ptr<BodyslidePreset> a_bodyslidePreset, bool a_update)
	{
		std::unique_lock lock{ _mutex };
		const auto actor = handle.get();
		if (!actor) {
			return false;
		}
		if (a_bodyslidePreset && a_bodyslidePreset->IsApplicable(actor.get())) {
			bodyslidePreset = a_bodyslidePreset;
			if (a_update) {
				bodyslidePreset->Apply(actor.get());
			}
			return true;
		}
		return false;
	}

	bool ReferenceConfiguration::SetRaceMenuPreset(std::shared_ptr<RaceMenuPreset> a_raceMenuPreset, bool a_update)
	{
		std::unique_lock lock{ _mutex };
		const auto actor = handle.get();
		if (!actor) {
			return false;
		}
		if (a_raceMenuPreset) {
			raceMenuPreset = a_raceMenuPreset;
			if (a_update) {
				raceMenuPreset->Apply(actor.get());
			}
			return true;
		}
		return false;
	}

	void ReferenceConfiguration::ApplyProfiles(RE::NiAVObject* a_part) const
	{
		std::unique_lock lock{ _mutex };
		const auto actor = handle.get();
		if (!actor) {
			logger::warn("Actor {:X} is no longer available, skipping profile application", formID);
			return;
		}
		if (texturePreset) {
			if (!a_part) {
				a_part = actor->Get3D();
			}
			if (!a_part) {
				logger::warn("Actor {} has no 3D model, skipping texture pack application", actor->formID);
			} else {
				texturePreset->Apply(a_part);
			}
		}
		if (bodyslidePreset) {
			bodyslidePreset->Apply(actor.get());
		}
		if (raceMenuPreset) {
			raceMenuPreset->Apply(actor.get());
		}
	}

	std::array<std::string, 3> ReferenceConfiguration::GetProfileNames() const
	{
		std::unique_lock lock{ _mutex };
		return {
			texturePreset ? texturePreset->GetName() : std::string{},
			bodyslidePreset ? std::string{ bodyslidePreset->GetName() } : std::string{},
			raceMenuPreset ? std::string{ raceMenuPreset->GetName() } : std::string{}
		};
	}

	void ReferenceConfiguration::Save(SKSE::SerializationInterface* a_intfc, uint32_t) const
	{
		std::unique_lock lock{ _mutex };
		a_intfc->WriteRecordData(formID);
		::stl::write_string(a_intfc, texturePreset ? texturePreset->GetName() : "");
		::stl::write_string(a_intfc, bodyslidePreset ? bodyslidePreset->GetName() : "");
		::stl::write_string(a_intfc, raceMenuPreset ? raceMenuPreset->GetName() : "");
	}

	std::optional<ReferenceConfiguration> ReferenceConfiguration::Load(SKSE::SerializationInterface* a_intfc, const Database& a_database, uint32_t)
	{
		RE::FormID formID;
		a_intfc->ReadRecordData(formID);
		if (!a_intfc->ResolveFormID(formID, formID)) {
			logger::warn("Error reading formID: {:X}", formID);
			return std::nullopt;
		}
		RE::TESForm* form = RE::TESForm::LookupByID(formID);
		RE::Actor* actor = form ? form->As<RE::Actor>() : nullptr;
		if (!actor) {
			logger::warn("Failed to cast form to actor for formID: {:X}", formID);
			return std::nullopt;
		}
		std::array<std::string, 3> profileNames;
		for (auto& name : profileNames) {
			if (!::stl::read_string(a_intfc, name)) {
				logger::error("Failed to read profiles for form {:X}", formID);
				return std::nullopt;
			}
		}
		const auto& [textureProfile, bodyslideProfile, raceMenuProfile] = profileNames;
		std::vector<std::string_view> textureNames = Util::StringSplit(textureProfile, ",");
		ReferenceConfiguration ret{ actor, a_database };
		if (ret.texturePreset) {
			for (auto& name : textureNames) {
				if (std::ranges::none_of(ret.texturePreset->packs, [&](auto& a_pack) { return a_pack && a_pack->GetName() == name; })) {
					if (auto newpack = a_database.FindTexturePackByName(name)) {
						ret.AddTexturePack(newpack, false);
					}
				}
			}
		}
		{
			std::unique_lock lock{ ret._mutex };
			if (ret.bodyslidePreset && ret.bodyslidePreset->GetName() != bodyslideProfile) {
				ret.bodyslidePreset = a_database.FindBodyslidePresetByName(bodyslideProfile);
			}
			if (ret.raceMenuPreset && ret.raceMenuPreset->GetName() != raceMenuProfile) {
				ret.raceMenuPreset = a_database.FindRaceMenuPresetByName(raceMenuProfile);
			}
		}
		return std::make_optional(std::move(ret));
	}
} 