#pragma once

#include <array>

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
	static void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset);

	// Call once per frame after polling events; computes pressed/released edges and mouse movement.
	void Update();

	bool IsKeyPressed(const int& key) const;
	bool IsKeyHeld(const int& key) const;
	bool IsKeyReleased(const int& key) const;
	// True only on the frame the key went down.
	bool WasKeyPressed(const int& key) const;

	bool IsMouseButtonDown(const int& button) const;
	// True only on the frame the button went down.
	bool WasMouseButtonPressed(const int& button) const;
	// Cursor movement since last frame, in pixels.
	glm::vec2 GetMouseDelta() const;
	// Mouse wheel notches turned since last frame: positive away from you (up), negative toward you.
	float GetScrollDelta() const;

	// Locked: cursor hidden and captured for mouse-look. Unlocked: normal cursor for UI.
	void SetCursorLocked(const bool& locked);
	bool IsCursorLocked() const;
private:
	static std::shared_ptr<Graphics> m_graphics;
	static std::shared_ptr<IMGUIManager> m_imguiManager;

	std::array<bool, GLFW_KEY_LAST + 1>				m_keys = {};
	std::array<bool, GLFW_KEY_LAST + 1>				m_previousKeys = {};
	std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1>	m_mouseButtons = {};
	std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1>	m_previousMouseButtons = {};
	glm::dvec2										m_cursorPosition = {};
	glm::vec2										m_mouseDelta = {};
	float											m_scrollDelta = 0.f;
	static double									m_scrollAccumulated;
	int												m_ignoreMouseFrames = 1;
	bool											m_cursorLocked = false;
};
