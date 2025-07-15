#pragma once

#include <string>
#include <memory>

#include "Logger.h"

enum class VSyncOptions
{
	NONE,
	ENABLED,
	RELAXED,
	MAILBOX
};

struct GraphicsSettings_t
{
	GraphicsSettings_t Defaults() const
	{
		GraphicsSettings_t defaultSettings = {};
		defaultSettings.m_sWindowName = "Lordi Engine";
		defaultSettings.m_uiWindowWidth = 1280;
		defaultSettings.m_uiWindowHeight = 720;
		defaultSettings.m_VSyncOption = VSyncOptions::ENABLED;
		return defaultSettings;
	}
	bool Empty() const
	{
		GraphicsSettings_t emptySettings = {};
		return *this == emptySettings;
	}
	bool operator==(const GraphicsSettings_t& other) const
	{
		return m_sWindowName == other.m_sWindowName &&
			   m_uiWindowWidth == other.m_uiWindowWidth &&
			   m_uiWindowHeight == other.m_uiWindowHeight &&
			   m_VSyncOption == other.m_VSyncOption;
	}

	std::string m_sWindowName = "";
	unsigned int m_uiWindowWidth = 0;
	unsigned int m_uiWindowHeight = 0;
	VSyncOptions m_VSyncOption = {};
};

class Settings
{
	friend class Lordi;
	static std::shared_ptr<Settings> _instance;
	Settings();
public:
	static std::shared_ptr<Settings> Get();

	~Settings();

	// delete move/copy
	Settings(const Settings&) = delete;
	Settings& operator=(const Settings& r) = delete;

	bool ValidateSettings(const GraphicsSettings_t& settings);

	void SetSettings(const GraphicsSettings_t& settings);

	GraphicsSettings_t GetGraphicsSettings() const;
private:
	GraphicsSettings_t m_tSettings = {};
};