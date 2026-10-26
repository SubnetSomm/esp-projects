#pragma once
struct MockMdns {bool begin(const char*){return true;}void addService(const char*,const char*,int){}};
extern MockMdns MDNS;
