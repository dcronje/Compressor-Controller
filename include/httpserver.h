// http_server.h
#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "lwip/tcp.h"
#include <string>
#include <vector>

// Function prototypes
void startHttpServer();
void stopHttpServer();

#endif // HTTPSERVER_H