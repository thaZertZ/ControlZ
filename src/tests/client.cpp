#include <iostream>
#include "../3rd-party/httplib.h"

int main() {
    httplib::Client Cli("raspi5-server:6969");

    if (auto Res = Cli.Get("/foo")) {
        Res->status;
        Res->body;
    }
}