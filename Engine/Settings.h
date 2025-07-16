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

enum class ColorBlendingOptions
{
	NONE,
	MIX,
	BITWISE
};

struct GraphicsSettings
{
	GraphicsSettings Defaults() const
	{
		GraphicsSettings defaultSettings = {};
		defaultSettings.m_sWindowName = "Lordi Engine";
		defaultSettings.m_uiWindowWidth = 1280;
		defaultSettings.m_uiWindowHeight = 720;
		defaultSettings.m_VSyncOption = VSyncOptions::ENABLED;
		defaultSettings.m_ColorBlendingOption = ColorBlendingOptions::NONE;
		return defaultSettings;
	}
	bool Empty() const
	{
		GraphicsSettings emptySettings = {};
		return *this == emptySettings;
	}
	bool operator==(const GraphicsSettings& other) const
	{
		return m_sWindowName == other.m_sWindowName &&
			   m_uiWindowWidth == other.m_uiWindowWidth &&
			   m_uiWindowHeight == other.m_uiWindowHeight &&
			   m_VSyncOption == other.m_VSyncOption;
	}

	std::string m_sWindowName = "";
	unsigned int m_uiWindowWidth = 0;
	unsigned int m_uiWindowHeight = 0;
	VSyncOptions m_VSyncOption = VSyncOptions::NONE;
	ColorBlendingOptions m_ColorBlendingOption = ColorBlendingOptions::NONE;
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

	bool ValidateSettings(const GraphicsSettings& settings);

	void SetSettings(const GraphicsSettings& settings);

	GraphicsSettings GetGraphicsSettings() const;
private:
	GraphicsSettings m_tSettings = {};
};