#include "Settings.h"

CLASS_SINGLETON_SOURCE(Settings)

Settings::~Settings()
{

}

Settings::Settings()
{

}

bool Settings::ValidateSettings(const GraphicsSettings& settings)
{
	LOG_INFO("Validating Graphics settings...");

	if (settings.sWindowName.empty())
	{
		LOG_WARNING("Window name cannot be empty");
		return false;
	}

	if (settings.uiWindowHeight <= 0 || settings.uiWindowWidth <= 0)
	{
		LOG_WARNING("Window size cannot be less than or equal to zero");
		return false;
	}

	m_tSettings = settings;

	LOG_SUCCESS("Graphics settings validated!");
	return true;
}

void Settings::SetGraphicsSettings(const GraphicsSettings& settings)
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
	if (m_tSettings.Empty() || !m_tSettings.Valid())
	{
		LOG_WARNING("Graphics settings are not set or valid, using defaults...");
		return m_tSettings.Defaults();
	}

	return m_tSettings;
}