#pragma once

#include "Graphics/Graphics.h"
#include "Graphics/IMGUI/IMGUIManager.h"

class InputManager
{
	static std::shared_ptr<InputManager> s_instance;
public:
	static std::shared_ptr<InputManager> Get();
	InputManager();
	~InputManager();
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	bool IsKeyPressed(const int& key) const;
	bool IsKeyHeld(const int& key) const;
	bool IsKeyReleased(const int& key) const;
private:
	static std::shared_ptr<Graphics> m_graphics;
	static std::shared_ptr<IMGUIManager> m_imguiManager;
};

