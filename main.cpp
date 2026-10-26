#include <glm/gtc/quaternion.hpp>

#include "Engine/Lordi.h"
#include "Engine/Game.h"
#include "Engine/Graphics/Models/MeshBuilder.h"
#include "Engine/Graphics/Models/ModelManager.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Objects/CameraManager.h"
#include "Engine/Objects/ObjectManager.h"

// A minimal Lordi game: a block turning in the sun on a stone floor, with shadows. Real games live
// in their own repositories with Lordi as a submodule (LV-EngineBuilder is one; see README.md).
class SampleGame : public Game
{
public:
	void OnLoadAssets() override
	{
		auto models = ModelManager::Get();
		MeshBuilder floor;
		floor.SetColor({ 0.45f, 0.47f, 0.46f }).AddBox({ 0.f, -0.05f, 0.f }, { 12.f, 0.1f, 12.f });
		MeshBuilder block;
		block.SetColor({ 0.80f, 0.25f, 0.20f }).AddBox({ 0.f, 0.f, 0.f }, { 1.f, 1.f, 1.f });
		uint32_t floorModel = models->CreateModel(floor.Build("Floor"))->GetIndex();
		uint32_t blockModel = models->CreateModel(block.Build("Block"))->GetIndex();

		auto objects = ObjectManager::Get();
		objects->CreateObject(Object({ 0.f, 0.f, 0.f }, glm::quat(1.f, 0.f, 0.f, 0.f), { 1.f, 1.f, 1.f }, floorModel));
		m_block = objects->CreateObject(Object({ 0.f, 0.9f, 0.f }, glm::quat(1.f, 0.f, 0.f, 0.f), { 1.f, 1.f, 1.f }, blockModel));

		// Looking down at the block from in front of it.
		glm::vec3 eye = { 0.f, 2.5f, 5.f };
		glm::quat look = glm::quatLookAt(glm::normalize(glm::vec3(0.f, 0.9f, 0.f) - eye), glm::vec3(0.f, 1.f, 0.f));
		auto cameras = CameraManager::Get();
		auto camera = cameras->CreateCamera(Camera(eye, look, { 1.f, 1.f, 1.f }, 60.f, 0.05f, 100.f));
		cameras->SetActiveCamera(camera);
	}

	void OnStart() override
	{
		// The default sun and sky; a little haze.
		SceneLighting lighting;
		lighting.fogDensity = 0.01f;
		Renderer::Get()->SetSceneLighting(lighting);
		Renderer::Get()->SetShadowSettings({ 2048, 20.f });
	}

	void OnUpdate(float deltaTime) override
	{
		m_angle += deltaTime * 0.6f;
		m_block->SetRotation(glm::angleAxis(m_angle, glm::vec3(0.f, 1.f, 0.f)));
	}

private:
	pObject m_block = nullptr;
	float m_angle = 0.f;
};

int main()
{
	auto lordi = Lordi::Get();
	auto settings = lordi->GetSettings();

	GraphicsSettings graphicsSettings {};
	graphicsSettings.sWindowName = "Lordi Engine";
	graphicsSettings.uiWindowWidth = 1280;
	graphicsSettings.uiWindowHeight = 720;
	graphicsSettings.eVSync = VSyncOptions::ENABLED;
	graphicsSettings.eColorBlending = ColorBlendingOptions::NONE;
	graphicsSettings.eMSAA = MSAAOptions::X4;

	settings->SetGraphicsSettings(graphicsSettings);

	lordi->SetGame(std::make_shared<SampleGame>());

	lordi->Start();

	return 0;
}
