#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#define _WIN32_WINNT 0x0A00 // Fix for Windows 10 and 11
#include "../3rd-party/httplib.h"

static bool Quit = false;
static std::mutex StdoutMtx;

void Receive(httplib::ws::WebSocketClient* Cli, const std::string& User) {
    std::string Message;
    while (Cli->read(Message)) {
        if (Message.empty()) continue;
        if (Message == "[i] '" + User + "' was banned") {
            std::cout << "\r[i] You have been banned\n";
            Cli->close();
            exit(0);
        }
        {
            std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
            std::cout << '\r' << Message << "\n\r>> ";
        }
        if (Message == "[i] The server was stopped") {
            std::cout << "\r  ";
            exit(0);
        }
    }

    if (!Quit) {
        std::cout << "\r  \n[!] Lost server connection\n";
        exit(0);
    }
}

std::vector<std::string> ParseList(std::string_view Str) {
    std::vector<std::string> Out;
    std::string Temp;
    for (const char& C : Str) {
        if (C == ';') {
            Out.push_back(Temp);
            Temp.clear();
        } else Temp += C;
    }
    if (!Temp.empty()) Out.push_back(Temp); // Maybe the empty check is not even needed
    return Out;
}

int main() {

    httplib::Client Register("127.0.0.1:6969");

    bool Loop = false;
    std::string User;
    do {
        std::cout << "Input username: ";
        std::getline(std::cin, User);
        if (User != "guest") {
            httplib::Result Res = Register.Post("/register?user=" + httplib::encode_uri_component(User));
            if (!Res) {
                std::cout << "[!] Server not available for user registration\n";
                break;
            }
            switch (Res.value().status) {
                case 400:
                    std::cout << "[!] Register endpoint returned 400 bad request\n";
                    Loop = true;
                    break;
                case 403:
                    std::cout << "[!] Trying to register with a reserved name\n";
                    Loop = true;
                    break;
                case 200:
                    break;
            }
        } // No else because if we connect with guest we are recognised automatically
    } while (Loop);

    // We don't use the logger anywhere at the moment
    //httplib::Client Logger("127.0.0.1:4444");
    httplib::ws::WebSocketClient Cli("ws://127.0.0.1:6969" + (User == "guest" ? "/chat" : ("/chat?user=" + httplib::encode_uri_component(User))));

    std::string Message;
    if (Cli.connect()) {
        std::cout << "[i] Joined chat\n";
        if (User == "guest") {
            Cli.read(Message); // Initial "joined" message
            Cli.send("/guest");
            std::string GuestNum;
            Cli.read(GuestNum);
            User = "GUEST-" + GuestNum;
        }
        std::cout << '\r' << Message;
        Message.clear();
        std::thread(Receive, &Cli, User).detach();

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

            if (Message == "/list") {
                httplib::Client ListCli("127.0.0.1:6969");
                httplib::Result Res = ListCli.Get("/list");
                std::vector<std::string> List = ParseList(Res->body);
                std::cout << "\r  \n[i] List of currently online users:\n";
                for (const auto& ListUser : List) {
                    std::cout << (ListUser != User ? "       - " : " (you) - ") << ListUser << '\n';
                }
                continue;
            }
            std::cout << "\e[1A\r";
            Cli.send(Message);
            if (Message == "/quit") {
                Quit = true;
                Cli.close();
                exit(0); // break; ?
            }
        }
    } else std::cout << "[!] Failed to connect to server\n"; // This should show up also after the !Res check in registration

}
