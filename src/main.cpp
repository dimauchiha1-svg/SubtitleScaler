#include "Papyrus.h"
#include "Settings.h"
#include "Subtitles.h"
#include "UI.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

namespace
{
	void SetupLog()
	{
		auto path = SKSE::log::log_directory();
		if (!path) {
			return;
		}
		*path /= "SubtitleScaler.log";

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log  = std::make_shared<spdlog::logger>("global log", std::move(sink));
		// [General] Logging=0 in the INI: the log file is created empty and nothing is written to it.
		log->set_level(Settings::ReadLogFlagFromIni() ? spdlog::level::info : spdlog::level::off);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			// All SKSE plugins are loaded by now, so SKSE Menu Framework (if installed) is available.
			UI::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Subtitles::RegisterMenuSink();
			Subtitles::InstallColorHooks();
			Subtitles::ApplyAll(true);
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Subtitles::ApplyAll(true);
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();

	spdlog::info("SubtitleScaler is loading (built {} {})", __DATE__, __TIME__);

	Settings::Load();

	SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
	SKSE::GetPapyrusInterface()->Register(Papyrus::Register);

	return true;
}
