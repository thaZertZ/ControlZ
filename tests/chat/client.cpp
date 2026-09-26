#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00 // Fix for Windows 10 and 11
#include "../../3rd-party/httplib.h"

std::string GetColorStr(std::uint8_t Color) {
    switch (Color) {
        case 0: return "\e[31m";
        case 1: return "\e[32m";
        case 2: return "\e[33m";
        case 3: return "\e[34m";
        case 4: return "\e[35m";
        case 5: return "\e[36m";
        case 6: return "\e[37m";
        case 7: return "\e[91m";
        case 8: return "\e[92m";
        case 9: return "\e[93m";
        case 10: return "\e[94m";
        case 11: return "\e[95m";
        case 12: return "\e[96m";
        case 13: return "\e[97m";
        default: return "\e[37m";
    }
}

static bool Quit = false;
static std::mutex StdoutMtx;

void Receive(httplib::ws::WebSocketClient* Cli, const std::string& User) {
    std::string Message;
    while (Cli->read(Message)) {
        if (Message.empty()) continue;
        if (Message.substr(0, 14) == "\e[96m[i]\e[0m '" && Message.substr(19) == User + "\e[0m' was \e[91mbanned\e[0m") { // Hopefully it works, skipping the escape chars
            std::cout << "\r\e[96m[i]\e[0m You have been \e[91mbanned\e[0m\n";
            Cli->close();
            exit(0);
        }
        {
            std::lock_guard<std::mutex> StdoutLock(StdoutMtx);
            std::cout << '\r' << Message << "\n\r>> ";
        }
        if (Message == "\e[96m[i]\e[0m The server was \e[91mstopped\e[0m") {
            std::cout << "\r  ";
            exit(0);
        }
    }

    if (!Quit) {
        std::cout << "\r  \n\e[91m[!]\e[0m Lost server connection\n";
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
        std::cout << "\e[97mInput username:\e[0m ";
        std::getline(std::cin, User);
        if (User.empty()) {
            std::cout << "\e[1A";
            Loop = true; // Otherwise we try to log in with an empty username
            continue;
        }
        if (User != "guest") {
            httplib::Result Res = Register.Post("/register?user=" + httplib::encode_uri_component(User));
            if (!Res) {
                std::cout << "\e[91m[!]\e[0m Server not available for user registration\n";
                return 1;
            }
            switch (Res.value().status) {
                case 400:
                    std::cout << "\e[91m[!]\e[0m Register endpoint returned 400 bad request\n";
                    Loop = true;
                    break;
                case 403:
                    std::cout << "\e[91m[!]\e[0m Trying to register with a reserved name\n";
                    Loop = true;
                    break;
                case 409:
                    std::cout << "\e[91m[!]\e[0m User already exists\n";
                    Loop = true;
                    break;
                case 200:
                    Loop = false; // Needed when multiple attempts are done
                    break;
            }
        } // No else because if we connect with guest we are recognised automatically
    } while (Loop);

    Loop = false;
    std::string ColorStr = "White";
    std::uint8_t Color = 6; // Value of White
    do {
        std::cout << "\e[97mChoose a color (default: \e[0mwhite\e[97m):\e[0m ";
        std::getline(std::cin, ColorStr);
        if (ColorStr.empty()) break;
        Loop = false; // Smart move
        if (ColorStr == "Red") { Color = 0; break; }
        if (ColorStr == "Green") { Color = 1; break; }
        if (ColorStr == "Yellow") { Color = 2; break; }
        if (ColorStr == "Blue") { Color = 3; break; }
        if (ColorStr == "Magenta") { Color = 4; break; }
        if (ColorStr == "Cyan") { Color = 5; break; }
        if (ColorStr == "White") { /* Already 6 */ break; }
        if (ColorStr == "RedB") { Color = 7; break; }
        if (ColorStr == "GreenB") { Color = 8; break; }
        if (ColorStr == "YellowB") { Color = 9; break; }
        if (ColorStr == "BlueB") { Color = 10; break; }
        if (ColorStr == "MagentaB") { Color = 11; break; }
        if (ColorStr == "CyanB") { Color = 12; break; }
        if (ColorStr == "WhiteB") { Color = 13; break; }
        Loop = true;
        std::cout << "\e[1A\e[0K";
    } while (Loop);
    
    // We don't use the logger anywhere at the moment
    //httplib::Client Logger("127.0.0.1:4444");
    httplib::ws::WebSocketClient Cli("ws://127.0.0.1:6969" +
        (User == "guest" ? "/chat" : ("/chat?user=" + httplib::encode_uri_component(User))) +
        "&color=" + httplib::encode_uri_component(std::to_string(Color)));

    std::string Message;
    if (Cli.connect()) {
        std::cout << "\e[96m[i]\e[0m Joined chat\n";
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
                std::cout << "\r  \n\e[96m[i]\e[0m List of currently online users:\n";
                std::string ColoredUser = GetColorStr(Color) + User + "\e[0m";
                for (const auto& ListUser : List) {
                    std::cout << (ListUser != ColoredUser ? "       - " : " (you) - ") << ListUser << '\n';
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
    } else std::cout << "\e[91m[!]\e[0m Failed to connect to server\n"; // This should show up also after the !Res check in registration

}
