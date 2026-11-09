// Built without the precompiled header, like Socket.cpp. The same everywhere: Monocypher (LV/Libraries/monocypher),
// compiled in, so a game on any platform can join a server on any other.
#include "Crypto.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <random>
#include <sstream>

extern "C" {
#include "../../Libraries/monocypher/monocypher.h"
}

namespace Net
{
	namespace
	{
		constexpr size_t MAC_BYTES = 16;
		constexpr size_t NONCE_BYTES = 24;
		constexpr size_t SIGNATURE_BYTES = 64;
		// What starts an identity key file, so one from before (a P-256 key) is told apart and replaced.
		const char* const IDENTITY_FILE_TAG = "lv-eddsa-blake2b";

		std::string ToHex(const uint8_t* data, size_t size)
		{
			static const char DIGITS[] = "0123456789abcdef";
			std::string hex;
			hex.reserve(size * 2);
			for (size_t i = 0; i < size; i++)
			{
				hex += DIGITS[data[i] >> 4];
				hex += DIGITS[data[i] & 15];
			}
			return hex;
		}

		// False for anything that isn't hex of exactly that many bytes: what the other side sends can't be
		// trusted to be.
		bool FromHex(const std::string& hex, uint8_t* bytes, size_t size)
		{
			auto digit = [](char c) {
				return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
			};
			if (hex.size() != size * 2)
				return false;
			for (size_t i = 0; i < size; i++)
			{
				int high = digit(hex[i * 2]);
				int low = digit(hex[i * 2 + 1]);
				if (high < 0 || low < 0)
					return false;
				bytes[i] = static_cast<uint8_t>(high * 16 + low);
			}
			return true;
		}

		// From the operating system's own source of randomness (what std::random_device draws on with each of
		// the compilers the game is built with).
		void RandomBytes(uint8_t* bytes, size_t size)
		{
			std::random_device device;
			for (size_t i = 0; i < size; i += sizeof(unsigned int))
			{
				unsigned int value = device();
				std::memcpy(bytes + i, &value, (std::min)(sizeof(value), size - i));
			}
		}

		// The nonce for a message: its number, then zeros.
		void NonceFor(uint64_t counter, uint8_t (&nonce)[NONCE_BYTES])
		{
			std::memset(nonce, 0, NONCE_BYTES);
			std::memcpy(nonce, &counter, sizeof(counter));
		}

		const char* const TO_SERVER_LABEL = "LV-EngineBuilder game to server";
		const char* const TO_CLIENT_LABEL = "LV-EngineBuilder server to game";
	}

	// ---- Cipher: XChaCha20-Poly1305

	Cipher::~Cipher()
	{
		crypto_wipe(m_key.data(), m_key.size());
	}

	bool Cipher::Seal(std::string& message)
	{
		// The message's number is its nonce: never the same twice under one key.
		uint8_t nonce[NONCE_BYTES];
		NonceFor(m_counter++, nonce);
		std::string sealed(message.size() + MAC_BYTES, '\0');
		auto* out = reinterpret_cast<uint8_t*>(sealed.data());
		crypto_aead_lock(out, out + message.size(), m_key.data(), nonce, nullptr, 0,
			reinterpret_cast<const uint8_t*>(message.data()), message.size());
		message = std::move(sealed);
		return true;
	}

	bool Cipher::Open(std::string& message)
	{
		if (message.size() < MAC_BYTES)
			return false;
		uint8_t nonce[NONCE_BYTES];
		NonceFor(m_counter++, nonce);
		size_t size = message.size() - MAC_BYTES;
		const auto* in = reinterpret_cast<const uint8_t*>(message.data());
		std::string opened(size, '\0');
		if (crypto_aead_unlock(reinterpret_cast<uint8_t*>(opened.data()), in + size, m_key.data(), nonce, nullptr, 0, in, size) != 0)
			return false;
		message = std::move(opened);
		return true;
	}

	// ---- KeyExchange: X25519

	KeyExchange::~KeyExchange()
	{
		crypto_wipe(m_secret.data(), m_secret.size());
	}

	std::unique_ptr<KeyExchange> KeyExchange::Create()
	{
		std::array<uint8_t, 32> secret, publicKey;
		RandomBytes(secret.data(), secret.size());
		crypto_x25519_public_key(publicKey.data(), secret.data());
		return std::unique_ptr<KeyExchange>(new KeyExchange(secret, ToHex(publicKey.data(), publicKey.size())));
	}

	bool KeyExchange::Derive(const std::string& peerPublic, bool server, const std::string& clientPublic, const std::string& serverPublic,
		std::unique_ptr<Cipher>& sending, std::unique_ptr<Cipher>& receiving) const
	{
		uint8_t peer[32];
		if (!FromHex(peerPublic, peer, sizeof(peer)))
			return false;
		uint8_t shared[32];
		crypto_x25519(shared, m_secret.data(), peer);
		// A key that came out all zeros means the other side sent a point that gives everyone the same secret.
		uint8_t zeros[32] = {};
		if (std::memcmp(shared, zeros, sizeof(shared)) == 0)
			return false;

		// A key each way: the shared secret hashed with which way it is and both sides' keys, so it's bound to
		// this connection.
		auto derive = [&](const char* label) {
			std::string context = std::string(label) + "|" + clientPublic + "|" + serverPublic;
			std::array<uint8_t, 32> key;
			crypto_blake2b_keyed(key.data(), key.size(), shared, sizeof(shared), reinterpret_cast<const uint8_t*>(context.data()), context.size());
			return std::unique_ptr<Cipher>(new Cipher(key));
		};
		auto toServer = derive(TO_SERVER_LABEL);
		auto toClient = derive(TO_CLIENT_LABEL);
		crypto_wipe(shared, sizeof(shared));
		sending = server ? std::move(toClient) : std::move(toServer);
		receiving = server ? std::move(toServer) : std::move(toClient);
		return true;
	}

	// ---- Identity: EdDSA (curve25519 and BLAKE2b)

	Identity::~Identity()
	{
		crypto_wipe(m_secret.data(), m_secret.size());
	}

	std::unique_ptr<Identity> Identity::LoadOrCreate(const std::string& path, std::string& error)
	{
		// The file holds the tag and the key's seed, in hex. One from before (a P-256 key) can't be used any
		// more: a new identity replaces it.
		uint8_t seed[32];
		bool loaded = false;
		if (std::filesystem::exists(path))
		{
			std::ifstream file(path, std::ios::binary);
			std::string tag, hex;
			file >> tag >> hex;
			loaded = tag == IDENTITY_FILE_TAG && FromHex(hex, seed, sizeof(seed));
			crypto_wipe(hex.data(), hex.size());
		}
		if (!loaded)
		{
			RandomBytes(seed, sizeof(seed));
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			std::string hex = ToHex(seed, sizeof(seed));
			file << IDENTITY_FILE_TAG << ' ' << hex << '\n';
			crypto_wipe(hex.data(), hex.size());
			if (!file)
			{
				crypto_wipe(seed, sizeof(seed));
				error = std::format("couldn't write {}", path);
				return nullptr;
			}
		}
		// The key pair from its seed (which this wipes).
		std::array<uint8_t, 64> secret;
		uint8_t publicKey[32];
		crypto_eddsa_key_pair(secret.data(), publicKey, seed);
		return std::unique_ptr<Identity>(new Identity(secret, ToHex(publicKey, sizeof(publicKey))));
	}

	std::string Identity::Sign(const std::string& data) const
	{
		uint8_t signature[SIGNATURE_BYTES];
		crypto_eddsa_sign(signature, m_secret.data(), reinterpret_cast<const uint8_t*>(data.data()), data.size());
		return ToHex(signature, sizeof(signature));
	}

	bool VerifySignature(const std::string& identity, const std::string& data, const std::string& signature)
	{
		uint8_t publicKey[32], raw[SIGNATURE_BYTES];
		if (!FromHex(identity, publicKey, sizeof(publicKey)) || !FromHex(signature, raw, sizeof(raw)))
			return false;
		return crypto_eddsa_check(raw, publicKey, reinterpret_cast<const uint8_t*>(data.data()), data.size()) == 0;
	}

	std::string HandshakeTranscript(const std::string& clientPublic, const std::string& serverPublic)
	{
		return "LV-EngineBuilder handshake 2|" + clientPublic + "|" + serverPublic;
	}

	std::string Fingerprint(const std::string& identity)
	{
		uint8_t publicKey[32];
		if (!FromHex(identity, publicKey, sizeof(publicKey)))
			return "";
		uint8_t hash[32];
		crypto_blake2b(hash, sizeof(hash), publicKey, sizeof(publicKey));
		return ToHex(hash, sizeof(hash));
	}

	KnownHosts::Result KnownHosts::Check(const std::string& host, const std::string& fingerprint) const
	{
		// Each line is "host fingerprint". Lines from before the identities changed kind (P-256 to EdDSA)
		// carry no "2:" and are dropped: a server seen then is taken at its word again, once.
		const std::string entry = "2:" + fingerprint;
		std::vector<std::string> kept;
		{
			std::ifstream file(m_path);
			std::string line;
			while (std::getline(file, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				size_t space = line.find(' ');
				if (!line.empty() && line[0] != '#' && space != std::string::npos && line.compare(space + 1, 2, "2:") != 0)
				{
					continue;
				}
				if (!line.empty() && line[0] != '#' && space != std::string::npos && line.substr(0, space) == host)
					return line.substr(space + 1) == entry ? Result::SAME : Result::CHANGED;
				kept.push_back(line);
			}
		}
		if (kept.empty())
			kept = { "# Servers joined before, and the identity each one proved it had. A server that turns up",
				"# with a different one is refused: delete its line only if you know why it changed." };
		kept.push_back(host + ' ' + entry);
		std::ofstream file(m_path, std::ios::trunc);
		for (const auto& line : kept)
			file << line << '\n';
		return Result::NEW;
	}
}
