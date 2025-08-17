#pragma once

#include "../Utility/Utility.h"

class PerformanceManager
{
	CLASS_SINGLETON_HEADER(PerformanceManager)
public:
	using Time = std::chrono::steady_clock::time_point;
	using Duration = std::chrono::duration<float, std::milli>;
	
	PerformanceManager();
	~PerformanceManager();

	void Tick();

	float GetFPS() const;

	void StartGraphicsCommandBufferTimer();
	void EndGraphicsCommandBufferTimer();
	Duration GetGraphicsCommandBufferDuration();

	void StartIMGUICommandBufferTimer();
	void EndIMGUICommandBufferTimer();
	Duration GetIMGUICommandBufferDuration();

	void StartCPUWaitForGPUTimer();
	void EndCPUWaitForGPUTimer();
	Duration GetCPUWaitForGPUDuration();
private:
	Time GetCurrentTime();
	float m_fps = 0;
	uint32_t m_frameCount = 0;
	Time m_lastFrameTime = {};
	Time m_graphicsCommandBufferStartTime = {};
	Duration m_graphicsCommandBufferDuration = {};
	Time m_imguiCommandBufferStartTime = {};
	Duration m_imguiCommandBufferDuration = {};
	Time m_cpuWaitForGpuStartTime = {};
	Duration m_cpuWaitForGpuDuration = {};
};