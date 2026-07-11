#include <iostream>
#include "../3rd-party/httplib.h"

int main() {
    httplib::Server Svr;

    Svr.Get("/foo", [](const httplib::Request&, httplib::Response Res) {
        Res.set_content("Hellou", "text/plain");
    });

    Svr.listen("0.0.0.0", 6969);
}
