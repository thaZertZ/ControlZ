#include <iostream>
#include <string>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <atomic>
#include <random>
#include <bit>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include "../../3rd-party/httplib.h"
#include "../../headers/WsEnvelope.hpp"
#include "../../headers/Util.hpp"

int main() {

    httplib::Server Svr;

    Svr.WebSocket("/ws", [](const httplib::Request& Req, httplib::ws::WebSocket& Ws) {
        std::string Packet;
        while (Ws.read(Packet)) {
            if (Packet.size() < CONTROLZ_WSENVELOPE_MINPACKETSIZE) {
                // Invalid packet size
                Ws.send(ControlZ::SerializeWsPacket(ControlZ::NackWsPacket()));
                continue;
            }
            ControlZ::WsPacketType Type = ControlZ::WsPacketType::Invalid;
            std::memcpy(&Type, Packet.data(), sizeof(ControlZ::WsPacketType));

            switch (Type) {
                
            }
        }
        ControlZ::WsPacketType Type;
    });

    return 0;
}
