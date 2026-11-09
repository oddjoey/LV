// Built without the precompiled header: Winsock 2 has to come before windows.h, which that pulls in.
// Elsewhere (macOS) it's the BSD sockets, under the same names as Winsock's below.
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SocketLength = int;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
using SocketLength = socklen_t;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
inline int closesocket(SOCKET socket) { return ::close(socket); }
inline int WSAGetLastError() { return errno; }
// A connect that's under way says EINPROGRESS here, where Winsock says it would block.
constexpr int WSAEWOULDBLOCK = EWOULDBLOCK;
constexpr int WSAECONNREFUSED = ECONNREFUSED;
constexpr int WSAETIMEDOUT = ETIMEDOUT;
constexpr int WSAEHOSTUNREACH = EHOSTUNREACH;
constexpr int WSAENETUNREACH = ENETUNREACH;
constexpr int WSAEADDRINUSE = EADDRINUSE;
constexpr int WSAEACCES = EACCES;
#endif

#include "Socket.h"
#include "Crypto.h"

#include <algorithm>
#include <cstring>
#include <format>

namespace Net
{
	namespace
	{
		// Bigger than any message is allowed to be: a game may send its whole world in one.
		constexpr uint32_t MAX_MESSAGE = 64u * 1024u * 1024u;

		bool Startup()
		{
#ifdef _WIN32
			static bool started = [] {
				WSADATA data = {};
				return WSAStartup(MAKEWORD(2, 2), &data) == 0;
			}();
			return started;
#else
			return true;
#endif
		}

		// Whether the last call only didn't finish because the socket doesn't wait.
		bool WouldBlock()
		{
			int code = WSAGetLastError();
#ifdef _WIN32
			return code == WSAEWOULDBLOCK;
#else
			return code == EWOULDBLOCK || code == EAGAIN || code == EINPROGRESS;
#endif
		}

		void SetNonBlocking(SOCKET socket)
		{
#ifdef _WIN32
			u_long on = 1;
			ioctlsocket(socket, FIONBIO, &on);
#else
			fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
			// A send to a closed connection fails rather than killing the program.
			int on = 1;
#ifdef SO_NOSIGPIPE
			setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
#endif
		}

		void MakeNonBlocking(SOCKET socket)
		{
			SetNonBlocking(socket);
			// Small messages (where everyone is) go at once, not batched up.
			int noDelay = 1;
			setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
		}

		std::string ErrorText(int code)
		{
			switch (code)
			{
				case WSAECONNREFUSED:	return "nothing is listening there";
				case WSAETIMEDOUT:		return "it timed out";
				case WSAEHOSTUNREACH:
				case WSAENETUNREACH:	return "that address can't be reached";
				case WSAEADDRINUSE:		return "the port is already in use";
				case WSAEACCES:			return "the port isn't allowed";
				default:				return std::format("network error {}", code);
			}
		}
	}

	Connection::Connection(uintptr_t socket, std::string address)
		: m_socket(socket), m_address(std::move(address))
	{
	}

	Connection::~Connection()
	{
		Close();
	}

	std::unique_ptr<Connection> Connection::Begin(const std::string& host, uint16_t port, std::string& error)
	{
		if (!Startup())
		{
			error = "networking didn't start";
			return nullptr;
		}
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		addrinfo* found = nullptr;
		if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0 || !found)
		{
			error = std::format("can't find {}", host);
			return nullptr;
		}

		SOCKET socket = ::socket(found->ai_family, found->ai_socktype, found->ai_protocol);
		if (socket == INVALID_SOCKET)
		{
			freeaddrinfo(found);
			error = ErrorText(WSAGetLastError());
			return nullptr;
		}
		MakeNonBlocking(socket);
		int result = ::connect(socket, found->ai_addr, static_cast<int>(found->ai_addrlen));
		char address[INET_ADDRSTRLEN] = {};
		inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(found->ai_addr)->sin_addr, address, sizeof(address));
		freeaddrinfo(found);

		bool connecting = result == SOCKET_ERROR && WouldBlock();
		if (result == SOCKET_ERROR && !connecting)
		{
			error = ErrorText(WSAGetLastError());
			closesocket(socket);
			return nullptr;
		}
		auto connection = std::unique_ptr<Connection>(new Connection(socket, address));
		connection->m_connecting = connecting;
		return connection;
	}

	Connection::Status Connection::PollConnect(std::string& error)
	{
		if (!IsOpen())
		{
			error = "the connection closed";
			return Status::FAILED;
		}
		if (!m_connecting)
			return Status::OPEN;

		fd_set writable;
		FD_ZERO(&writable);
		FD_SET(static_cast<SOCKET>(m_socket), &writable);
		fd_set failed;
		FD_ZERO(&failed);
		FD_SET(static_cast<SOCKET>(m_socket), &failed);
		timeval now = { 0, 0 };
		int ready = select(static_cast<int>(m_socket) + 1, nullptr, &writable, &failed, &now);
		if (ready == 0)
			return Status::CONNECTING;
		if (ready < 0 || FD_ISSET(static_cast<SOCKET>(m_socket), &failed))
		{
			int code = WSAECONNREFUSED;
			SocketLength length = sizeof(code);
			if (ready > 0)
				getsockopt(static_cast<SOCKET>(m_socket), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&code), &length);
			error = ErrorText(code);
			Close();
			return Status::FAILED;
		}
		m_connecting = false;
		Flush();
		return Status::OPEN;
	}

	bool Connection::Send(const std::string& message)
	{
		if (!IsOpen())
			return false;
		std::string sealed = message;
		if (m_sending && !m_sending->Seal(sealed))
		{
			Close();
			return false;
		}
		uint32_t length = static_cast<uint32_t>(sealed.size());
		char header[4];
		std::memcpy(header, &length, sizeof(header));
		m_outgoing.append(header, sizeof(header));
		m_outgoing += sealed;
		Flush();
		return IsOpen();
	}

	void Connection::Encrypt(std::unique_ptr<Cipher> sending, std::unique_ptr<Cipher> receiving)
	{
		m_sending = std::move(sending);
		m_receiving = std::move(receiving);
	}

	void Connection::Flush()
	{
		while (IsOpen() && !m_connecting && !m_outgoing.empty())
		{
			int chunk = static_cast<int>((std::min)(m_outgoing.size(), size_t(1) << 20));
			int sent = ::send(static_cast<SOCKET>(m_socket), m_outgoing.data(), chunk, 0);
			if (sent == SOCKET_ERROR)
			{
				if (!WouldBlock())
					Close();
				return;
			}
			m_outgoing.erase(0, static_cast<size_t>(sent));
		}
	}

	bool Connection::Receive(std::vector<std::string>& messages)
	{
		if (!IsOpen())
			return false;
		if (m_connecting)
			return true;
		Flush();
		char buffer[64 * 1024];
		while (true)
		{
			int received = ::recv(static_cast<SOCKET>(m_socket), buffer, sizeof(buffer), 0);
			if (received > 0)
			{
				m_incoming.append(buffer, static_cast<size_t>(received));
				continue;
			}
			if (received == 0 || !WouldBlock())
				Close();
			break;
		}

		size_t at = 0;
		while (m_incoming.size() - at >= 4)
		{
			uint32_t length = 0;
			std::memcpy(&length, m_incoming.data() + at, sizeof(length));
			if (length > MAX_MESSAGE)
			{
				Close();
				break;
			}
			if (m_incoming.size() - at - 4 < length)
				break;
			std::string message = m_incoming.substr(at + 4, length);
			at += 4 + length;
			// Anything that isn't exactly what the other end sealed: someone in between, so no more.
			if (m_receiving && !m_receiving->Open(message))
			{
				Close();
				break;
			}
			messages.push_back(std::move(message));
		}
		m_incoming.erase(0, at);
		return IsOpen() || !messages.empty();
	}

	void Connection::Close()
	{
		if (m_socket != INVALID)
		{
			closesocket(static_cast<SOCKET>(m_socket));
			m_socket = INVALID;
		}
	}

	Listener::~Listener()
	{
		if (m_socket != ~uintptr_t(0))
			closesocket(static_cast<SOCKET>(m_socket));
	}

	std::unique_ptr<Listener> Listener::Listen(uint16_t port, std::string& error)
	{
		if (!Startup())
		{
			error = "networking didn't start";
			return nullptr;
		}
		SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket == INVALID_SOCKET)
		{
			error = ErrorText(WSAGetLastError());
			return nullptr;
		}
#ifndef _WIN32
		// A server restarted straight away can have its port back.
		int reuse = 1;
		setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_ANY);
		address.sin_port = htons(port);
		if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR
			|| listen(socket, SOMAXCONN) == SOCKET_ERROR)
		{
			error = ErrorText(WSAGetLastError());
			closesocket(socket);
			return nullptr;
		}
		SetNonBlocking(socket);
		return std::unique_ptr<Listener>(new Listener(socket));
	}

	std::unique_ptr<Connection> Listener::Accept()
	{
		sockaddr_in from = {};
		SocketLength length = sizeof(from);
		SOCKET socket = accept(static_cast<SOCKET>(m_socket), reinterpret_cast<sockaddr*>(&from), &length);
		if (socket == INVALID_SOCKET)
			return nullptr;
		MakeNonBlocking(socket);
		char address[INET_ADDRSTRLEN] = {};
		inet_ntop(AF_INET, &from.sin_addr, address, sizeof(address));
		return std::unique_ptr<Connection>(new Connection(socket, address));
	}
}
