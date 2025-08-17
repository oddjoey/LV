#include "Engine/Lordi.h"

int main() 
{
	auto lordi = Lordi::Get();
	auto settings = lordi->GetSettings();

	GraphicsSettings graphicsSettings {};
	graphicsSettings.sWindowName = "Lordi Engine";
	graphicsSettings.uiWindowWidth = 1920;
	graphicsSettings.uiWindowHeight = 1080;
	graphicsSettings.eVSync = VSyncOptions::NONE;
	graphicsSettings.eColorBlending = ColorBlendingOptions::NONE;
	graphicsSettings.eMSAA = MSAAOptions::X8;

	settings->SetGraphicsSettings(graphicsSettings);

	lordi->Start();
	
	return 0;
}