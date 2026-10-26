#pragma once
#include "Arduino.h"
constexpr int WIFI_AP_STA=3,WIFI_STA=1,WL_CONNECTED=3,WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2;
class IPAddress {
 uint8_t b[4];
public:
 IPAddress(uint8_t a=0,uint8_t c=0,uint8_t d=0,uint8_t e=0):b{a,c,d,e}{}
 uint8_t operator[](int i)const{return b[i];}
 bool operator!=(const IPAddress &o)const{return std::memcmp(b,o.b,4)!=0;}
 String toString()const {char t[20];std::snprintf(t,sizeof(t),"%u.%u.%u.%u",b[0],b[1],b[2],b[3]);return String(t);}
};
class MockWifi {
public:
 int wifiStatus=0,radioMode=0,attempts=0,scanStatus=WIFI_SCAN_FAILED;
 bool persistence=true,autoReconnect=false;
 String lastSsid,lastPassword;
 IPAddress address{192,168,1,80},mask{255,255,255,0},apAddress;
 void persistent(bool on){persistence=on;}
 bool setHostname(const char*){return true;}
 bool mode(int m){radioMode=m;return true;}
 void disconnect(bool,bool){wifiStatus=0;}
 void setAutoReconnect(bool on){autoReconnect=on;}
 int status()const{return wifiStatus;}
 int begin(const char *ssid,const char *password){++attempts;lastSsid=ssid;lastPassword=password?password:"";return 0;}
 bool softAPConfig(IPAddress ip,IPAddress,IPAddress){apAddress=ip;return true;}
 bool softAP(const char*,const char*,int,bool,int){return true;}
 IPAddress localIP()const{return address;}
 IPAddress subnetMask()const{return mask;}
 int scanComplete()const{return scanStatus;}
 void scanDelete(){scanStatus=WIFI_SCAN_FAILED;}
 int scanNetworks(bool,bool,bool,int){scanStatus=WIFI_SCAN_RUNNING;return scanStatus;}
};
extern MockWifi WiFi;
