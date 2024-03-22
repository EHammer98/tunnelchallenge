#include <stdio.h>
#include <string.h>
#include <iostream>
#include <chrono>
#include <iostream>
#include <thread>
#include <curl/curl.h>

class HueController
{
private:
    CURL *_curl;
    std::string _IP;

public:
    HueController(std::string IP);
    ~HueController();
    void setLevel(int level, int id);
    void toggleLamp(bool state, int id);
    void sendPUTrequest(std::string url, std::string data);
};