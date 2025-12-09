#pragma once

#include <stdexcept>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <vector>

namespace Engine {

    // safe printf-style formatting into std::string
    inline std::string vformat(const char* fmt, va_list args) {
        if (!fmt) {
            return {};
        }

        va_list argsCopy;
        va_copy(argsCopy, args);

        // First call to get required size
        int len = std::vsnprintf(nullptr, 0, fmt, argsCopy);
        va_end(argsCopy);

        if (len <= 0) {
            return {};
        }

        std::string result;
        result.resize(static_cast<std::size_t>(len));

        std::vsnprintf(result.data(), result.size() + 1, fmt, args);
        return result;
    }

    inline std::string format(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        std::string s = vformat(fmt, args);
        va_end(args);
        return s;
    }


    class EngException : public std::runtime_error {

        std::string mFile;
        std::string mFunction;
        int mLine;
    public:

        /** Constrcutor */
        EngException(std::string message, std::string file, int line, std::string func)
        : std::runtime_error(std::move(message)),mFile(std::move(file)),
        mFunction(std::move(func)),mLine(line){}

        void log(const char* contextFormat = nullptr, ...) const noexcept {
            std::string context;
            if (contextFormat) {
                va_list args;
                va_start(args, contextFormat);
                context = vformat(contextFormat, args);
                va_end(args);
            }

            std::fprintf(stderr, "[ERROR] %s(%d) in %s: ", mFile.c_str(), mLine, mFunction.c_str());

            if (!context.empty()) {
                std::fprintf(stderr, "%s: ", context.c_str());
            }

            std::fprintf(stderr, "%s\n", what());
        }

        [[noreturn]]
        static void Throw(const char* file, int line, const char* function, const char* fmt, ...) {
            va_list args;
            va_start(args, fmt);
            std::string msg = vformat(fmt, args);
            va_end(args);

            throw EngException{ std::move(msg), file ? file : "", line, function ? function : "" };
        }


        /**
         * @brief Throw a formatted exception involving the operating system error code (errno).
         * Use it when:
         * - An OS-level call failed (e.g., open, read, write, fopen, sockets, file operations)
         */
        [[noreturn]]
        static void ThrowErrno(const char* file, int line, const char* function, int err, const char* fmt, ...) {
            // User message
            va_list args;
            va_start(args, fmt);
            std::string userMsg = vformat(fmt, args);
            va_end(args);

            // errno message
            char buf[256]{};

            #if defined(_WIN32)
                    strerror_s(buf, sizeof(buf), err);
            #else
                    // POSIX strerror_r
                    if (strerror_r(err, buf, sizeof(buf)) != 0)
                    {
                        std::snprintf(buf, sizeof(buf),
                                    "Unknown error (errno %d)", err);
                    }
            #endif

            std::string msg = userMsg + "\n[errno " + std::to_string(err) + "] " + buf;

            throw EngException{ std::move(msg), file ? file : "", line, function ? function : "" };
        }
    };

    #define ENG_THROW(...) \
        ::Engine::EngException::Throw(__FILE__, __LINE__, __func__, __VA_ARGS__)


    // Throw including an explicit errno value:
    //   APP_THROW_ERRNO(e, "read() failed");
    #define ENG_THROW_ERRNO(err, ...) \
        ::Engine::EngException::ThrowErrno(__FILE__, __LINE__, __func__, (err), __VA_ARGS__)


    // Throw using current errno:
    //   ENG_THROW_LAST_ERRNO("open() failed for '%s'", path.c_str());
    #define APP_THROW_LAST_ERRNO(...) \
        ::Engine::EngException::ThrowErrno(__FILE__, __LINE__, __func__, errno, __VA_ARGS__)

} // namespace Engine

/**
 * Examples:
 * 1) 
 * FILE* f = fopen("config.ini", "r");
 * if (!f) {
 *  ENG_THROW_LAST_ERRNO("Could not open config file");
 *  ENG_THROW_ERRNO(e, "Could not open config file");
 * }
 * 
 * 2)
 * if (index >= items.size()) {
 *  ENG_THROW("Index %d out of range", index);
 * }
 *   
 * 
 * 3)
 * try{
 *     // something
 * }
 * catch(const Engine::AppException& exp){
 *     ex.log("Sfailed"); 
 *     return 1;
 * }
 * 
**/