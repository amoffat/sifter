/*
 * Copyright (C) 2013 Andrew Moffat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */



#include <string>
#include <sstream>
#include <cstring>

extern "C" {
#include "mongoose.h"
}

#include "web_server.h"
#include "sifter.h"
#include "logging.h"


/*
 * for uploaded files
 */
const char* TMP_DIR = "/tmp";


Server::Server() {
    pendingMatches = 0;
}

void Server::serve(int port) {
    this->port = port;
    log_server("starting up", logging::HIGH);

    mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    callbacks.begin_request = handleRequest;
    callbacks.upload = handleUpload;


    // NULL is the sentinel
    const char *options[] = {"listening_ports", std::to_string(port).c_str(),
        NULL};

    ctx = mg_start(&callbacks, this, options);
}

void Server::stop() {
    //log_server("stopping", logging::HIGH);
    mg_stop(ctx);
}

void Server::setHealthyThreshold(int healthyThreshold) {
    this->healthyThreshold = healthyThreshold;
}

void Server::setMatcher(Matcher matcher) {
    this->matcher = matcher;
}

MatchInfo Server::match(const path& imagePath) {
    pendingMatches++;
    auto info = matcher(imagePath);
    pendingMatches--;
    return info;
}

bool Server::isHealthy() {
    return pendingMatches < healthyThreshold;
}

std::string Server::createResponse(int code, const std::string &codeMsg,
        const std::string &contentType,
        const std::string &msg) const {
    std::stringstream buf;

    buf << "HTTP/1.1 " << code << " " << codeMsg << "\r\n";
    buf << "Content-Type: " << contentType << "\r\n";
    buf << "Content-Length: " << msg.size() << "\r\n";
    buf << "\r\n";
    buf << msg;

    return buf.str();
}

void Server::OKJSON(mg_connection *conn, const std::string &json) const {
    OK(conn, json, "application/json");
}

void Server::OK(mg_connection *conn, const std::string &msg, const std::string &contentType) const {
    auto response = createResponse(200, "OK", contentType, msg);
    mg_write(conn, response.c_str(), response.size());
}

void Server::errorNotAllowed(mg_connection *conn) const {
    auto response = createResponse(405, "Method Not Allowed", "text/plain", "");
    mg_write(conn, response.c_str(), response.size());
}

void Server::errorNotFound(mg_connection *conn) const {
    auto response = createResponse(404, "Not Found", "text/plain", "");
    mg_write(conn, response.c_str(), response.size());
}



void handleUpload(mg_connection *conn, const char *path) {
    const mg_request_info *request = mg_get_request_info(conn);
    auto server = reinterpret_cast<Server *>(request->user_data);

    MatchInfo match = server->match(path);
    server->OKJSON(conn, match.json());
}

int handleRequest(mg_connection *conn) {
    const mg_request_info *request = mg_get_request_info(conn);
    auto server = reinterpret_cast<Server *>(request->user_data);

    std::string response;
    std::string path(request->uri);
    std::string method(request->request_method);

    dlog("got request to " << path, logging::HIGH);


    /*
     * more elaborate method handling will require a better dispatcher, but
     * for the few methods we're handling now, this is fine
     */

    /*
     * our main image matching endpoint
     */
    if (path.compare("/match") == 0) {
        if (method.compare("POST") == 0) {
            mg_upload(conn, TMP_DIR);
        }
        else {
            server->errorNotAllowed(conn);
        }
    }
    /*
     * for AWS ELB health checks
     */
    else if (path.compare("/health") == 0) {
        if (method.compare("GET") == 0) {
            if (server->isHealthy()) {
                server->OK(conn);
            }
            else {
                server->errorNotFound(conn);
            }
        }
        else {
            server->errorNotAllowed(conn);
        }
    }
    else {
        server->errorNotFound(conn);
    }

    /*
     * non-zero tells mongoose that our function has replied to the client
     */
    return 1;
}
