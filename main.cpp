#include "Engine/Lordi.h"

int main() 
{
	auto lordi = Lordi::Get();
	auto settings = lordi->GetSettings();

	GraphicsSettings graphicsSettings;
	graphicsSettings.sWindowName = "Lordi Engine";
	graphicsSettings.uiWindowWidth = 1280;
	graphicsSettings.uiWindowHeight = 720;
	graphicsSettings.eVSync = VSyncOptions::MAILBOX;
	graphicsSettings.eColorBlending = ColorBlendingOptions::NONE;
	graphicsSettings.eMSAA = MSAAOptions::X1;

	settings->SetSettings(graphicsSettings);

	lordi->Initialize();
	
	return 0;
}