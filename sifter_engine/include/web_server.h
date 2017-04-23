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


#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_


#include <functional>
#include <vector>
#include <atomic>

#include "sifter.h"


extern "C" {
#include "mongoose.h"
}


#define log_server(msg, p) dlog("web server " << port << ": " << msg, p)


typedef std::function<MatchInfo(const path &)> Matcher;


int handleRequest(mg_connection *conn);
void handleUpload(mg_connection *conn, const char *path);

class Server {
public:
    Server();

    void setHealthyThreshold(int healthyThreshold);
    void setMatcher(Matcher matcher);
    void serve(int port);
    void stop();
    MatchInfo match(const path& imagePath);
    std::string createResponse(int code, const std::string &codeMsg,
        const std::string &msg,
        const std::string &contentType) const;

    bool isHealthy();
    void OKJSON(mg_connection *conn, const std::string &msg) const;
    void OK(mg_connection *conn, const std::string &msg="", const std::string &contentType="text/plain") const;
    void errorNotAllowed(mg_connection *conn) const;
    void errorNotFound(mg_connection *conn) const;

private:
    mg_context *ctx = nullptr;
    int port = 0;
    int healthyThreshold = 2;
    Matcher matcher;
    std::atomic_int pendingMatches;
};



#endif /* WEB_SERVER_H_ */
