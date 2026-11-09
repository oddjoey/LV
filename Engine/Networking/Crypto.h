#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Encryption between a game and its server (Monocypher, compiled in: the same on every platform), so passwords
// and everything else sent can't be read or changed on the way.
//
// The handshake, before anything private is said:
//   client -> server   its KeyExchange's public key
//   server -> client   its KeyExchange's public key, its Identity, and the Identity's signature of
//                      HandshakeTranscript(both keys)
// 
// Each side makes a new X25519 key pair for the connection, and from its own private key and the
// other's public key both work out the same secret, which nobody watching can. Two XChaCha20-Poly1305
// keys come from it (hashed with BLAKE2b), one each way (Connection::Encrypt). Identities sign with EdDSA. The server's signature proves the exchange is
// with the holder of its identity key, which it keeps for good: the client remembers each server's
// identity the first time (KnownHosts), and won't talk to a server at that address with another.

namespace Net
{
	// One direction of an encrypted connection: XChaCha20-Poly1305. Messages are numbered, so one that's
	// changed, replayed, dropped or reordered fails to open.
	class Cipher
	{
	public:
		~Cipher();
		Cipher(const Cipher&) = delete;
		Cipher& operator=(const Cipher&) = delete;

		// Encrypts in place (the tag goes on the end).
		bool Seal(std::string& message);
		// Decrypts in place. False if it isn't exactly what the other side sealed.
		bool Open(std::string& message);
	private:
		friend class KeyExchange;
		explicit Cipher(const std::array<uint8_t, 32>& key) : m_key(key) {}

		std::array<uint8_t, 32>	m_key = {};
		uint64_t		m_counter = 0;
	};

	// One side's half of the key exchange, made fresh for each connection.
	class KeyExchange
	{
	public:
		~KeyExchange();
		KeyExchange(const KeyExchange&) = delete;
		KeyExchange& operator=(const KeyExchange&) = delete;

		static std::unique_ptr<KeyExchange> Create();

		// To send to the other side, as hex.
		const std::string& PublicKey() const { return m_public; }

		// The keys each way, from the other side's public key. False if it isn't a proper key.
		bool Derive(const std::string& peerPublic, bool server, const std::string& clientPublic, const std::string& serverPublic,
			std::unique_ptr<Cipher>& sending, std::unique_ptr<Cipher>& receiving) const;
	private:
		explicit KeyExchange(const std::array<uint8_t, 32>& secret, std::string publicKey) : m_secret(secret), m_public(std::move(publicKey)) {}

		std::array<uint8_t, 32>	m_secret = {};
		std::string		m_public = "";
	};

	// The server's identity: an EdDSA signing key it keeps in a file, so players can tell it's the
	// same server they joined before. A key file from before (P-256) is replaced with a new one.
	class Identity
	{
	public:
		~Identity();
		Identity(const Identity&) = delete;
		Identity& operator=(const Identity&) = delete;

		// Read from the file, or made and written there if there isn't one.
		static std::unique_ptr<Identity> LoadOrCreate(const std::string& path, std::string& error);
		const std::string& PublicKey() const { return m_public; }
		std::string Sign(const std::string& data) const;
	private:
		explicit Identity(const std::array<uint8_t, 64>& secret, std::string publicKey) : m_secret(secret), m_public(std::move(publicKey)) {}

		std::array<uint8_t, 64>	m_secret = {};
		std::string		m_public = "";
	};

	bool VerifySignature(const std::string& identity, const std::string& data, const std::string& signature);
	// What the server signs: both sides' keys for this connection.
	std::string HandshakeTranscript(const std::string& clientPublic, const std::string& serverPublic);
	// A short-to-compare name for an identity: the BLAKE2b-256 of it, in hex.
	std::string Fingerprint(const std::string& identity);

	// The servers a client has talked to, and each one's identity fingerprint, in a text file: one
	// "host:port fingerprint" a line. The first time, a server is taken at its word (as SSH does);
	// after that it has to be the same.
	class KnownHosts
	{
	public:
		enum class Result { NEW, SAME, CHANGED };

		explicit KnownHosts(std::string path) : m_path(std::move(path)) {}
		// Checks a server's fingerprint, remembering it if the server is new.
		Result Check(const std::string& host, const std::string& fingerprint) const;
	private:
		std::string		m_path = "";
	};
}
