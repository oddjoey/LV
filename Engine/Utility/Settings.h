#pragma once

#include <string>
#include <memory>

#include "Logger.h"
#include "Utility.h"

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

enum class MSAAOptions
{
	X1,
	X2,
	X4,
	X8,
	X16,
	X32,
	X64,
	MAX
};

struct GraphicsSettings
{
	GraphicsSettings Defaults() const
	{
		GraphicsSettings defaultSettings = {};
		defaultSettings.sWindowName = "Lordi Engine";
		defaultSettings.uiWindowWidth = 1280;
		defaultSettings.uiWindowHeight = 720;
		defaultSettings.eVSync = VSyncOptions::ENABLED;
		defaultSettings.eColorBlending = ColorBlendingOptions::NONE;
		defaultSettings.eMSAA = MSAAOptions::X1;

		return defaultSettings;
	}
	bool Empty() const
	{
		GraphicsSettings emptySettings = {};
		return *this == emptySettings;
	}
	bool Valid() const
	{
		if (sWindowName.empty())
			return false;
		if (uiWindowHeight <= 0)
			return false;
		if (uiWindowWidth <= 0)
			return false;
		
		return true;
	}
	bool operator==(const GraphicsSettings& other) const
	{
		return sWindowName == other.sWindowName &&
			uiWindowWidth == other.uiWindowWidth &&
			uiWindowHeight == other.uiWindowHeight &&
			eVSync == other.eVSync &&
			eColorBlending == other.eColorBlending &&
			eMSAA == other.eMSAA;
	}

	std::string sWindowName = "";
	unsigned int uiWindowWidth = 0;
	unsigned int uiWindowHeight = 0;
	VSyncOptions eVSync = VSyncOptions::NONE;
	ColorBlendingOptions eColorBlending = ColorBlendingOptions::NONE;
	MSAAOptions eMSAA = MSAAOptions::X1;
};

class Settings
{
	friend class Lordi;
	CLASS_SINGLETON_HEADER(Settings)
public:
	~Settings();

	// delete move/copy
	Settings(const Settings&) = delete;
	Settings& operator=(const Settings& r) = delete;

	bool ValidateSettings(const GraphicsSettings& settings);

	void SetGraphicsSettings(const GraphicsSettings& settings);

	GraphicsSettings GetGraphicsSettings() const;
private:
	Settings();
	GraphicsSettings m_tSettings = {};
};