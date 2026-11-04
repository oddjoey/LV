#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Messages over TCP (Winsock), for a game and its server. Every call returns straight away: a
// message is a length-prefixed block, sent as fast as the socket takes it and handed back once all
// of it has arrived. Once both ends have agreed keys (see Crypto.h), every message is encrypted.
namespace Net
{
	class Cipher;

	class Connection
	{
	public:
		enum class Status { CONNECTING, OPEN, FAILED };

		~Connection();
		Connection(const Connection&) = delete;
		Connection& operator=(const Connection&) = delete;

		// Starts connecting to host (a name or an address) without waiting for it: see PollConnect.
		// Null, with why, if it can't even start.
		static std::unique_ptr<Connection> Begin(const std::string& host, uint16_t port, std::string& error);
		// Whether the connection has been made yet, or failed (and why).
		Status PollConnect(std::string& error);

		// From here on, every message is sealed with one cipher on the way out and opened with the other
		// on the way in; one that won't open closes the connection.
		void Encrypt(std::unique_ptr<Cipher> sending, std::unique_ptr<Cipher> receiving);
		bool IsEncrypted() const { return m_sending != nullptr; }

		// Queues a message and sends what the socket will take of everything queued (nothing until it
		// has connected). False once closed.
		bool Send(const std::string& message);
		// Sends more of what's queued.
		void Flush();
		// Every message that has fully arrived since the last call. False once the other end has gone.
		bool Receive(std::vector<std::string>& messages);
		bool IsOpen() const { return m_socket != INVALID; }
		void Close();
		// "192.168.1.20", for the player list.
		const std::string& GetAddress() const { return m_address; }
	private:
		friend class Listener;
		static constexpr uintptr_t INVALID = ~uintptr_t(0);
		Connection(uintptr_t socket, std::string address);

		uintptr_t				m_socket = INVALID;
		bool					m_connecting = false;
		std::unique_ptr<Cipher>	m_sending = nullptr;
		std::unique_ptr<Cipher>	m_receiving = nullptr;
		std::string				m_address = "";
		std::string				m_outgoing = "";
		std::string				m_incoming = "";
	};

	class Listener
	{
	public:
		~Listener();
		Listener(const Listener&) = delete;
		Listener& operator=(const Listener&) = delete;

		// Listens on every network interface. Null, with why, if the port can't be had.
		static std::unique_ptr<Listener> Listen(uint16_t port, std::string& error);
		// A player who has just connected, or null.
		std::unique_ptr<Connection> Accept();
	private:
		explicit Listener(uintptr_t socket) : m_socket(socket) {}
		uintptr_t				m_socket = ~uintptr_t(0);
	};
}
