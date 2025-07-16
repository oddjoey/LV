#include "Settings.h"

std::shared_ptr<Settings> Settings::_instance = nullptr;

std::shared_ptr<Settings> Settings::Get()
{
	if (_instance == nullptr)
		_instance = std::shared_ptr<Settings>(new Settings());
	return _instance;
}

bool Settings::ValidateSettings(const GraphicsSettings& settings)
{
	LOG_INFO("Validating Graphics settings...");

	if (settings.m_sWindowName.empty())
	{
		LOG_WARNING("Window name cannot be empty");
		return false;
	}

	if (settings.m_uiWindowHeight <= 0 || settings.m_uiWindowWidth <= 0)
	{
		LOG_WARNING("Window size cannot be less than or equal to zero");
		return false;
	}

	m_tSettings = settings;

	LOG_SUCCESS("Graphics settings validated successfully!");
	return true;
}

void Settings::SetSettings(const GraphicsSettings& settings)
{
	LOG_INFO("Setting Graphics settings...");

	if (ValidateSettings(settings))
	{
		m_tSettings = settings;
		LOG_SUCCESS("Graphics settings set successfully!");
	}
}

GraphicsSettings Settings::GetGraphicsSettings() const
{
	return m_tSettings;
}

Settings::~Settings()
{
}

Settings::Settings()
{

}