#include "AudioManager.h"

#include <algorithm>
#include <cmath>

#include <objbase.h>
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

CLASS_SINGLETON_SOURCE(AudioManager)

AudioManager::AudioManager()
{
	// XAudio2 needs COM. Another part of the program may have started it its own way already, which is
	// fine: only a call that started it here is matched with an uninitialise.
	HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	m_comStarted = SUCCEEDED(com);

	if (FAILED(XAudio2Create(&m_device, 0, XAUDIO2_DEFAULT_PROCESSOR)))
	{
		m_device = nullptr;
		return;
	}
	if (FAILED(m_device->CreateMasteringVoice(&m_master, 2, SAMPLE_RATE)))
	{
		m_device->Release();
		m_device = nullptr;
		return;
	}
	for (auto& bus : m_buses)
		if (FAILED(m_device->CreateSubmixVoice(&bus, 2, SAMPLE_RATE)))
			bus = nullptr;
}

AudioManager::~AudioManager()
{
	for (auto& voice : m_voices)
		voice.voice->DestroyVoice();
	m_voices.clear();
	for (auto& bus : m_buses)
		if (bus)
			bus->DestroyVoice();
	if (m_master)
		m_master->DestroyVoice();
	if (m_device)
		m_device->Release();
	if (m_comStarted)
		CoUninitialize();
}

bool AudioManager::IsAvailable() const
{
	return m_device && m_master;
}

AudioManager::Clip AudioManager::AddClip(std::vector<int16_t> samples)
{
	m_clips.push_back(std::move(samples));
	return static_cast<Clip>(m_clips.size());
}

void AudioManager::SetPan(IXAudio2SourceVoice* voice, Bus bus, float pan)
{
	// Equal-power pan of the mono voice across the bus's two channels.
	float angle = (std::clamp(pan, -1.f, 1.f) + 1.f) * 0.25f * 3.14159265f;
	float matrix[2] = { std::cos(angle), std::sin(angle) };
	voice->SetOutputMatrix(m_buses[static_cast<size_t>(bus)], 1, 2, matrix);
}

IXAudio2SourceVoice* AudioManager::StartVoice(Clip clip, Bus bus, float volume, float pitch, float pan, bool looping)
{
	if (!IsAvailable() || clip == 0 || clip > m_clips.size() || !m_buses[static_cast<size_t>(bus)] || m_voices.size() >= MAX_VOICES)
		return nullptr;
	const auto& samples = m_clips[clip - 1];
	if (samples.empty())
		return nullptr;

	WAVEFORMATEX format = {};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 1;
	format.nSamplesPerSec = SAMPLE_RATE;
	format.wBitsPerSample = 16;
	format.nBlockAlign = 2;
	format.nAvgBytesPerSec = SAMPLE_RATE * 2;

	XAUDIO2_SEND_DESCRIPTOR send = { 0, m_buses[static_cast<size_t>(bus)] };
	XAUDIO2_VOICE_SENDS sends = { 1, &send };
	IXAudio2SourceVoice* voice = nullptr;
	if (FAILED(m_device->CreateSourceVoice(&voice, &format, 0, MAX_PITCH, nullptr, &sends)))
		return nullptr;

	XAUDIO2_BUFFER buffer = {};
	buffer.AudioBytes = static_cast<UINT32>(samples.size() * sizeof(int16_t));
	buffer.pAudioData = reinterpret_cast<const BYTE*>(samples.data());
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = looping ? XAUDIO2_LOOP_INFINITE : 0;
	if (FAILED(voice->SubmitSourceBuffer(&buffer)))
	{
		voice->DestroyVoice();
		return nullptr;
	}
	voice->SetVolume((std::max)(volume, 0.f));
	voice->SetFrequencyRatio(std::clamp(pitch, 0.05f, MAX_PITCH));
	SetPan(voice, bus, pan);
	voice->Start();
	return voice;
}

void AudioManager::Play(Clip clip, Bus bus, float volume, float pitch, float pan)
{
	if (volume <= 0.001f)
		return;
	if (auto voice = StartVoice(clip, bus, volume, pitch, pan, false))
		m_voices.push_back({ voice, 0 });
}

AudioManager::Loop AudioManager::StartLoop(Clip clip, Bus bus, float volume, float pitch, float pan)
{
	auto voice = StartVoice(clip, bus, volume, pitch, pan, true);
	if (!voice)
		return 0;
	Loop loop = m_nextLoop++;
	m_voices.push_back({ voice, loop });
	return loop;
}

void AudioManager::SetLoop(Loop loop, float volume, float pitch, float pan)
{
	for (auto& voice : m_voices)
		if (voice.loop == loop && loop != 0)
		{
			voice.voice->SetVolume((std::max)(volume, 0.f));
			voice.voice->SetFrequencyRatio(std::clamp(pitch, 0.05f, MAX_PITCH));
			// Each voice sends to just its bus, so the matrix needs no destination named.
			float angle = (std::clamp(pan, -1.f, 1.f) + 1.f) * 0.25f * 3.14159265f;
			float matrix[2] = { std::cos(angle), std::sin(angle) };
			voice.voice->SetOutputMatrix(nullptr, 1, 2, matrix);
		}
}

void AudioManager::StopLoop(Loop loop)
{
	if (loop == 0)
		return;
	for (auto it = m_voices.begin(); it != m_voices.end();)
		if (it->loop == loop)
		{
			it->voice->Stop();
			it->voice->DestroyVoice();
			it = m_voices.erase(it);
		}
		else
			++it;
}

void AudioManager::SetMasterVolume(float volume)
{
	if (m_master)
		m_master->SetVolume(std::clamp(volume, 0.f, 1.f));
}

void AudioManager::SetBusVolume(Bus bus, float volume)
{
	if (auto voice = m_buses[static_cast<size_t>(bus)])
		voice->SetVolume(std::clamp(volume, 0.f, 1.f));
}

void AudioManager::Update()
{
	// One-shots that have played out are let go; loops stay until stopped.
	for (auto it = m_voices.begin(); it != m_voices.end();)
	{
		XAUDIO2_VOICE_STATE state = {};
		it->voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		if (it->loop == 0 && state.BuffersQueued == 0)
		{
			it->voice->DestroyVoice();
			it = m_voices.erase(it);
		}
		else
			++it;
	}
}
