#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <termcolor.hpp>

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
				std::cout << termcolor::bright_green << "[" << valueName << "] ";

			std::cout << termcolor::reset << value << '\n';
		}
	}
	template <class T>
	static void Print(const std::vector<T>& elements, const char* valueName, const LogLevel& level)
	{
		PrintSuffex(level);

		std::cout << termcolor::yellow << "[" << elements.size() << "] ";

		if (level == LogLevel::DEBUG)
			std::cout << termcolor::bright_green << "[" << valueName << "]\n";

		int i = 0;
		for (const auto& element : elements)
			std::cout << termcolor::white << "	- " << termcolor::yellow << "[" << i++ << "] " << termcolor::bright_yellow << element << '\n';

		std::cout << termcolor::reset;
	}
private:
	static void PrintSuffex(const LogLevel& level)
	{
		switch (level)
		{
			case LogLevel::DEBUG:
				std::cout << termcolor::blue << "[DEBUG] ";
				break;

			case LogLevel::INFO:
				std::cout << termcolor::white << "[INFO] ";
				break;

			case LogLevel::SUCCESS:
				std::cout << termcolor::green << "[SUCCESS] ";
				break;

			case LogLevel::WARNING:
				std::cout << termcolor::yellow << "[WARNING] ";
				break;

			case LogLevel::FATAL:
				std::cout << termcolor::red << "[FATAL] ";
				break;

			default:
				std::cerr << termcolor::bright_magenta << "[UNKNOWN] ";
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