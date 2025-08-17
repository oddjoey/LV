#pragma once

#include "../Graphics/Graphics.h"
#include "../Graphics/IMGUI/IMGUIManager.h"
#include "Utility.h"

class InputManager
{
	CLASS_SINGLETON_HEADER(InputManager)
public:
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

