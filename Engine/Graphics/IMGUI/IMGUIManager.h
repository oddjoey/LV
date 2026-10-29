#pragma once

#include "../Graphics.h"
#include "../../Objects/CameraManager.h"
#include "../../Objects/ObjectManager.h"
#include "../Models/ModelManager.h"
#include "../Models/TextureManager.h"
#include "../../Utility/Utility.h"
#include "../../Utility/PerformanceManager.h"

#include <functional>

#include <imgui.h>
#include <../../../Libraries/imgui/backends/imgui_impl_glfw.h>
#include <../../../Libraries/imgui/backends/imgui_impl_vulkan.h>

class IMGUIManager
{
	friend class InputManager;
	friend class Renderer;
	CLASS_SINGLETON_HEADER(IMGUIManager)
public:
	IMGUIManager();
	~IMGUIManager();

	void WindowSetup();

	// Called every frame, after the engine's own debug window, for game UI.
	void SetGUICallback(const std::function<void()>& callback);

	// The engine's debug panels (performance, cameras, objects), drawn into the current ImGui
	// window, so a game can put them in a menu of its own. The performance figures can be left out.
	void DrawDebugContents(bool showPerformance = true);
	// Turns the built-in "Lordi" window and its Home key on or off (on by default). A game that
	// shows DrawDebugContents itself turns it off.
	void SetDebugWindowEnabled(bool enabled);

	void RecordCommandBuffer(const vk::CommandBuffer& commandBuffer);

	void Cleanup();
private:
	static void CheckVKResult(VkResult err);
	void CreateDescriptorPool();
	vk::DescriptorPool						m_descriptorPool = {};
	bool									m_showIMGUI = false; // debug window; HOME toggles it
	bool									m_debugWindowEnabled = true;
	std::function<void()>					m_guiCallback = nullptr;

	std::shared_ptr<Graphics>				m_graphics = nullptr;
	std::shared_ptr<CameraManager>			m_cameraManager = nullptr;
	std::shared_ptr<ObjectManager>			m_objectManager = nullptr;
	std::shared_ptr<ModelManager>			m_modelManager = nullptr;
	std::shared_ptr<TextureManager>			m_textureManager = nullptr;
	std::shared_ptr<PerformanceManager>		m_performanceManager = nullptr;
};