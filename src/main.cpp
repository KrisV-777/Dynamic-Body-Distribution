#include "DBD/Distribution.h"
#include "DBD/Hooks/Hooks.h"
#include "DBD/Serialization.h"
#include "Papyrus/Functions.h"

inline void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kSaveGame:
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		DBD::Distribution::GetSingleton()->Initialize();
		break;
	case SKSE::MessagingInterface::kNewGame:
	case SKSE::MessagingInterface::kPostLoadGame:
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

	DBD::Hooks::Install();

	const auto msging = SKSE::GetMessagingInterface();
	if (!msging->RegisterListener("SKSE", SKSEMessageHandler)) {
		logger::critical("Failed to register Listener");
		return false;
	}

	const auto papyrus = SKSE::GetPapyrusInterface();
	if (!papyrus) {
		logger::critical("Failed to get papyrus interface");
		return false;
	}
	papyrus->Register(Papyrus::RegisterFunctions);

	const auto serialization = SKSE::GetSerializationInterface();
	serialization->SetUniqueID('dbd');
	serialization->SetSaveCallback(DBD::Serialize::SaveCallback);
	serialization->SetLoadCallback(DBD::Serialize::LoadCallback);
	serialization->SetRevertCallback(DBD::Serialize::RevertCallback);
	serialization->SetFormDeleteCallback(DBD::Serialize::FormDeleteCallback);

	logger::info("{} loaded"sv, plugin->GetName());

	return true;
}