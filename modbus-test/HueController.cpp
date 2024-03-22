#include "HueController.h"

HueController::HueController(std::string IP) {
    _curl = curl_easy_init();
    _IP = IP;
    std::cout << "HueController init" << std::endl;
}

HueController::~HueController() {
    curl_easy_cleanup(_curl);
}

void HueController::setLevel(int level, int id) {
    std::string URL = "http://" + _IP + "/api/6VXhfzufKJBijZpBj0lcUZUYULfXtDVR73c3zvX6/lights/" + std::to_string(id) + "/state";
    std::string brightnessStr = std::to_string(level); // 0 - 255
    std::string jsonData = "{ \"bri\": " + brightnessStr + " }";
    sendPUTrequest(URL, jsonData);
}

void HueController::toggleLamp(bool state, int id) {
    std::string URL = "http://" + _IP + "/api/6VXhfzufKJBijZpBj0lcUZUYULfXtDVR73c3zvX6/lights/" + std::to_string(id) + "/state";
    std::string stateStr = (state) ? "true" : "false";
    std::string jsonData = "{ \"on\": " + stateStr + " }";
    sendPUTrequest(URL, jsonData);
}

void HueController::sendPUTrequest(std::string url, std::string data) {
    curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, data.c_str());
    CURLcode res = curl_easy_perform(_curl);
    if (res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
}