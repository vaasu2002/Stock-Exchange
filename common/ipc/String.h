#pragma once

#include <cstring>
#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <ostream>
#include <istream>

namespace Exchange::Core {
    
    class String {
    public:
        using sizeType = std::size_t;
        static constexpr sizeType npos = static_cast<sizeType>(-1);

    private:
        char* mBuffer;
        sizeType mSize;

        static const char& nullChar()
        {
            static const char null = '\0';
            return null;
        }
    
    public:

        // ------------------------------------ <Constructors> ------------------------------------

        /** @brief Constructor */
        String() : mBuffer(nullptr), mSize(0) {}

        /** @brief Constructor */
        String(const char* str) {
            if (str && *str) {
                mSize = std::strlen(str);
                mBuffer = new char[mSize + 1];
                std::memcpy(mBuffer, str, mSize + 1);
            }
            else {
                mBuffer = nullptr;
                mSize = 0;
            }
        }

        /** @brief Constructor */
        String(const char* str, sizeType len) {
            if (str && len > 0) {
                mSize = len;
                mBuffer = new char[mSize + 1];
                std::memcpy(mBuffer, str, mSize);
                mBuffer[mSize] = nullChar();
            }
            else {
                mBuffer = nullptr;
                mSize = 0;
            }
        }

        /** @brief Constructor */
        String(const std::string& str) {
            mSize = std::strlen(str.c_str());
            if (mSize > 0) {
                mBuffer = new char[mSize + 1];
                std::memcpy(mBuffer, str.c_str(), mSize + 1);
            }
            else {
                mBuffer = nullptr;
            }
        }

        /** @brief Copy constructor */
        String(const String& other) {
            if (other.mBuffer) {
                mSize = other.mSize;
                mBuffer = new char [mSize + 1];
                std::memcpy(mBuffer, other.mBuffer, mSize + 1);
            }
            else {
                mBuffer = nullptr;
                mSize = 0;
            }
        }

        /** @brief Move constructor */
        String(String&& other) : mBuffer(other.mBuffer), mSize(other.mSize) {
            other.mBuffer = nullptr;
            other.mSize = 0;
        }

        /** @brief Fill constructor */
        String(sizeType count, char ch) {
            if (count > 0) {
                mSize = count;
                mBuffer = new char[mSize + 1];
                std::memset(mBuffer, ch, mSize);
                mBuffer[mSize] = '\0';
            } else {
                mBuffer = nullptr;
                mSize = 0;
            }
        }

        /** @brief Destructor */
        ~String() {
            delete[] mBuffer;
            mSize = 0;
        }

        // ------------------------------------ <Assignment operators> ------------------------------------

        /** @brief Copy assignment */
        String& operator=(const String& other) {
            if (this != &other) {
                String temp(other);
                swap(temp);
            }
            return *this;
        }

        /** @brief Move assignment */
        String& operator=(String&& other) {
            if (this != &other) {
                delete[] mBuffer;
                mBuffer = other.mBuffer;
                mSize = other.mSize;
                other.mBuffer = nullptr;
                other.mSize = 0;
            }
            return *this;
        }

        /** @brief From C-string */
        String& operator=(const char* str) {
            String temp(str);
            swap(temp);
            return *this;
        }

        // ------------------------------------ <Implicit conversions> ------------------------------------

        operator const char*() const {
            return mBuffer ? mBuffer : "";
        }

        const char* get() const {
            return mBuffer ? mBuffer : "";
        }

        char* get() {
            return mBuffer;
        }

        std::string toString() const {
            return static_cast<std::string>(mBuffer);
        }

        // ------------------------------------ <Capacity> ------------------------------------

        sizeType size() const noexcept {
            return mSize;
        }

        bool empty() const noexcept {
            return mSize == 0;
        }

        // ------------------------------------ <Element access> ------------------------------------

        char& operator[](sizeType pos) noexcept {
            return mBuffer[pos];
        }

        const char& operator[](sizeType pos) const noexcept {
            return mBuffer ? mBuffer[pos] : nullChar();
        }

        char& at(sizeType pos) {
            if (pos >= mSize) {
                throw std::out_of_range("String::at: index out of range");
            }
            return mBuffer[pos];
        }
    
        const char& at(sizeType pos) const {
            if (pos >= mSize) {
                throw std::out_of_range("String::at: index out of range");
            }
            return mBuffer[pos];
        }
        
        char& front() noexcept {
            return mBuffer[0];
        }
        
        const char& front() const noexcept {
            return mBuffer ? mBuffer[0] : nullChar();
        }
        
        char& back() noexcept {
            return mBuffer[mSize - 1];
        }
        
        const char& back() const noexcept {
            return mBuffer && mSize > 0 ? mBuffer[mSize - 1] : nullChar();
        }

        // ------------------------------------ <Modifiers> ------------------------------------

        void clear() noexcept {
            delete[] mBuffer;
            mBuffer = nullptr;
            mSize = 0;
        }
    
        String& append(const char* str) {
            if (str && *str) {
                sizeType str_len = std::strlen(str);
                sizeType new_size = mSize + str_len;
                char* new_buffer = new char[new_size + 1];
                
                if (mBuffer) {
                    std::memcpy(new_buffer, mBuffer, mSize);
                }
                std::memcpy(new_buffer + mSize, str, str_len + 1);
                
                delete[] mBuffer;
                mBuffer = new_buffer;
                mSize = new_size;
            }
            return *this;
        }
        
        String& append(const String& str) {
            return append(str.mBuffer);
        }
        
        String& append(char ch) {
            char temp[2] = {ch, nullChar()};
            return append(temp);
        }
        
        String& append(const char* str, sizeType count) {
            if (str && count > 0) {
                sizeType new_size = mSize + count;
                char* new_buffer = new char[new_size + 1];
                
                if (mBuffer) {
                    std::memcpy(new_buffer, mBuffer, mSize);
                }
                std::memcpy(new_buffer + mSize, str, count);
                new_buffer[new_size] = nullChar();
                
                delete[] mBuffer;
                mBuffer = new_buffer;
                mSize = new_size;
            }
            return *this;
        }

        String& operator+=(const String& str) {
            return append(str);
        }
        
        String& operator+=(const char* str) {
            return append(str);
        }
        
        String& operator+=(char ch) {
            return append(ch);
        }



        friend String operator+(const String& lhs, const String& rhs) {
            String result;
            result.mSize = lhs.mSize + rhs.mSize;
            result.mBuffer = new char[result.mSize + 1];
            
            if (lhs.mBuffer) {
                std::memcpy(result.mBuffer, lhs.mBuffer, lhs.mSize);
            }
            if (rhs.mBuffer) {
                std::memcpy(result.mBuffer + lhs.mSize, rhs.mBuffer, rhs.mSize);
            }
            result.mBuffer[result.mSize] = '\0';
            
            return result;
        }
        
        friend String operator+(String&& lhs, const String& rhs) {
            lhs.append(rhs);
            return std::move(lhs);
        }
        
        friend String operator+(const String& lhs, String&& rhs) {
            String result(lhs);
            result.append(rhs);
            return result;
        }
        
        friend String operator+(String&& lhs, String&& rhs) {
            lhs.append(rhs);
            return std::move(lhs);
        }
        
        friend String operator+(const String& lhs, const char* rhs) {
            String result(lhs);
            result.append(rhs);
            return result;
        }
        
        friend String operator+(const char* lhs, const String& rhs) {
            String result(lhs);
            result.append(rhs);
            return result;
        }
        
        friend String operator+(const String& lhs, char rhs) {
            String result(lhs);
            result.append(rhs);
            return result;
        }
        
        friend String operator+(char lhs, const String& rhs) {
            String result(1, lhs);
            result.append(rhs);
            return result;
        }

        friend String operator+(String&& lhs, const char* rhs) {
            lhs.append(rhs);
            return std::move(lhs);
        }

        friend String operator+(String&& lhs, char rhs) {
            lhs.append(rhs);
            return std::move(lhs);
        }

        // ------------------------------------ <Comparison> ------------------------------------

        int compare(const String& other) const noexcept {
            if (!mBuffer && !other.mBuffer) return 0;
            if (!mBuffer) return -1;
            if (!other.mBuffer) return 1;
            return std::strcmp(mBuffer, other.mBuffer);
        }
        
        int compare(const char* str) const noexcept {
            if (!mBuffer && !str) return 0;
            if (!mBuffer) return -1;
            if (!str) return 1;
            return std::strcmp(mBuffer, str);
        }

        friend bool operator==(const String& lhs, const String& rhs) noexcept {
            return lhs.compare(rhs) == 0;
        }
        
        friend bool operator==(const String& lhs, const char* rhs) noexcept {
            return lhs.compare(rhs) == 0;
        }
        
        friend bool operator==(const char* lhs, const String& rhs) noexcept {
            return rhs.compare(lhs) == 0;
        }



        // ------------------------------------ <Search> ------------------------------------
    
        sizeType find(const char* str, sizeType pos = 0) const noexcept {
            if (!mBuffer || !str || pos >= mSize) {
                return npos;
            }
            const char* result = std::strstr(mBuffer + pos, str);
            return result ? static_cast<sizeType>(result - mBuffer) : npos;
        }
        
        sizeType find(char ch, sizeType pos = 0) const noexcept {
            if (!mBuffer || pos >= mSize) {
                return npos;
            }
            const char* result = std::strchr(mBuffer + pos, ch);
            return result ? static_cast<sizeType>(result - mBuffer) : npos;
        }
        
        sizeType find(const String& str, sizeType pos = 0) const noexcept {
            return find(str.mBuffer, pos);
        }

        void swap(String& other) {
            std::swap(mBuffer, other.mBuffer);
            std::swap(mSize, other.mSize);
        }
    };
}