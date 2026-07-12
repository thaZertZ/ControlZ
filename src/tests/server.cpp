#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <cstdint>
#include <algorithm>
#define _WIN32_WINNT 0x0A00 // Fix for Windows 10 and 11
#define WIN32_LEAN_AND_MEAN
#include "../3rd-party/httplib.h"

struct ChatClient {
    httplib::ws::WebSocket* Ws = nullptr;
    std::string Username = "";
    std::uint64_t GuestNum = 0;
    bool Online = false; // They have just used "/login" and not "/chat"
};

static std::vector<ChatClient> Clients;
static std::mutex ClientsMtx;
static std::uint64_t GuestCount = 0;
static std::mutex GuestMtx;
static std::mutex StdoutMtx;

bool ExistUser(std::string_view User) { // Return `true` if the user exists
    bool Exist = false;
    {
        std::lock_guard<std::mutex> Lock(ClientsMtx);
        for (const auto& Cli : Clients) {
            if (User == Cli.Username) {
                Exist = true;
                break;
            }
        }
    }
    return Exist;
}
bool ExistUserOnline(std::string_view User) { // Return `true` only if the user is online
    bool Exist = false;
    {
        std::lock_guard<std::mutex> Lock(ClientsMtx);
        for (const auto& Cli : Clients) {
            if (User == Cli.Username && Cli.Online) {
                Exist = true;
                break;
            }
        }
    }
    return Exist;
}
bool ExistUserOffline(std::string_view User) { // Return `true` only if user is offline
    bool Exist = false;
    {
        std::lock_guard<std::mutex> Lock(ClientsMtx);
        for (const auto& Cli : Clients) {
            if (User == Cli.Username && !Cli.Online) {
                Exist = true;
                break;
            }
        }
    }
    return Exist;
}

std::vector<ChatClient>::iterator GetItr(std::string_view User, bool DoLock = true) { // Get an interator assumin the user exists
    if (DoLock) std::lock_guard<std::mutex> Lock(ClientsMtx);
    auto it = std::find_if(Clients.begin(), Clients.end(), [&](const ChatClient& C) {
        return C.Username == User;
    });
    return it;
}

/*
std::uint64_t IndexOf(std::string_view User, bool DoLock = true) { // Return the index of the user in Clients, assuming the user exists
    std::uint64_t Index = 0;
    {
        if (DoLock) std::lock_guard<std::mutex> Lock(ClientsMtx);
        for (; Index < Clients.size(); ++Index) {
            if (Clients[Index].Username == User) break;
        }
    }
    return Index;
}
*/

void RegisterUser(const std::string& User) { // Helper function
    std::lock_guard<std::mutex> Lock(ClientsMtx);
    Clients.emplace_back(nullptr, User, 0, false);
}

void RemoveUser(httplib::ws::WebSocket* Ws, bool DoLock = true) {
    if (DoLock) std::lock_guard<std::mutex> Lock(ClientsMtx);
    auto it = std::find_if(Clients.begin(), Clients.end(), [&](const ChatClient& Cli) {
        return Cli.Ws == Ws;
    });
    if (it == Clients.end()) return;
    httplib::ws::WebSocket* Socket = it->Ws;
    Clients.erase(it);
    if (Socket) Socket->close();
}

void Broadcast(const std::string& Message) {
    std::lock_guard<std::mutex> Lock(ClientsMtx);
    for (const auto& Cli : Clients) {
        if (Cli.Ws) Cli.Ws->send(Message); // Prevent nullptr dereference for registered users
    }
}

void SvrInput(httplib::Client* Logger) {
    std::string Message;
    std::cout << '\n';
    while (true) {
        {
            std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
            std::cout << "\r>> ";
        }
        if (!std::getline(std::cin, Message)) break; // Handle stdin closing
        if (Message.empty()) {
            std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
            std::cout << "\e[1A";
            continue;
        }

        if (Message == "/stop") {
            Broadcast("[i] The server was stopped");
            std::lock_guard<std::mutex> Lock(ClientsMtx);
            for (auto& Cli : Clients) {
                if (Cli.Ws) Cli.Ws->close(); // Handle nullptr here as well
            }
            Logger->Post("/log?user=SERVER&msg=Stopped+server");
            exit(0);
        }
        if (Message.size() > 5 && Message.substr(0, 4) == "/ban") { // 6 characters needed because ("/ban" = 4) + (' ' = 1) + User
            std::string User = Message.substr(5);
            if (User == "SERVER") {
                std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
                std::cout << "\r[!] Can't ban SERVER\n>> ";
                continue;
            } else if (ExistUserOnline(User)) {
                Broadcast("[i] '" + User + "' was banned"); // Send it before closing the socket so the banned client can exit
                Logger->Post("/log?user=SERVER&msg=" + httplib::encode_uri_component('\'' + User + '\'') + "+was+banned");
                {
                    std::lock_guard<std::mutex> Lock(ClientsMtx);
                    RemoveUser(GetItr(User, false)->Ws, false);
                }
            } else {
                std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
                std::cout << "\r[!] Can't ban user: does not exist\n>> ";
            }
            continue;
        }

        // Regular message
        std::cout << "\e[1A\r";
        Broadcast("[SERVER] " + Message);
    }
}

void Receiver() { // May not work
    httplib::ws::WebSocketClient Ws("ws://127.0.0.1:6969/chat?user=SERVER"); // Connect to ourselves to receive broadcasts
    Ws.connect();
    std::string Message;
    while (Ws.read(Message)) {
        std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
        std::cout << '\r' << Message << "\r\n>> "; // Output like clients do
    }
    std::cout << "  ";
}

int main() {

    httplib::Server Svr;
    httplib::Client Logger("127.0.0.1:4444");

    Svr.Post("/register", [&](const httplib::Request& Req, httplib::Response& Res) {
        if (!Req.has_param("user")) {
            Res.status = 400; // Bad request
            Logger.Post("/log?user=SERVER&msg=Attempt+to+register+with+no+user+param");
            return;
        }
        std::string User = httplib::decode_uri_component(Req.get_param_value("user"));
        // Check for reserved names
        if (User == "i" || User == "!" || User == "SERVER" || User.contains(';')) {
            Res.status = 403; // Forbidden
            Logger.Post("/log?user=SERVER&msg=User+tried+to+register+with+a+reserved+name");
            return;
        }
        // Do a generic check otherwise we can end up with two users called the same
        if (ExistUser(User)) {
            Res.status = 409; // Conflict
            Logger.Post("/log?user=SERVER&msg=User+" + httplib::encode_uri_component('\'' + User + '\'') + "+already+exists");
            return;
        }
        RegisterUser(User);
        Res.status = 200; // Success
        Logger.Post("/log?user=SERVER&msg=User+" + httplib::encode_uri_component('\'' + User + '\'') + "+registered+successfully");
    });

    Svr.Get("/list", [](const httplib::Request& Req, httplib::Response& Res) {
        std::string List;
        {
            std::lock_guard<std::mutex> Lock(ClientsMtx);
            for (const auto& Cli : Clients) {
                if (Cli.Ws) List.append(';' + Cli.Username);
            }
        }
        List.erase(List.begin()); // Remove the first ';'
        Res.set_content(List, "text/plain");
    });

    Svr.WebSocket("/chat", [&](const httplib::Request& Req, httplib::ws::WebSocket& Ws) {
        bool HasUser = Req.has_param("user");
        std::string User;
        {
            std::lock_guard<std::mutex> GuestLock(GuestMtx); // Lock it until we are done
            User = HasUser ? httplib::decode_uri_component(Req.get_param_value("user")) : ("GUEST-" + std::to_string(++GuestCount));
            
            if (HasUser && ExistUserOffline(User)) { // Register the user as online
                std::lock_guard<std::mutex> Lock(ClientsMtx);
                if (User == "SERVER")
                Clients.emplace_back(&Ws, "SERVER", 0, true);
                else {
                    auto Client = GetItr(User, false);
                    Client->Ws = &Ws;
                    Client->Online = true;
                    Logger.Post("/log?user=SERVER&msg=User+" + httplib::encode_uri_component('\'' + User + '\'') + "+logged+in+successfully");
                }
            } else { // We are a guest
                std::lock_guard<std::mutex> Lock(ClientsMtx);
                Clients.emplace_back(&Ws, User, GuestCount, true);
                if (User != "SERVER") Logger.Post("/log?user=SERVER&msg=Guest+user+" + std::to_string(GuestCount) + "+logged+in+successfully");
            }
        }

        Broadcast("[i] '" + User + "' joined");

        std::string Message;
        bool Quit = false;
        bool CanUseGuestCmd = true;
        while (Ws.read(Message)) {
            if (Message == "/quit") {
                RemoveUser(&Ws);
                Broadcast("[i] '" + User + "' quit the chat");
                Logger.Post("/log?user=SERVER&msg=User+" + httplib::encode_uri_component('\'' + User + '\'') + "+quit");
                Quit = true;
                break;
            }
            if (Message == "/guest" && CanUseGuestCmd) {
                std::lock_guard<std::mutex> Lock(ClientsMtx);
                Ws.send(std::to_string(GetItr(User, false)->GuestNum));
                CanUseGuestCmd = false;
                continue;
            }
            Broadcast('[' + User + "] " + Message);
        }

        if (!Quit) {
            // The connection dropped since we can't read anymore
            if (ExistUser(User)) {
                // Do not remove it because race conditions make the input thread crash with out of bounds exception
                Broadcast("[i] '" + User + "' left the chat");
                Logger.Post("/log?user=SERVER&msg=User+" + httplib::encode_uri_component('\'' + User + '\'') + "+left+forcefully");
                RemoveUser(&Ws);
            }
        }
    });

    std::cout << "[i] Started server\n\n";
    Logger.Post("/log?user=SERVER&msg=Started+server");

    // Create two client threads for this server after printing
    std::thread(Receiver).detach();
    std::thread(SvrInput, &Logger).detach();
    Svr.listen("0.0.0.0", 6969);
}
