// macOS: the voices are mixed here, in software, and played through a Core Audio output queue.
// (Windows has XAudio2, in AudioManager.cpp.) The same buses, volumes, pitch and pan as there.
#ifdef __APPLE__

#include "AudioManager.h"

#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <cmath>
#include <mutex>

CLASS_SINGLETON_SOURCE(AudioManager)

namespace
{
	constexpr int FRAMES_PER_BUFFER = 512;		// about 12 ms at 44.1 kHz
	constexpr int BUFFERS = 3;

	// Equal-power pan of a mono voice across two channels.
	void PanGains(float pan, float& left, float& right)
	{
		float angle = (std::clamp(pan, -1.f, 1.f) + 1.f) * 0.25f * 3.14159265f;
		left = std::cos(angle);
		right = std::sin(angle);
	}
}

struct AudioManager::Mixer
{
	struct Voice
	{
		const std::vector<int16_t>* samples = nullptr;
		double position = 0.0;	// in samples, between them when the pitch isn't 1
		float volume = 1.f;
		float pitch = 1.f;
		float left = 0.707f, right = 0.707f;
		size_t bus = 0;
		Loop loop = 0;
	};

	AudioQueueRef queue = nullptr;
	std::mutex lock;
	std::vector<Voice> voices;
	float master = 1.f;
	std::array<float, static_cast<size_t>(Bus::COUNT)> buses = { 1.f, 1.f, 1.f };

	// Called by Core Audio on its own thread, whenever a buffer's been played and wants filling.
	static void Fill(void* user, AudioQueueRef queue, AudioQueueBufferRef buffer)
	{
		auto mixer = static_cast<Mixer*>(user);
		auto out = static_cast<float*>(buffer->mAudioData);
		std::fill(out, out + FRAMES_PER_BUFFER * 2, 0.f);
		{
			std::lock_guard<std::mutex> guard(mixer->lock);
			for (auto& voice : mixer->voices)
			{
				const auto& samples = *voice.samples;
				float gain = voice.volume * mixer->buses[voice.bus] * mixer->master;
				for (int frame = 0; frame < FRAMES_PER_BUFFER; frame++)
				{
					if (voice.position >= samples.size())
					{
						if (voice.loop == 0)
							break;
						voice.position = std::fmod(voice.position, static_cast<double>(samples.size()));
					}
					// Linear between neighbouring samples, for pitches other than 1.
					size_t at = static_cast<size_t>(voice.position);
					double between = voice.position - at;
					size_t next = at + 1 < samples.size() ? at + 1 : (voice.loop != 0 ? 0 : at);
					float sample = static_cast<float>((samples[at] * (1.0 - between) + samples[next] * between) / 32768.0) * gain;
					out[frame * 2] += sample * voice.left;
					out[frame * 2 + 1] += sample * voice.right;
					voice.position += voice.pitch;
				}
			}
		}
		for (int i = 0; i < FRAMES_PER_BUFFER * 2; i++)
			out[i] = std::clamp(out[i], -1.f, 1.f);
		buffer->mAudioDataByteSize = FRAMES_PER_BUFFER * 2 * sizeof(float);
		AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
	}
};

AudioManager::AudioManager()
{
	auto mixer = new Mixer();
	AudioStreamBasicDescription format = {};
	format.mSampleRate = SAMPLE_RATE;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	format.mChannelsPerFrame = 2;
	format.mBitsPerChannel = 32;
	format.mFramesPerPacket = 1;
	format.mBytesPerFrame = 2 * sizeof(float);
	format.mBytesPerPacket = format.mBytesPerFrame;
	if (AudioQueueNewOutput(&format, Mixer::Fill, mixer, nullptr, nullptr, 0, &mixer->queue) != noErr)
	{
		delete mixer;
		return;
	}
	// Primed with silence, then it keeps asking for more.
	for (int i = 0; i < BUFFERS; i++)
	{
		AudioQueueBufferRef buffer = nullptr;
		if (AudioQueueAllocateBuffer(mixer->queue, FRAMES_PER_BUFFER * 2 * sizeof(float), &buffer) == noErr)
			Mixer::Fill(mixer, mixer->queue, buffer);
	}
	if (AudioQueueStart(mixer->queue, nullptr) != noErr)
	{
		AudioQueueDispose(mixer->queue, true);
		delete mixer;
		return;
	}
	m_mixer = mixer;
}

AudioManager::~AudioManager()
{
	if (!m_mixer)
		return;
	AudioQueueStop(m_mixer->queue, true);
	AudioQueueDispose(m_mixer->queue, true);
	delete m_mixer;
	m_mixer = nullptr;
}

bool AudioManager::IsAvailable() const
{
	return m_mixer != nullptr;
}

AudioManager::Clip AudioManager::AddClip(std::vector<int16_t> samples)
{
	// Kept in a deque, so the clips already playing stay where the mixer has them.
	m_clips.push_back(std::move(samples));
	return static_cast<Clip>(m_clips.size());
}

void AudioManager::Play(Clip clip, Bus bus, float volume, float pitch, float pan)
{
	if (!m_mixer || volume <= 0.001f || clip == 0 || clip > m_clips.size() || m_clips[clip - 1].empty())
		return;
	std::lock_guard<std::mutex> guard(m_mixer->lock);
	if (m_mixer->voices.size() >= MAX_VOICES)
		return;
	Mixer::Voice voice;
	voice.samples = &m_clips[clip - 1];
	voice.volume = (std::max)(volume, 0.f);
	voice.pitch = std::clamp(pitch, 0.05f, MAX_PITCH);
	PanGains(pan, voice.left, voice.right);
	voice.bus = static_cast<size_t>(bus);
	m_mixer->voices.push_back(voice);
}

AudioManager::Loop AudioManager::StartLoop(Clip clip, Bus bus, float volume, float pitch, float pan)
{
	if (!m_mixer || clip == 0 || clip > m_clips.size() || m_clips[clip - 1].empty())
		return 0;
	std::lock_guard<std::mutex> guard(m_mixer->lock);
	if (m_mixer->voices.size() >= MAX_VOICES)
		return 0;
	Mixer::Voice voice;
	voice.samples = &m_clips[clip - 1];
	voice.volume = (std::max)(volume, 0.f);
	voice.pitch = std::clamp(pitch, 0.05f, MAX_PITCH);
	PanGains(pan, voice.left, voice.right);
	voice.bus = static_cast<size_t>(bus);
	voice.loop = m_nextLoop++;
	m_mixer->voices.push_back(voice);
	return voice.loop;
}

void AudioManager::SetLoop(Loop loop, float volume, float pitch, float pan)
{
	if (!m_mixer || loop == 0)
		return;
	std::lock_guard<std::mutex> guard(m_mixer->lock);
	for (auto& voice : m_mixer->voices)
		if (voice.loop == loop)
		{
			voice.volume = (std::max)(volume, 0.f);
			voice.pitch = std::clamp(pitch, 0.05f, MAX_PITCH);
			PanGains(pan, voice.left, voice.right);
		}
}

void AudioManager::StopLoop(Loop loop)
{
	if (!m_mixer || loop == 0)
		return;
	std::lock_guard<std::mutex> guard(m_mixer->lock);
	std::erase_if(m_mixer->voices, [&](const Mixer::Voice& voice) { return voice.loop == loop; });
}

void AudioManager::SetMasterVolume(float volume)
{
	if (!m_mixer)
		return;
	std::lock_guard<std::mutex> guard(m_mixer->lock);
	m_mixer->master = std::clamp(volume, 0.f, 1.f);
}

void AudioManager::SetBusVolume(Bus bus, float volume)
{
	if (!m_mixer)
		return;
	std::lock_guard<std::mutex> guard(m_mixer->lock);
	m_mixer->buses[static_cast<size_t>(bus)] = std::clamp(volume, 0.f, 1.f);
}

void AudioManager::Update()
{
	// One-shots that have played out are let go; loops stay until stopped.
	if (!m_mixer)
		return;
	std::lock_guard<std::mutex> guard(m_mixer->lock);
	std::erase_if(m_mixer->voices, [](const Mixer::Voice& voice) { return voice.loop == 0 && voice.position >= voice.samples->size(); });
}

#endif
