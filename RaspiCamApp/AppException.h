#pragma once
#include <stdexcept> 
#include <sstream>

// Application Exception
class AppException : public std::exception
{
public:
	AppException(const std::string& msg) : _msg(msg) { }

	static inline AppException CreateExceptionLinux(const std::string& errorStr, const uint32_t winErrorCode, const std::string& functionName, const std::string& fileName, int lineNo)
	{
		std::ostringstream errorLog;

		errorLog << functionName << " : " << errorStr << " at " << fileName << ":" << lineNo << std::endl;
		AppException exception(errorLog.str());

		return exception;
	}

	const char* what() const noexcept {return _msg.c_str(); };

private:
	std::string _msg;
};

#define APPTHROWEXC( errorStr, winErrorCode )                                                        \
do                                                                                                   \
{                                                                                                    \
    throw AppException::CreateExceptionLinux(errorStr, winErrorCode, __FUNCTION__, __FILE__, __LINE__); \
} while (0)

#define APPWIN32_CK( check, str )               \
    do                                          \
    {                                           \
        if (check == false)			\
        {                                       \
            APPTHROWEXC(str, 0);	\
	}                                       \
    }                                           \
    while (0)

