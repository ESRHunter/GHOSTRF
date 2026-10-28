#pragma once
// Mock Environment for Serial CLI Testing on Host C++

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>

// Arduino String class simulation
class String : public std::string {
public:
    String() : std::string() {}
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string &s) : std::string(s) {}
    String(int val) : std::string(std::to_string(val)) {}
    String(unsigned int val) : std::string(std::to_string(val)) {}
    String(long val) : std::string(std::to_string(val)) {}
    String(unsigned long val) : std::string(std::to_string(val)) {}
    String(float val, int dec = 2) {
        char b[32]; snprintf(b, sizeof(b), "%.*f", dec, val);
        *this = b;
    }
    String(double val, int dec = 2) {
        char b[32]; snprintf(b, sizeof(b), "%.*f", dec, val);
        *this = b;
    }
};

// Mock Serial device
class MockSerialPort {
public:
    std::string rxBuffer;
    size_t      rxPos = 0;
    std::string txBuffer;
    std::vector<std::string> outputLines;

    int available() {
        return (rxPos < rxBuffer.size()) ? (int)(rxBuffer.size() - rxPos) : 0;
    }

    int read() {
        if (rxPos < rxBuffer.size()) {
            return (unsigned char)rxBuffer[rxPos++];
        }
        return -1;
    }

    size_t write(char c) {
        txBuffer += c;
        if (c == '\n') {
            outputLines.push_back(txBuffer);
            txBuffer.clear();
        }
        return 1;
    }

    size_t write(uint8_t c) {
        return write((char)c);
    }

    size_t print(const char* s) {
        if (!s) return 0;
        size_t len = strlen(s);
        for (size_t i = 0; i < len; i++) write(s[i]);
        return len;
    }

    size_t print(const String &s) {
        return print(s.c_str());
    }

    size_t print(int v) { return print(std::to_string(v).c_str()); }
    size_t print(unsigned int v) { return print(std::to_string(v).c_str()); }
    size_t print(long v) { return print(std::to_string(v).c_str()); }
    size_t print(unsigned long v) { return print(std::to_string(v).c_str()); }
    size_t print(float v, int dec = 2) {
        char b[32]; snprintf(b, sizeof(b), "%.*f", dec, v);
        return print(b);
    }

    size_t println(const char* s = "") {
        size_t n = print(s);
        write('\n');
        return n + 1;
    }

    size_t println(const String &s) {
        return println(s.c_str());
    }

    size_t println(int v) { return println(std::to_string(v).c_str()); }
    size_t println(unsigned int v) { return println(std::to_string(v).c_str()); }
    size_t println(long v) { return println(std::to_string(v).c_str()); }
    size_t println(unsigned long v) { return println(std::to_string(v).c_str()); }

    size_t printf(const char* fmt, ...) {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        if (n > 0) print(buf);
        return (n > 0) ? (size_t)n : 0;
    }

    void feed(const std::string& input) {
        rxBuffer += input;
    }

    void clear() {
        rxBuffer.clear();
        rxPos = 0;
        txBuffer.clear();
        outputLines.clear();
    }
};

extern MockSerialPort Serial;

// ESP mock
struct MockESP {
    uint32_t getFreeHeap() { return 245000; }
    void restart() {}
};
extern MockESP ESP;

// LittleFS file mock
namespace fs {
class File {
private:
    std::string _content;
    size_t      _pos;
    bool        _open;
    bool        _dir;
public:
    File() : _pos(0), _open(false), _dir(false) {}
    File(const std::string& data, bool open = true, bool dir = false)
        : _content(data), _pos(0), _open(open), _dir(dir) {}

    operator bool() const { return _open; }
    bool available() const { return _open && _pos < _content.size(); }
    int read() { return available() ? (uint8_t)_content[_pos++] : -1; }
    size_t read(uint8_t* buf, size_t maxLen) {
        if (!available() || !buf || maxLen == 0) return 0;
        size_t toRead = maxLen;
        if (_pos + toRead > _content.size()) toRead = _content.size() - _pos;
        memcpy(buf, _content.data() + _pos, toRead);
        _pos += toRead;
        return toRead;
    }
    size_t size() const { return _content.size(); }
    bool isDirectory() const { return _dir; }
    void close() { _open = false; }
};
} // namespace fs

class MockLittleFS {
public:
    std::map<std::string, std::string> files;
    bool mounted = true;

    bool exists(const char* path) {
        return files.find(path) != files.end();
    }

    fs::File open(const char* path, const char* = "r") {
        auto it = files.find(path);
        if (it != files.end()) {
            return fs::File(it->second, true, false);
        }
        return fs::File("", false, false);
    }

    bool format() {
        files.clear();
        return true;
    }

    size_t totalBytes() { return 896 * 1024; }
    size_t usedBytes() { return files.size() * 1024; }
};

extern MockLittleFS LittleFS;
