// Built without the precompiled header, like Socket.cpp: it needs windows.h on its own terms.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "Crypto.h"

#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>

#pragma comment(lib, "bcrypt.lib")

namespace Net
{
	namespace
	{
		constexpr ULONG NONCE_BYTES = 12;
		constexpr ULONG TAG_BYTES = 16;
		constexpr ULONG KEY_BYTES = 32;
		constexpr ULONG P256_BYTES = 32;

		struct Providers
		{
			BCRYPT_ALG_HANDLE aes = nullptr;
			BCRYPT_ALG_HANDLE ecdh = nullptr;
			BCRYPT_ALG_HANDLE ecdsa = nullptr;
			BCRYPT_ALG_HANDLE sha256 = nullptr;
			bool ready = false;
		};

		const Providers& Algorithms()
		{
			static Providers providers;
			providers.ready = BCryptOpenAlgorithmProvider(&providers.aes, BCRYPT_AES_ALGORITHM, nullptr, 0) == 0
				&& BCryptSetProperty(providers.aes, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM), 0) == 0
				&& BCryptOpenAlgorithmProvider(&providers.ecdh, BCRYPT_ECDH_P256_ALGORITHM, nullptr, 0) == 0
				&& BCryptOpenAlgorithmProvider(&providers.ecdsa, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) == 0
				&& BCryptOpenAlgorithmProvider(&providers.sha256, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0;
			return providers;
		}

		std::string ToHex(const unsigned char* data, size_t size)
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

		// False for anything that isn't hex: what the other side sends can't be trusted to be.
		bool FromHex(const std::string& hex, std::vector<unsigned char>& bytes)
		{
			auto digit = [](char c) {
				return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
			};
			if (hex.size() % 2 != 0 || hex.size() > 4096)
				return false;
			bytes.clear();
			for (size_t i = 0; i < hex.size(); i += 2)
			{
				int high = digit(hex[i]);
				int low = digit(hex[i + 1]);
				if (high < 0 || low < 0)
					return false;
				bytes.push_back(static_cast<unsigned char>(high * 16 + low));
			}
			return true;
		}

		std::vector<unsigned char> Sha256(const void* data, size_t size)
		{
			std::vector<unsigned char> hash(32);
			BCryptHash(Algorithms().sha256, nullptr, 0, static_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(size),
				hash.data(), static_cast<ULONG>(hash.size()));
			return hash;
		}

		std::string ExportPublic(BCRYPT_KEY_HANDLE key)
		{
			ULONG size = 0;
			if (BCryptExportKey(key, nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &size, 0) != 0)
				return "";
			std::vector<unsigned char> blob(size);
			if (BCryptExportKey(key, nullptr, BCRYPT_ECCPUBLIC_BLOB, blob.data(), size, &size, 0) != 0)
				return "";
			return ToHex(blob.data(), size);
		}

		// A P-256 public key from the other side: the right kind and size, or nothing.
		BCRYPT_KEY_HANDLE ImportPublic(BCRYPT_ALG_HANDLE algorithm, ULONG magic, const std::string& hex)
		{
			std::vector<unsigned char> blob;
			if (!FromHex(hex, blob) || blob.size() != sizeof(BCRYPT_ECCKEY_BLOB) + 2 * P256_BYTES)
				return nullptr;
			BCRYPT_ECCKEY_BLOB header = {};
			std::memcpy(&header, blob.data(), sizeof(header));
			if (header.dwMagic != magic || header.cbKey != P256_BYTES)
				return nullptr;
			BCRYPT_KEY_HANDLE key = nullptr;
			if (BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, blob.data(), static_cast<ULONG>(blob.size()), 0) != 0)
				return nullptr;
			return key;
		}

		// An AES-256-GCM key, or null.
		BCRYPT_KEY_HANDLE MakeAesKey(const std::vector<unsigned char>& key)
		{
			BCRYPT_KEY_HANDLE made = nullptr;
			if (BCryptGenerateSymmetricKey(Algorithms().aes, &made, nullptr, 0, const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0) != 0)
				return nullptr;
			return made;
		}
	}

	// ---- Cipher

	Cipher::~Cipher()
	{
		if (m_key)
			BCryptDestroyKey(static_cast<BCRYPT_KEY_HANDLE>(m_key));
	}

	bool Cipher::Seal(std::string& message)
	{
		// The message's number is its nonce: never the same twice under one key.
		unsigned char nonce[NONCE_BYTES] = {};
		std::memcpy(nonce + 4, &m_counter, sizeof(m_counter));
		m_counter++;
		unsigned char tag[TAG_BYTES] = {};
		BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
		BCRYPT_INIT_AUTH_MODE_INFO(info);
		info.pbNonce = nonce;
		info.cbNonce = NONCE_BYTES;
		info.pbTag = tag;
		info.cbTag = TAG_BYTES;

		std::string sealed(message.size(), '\0');
		ULONG written = 0;
		if (BCryptEncrypt(static_cast<BCRYPT_KEY_HANDLE>(m_key), reinterpret_cast<PUCHAR>(message.data()), static_cast<ULONG>(message.size()),
			&info, nullptr, 0, reinterpret_cast<PUCHAR>(sealed.data()), static_cast<ULONG>(sealed.size()), &written, 0) != 0)
			return false;
		sealed.append(reinterpret_cast<const char*>(tag), TAG_BYTES);
		message = std::move(sealed);
		return true;
	}

	bool Cipher::Open(std::string& message)
	{
		if (message.size() < TAG_BYTES)
			return false;
		unsigned char nonce[NONCE_BYTES] = {};
		std::memcpy(nonce + 4, &m_counter, sizeof(m_counter));
		m_counter++;
		unsigned char tag[TAG_BYTES] = {};
		size_t size = message.size() - TAG_BYTES;
		std::memcpy(tag, message.data() + size, TAG_BYTES);
		BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
		BCRYPT_INIT_AUTH_MODE_INFO(info);
		info.pbNonce = nonce;
		info.cbNonce = NONCE_BYTES;
		info.pbTag = tag;
		info.cbTag = TAG_BYTES;

		std::string opened(size, '\0');
		ULONG written = 0;
		if (BCryptDecrypt(static_cast<BCRYPT_KEY_HANDLE>(m_key), reinterpret_cast<PUCHAR>(message.data()), static_cast<ULONG>(size),
			&info, nullptr, 0, reinterpret_cast<PUCHAR>(opened.data()), static_cast<ULONG>(size), &written, 0) != 0)
			return false;
		message = std::move(opened);
		return true;
	}

	// ---- KeyExchange

	KeyExchange::~KeyExchange()
	{
		if (m_key)
			BCryptDestroyKey(static_cast<BCRYPT_KEY_HANDLE>(m_key));
	}

	std::unique_ptr<KeyExchange> KeyExchange::Create()
	{
		if (!Algorithms().ready)
			return nullptr;
		BCRYPT_KEY_HANDLE key = nullptr;
		if (BCryptGenerateKeyPair(Algorithms().ecdh, &key, 256, 0) != 0)
			return nullptr;
		if (BCryptFinalizeKeyPair(key, 0) != 0)
		{
			BCryptDestroyKey(key);
			return nullptr;
		}
		std::string publicKey = ExportPublic(key);
		if (publicKey.empty())
		{
			BCryptDestroyKey(key);
			return nullptr;
		}
		return std::unique_ptr<KeyExchange>(new KeyExchange(key, std::move(publicKey)));
	}

	bool KeyExchange::Derive(const std::string& peerPublic, bool server, const std::string& clientPublic, const std::string& serverPublic,
		std::unique_ptr<Cipher>& sending, std::unique_ptr<Cipher>& receiving) const
	{
		BCRYPT_KEY_HANDLE peer = ImportPublic(Algorithms().ecdh, BCRYPT_ECDH_PUBLIC_P256_MAGIC, peerPublic);
		if (!peer)
			return false;
		BCRYPT_SECRET_HANDLE secret = nullptr;
		bool agreed = BCryptSecretAgreement(static_cast<BCRYPT_KEY_HANDLE>(m_key), peer, &secret, 0) == 0;
		BCryptDestroyKey(peer);
		if (!agreed)
			return false;

		// SHA-256 of a label for the direction, the shared secret, and both keys: a different key
		// each way, tied to this exchange.
		std::string transcript = clientPublic + serverPublic;
		auto derive = [&](const char* label, std::vector<unsigned char>& key) {
			BCryptBuffer buffers[3] = {};
			buffers[0] = { static_cast<ULONG>(sizeof(BCRYPT_SHA256_ALGORITHM)), KDF_HASH_ALGORITHM, const_cast<wchar_t*>(BCRYPT_SHA256_ALGORITHM) };
			buffers[1] = { static_cast<ULONG>(std::strlen(label)), KDF_SECRET_PREPEND, const_cast<char*>(label) };
			buffers[2] = { static_cast<ULONG>(transcript.size()), KDF_SECRET_APPEND, transcript.data() };
			BCryptBufferDesc parameters = { BCRYPTBUFFER_VERSION, static_cast<ULONG>(std::size(buffers)), buffers };
			key.assign(KEY_BYTES, 0);
			ULONG size = 0;
			return BCryptDeriveKey(secret, BCRYPT_KDF_HASH, &parameters, key.data(), KEY_BYTES, &size, 0) == 0 && size == KEY_BYTES;
		};
		std::vector<unsigned char> toServer, toClient;
		bool derived = derive("LV-EngineBuilder game to server", toServer) && derive("LV-EngineBuilder server to game", toClient);
		BCryptDestroySecret(secret);
		if (!derived)
			return false;

		BCRYPT_KEY_HANDLE sendKey = MakeAesKey(server ? toClient : toServer);
		BCRYPT_KEY_HANDLE receiveKey = MakeAesKey(server ? toServer : toClient);
		SecureZeroMemory(toServer.data(), toServer.size());
		SecureZeroMemory(toClient.data(), toClient.size());
		if (!sendKey || !receiveKey)
		{
			if (sendKey)
				BCryptDestroyKey(sendKey);
			if (receiveKey)
				BCryptDestroyKey(receiveKey);
			return false;
		}
		sending.reset(new Cipher(sendKey));
		receiving.reset(new Cipher(receiveKey));
		return true;
	}

	// ---- Identity

	Identity::~Identity()
	{
		if (m_key)
			BCryptDestroyKey(static_cast<BCRYPT_KEY_HANDLE>(m_key));
	}

	std::unique_ptr<Identity> Identity::LoadOrCreate(const std::string& path, std::string& error)
	{
		if (!Algorithms().ready)
		{
			error = "Windows cryptography didn't start";
			return nullptr;
		}
		BCRYPT_KEY_HANDLE key = nullptr;
		if (std::filesystem::exists(path))
		{
			std::ifstream file(path, std::ios::binary);
			std::vector<unsigned char> blob((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			if (BCryptImportKeyPair(Algorithms().ecdsa, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key, blob.data(), static_cast<ULONG>(blob.size()), 0) != 0)
			{
				error = std::format("{} isn't a server identity key", path);
				return nullptr;
			}
			SecureZeroMemory(blob.data(), blob.size());
		}
		else
		{
			if (BCryptGenerateKeyPair(Algorithms().ecdsa, &key, 256, 0) != 0 || BCryptFinalizeKeyPair(key, 0) != 0)
			{
				error = "couldn't make a server identity key";
				return nullptr;
			}
			ULONG size = 0;
			BCryptExportKey(key, nullptr, BCRYPT_ECCPRIVATE_BLOB, nullptr, 0, &size, 0);
			std::vector<unsigned char> blob(size);
			if (BCryptExportKey(key, nullptr, BCRYPT_ECCPRIVATE_BLOB, blob.data(), size, &size, 0) != 0)
			{
				BCryptDestroyKey(key);
				error = "couldn't save the server identity key";
				return nullptr;
			}
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			file.write(reinterpret_cast<const char*>(blob.data()), static_cast<std::streamsize>(blob.size()));
			SecureZeroMemory(blob.data(), blob.size());
			if (!file)
			{
				BCryptDestroyKey(key);
				error = std::format("couldn't write {}", path);
				return nullptr;
			}
		}
		std::string publicKey = ExportPublic(key);
		return std::unique_ptr<Identity>(new Identity(key, std::move(publicKey)));
	}

	std::string Identity::Sign(const std::string& data) const
	{
		auto hash = Sha256(data.data(), data.size());
		ULONG size = 0;
		if (BCryptSignHash(static_cast<BCRYPT_KEY_HANDLE>(m_key), nullptr, hash.data(), static_cast<ULONG>(hash.size()), nullptr, 0, &size, 0) != 0)
			return "";
		std::vector<unsigned char> signature(size);
		if (BCryptSignHash(static_cast<BCRYPT_KEY_HANDLE>(m_key), nullptr, hash.data(), static_cast<ULONG>(hash.size()),
			signature.data(), size, &size, 0) != 0)
			return "";
		return ToHex(signature.data(), size);
	}

	bool VerifySignature(const std::string& identity, const std::string& data, const std::string& signature)
	{
		if (!Algorithms().ready)
			return false;
		std::vector<unsigned char> signatureBytes;
		if (!FromHex(signature, signatureBytes) || signatureBytes.size() != 2 * P256_BYTES)
			return false;
		BCRYPT_KEY_HANDLE key = ImportPublic(Algorithms().ecdsa, BCRYPT_ECDSA_PUBLIC_P256_MAGIC, identity);
		if (!key)
			return false;
		auto hash = Sha256(data.data(), data.size());
		bool valid = BCryptVerifySignature(key, nullptr, hash.data(), static_cast<ULONG>(hash.size()),
			signatureBytes.data(), static_cast<ULONG>(signatureBytes.size()), 0) == 0;
		BCryptDestroyKey(key);
		return valid;
	}

	std::string HandshakeTranscript(const std::string& clientPublic, const std::string& serverPublic)
	{
		return "LV-EngineBuilder handshake 1|" + clientPublic + "|" + serverPublic;
	}

	std::string Fingerprint(const std::string& identity)
	{
		std::vector<unsigned char> blob;
		if (!FromHex(identity, blob))
			return "";
		auto hash = Sha256(blob.data(), blob.size());
		return ToHex(hash.data(), hash.size());
	}

	KnownHosts::Result KnownHosts::Check(const std::string& host, const std::string& fingerprint) const
	{
		{
			std::ifstream file(m_path);
			std::string line;
			while (std::getline(file, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (line.empty() || line[0] == '#')
					continue;
				size_t space = line.find(' ');
				if (space == std::string::npos || line.substr(0, space) != host)
					continue;
				return line.substr(space + 1) == fingerprint ? Result::SAME : Result::CHANGED;
			}
		}
		bool fresh = !std::filesystem::exists(m_path);
		std::ofstream file(m_path, std::ios::app);
		if (fresh)
			file << "# Servers joined before, and the identity each one proved it had. A server that turns up\n"
				"# with a different one is refused: delete its line only if you know why it changed.\n";
		file << host << ' ' << fingerprint << '\n';
		return Result::NEW;
	}
}
