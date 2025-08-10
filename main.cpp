#include "Engine/Lordi.h"

int main() 
{
	auto lordi = Lordi::Get();
	auto settings = lordi->GetSettings();

	GraphicsSettings graphicsSettings {};
	graphicsSettings.sWindowName = "Lordi Engine";
	graphicsSettings.uiWindowWidth = 800;
	graphicsSettings.uiWindowHeight = 600;

	graphicsSettings.eVSync = VSyncOptions::NONE;
	graphicsSettings.eColorBlending = ColorBlendingOptions::NONE;
	graphicsSettings.eMSAA = MSAAOptions::X1;

	settings->SetGraphicsSettings(graphicsSettings);

	lordi->Initialize();
	
	return 0;
}