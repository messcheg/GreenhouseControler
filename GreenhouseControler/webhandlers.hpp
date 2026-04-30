#pragma once
#include <ESP8266WebServer.h>

void registerWebHandlers(ESP8266WebServer& server);
void handleNotFound();