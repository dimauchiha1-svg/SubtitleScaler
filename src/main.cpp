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
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			// Все SKSE-плагины уже загружены, значит, SKSE Menu Framework (если он есть) доступен.
			UI::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Subtitles::RegisterMenuSink();
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

	spdlog::info("SubtitleScaler загружается");

	Settings::Load();

	SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
	SKSE::GetPapyrusInterface()->Register(Papyrus::Register);

	return true;
}
