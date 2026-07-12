#include <iostream>
#include <string>
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include "../3rd-party/httplib.h"

std::mutex StdoutMtx;

int main() {
    httplib::Server Svr;

    Svr.Post("/log", [](const httplib::Request& Req, httplib::Response& Res) {
        std::string Message, User;
        if (!Req.has_param("user") || !Req.has_param("msg")) {
            Res.status = 400; // Bad request
            return;
        }
        User = Req.get_param_value("user");
        Message = httplib::decode_uri_component(Req.get_param_value("msg"));
        {
            std::lock_guard<std::mutex> Lock(StdoutMtx);
            std::cout << "[LOG: " << User << "] " << Message << '\n';
        }
    });

    std::cout << "[i] Started logserver\n";
    Svr.listen("0.0.0.0", 4444);
}