#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <cstdint>
#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00 // Fix for Windows 10 and 11
#include "../../3rd-party/httplib.h"

enum class ClientColor : std::uint8_t {
    Red, Green, Yellow, Blue, Magenta, Cyan, White, // Normal
    RedB, GreenB, YellowB, BlueB, MagentaB, CyanB, WhiteB // Strong
};

std::string ColorStr(ClientColor Color) {
    switch (Color) {
        case ClientColor::Red: return "\e[31m";
        case ClientColor::Green: return "\e[32m";
        case ClientColor::Yellow: return "\e[33m";
        case ClientColor::Blue: return "\e[34m";
        case ClientColor::Magenta: return "\e[35m";
        case ClientColor::Cyan: return "\e[36m";
        case ClientColor::White: return "\e[37m";
        case ClientColor::RedB: return "\e[91m";
        case ClientColor::GreenB: return "\e[92m";
        case ClientColor::YellowB: return "\e[93m";
        case ClientColor::BlueB: return "\e[94m";
        case ClientColor::MagentaB: return "\e[95m";
        case ClientColor::CyanB: return "\e[96m";
        case ClientColor::WhiteB: return "\e[97m";
        default: return "\e[37m";
    }
}

struct ChatClient {
    httplib::ws::WebSocket* Ws = nullptr;
    std::string Username = "";
    std::uint64_t GuestNum = 0;
    ClientColor Color = ClientColor::White;
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
    std::unique_lock<std::mutex> Lock(ClientsMtx, std::defer_lock);
    if (DoLock) Lock.lock();
    auto it = std::find_if(Clients.begin(), Clients.end(), [&](const ChatClient& C) {
        return C.Username == User;
    });
    return it;
}

ClientColor GetUserColor(std::string_view User, bool DoLock = true) { // Get a user's color assuming it exists
    ClientColor Color = ClientColor::White;
    {
        std::unique_lock<std::mutex> Lock(ClientsMtx, std::defer_lock);
        if (DoLock) Lock.lock();
        for (const auto& Cli : Clients) {
            if (Cli.Username == User) {
                Color = Cli.Color;
                break;
            }
        }
    }
    return Color;
}

void RegisterUser(const std::string& User) { // Helper function
    std::lock_guard<std::mutex> Lock(ClientsMtx);
    Clients.emplace_back(nullptr, User, 0, ClientColor::White, false);
}

void RemoveUser(httplib::ws::WebSocket* Ws, bool DoLock = true) {
    std::unique_lock<std::mutex> Lock(ClientsMtx, std::defer_lock);
    if (DoLock) Lock.lock();
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
            Broadcast("\e[96m[i]\e[0m The server was \e[91mstopped\e[0m");
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
                std::cout << "\r\e[91m[!]\e[0m Can't ban SERVER\n>> ";
                continue;
            } else if (ExistUserOnline(User)) {
                Broadcast("\e[96m[i]\e[0m '" + ColorStr(GetUserColor(User)) + User + "\e[0m' was \e[91mbanned\e[0m"); // Send it before closing the socket so the banned client can exit
                Logger->Post("/log?user=SERVER&msg=" + httplib::encode_uri_component('\'' + User + '\'') + "+was+banned");
                {
                    std::lock_guard<std::mutex> Lock(ClientsMtx);
                    RemoveUser(GetItr(User, false)->Ws, false);
                }
            } else {
                std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
                std::cout << "\r\e[91m[!]\e[0m Can't ban user: does not exist\n>> ";
            }
            continue;
        }
        if (Message == "/list") {
            std::lock_guard<std::mutex> Lock(ClientsMtx);
            std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
            std::cout << "\e[96m[i]\e[0m List of currently online users:\n";
            for (const auto& Cli : Clients) {
                std::cout << (Cli.Username == "SERVER" ? " (you) - " : "       - ") << ColorStr(GetUserColor(Cli.Username, false)) << Cli.Username << "\e[0m\n";
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
                if (Cli.Ws) List.append(';' + ColorStr(GetUserColor(Cli.Username, false)) + Cli.Username + "\e[0m");
            }
        }
        List.erase(List.begin()); // Remove the first ';'
        Res.set_content(List, "text/plain");
    });

    Svr.WebSocket("/chat", [&](const httplib::Request& Req, httplib::ws::WebSocket& Ws) {
        bool HasUser = Req.has_param("user");
        std::string User;
        ClientColor Color = ClientColor::White;
        {
            std::lock_guard<std::mutex> GuestLock(GuestMtx); // Lock it until we are done
            User = HasUser ? httplib::decode_uri_component(Req.get_param_value("user")) : ("GUEST-" + std::to_string(++GuestCount));
            // Casting eww
            Color = static_cast<ClientColor>(Req.has_param("color") ? std::stoi(httplib::decode_uri_component(Req.get_param_value("color"))) : 6);

            if (HasUser && ExistUserOffline(User)) { // Register the user as online
                std::lock_guard<std::mutex> Lock(ClientsMtx);
                if (User == "SERVER")
                Clients.emplace_back(&Ws, "SERVER", 0, Color, true);
                else {
                    auto Client = GetItr(User, false);
                    Client->Ws = &Ws;
                    Client->Online = true;
                    Client->Color = Color;
                    Logger.Post("/log?user=SERVER&msg=User+" + httplib::encode_uri_component('\'' + User + '\'') + "+logged+in+successfully");
                }
            } else { // We are a guest
                std::lock_guard<std::mutex> Lock(ClientsMtx);
                Clients.emplace_back(&Ws, User, GuestCount, Color, true);
                if (User != "SERVER") Logger.Post("/log?user=SERVER&msg=Guest+user+" + std::to_string(GuestCount) + "+logged+in+successfully");
            }
        }

        Broadcast("\e[96m[i]\e[0m '" + ColorStr(Color) + User + "\e[0m' joined");

        std::string Message;
        bool Quit = false;
        bool CanUseGuestCmd = true;
        while (Ws.read(Message)) {
            if (Message == "/quit") {
                RemoveUser(&Ws);
                Broadcast("\e[96m[i]\e[0m '" + ColorStr(Color) + User + "\e[0m' quit the chat");
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
            Broadcast(ColorStr(Color) + '[' + User + "]\e[0m " + Message);
        }

        if (!Quit) {
            // The connection dropped since we can't read anymore
            if (ExistUser(User)) {
                // Do not remove it because race conditions make the input thread crash with out of bounds exception
                Broadcast("\e[96m[i]\e[0m '" + ColorStr(Color) + User + "\e[0m' left the chat");
                Logger.Post("/log?user=SERVER&msg=User+" + httplib::encode_uri_component('\'' + User + '\'') + "+left+forcefully");
                RemoveUser(&Ws);
            }
        }
    });

    std::cout << "\e[96m[i]\e[0m \e[92mStarted\e[0m server\n";
    Logger.Post("/log?user=SERVER&msg=Started+server");

    // Create two client threads for this server after printing
    std::thread(Receiver).detach();
    std::thread(SvrInput, &Logger).detach();
    Svr.listen("0.0.0.0", 6969);
}
