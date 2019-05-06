#pragma once
#include <stdexcept> 
#include <sstream>

// Application Exception
class AppException : public std::exception
{
public:
	AppException(const std::string& msg) : exception(msg.c_str()) { }

	static inline AppException CreateExceptionWin(const std::string& errorStr, const uint32_t winErrorCode, const std::string& functionName, const std::string& fileName, int lineNo)
	{
		std::ostringstream errorLog;
		if (winErrorCode != ERROR_SUCCESS)
		{
			CHAR* buf;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK, 0, winErrorCode, 0, (LPSTR)&buf, 0, NULL);
			errorLog << functionName << " : " << errorStr << " with (" << buf << ") at " << fileName << ":" << lineNo << std::endl;
		}
		else
		{
			errorLog << functionName << " : " << errorStr << " at " << fileName << ":" << lineNo << std::endl;
		}
		AppException exception(errorLog.str());

		return exception;
	}
};

#define APPTHROWEXC( errorStr, winErrorCode )                                                        \
do                                                                                                   \
{                                                                                                    \
    throw AppException::CreateExceptionWin(errorStr, winErrorCode, __FUNCTION__, __FILE__, __LINE__); \
} while (0)

#define APPWIN32_CK( check, str )               \
    do                                          \
    {                                           \
        if (check == false)						\
        {                                       \
            APPTHROWEXC(str, GetLastError());	\
		}                                       \
    }                                           \
    while (0)

#define APPHRESULT_CK( call, descr )											\
    do																			\
    {																			\
		HRESULT hres = call;													\
        if (hres != S_OK)														\
        {																		\
			std::stringstream errStr;											\
			errStr << std::string(descr) << " HRESULT: 0x" << std::hex << hres;	\
            APPTHROWEXC(errStr.str(), 0);										\
		}																		\
    }																			\
    while (0)
