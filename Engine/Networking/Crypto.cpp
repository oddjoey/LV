// Built without the precompiled header, like Socket.cpp: it needs windows.h on its own terms.
// On Windows it's Windows' own cryptography (CNG); elsewhere (macOS) OpenSSL 3. Both write keys and
// signatures the same way (CNG's blobs, and r then s), so a game on one can join a server on the other.
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#endif

#include "Crypto.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>

namespace Net
{
	namespace
	{
		constexpr size_t NONCE_BYTES = 12;
		constexpr size_t TAG_BYTES = 16;
		constexpr size_t KEY_BYTES = 32;
		constexpr size_t P256_BYTES = 32;

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

		// The nonce for a message: its number, after four zero bytes.
		void NonceFor(uint64_t counter, unsigned char (&nonce)[NONCE_BYTES])
		{
			std::memset(nonce, 0, NONCE_BYTES);
			std::memcpy(nonce + 4, &counter, sizeof(counter));
		}

		const char* const TO_SERVER_LABEL = "LV-EngineBuilder game to server";
		const char* const TO_CLIENT_LABEL = "LV-EngineBuilder server to game";
	}
}

#ifdef _WIN32

namespace Net
{
	namespace
	{
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
		unsigned char nonce[NONCE_BYTES];
		NonceFor(m_counter++, nonce);
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
		unsigned char nonce[NONCE_BYTES];
		NonceFor(m_counter++, nonce);
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
		bool derived = derive(TO_SERVER_LABEL, toServer) && derive(TO_CLIENT_LABEL, toClient);
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
}

#else

namespace Net
{
	namespace
	{
		// CNG's key blobs: a magic number and the key size, then X and Y (and d, for a private key).
		constexpr uint32_t ECDH_PUBLIC_MAGIC = 0x314B4345;		// "ECK1"
		constexpr uint32_t ECDSA_PUBLIC_MAGIC = 0x31534345;		// "ECS1"
		constexpr uint32_t ECDSA_PRIVATE_MAGIC = 0x32534345;	// "ECS2"
		constexpr size_t BLOB_HEADER = 8;

		std::vector<unsigned char> Sha256(const void* data, size_t size)
		{
			std::vector<unsigned char> hash(32);
			unsigned int length = 0;
			EVP_Digest(data, size, hash.data(), &length, EVP_sha256(), nullptr);
			return hash;
		}

		EVP_PKEY* NewP256Key()
		{
			return EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
		}

		// X and Y (and d, if asked and it has one), each 32 bytes, big-endian.
		bool KeyParts(EVP_PKEY* key, std::vector<unsigned char>& xy, std::vector<unsigned char>* d)
		{
			unsigned char point[1 + 2 * P256_BYTES] = {};
			size_t length = 0;
			if (EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY, point, sizeof(point), &length) != 1
				|| length != sizeof(point) || point[0] != 0x04)
				return false;
			xy.assign(point + 1, point + sizeof(point));
			if (d)
			{
				BIGNUM* secret = nullptr;
				if (EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_PRIV_KEY, &secret) != 1)
					return false;
				d->assign(P256_BYTES, 0);
				bool fits = BN_bn2binpad(secret, d->data(), static_cast<int>(P256_BYTES)) == static_cast<int>(P256_BYTES);
				BN_clear_free(secret);
				return fits;
			}
			return true;
		}

		std::vector<unsigned char> Blob(uint32_t magic, const std::vector<unsigned char>& parts)
		{
			std::vector<unsigned char> blob(BLOB_HEADER);
			uint32_t size = static_cast<uint32_t>(P256_BYTES);
			std::memcpy(blob.data(), &magic, 4);
			std::memcpy(blob.data() + 4, &size, 4);
			blob.insert(blob.end(), parts.begin(), parts.end());
			return blob;
		}

		std::string ExportPublic(EVP_PKEY* key, uint32_t magic)
		{
			std::vector<unsigned char> xy;
			if (!KeyParts(key, xy, nullptr))
				return "";
			auto blob = Blob(magic, xy);
			return ToHex(blob.data(), blob.size());
		}

		// A P-256 key from X and Y (and d), checked to be a real point on the curve.
		EVP_PKEY* FromParts(const unsigned char* xy, const unsigned char* d)
		{
			unsigned char point[1 + 2 * P256_BYTES];
			point[0] = 0x04;
			std::memcpy(point + 1, xy, 2 * P256_BYTES);
			OSSL_PARAM_BLD* builder = OSSL_PARAM_BLD_new();
			BIGNUM* secret = d ? BN_bin2bn(d, static_cast<int>(P256_BYTES), nullptr) : nullptr;
			OSSL_PARAM_BLD_push_utf8_string(builder, OSSL_PKEY_PARAM_GROUP_NAME, "prime256v1", 0);
			OSSL_PARAM_BLD_push_octet_string(builder, OSSL_PKEY_PARAM_PUB_KEY, point, sizeof(point));
			if (secret)
				OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_PRIV_KEY, secret);
			OSSL_PARAM* params = OSSL_PARAM_BLD_to_param(builder);
			EVP_PKEY_CTX* context = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
			EVP_PKEY* key = nullptr;
			bool made = params && context && EVP_PKEY_fromdata_init(context) == 1
				&& EVP_PKEY_fromdata(context, &key, d ? EVP_PKEY_KEYPAIR : EVP_PKEY_PUBLIC_KEY, params) == 1;
			EVP_PKEY_CTX_free(context);
			OSSL_PARAM_free(params);
			OSSL_PARAM_BLD_free(builder);
			BN_clear_free(secret);
			if (!made)
				return nullptr;
			EVP_PKEY_CTX* check = EVP_PKEY_CTX_new(key, nullptr);
			bool valid = check && EVP_PKEY_public_check(check) == 1;
			EVP_PKEY_CTX_free(check);
			if (!valid)
			{
				EVP_PKEY_free(key);
				return nullptr;
			}
			return key;
		}

		// A P-256 public key from the other side: the right kind and size, or nothing.
		EVP_PKEY* ImportPublic(uint32_t magic, const std::string& hex)
		{
			std::vector<unsigned char> blob;
			if (!FromHex(hex, blob) || blob.size() != BLOB_HEADER + 2 * P256_BYTES)
				return nullptr;
			uint32_t blobMagic = 0, size = 0;
			std::memcpy(&blobMagic, blob.data(), 4);
			std::memcpy(&size, blob.data() + 4, 4);
			if (blobMagic != magic || size != P256_BYTES)
				return nullptr;
			return FromParts(blob.data() + BLOB_HEADER, nullptr);
		}

		// An AES-256-GCM key, kept for the cipher.
		using AesKey = std::array<unsigned char, KEY_BYTES>;
		AesKey* MakeAesKey(const std::vector<unsigned char>& key)
		{
			auto made = new AesKey();
			std::memcpy(made->data(), key.data(), KEY_BYTES);
			return made;
		}
	}

	// ---- Cipher

	Cipher::~Cipher()
	{
		if (auto key = static_cast<AesKey*>(m_key))
		{
			OPENSSL_cleanse(key->data(), key->size());
			delete key;
		}
	}

	bool Cipher::Seal(std::string& message)
	{
		// The message's number is its nonce: never the same twice under one key.
		unsigned char nonce[NONCE_BYTES];
		NonceFor(m_counter++, nonce);
		EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
		std::string sealed(message.size() + TAG_BYTES, '\0');
		int written = 0, last = 0;
		bool ok = context && EVP_EncryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
			&& EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(NONCE_BYTES), nullptr) == 1
			&& EVP_EncryptInit_ex(context, nullptr, nullptr, static_cast<AesKey*>(m_key)->data(), nonce) == 1
			&& EVP_EncryptUpdate(context, reinterpret_cast<unsigned char*>(sealed.data()), &written,
				reinterpret_cast<const unsigned char*>(message.data()), static_cast<int>(message.size())) == 1
			&& EVP_EncryptFinal_ex(context, reinterpret_cast<unsigned char*>(sealed.data()) + written, &last) == 1
			&& EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, static_cast<int>(TAG_BYTES), sealed.data() + message.size()) == 1;
		EVP_CIPHER_CTX_free(context);
		if (!ok)
			return false;
		message = std::move(sealed);
		return true;
	}

	bool Cipher::Open(std::string& message)
	{
		if (message.size() < TAG_BYTES)
			return false;
		unsigned char nonce[NONCE_BYTES];
		NonceFor(m_counter++, nonce);
		size_t size = message.size() - TAG_BYTES;
		unsigned char tag[TAG_BYTES];
		std::memcpy(tag, message.data() + size, TAG_BYTES);
		EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
		std::string opened(size, '\0');
		int written = 0, last = 0;
		bool ok = context && EVP_DecryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
			&& EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(NONCE_BYTES), nullptr) == 1
			&& EVP_DecryptInit_ex(context, nullptr, nullptr, static_cast<AesKey*>(m_key)->data(), nonce) == 1
			&& EVP_DecryptUpdate(context, reinterpret_cast<unsigned char*>(opened.data()), &written,
				reinterpret_cast<const unsigned char*>(message.data()), static_cast<int>(size)) == 1
			&& EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG, static_cast<int>(TAG_BYTES), tag) == 1
			&& EVP_DecryptFinal_ex(context, reinterpret_cast<unsigned char*>(opened.data()) + written, &last) == 1;
		EVP_CIPHER_CTX_free(context);
		if (!ok)
			return false;
		message = std::move(opened);
		return true;
	}

	// ---- KeyExchange

	KeyExchange::~KeyExchange()
	{
		EVP_PKEY_free(static_cast<EVP_PKEY*>(m_key));
	}

	std::unique_ptr<KeyExchange> KeyExchange::Create()
	{
		EVP_PKEY* key = NewP256Key();
		if (!key)
			return nullptr;
		std::string publicKey = ExportPublic(key, ECDH_PUBLIC_MAGIC);
		if (publicKey.empty())
		{
			EVP_PKEY_free(key);
			return nullptr;
		}
		return std::unique_ptr<KeyExchange>(new KeyExchange(key, std::move(publicKey)));
	}

	bool KeyExchange::Derive(const std::string& peerPublic, bool server, const std::string& clientPublic, const std::string& serverPublic,
		std::unique_ptr<Cipher>& sending, std::unique_ptr<Cipher>& receiving) const
	{
		EVP_PKEY* peer = ImportPublic(ECDH_PUBLIC_MAGIC, peerPublic);
		if (!peer)
			return false;
		// The shared secret: the X of the agreed point, big-endian, as CNG's hash KDF takes it.
		std::vector<unsigned char> secret(P256_BYTES);
		size_t length = secret.size();
		EVP_PKEY_CTX* context = EVP_PKEY_CTX_new(static_cast<EVP_PKEY*>(m_key), nullptr);
		bool agreed = context && EVP_PKEY_derive_init(context) == 1 && EVP_PKEY_derive_set_peer(context, peer) == 1
			&& EVP_PKEY_derive(context, secret.data(), &length) == 1 && length == P256_BYTES;
		EVP_PKEY_CTX_free(context);
		EVP_PKEY_free(peer);
		if (!agreed)
			return false;

		// SHA-256 of a label for the direction, the shared secret, and both keys: a different key
		// each way, tied to this exchange.
		std::string transcript = clientPublic + serverPublic;
		auto derive = [&](const char* label) {
			std::vector<unsigned char> input(label, label + std::strlen(label));
			input.insert(input.end(), secret.begin(), secret.end());
			input.insert(input.end(), transcript.begin(), transcript.end());
			auto key = Sha256(input.data(), input.size());
			OPENSSL_cleanse(input.data(), input.size());
			return key;
		};
		auto toServer = derive(TO_SERVER_LABEL);
		auto toClient = derive(TO_CLIENT_LABEL);
		OPENSSL_cleanse(secret.data(), secret.size());
		sending.reset(new Cipher(MakeAesKey(server ? toClient : toServer)));
		receiving.reset(new Cipher(MakeAesKey(server ? toServer : toClient)));
		OPENSSL_cleanse(toServer.data(), toServer.size());
		OPENSSL_cleanse(toClient.data(), toClient.size());
		return true;
	}

	// ---- Identity

	Identity::~Identity()
	{
		EVP_PKEY_free(static_cast<EVP_PKEY*>(m_key));
	}

	std::unique_ptr<Identity> Identity::LoadOrCreate(const std::string& path, std::string& error)
	{
		EVP_PKEY* key = nullptr;
		if (std::filesystem::exists(path))
		{
			// The same file a Windows server writes: CNG's private key blob.
			std::ifstream file(path, std::ios::binary);
			std::vector<unsigned char> blob((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			uint32_t magic = 0, size = 0;
			if (blob.size() == BLOB_HEADER + 3 * P256_BYTES)
			{
				std::memcpy(&magic, blob.data(), 4);
				std::memcpy(&size, blob.data() + 4, 4);
			}
			if (magic == ECDSA_PRIVATE_MAGIC && size == P256_BYTES)
				key = FromParts(blob.data() + BLOB_HEADER, blob.data() + BLOB_HEADER + 2 * P256_BYTES);
			OPENSSL_cleanse(blob.data(), blob.size());
			if (!key)
			{
				error = std::format("{} isn't a server identity key", path);
				return nullptr;
			}
		}
		else
		{
			key = NewP256Key();
			std::vector<unsigned char> xy, d;
			if (!key || !KeyParts(key, xy, &d))
			{
				EVP_PKEY_free(key);
				error = "couldn't make a server identity key";
				return nullptr;
			}
			xy.insert(xy.end(), d.begin(), d.end());
			auto blob = Blob(ECDSA_PRIVATE_MAGIC, xy);
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			file.write(reinterpret_cast<const char*>(blob.data()), static_cast<std::streamsize>(blob.size()));
			OPENSSL_cleanse(blob.data(), blob.size());
			OPENSSL_cleanse(d.data(), d.size());
			if (!file)
			{
				EVP_PKEY_free(key);
				error = std::format("couldn't write {}", path);
				return nullptr;
			}
		}
		std::string publicKey = ExportPublic(key, ECDSA_PUBLIC_MAGIC);
		return std::unique_ptr<Identity>(new Identity(key, std::move(publicKey)));
	}

	std::string Identity::Sign(const std::string& data) const
	{
		// Signs the SHA-256 of it, then gives r and s as 32 bytes each, as CNG does.
		auto hash = Sha256(data.data(), data.size());
		EVP_PKEY_CTX* context = EVP_PKEY_CTX_new(static_cast<EVP_PKEY*>(m_key), nullptr);
		std::vector<unsigned char> der(128);
		size_t length = der.size();
		bool signedIt = context && EVP_PKEY_sign_init(context) == 1 && EVP_PKEY_sign(context, der.data(), &length, hash.data(), hash.size()) == 1;
		EVP_PKEY_CTX_free(context);
		if (!signedIt)
			return "";
		const unsigned char* at = der.data();
		ECDSA_SIG* signature = d2i_ECDSA_SIG(nullptr, &at, static_cast<long>(length));
		if (!signature)
			return "";
		const BIGNUM* r = nullptr;
		const BIGNUM* s = nullptr;
		ECDSA_SIG_get0(signature, &r, &s);
		unsigned char raw[2 * P256_BYTES];
		bool fits = BN_bn2binpad(r, raw, static_cast<int>(P256_BYTES)) == static_cast<int>(P256_BYTES)
			&& BN_bn2binpad(s, raw + P256_BYTES, static_cast<int>(P256_BYTES)) == static_cast<int>(P256_BYTES);
		ECDSA_SIG_free(signature);
		return fits ? ToHex(raw, sizeof(raw)) : "";
	}

	bool VerifySignature(const std::string& identity, const std::string& data, const std::string& signature)
	{
		std::vector<unsigned char> raw;
		if (!FromHex(signature, raw) || raw.size() != 2 * P256_BYTES)
			return false;
		EVP_PKEY* key = ImportPublic(ECDSA_PUBLIC_MAGIC, identity);
		if (!key)
			return false;
		ECDSA_SIG* parsed = ECDSA_SIG_new();
		ECDSA_SIG_set0(parsed, BN_bin2bn(raw.data(), static_cast<int>(P256_BYTES), nullptr),
			BN_bin2bn(raw.data() + P256_BYTES, static_cast<int>(P256_BYTES), nullptr));
		unsigned char* der = nullptr;
		int derLength = i2d_ECDSA_SIG(parsed, &der);
		ECDSA_SIG_free(parsed);
		auto hash = Sha256(data.data(), data.size());
		EVP_PKEY_CTX* context = EVP_PKEY_CTX_new(key, nullptr);
		bool valid = derLength > 0 && context && EVP_PKEY_verify_init(context) == 1
			&& EVP_PKEY_verify(context, der, static_cast<size_t>(derLength), hash.data(), hash.size()) == 1;
		EVP_PKEY_CTX_free(context);
		OPENSSL_free(der);
		EVP_PKEY_free(key);
		return valid;
	}
}

#endif

namespace Net
{
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
