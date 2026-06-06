#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <format>
#include <type_traits>

#include <termcolor.hpp>
#include <magic_enum.hpp>

class Logger
{
public:
	enum class LogLevel { DEBUG, INFO, SUCCESS, WARNING, FATAL };

	template <typename T>
	static void Print(const T& value, const char* valueName, const LogLevel& level)
	{
		PrintSuffex(level);

		const auto& type = typeid(value);
		
		auto pCharConstCheck = std::make_unique<char[]>(11);
		strncpy_s(pCharConstCheck.get(), 11, type.name(), 10);

		if (type == typeid(std::string) || !strcmp(pCharConstCheck.get(), "char const"))
			std::cout << termcolor::bright_grey << value << termcolor::reset << '\n';
		else
		{
			if (level == LogLevel::DEBUG)
				std::cout << termcolor::white << "[" << termcolor::green << valueName << termcolor::white << "] ";
			
			if constexpr (std::is_enum_v<T>)
				std::cout << termcolor::bright_cyan << magic_enum::enum_name(value) << '\n';
			else if (std::is_pointer_v<T>)
				std::cout << termcolor::bright_blue << "0x" << std::hex << value << '\n';
			else
				std::cout << termcolor::reset << value << '\n';

			std::cout << termcolor::reset << std::dec;
		}
	}
	template <class T>
	static void Print(const std::vector<T>& elements, const char* valueName, const LogLevel& level)
	{
		PrintSuffex(level);

		std::cout << termcolor::yellow << "[" << elements.size() << "] ";

		if (level == LogLevel::DEBUG)
			std::cout << termcolor::white << "[" << termcolor::green << valueName << termcolor::white << "]\n";

		int i = 0;
		for (const auto& element : elements)
			std::cout << termcolor::white << "	- " << termcolor::yellow << "[" << i++ << "] " << termcolor::bright_yellow << element << '\n';

		std::cout << termcolor::reset;
	}
	template <class T1, class T2>
	static void Print(const std::pair<T1, T2>& elements, const char* valueName, const LogLevel& level)
	{
		PrintSuffex(level);

		if (level == LogLevel::DEBUG)
			std::cout << termcolor::white << "[" << termcolor::green << valueName << termcolor::white << "] ";

		std::cout << termcolor::reset << "[" << elements.first << ", " << elements.second << "]\n";
	}
private:
	static void PrintSuffex(const LogLevel& level)
	{
		switch (level)
		{
			case LogLevel::DEBUG:
				std::cout << termcolor::white << "[" << termcolor::bright_cyan << "DEBUG" << termcolor::white << "] ";
				break;

			case LogLevel::INFO:
				std::cout << termcolor::white << "[" << termcolor::bright_grey << "INFO" << termcolor::white << "] ";
				break;

			case LogLevel::SUCCESS:
				std::cout << termcolor::white << "[" << termcolor::bright_green << "SUCCESS" << termcolor::white << "] ";
				break;

			case LogLevel::WARNING:
				std::cout << termcolor::white << "[" << termcolor::yellow << "WARNING" << termcolor::white << "] ";
				break;

			case LogLevel::FATAL:
				std::cout << termcolor::white << "[" << termcolor::red << "FATAL" << termcolor::white << "] ";
				break;

			default:
				std::cerr << termcolor::white << "[" << termcolor::bright_magenta << "UNKNOWN" << termcolor::white << "] ";
				break;
		}
	}
};

#ifdef _DEBUG
	#define LOG_DEBUG(value)	Logger::Print(value, #value, Logger::LogLevel::DEBUG)
#else
	#define LOG_DEBUG(value)	((void)0) // Do nothing in release mode
#endif // _DEBUG

#define LOG_INFO(value)		Logger::Print(value, #value, Logger::LogLevel::INFO)
#define LOG_SUCCESS(value)	Logger::Print(value, #value, Logger::LogLevel::SUCCESS)
#define LOG_WARNING(value)	Logger::Print(value, #value, Logger::LogLevel::WARNING)
#define LOG_FATAL(value)	Logger::Print(value, #value, Logger::LogLevel::FATAL)