#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstdint>
#define _WIN32_WINNT 0x0A00 // Fix for Windows 10 and 11
#define WIN32_LEAN_AND_MEAN
#include "../3rd-party/httplib.h"

struct ChatClient {
    httplib::ws::WebSocket* Ws = nullptr;
    std::string Username = "";
};

static std::vector<ChatClient> Clients;
static bool Stopping = false;
static std::mutex ClientsMtx;

void Broadcast(const std::string& Message) {
    std::lock_guard<std::mutex> Lock(ClientsMtx);
    for (const ChatClient& Cli : Clients) {
        Cli.Ws->send(Message);
    }
}

void SvrInput() {
    std::string Message;
    while (true) {
        std::cout << ">> ";
        if (!std::getline(std::cin, Message)) break; // Handle stdin closing

        if (!Message.empty()) {
            if (Message == "/stop") {
                Stopping = true;
                std::cout << "[i] The server was stopped\n";
                Broadcast("[i] The server was stopped");
                {
                    std::lock_guard<std::mutex> Lock(ClientsMtx);
                    for (const auto& Cli : Clients) {
                        Cli.Ws->close();
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                exit(0);
            }
            if (Message.size() > 5 && Message.substr(0, 4) == "/ban") {
                std::string User = Message.substr(5);
                //httplib::ws::WebSocket* WsToClose = nullptr;

                auto it = Clients.end();
                {
                    std::lock_guard<std::mutex> Lock(ClientsMtx);
                    it = std::find_if(Clients.begin(), Clients.end(), [&](const ChatClient& C) {
                        return C.Username == User;
                    });

                    if (it == Clients.end()) {
                        std::cout << "[!] Cannot ban user: does not exist\n";
                        continue;
                    }
                }

                std::cout << "[i] Banned '" << User << "'\n";
                Broadcast("[i] '" + User + "' was banned");
                //if (WsToClose) WsToClose->close();
                Clients.erase(it);
                continue;
            }
            Broadcast("[SERVER] " + Message);
            std::cout << "\e[1A[SERVER] " << Message << '\n';
        }
    }
}

int main() {
    httplib::Server Svr;

    std::thread(SvrInput).detach();

    Svr.Get("/exist-user", [](const httplib::Request& Req, httplib::Response& Res) {
        std::string User;
        if (Req.has_param("name")) {
            User = Req.get_param_value("name");
            bool Taken = false;
            {
                std::lock_guard<std::mutex> Lock(ClientsMtx);
                Taken = std::any_of(Clients.begin(), Clients.end(), [&](const ChatClient& C) {
                    return C.Username == User;
                });
            }
            Res.set_content(Taken ? "true" : "false", "text/plain");
        } else {
            Res.set_content("error", "text/plain");
        }
    });

    Svr.Get("/list", [](const httplib::Request& Req, httplib::Response& Res) {
        std::string List;
        {
            std::lock_guard<std::mutex> Lock(ClientsMtx);
            for (std::uint64_t i = 0; i < Clients.size(); ++i) {
                List += Clients[i].Username;
                if (i < Clients.size() - 1) List += ',';
            }
        }
        Res.set_content(List, "text/csv");
    });

    Svr.WebSocket("/chat", [](const httplib::Request& Req, httplib::ws::WebSocket& Ws) {
        std::string User = Req.has_param("name") ? httplib::decode_uri_component(Req.get_param_value("name")) : "UNKNOWN";

        {
            std::lock_guard<std::mutex> Lock(ClientsMtx);
            Clients.push_back({&Ws, User});
        }

        bool Quit = false;
        std::cout << "\r[i] '" << User << "' joined\n" << ">> " << std::flush;
        Broadcast("[i] '" + User + "' joined");
        std::string Message;
        while (Ws.read(Message)) {
            if (Message == "/quit") {
                std::cout << "\r[i] '" << User << "' quit the chat\n" << ">> " << std::flush;
                Broadcast("[i] '" + User + "' quit the chat");
                Quit = true;
                break;
            }
            std::cout << "\r[" << User << "] " << Message << '\n' << ">> " << std::flush;
            Broadcast('[' + User + "] " + Message);
        }

        bool Banned = false;
        {
            std::lock_guard<std::mutex> Lock(ClientsMtx);
            auto it = std::find_if(Clients.begin(), Clients.end(), [&](const ChatClient& C) {
                return C.Ws == &Ws;
            });

            if (it != Clients.end())
                Clients.erase(it);
            else if (!Quit && !Stopping)
                Banned = true;
        }

        if (!Stopping && !Quit && !Banned) {
            Broadcast("[i] '" + User + "' left the chat");
            std::cout << "\r[i] '" << User << "' left the chat\n" << ">> " << std::flush;
        }
    });

    std::cout << "[i] Started server\n\n";
    Svr.listen("0.0.0.0", 6969);
}
