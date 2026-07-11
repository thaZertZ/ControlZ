#include <iostream>
#include <string>
#include <thread>
#include <vector>
#define _WIN32_WINNT 0x0A00 // Fix for Windows 10 and 11
#include "../3rd-party/httplib.h"

static bool Quit = false;

void Receive(httplib::ws::WebSocketClient* Cli, const std::string& User) {
    std::string Message;
    while (Cli->read(Message)) {
        if (Message == "[i] '" + User + "' was banned") {
            std::cout << "\e[1A\r[i] You have been banned\n";
            Cli->close();
            exit(0);
        }
        std::cout << '\r' << Message << "\n>> " << std::flush;
        if (Message == "[i] The server was stopped") {
            std::cout << "\r  ";
            exit(0);
        }
    }

    if (!Quit) {
        std::cout << "\n[!] Lost server connection\n";
        exit(0);
    }
}

std::vector<std::string> ParseCSV(std::string_view Str) {
    std::vector<std::string> Out;
    std::string Temp;
    for (const char& C : Str) {
        if (C == ',') {
            if (!Temp.empty()) Out.push_back(Temp);
            Temp.clear();
        } else Temp += C;
    }
    if (!Temp.empty()) Out.push_back(Temp); // Insert the last user
    return Out;
}

int main() {

    httplib::Client PreJoin("127.0.0.1:6969");

    Ask:
    std::string User;
    std::cout << "Input username: ";
    std::getline(std::cin, User);
    if (User == "SERVER" || User == "i" || User == "!" || User.contains(',')) {
        std::cout << "[!] Invalid username" << (User.contains(',') ? ": can't contain ','\n" : ": it's a reserved name\n");
        goto Ask;
    }
    httplib::Result Res = PreJoin.Get("/exist-user?name=" + httplib::encode_uri_component(User));

    if (!Res) {
        std::cout << "[!] Can't connect to server to validate username\n";
        return 1;
    }
    if (Res->body == "true") {
        std::cout << "[!] Username already taken\n";
        goto Ask;
    }
    else if (Res->body == "error") {
        std::cout << "[!] An error occurred, try again\n";
        goto Ask;
    }

    httplib::ws::WebSocketClient Cli("ws://127.0.0.1:6969/chat?name=" + httplib::encode_uri(User));

    if (Cli.connect()) {
        std::cout << "[i] Joined chat\n";

        std::thread(Receive, &Cli, User).detach();

        while (true) {
            std::string Message;
            std::cout << ">> " << std::flush;
            std::getline(std::cin, Message);
            std::cout << "\e[1A" << std::flush;
            if (!Message.empty()) {
                if (Message == "/list") {
                    httplib::Client ListUsers("127.0.0.1:6969");
                    auto Res = ListUsers.Get("/list");
                    auto List = ParseCSV(Res->body);
                    std::cout << "\e[1A\nList of currently online users:\n";
                    for (const auto& User : List) {
                        std::cout << " - " << User << '\n';
                    }
                    std::cout << "\n>> " << std::flush;
                    continue;
                }
                Cli.send(Message);
                if (Message == "/quit") {
                    Quit = true;
                    Cli.close();
                    exit(0);
                }
            }
        }
    } else {
        std::cout << "[!] Can't connect to server\n";
    }
}
