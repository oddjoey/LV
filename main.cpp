#include "Engine/Lordi.h"

int main() 
{
	auto lordi = Lordi::Get();
	auto settings = lordi->GetSettings();

	GraphicsSettings_t graphicsSettings;
	graphicsSettings.m_sWindowName = "Lordi Engine";
	graphicsSettings.m_uiWindowWidth = 1280;
	graphicsSettings.m_uiWindowHeight = 720;
	graphicsSettings.m_VSyncOption = VSyncOptions::NONE;

	settings->SetSettings(graphicsSettings);

	lordi->Initialize();

    return 0;
}