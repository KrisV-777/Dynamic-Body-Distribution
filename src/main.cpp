#include "DBD/Distribution.h"

inline void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kSaveGame:
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		{
			DBD::Distribution::GetSingleton()->Initialize();
		}
		break;
	//case SKSE::MessagingInterface::kNewGame:
	case SKSE::MessagingInterface::kPostLoadGame:
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			auto plbase = player->GetActorBase();
			plbase->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kFemale);
			DBD::Distribution::GetSingleton()->DistributeTexture(player, "pureskin");

			auto ingrid = RE::TESForm::LookupByID<RE::Actor>(0xAAF9A);
			if (ingrid) {
				auto inbase = ingrid->GetActorBase();
				DBD::Distribution::GetSingleton()->DistributeTexture(ingrid, "pureskin");
				[[maybe_unused]] auto in_headdata = inbase->headRelatedData;
				[[maybe_unused]] auto in_face = inbase->GetHeadPartByType(RE::TESNPC::HeadPartType::kFace);
			}

			[[maybe_unused]] auto pl_headdata = plbase->headRelatedData;
			[[maybe_unused]] auto pl_face = plbase->GetHeadPartByType(RE::TESNPC::HeadPartType::kFace);
			logger::info("nop");
		}
		break;
	}
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.AuthorName("Scrab Joséline"sv);
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });
	return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION.pack();
	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	const auto InitLogger = []() -> bool {
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path)
			return false;
		*path /= fmt::format(FMT_STRING("{}.log"), Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif
		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(spdlog::level::trace);
		log->flush_on(spdlog::level::trace);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%s(%#): [%^%l%$] %v"s);

		logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());
		return true;
	};
	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	} else if (!InitLogger()) {
		return false;
	}

	SKSE::Init(a_skse);
	logger::info("{} loaded"sv, Plugin::NAME);

	DBD::Distribution::GetSingleton()->Initialize();

	const auto msging = SKSE::GetMessagingInterface();
	if (!msging->RegisterListener("SKSE", SKSEMessageHandler)) {
		logger::critical("Failed to register Listener");
		return false;
	}

	logger::info("Initialization complete");

	return true;
}