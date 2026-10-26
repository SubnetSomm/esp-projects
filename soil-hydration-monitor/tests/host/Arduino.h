#pragma once
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstring>
class String : public std::string {
public:
 using std::string::string;
 String(const std::string &s):std::string(s){}
 String(int n):std::string(std::to_string(n)){}
 String(unsigned n):std::string(std::to_string(n)){}
 String(unsigned long n):std::string(std::to_string(n)){}
 size_t length() const{return size();}
};
inline String operator+(const String &a,const String &b){return String(static_cast<const std::string &>(a)+static_cast<const std::string &>(b));}
inline String operator+(const char *a,const String &b){return String(a)+b;}
inline String operator+(const String &a,const char *b){return a+String(b);}
extern uint32_t mockMillis;
inline uint32_t millis(){return mockMillis;}
struct MockEsp {uint64_t getEfuseMac(){return 0x123456;}};
extern MockEsp ESP;
