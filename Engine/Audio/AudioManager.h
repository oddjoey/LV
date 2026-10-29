#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "../Utility/Utility.h"

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SubmixVoice;
struct IXAudio2SourceVoice;

// Sound through XAudio2, which every Windows 10 and 11 machine has. Sounds are mono clips of 16-bit
// samples at SAMPLE_RATE, made however the game likes (loaded, or synthesised). Each plays on a bus,
// so effects, ambience and the interface can each have their own volume under the master one.
// Without a working audio device every call quietly does nothing.
class AudioManager
{
	CLASS_SINGLETON_HEADER(AudioManager)
public:
	static constexpr int SAMPLE_RATE = 44100;
	enum class Bus { EFFECTS, AMBIENCE, INTERFACE, COUNT };
	using Clip = uint32_t;	// 0 is no clip
	using Loop = uint32_t;	// 0 is no loop

	AudioManager();
	~AudioManager();
	bool IsAvailable() const;

	Clip AddClip(std::vector<int16_t> samples);
	// Once through. Volume is linear (1 = as recorded), pitch a playback speed ratio (up to 4), pan
	// from -1 (left) to 1 (right).
	void Play(Clip clip, Bus bus, float volume, float pitch = 1.f, float pan = 0.f);
	// Over and over until stopped, changed as it goes: an engine's note, a motor's hum.
	Loop StartLoop(Clip clip, Bus bus, float volume, float pitch = 1.f, float pan = 0.f);
	void SetLoop(Loop loop, float volume, float pitch, float pan);
	void StopLoop(Loop loop);

	void SetMasterVolume(float volume);
	void SetBusVolume(Bus bus, float volume);
	// Once a frame: lets go of the voices that have finished.
	void Update();
private:
	struct Voice
	{
		IXAudio2SourceVoice* voice = nullptr;
		Loop loop = 0;
	};
	IXAudio2SourceVoice* StartVoice(Clip clip, Bus bus, float volume, float pitch, float pan, bool looping);
	void SetPan(IXAudio2SourceVoice* voice, Bus bus, float pan);

	IXAudio2*										m_device = nullptr;
	IXAudio2MasteringVoice*							m_master = nullptr;
	std::array<IXAudio2SubmixVoice*, static_cast<size_t>(Bus::COUNT)> m_buses = {};
	std::vector<std::vector<int16_t>>				m_clips = {};
	std::vector<Voice>								m_voices = {};
	Loop											m_nextLoop = 1;
	bool											m_comStarted = false;

	static constexpr size_t MAX_VOICES = 48;
	static constexpr float MAX_PITCH = 4.f;
};
