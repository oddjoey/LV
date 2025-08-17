#include "PerformanceManager.h"

CLASS_SINGLETON_SOURCE(PerformanceManager)

PerformanceManager::PerformanceManager()
{
}

PerformanceManager::~PerformanceManager()
{
}

void PerformanceManager::Tick()
{
	m_frameCount++;
	Time currentTime = GetCurrentTime();
	std::chrono::duration<double> duration = currentTime - m_lastFrameTime;

	if (duration.count() >= 1.0f)
	{
		m_fps = static_cast<float>(m_frameCount / duration.count());
		m_frameCount = 0;
		m_lastFrameTime = currentTime;
	}
}

float PerformanceManager::GetFPS() const
{
	return m_fps;
}

void PerformanceManager::StartGraphicsCommandBufferTimer()
{
	m_graphicsCommandBufferStartTime = GetCurrentTime();
}

void PerformanceManager::EndGraphicsCommandBufferTimer()
{
	m_graphicsCommandBufferDuration = std::chrono::duration_cast<Duration>(GetCurrentTime() - m_graphicsCommandBufferStartTime);
}

PerformanceManager::Duration PerformanceManager::GetGraphicsCommandBufferDuration()
{
	return m_graphicsCommandBufferDuration;
}

void PerformanceManager::StartIMGUICommandBufferTimer()
{
	m_imguiCommandBufferStartTime = GetCurrentTime();
}

void PerformanceManager::EndIMGUICommandBufferTimer()
{
	m_imguiCommandBufferDuration = std::chrono::duration_cast<Duration>(GetCurrentTime() - m_imguiCommandBufferStartTime);
}

PerformanceManager::Duration PerformanceManager::GetIMGUICommandBufferDuration()
{
	return m_imguiCommandBufferDuration;
}

void PerformanceManager::StartCPUWaitForGPUTimer()
{
	m_cpuWaitForGpuStartTime = GetCurrentTime();
}

void PerformanceManager::EndCPUWaitForGPUTimer()
{
	m_cpuWaitForGpuDuration = std::chrono::duration_cast<Duration>(GetCurrentTime() - m_cpuWaitForGpuStartTime);
}

PerformanceManager::Duration PerformanceManager::GetCPUWaitForGPUDuration()
{
	return m_cpuWaitForGpuDuration;
}

PerformanceManager::Time PerformanceManager::GetCurrentTime()
{
	return std::chrono::high_resolution_clock::now();
}
