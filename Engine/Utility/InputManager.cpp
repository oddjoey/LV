#include "InputManager.h"

CLASS_SINGLETON_SOURCE(InputManager)

std::shared_ptr<Graphics> InputManager::m_graphics = nullptr;
std::shared_ptr<IMGUIManager> InputManager::m_imguiManager = nullptr;

InputManager::InputManager()
{
	m_graphics = Graphics::Get();
	glfwSetKeyCallback(m_graphics->m_window, KeyCallback);
	m_imguiManager = IMGUIManager::Get();
}

InputManager::~InputManager()
{
}

void InputManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Installing this callback replaced the one ImGui's GLFW backend set up, so pass keys on to it.
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

	switch (action)
	{
		case GLFW_PRESS:
		{
			switch (key)
			{
				case GLFW_KEY_HOME:
				{
					if (m_imguiManager->m_debugWindowEnabled)
						m_imguiManager->m_showIMGUI = !m_imguiManager->m_showIMGUI;
					break;
				}
			}
			break;
		}
	}
}

void InputManager::Update()
{
	m_previousKeys = m_keys;
	for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++)
		m_keys[key] = glfwGetKey(m_graphics->m_window, key) == GLFW_PRESS;

	m_previousMouseButtons = m_mouseButtons;
	for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; button++)
		m_mouseButtons[button] = glfwGetMouseButton(m_graphics->m_window, button) == GLFW_PRESS;

	glm::dvec2 cursor = {};
	glfwGetCursorPos(m_graphics->m_window, &cursor.x, &cursor.y);
	m_mouseDelta = m_ignoreMouseFrames > 0 ? glm::vec2(0.f) : glm::vec2(cursor - m_cursorPosition);
	m_cursorPosition = cursor;
	if (m_ignoreMouseFrames > 0)
		m_ignoreMouseFrames--;
}

bool InputManager::WasKeyPressed(const int& key) const
{
	if (key < 0 || key > GLFW_KEY_LAST)
		return false;
	return m_keys[key] && !m_previousKeys[key];
}

bool InputManager::IsMouseButtonDown(const int& button) const
{
	if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
		return false;
	return m_mouseButtons[button];
}

bool InputManager::WasMouseButtonPressed(const int& button) const
{
	if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
		return false;
	return m_mouseButtons[button] && !m_previousMouseButtons[button];
}

glm::vec2 InputManager::GetMouseDelta() const
{
	return m_mouseDelta;
}

void InputManager::SetCursorLocked(const bool& locked)
{
	if (locked == m_cursorLocked)
		return;

	m_cursorLocked = locked;
	glfwSetInputMode(m_graphics->m_window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(m_graphics->m_window, GLFW_RAW_MOUSE_MOTION, locked ? GLFW_TRUE : GLFW_FALSE);

	// GLFW warps the cursor when the mode changes, sometimes a frame or two later; don't let
	// that jump read as mouse-look.
	m_ignoreMouseFrames = 3;

	// While locked, keep ImGui from reacting to the hidden cursor or to movement keys.
	ImGuiIO& io = ImGui::GetIO();
	if (locked)
		io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard;
	else
		io.ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard);
}

bool InputManager::IsCursorLocked() const
{
	return m_cursorLocked;
}

bool InputManager::IsKeyPressed(const int& key) const
{
	return glfwGetKey(m_graphics->m_window, key) == GLFW_PRESS;
}

bool InputManager::IsKeyHeld(const int& key) const
{
	return glfwGetKey(m_graphics->m_window, key) == GLFW_REPEAT;
}

bool InputManager::IsKeyReleased(const int& key) const
{
	return glfwGetKey(m_graphics->m_window, key) == GLFW_RELEASE;
}