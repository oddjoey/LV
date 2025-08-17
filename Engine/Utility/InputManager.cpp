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
	switch (action)
	{
		case GLFW_PRESS:
		{
			switch (key)
			{
				case GLFW_KEY_HOME:
				{
					m_imguiManager->m_showIMGUI = !m_imguiManager->m_showIMGUI;
					break;
				}
			}
			break;
		}
	}
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