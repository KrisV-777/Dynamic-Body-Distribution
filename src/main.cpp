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
			const auto dist = DBD::Distribution::GetSingleton();
			const auto player = RE::PlayerCharacter::GetSingleton();
			dist->DistributeTexture(player, "pureskin");

			// auto playerModel = player->Get3D(false);


			// auto plbase = player->GetActorBase();
			// plbase->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kFemale);
			// DBD::Distribution::GetSingleton()->DistributeTexture(player, "pureskin");

			// auto ingrid = RE::TESForm::LookupByID<RE::Actor>(0xAAF9A);
			// if (ingrid) {
			// 	auto inbase = ingrid->GetActorBase();
			// 	DBD::Distribution::GetSingleton()->DistributeTexture(ingrid, "pureskin");
			// 	[[maybe_unused]] auto in_headdata = inbase->headRelatedData;
			// 	[[maybe_unused]] auto in_face = inbase->GetHeadPartByType(RE::TESNPC::HeadPartType::kFace);
			// }

			// [[maybe_unused]] auto pl_headdata = plbase->headRelatedData;
			// [[maybe_unused]] auto pl_face = plbase->GetHeadPartByType(RE::TESNPC::HeadPartType::kFace);
			// logger::info("nop");
		}
		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	const auto plugin = SKSE::PluginDeclaration::GetSingleton();
	const auto InitLogger = [&plugin]() -> bool {
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path)
			return false;
		*path /= std::format("{}.log", plugin->GetName());
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif
		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
#ifndef NDEBUG
		log->set_level(spdlog::level::trace);
		log->flush_on(spdlog::level::trace);
#else
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
#endif
		spdlog::set_default_logger(std::move(log));
#ifndef NDEBUG
		spdlog::set_pattern("%s(%#): [%T] [%^%l%$] %v"s);
#else
		spdlog::set_pattern("[%T] [%^%l%$] %v"s);
#endif

		logger::info("{} v{}", plugin->GetName(), plugin->GetVersion());
		return true;
	};
	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible");
		return false;
	} else if (!InitLogger()) {
		return false;
	}

	SKSE::Init(a_skse);

	DBD::Distribution::GetSingleton()->Initialize();

	const auto msging = SKSE::GetMessagingInterface();
	if (!msging->RegisterListener("SKSE", SKSEMessageHandler)) {
		logger::critical("Failed to register Listener");
		return false;
	}

	logger::info("{} loaded"sv, plugin->GetName());

	return true;
}