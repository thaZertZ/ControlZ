#ifndef CONTROLZ_WSENVELOPE_HPP
#define CONTROLZ_WSENVELOPE_HPP

#include <string>
#include <cstring>
#include <cstdint>
#include <concepts>
#include "Util.hpp"

// TODO: make std::string casting operators for all packet classes that serialize them, redirecting to a SerializeWsPacket() function
// TODO: for easier in-place creation of packet objects (especially for DM), try to overload the function using move semantics

namespace ControlZ {

#pragma pack(push, 1)

#define CONTROLZ_WSENVELOPE_MINPACKETSIZE 4

/// @brief The type of WebSocket packet
enum class WsPacketType : std::uint8_t {
    Invalid /* purely internal */,
    Ack,
    Auth,
    Deauth,
    Download /*Dload*/, DownloadResponse, DownloadChunk,
    DM,
    InfoUser, InfoUserResponse,
    InfoChat, InfoChatResponse,
    Nack,
    Send,
    Update /*Upd*/, UpdateResponse,
    Upload /*Upl*/, UploadResponse, UploadChunk
};

class AckWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::Ack;
    static constexpr const char Magic[3] = {'A', 'C', 'K'};

    AckWsPacket() = default;
    ~AckWsPacket() = default;

    std::string Serialize() const {
        std::string Out(sizeof(AckWsPacket), '\0');
        std::memcpy(Out.data(), this, sizeof(AckWsPacket));\
        return Out;
    }
};

enum class CreateAuthWsPacketStatus : std::uint8_t {
    OK, UsernameHashNot32, PswdHashNot32, Both
};

CreateAuthWsPacketStatus operator|=(const CreateAuthWsPacketStatus& Lhs, const CreateAuthWsPacketStatus& Rhs) {
    return static_cast<CreateAuthWsPacketStatus>(
        static_cast<std::uint8_t>(Lhs) | static_cast<std::uint8_t>(Rhs)
    );
}

class AuthWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::Auth;
    std::string UsernameHash;
    std::string PswdHash;

    AuthWsPacket() = default;
    ~AuthWsPacket() = default;

    CreateAuthWsPacketStatus Create(const std::string& UH, const std::string& PH) {
        CreateAuthWsPacketStatus Status = CreateAuthWsPacketStatus::OK;
        if (UH.size() != 32) Status |= CreateAuthWsPacketStatus::UsernameHashNot32;
        if (PH.size() != 32) Status |= CreateAuthWsPacketStatus::PswdHashNot32;
        if (Status != CreateAuthWsPacketStatus::OK) return Status;

        UsernameHash = UH;
        PswdHash = PH;
        return Status;
    }

    std::string Serialize() const {
        std::string Out(sizeof(AuthWsPacket), '\0');
        throw std::runtime_error("AuthWsPacket.Serialize(): serialization not yet implemented due to unkown hash sizes");
    }
};

enum class CreateDeauthWsPacketStatus : std::uint8_t {
    OK, UsernameHashNot32, UserTokenNot8, Both
};

CreateDeauthWsPacketStatus operator|=(const CreateDeauthWsPacketStatus& Lhs, const CreateDeauthWsPacketStatus& Rhs) {
    return static_cast<CreateDeauthWsPacketStatus>(
        static_cast<std::uint8_t>(Lhs) | static_cast<std::uint8_t>(Rhs)
    );
}

class DeauthWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::Deauth;
    std::string UsernameHash;
    std::string UserToken;

    DeauthWsPacket() = default;
    ~DeauthWsPacket() = default;

    CreateDeauthWsPacketStatus Create(const std::string& UH, const std::string& UT) {
        CreateDeauthWsPacketStatus Status = CreateDeauthWsPacketStatus::OK;
        if (UH.size() != 32) Status |= CreateDeauthWsPacketStatus::UsernameHashNot32;
        if (UT.size() != 8) Status |= CreateDeauthWsPacketStatus::UserTokenNot8;
        if (Status != CreateDeauthWsPacketStatus::OK) return Status;

        UsernameHash = UH;
        UserToken = UT;
        return Status;
    }

    std::string Serialize() const {
        std::string Out(sizeof(DeauthWsPacket), '\0');
        throw std::runtime_error("DeauthWsPacket.Serialize(): serialization not yet implemented due to unkown hash sizes");
    }
};

// Bitmask-like
enum class CreateDownloadWsPacketStatus : std::uint8_t {
    OK = 0, // OK :)
    // Bit 0                   Bit 1                    Bits 0 and 1
    FilenameB32Not8 = 1,       ExtensionMoreThan31 = 2, BothTimestamp = 3,
    // Bit 2
    FilenameEmpty = 4,
    // Bits 0, 1, and 2
    AllErrors
};

CreateDownloadWsPacketStatus operator|=(const CreateDownloadWsPacketStatus& Lhs, const CreateDownloadWsPacketStatus& Rhs) {
    return static_cast<CreateDownloadWsPacketStatus>(
        static_cast<std::uint8_t>(Lhs) | static_cast<std::uint8_t>(Rhs)
    );
}

#define CONTROLZ_WSENVELOPE_CHUNKSIZE 4096

class DownloadWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::Download;
    bool QueryMethod = true;

    union Payload {
        struct Base32Payload {
            std::string FilenameB32;
            std::string Extension;
            Base32Payload() = default;
            ~Base32Payload() = default;
        } Base32Timestamp;
        struct RawPayload {
            std::uint64_t FilenameSize = 0;
            std::string Filename;
            RawPayload() = default;
            ~RawPayload() = default;
        } RawFilename;

        Payload() : Base32Timestamp() {}
        ~Payload() {}
    } Data;

    DownloadWsPacket() = default;
    ~DownloadWsPacket() = default;

    CreateDownloadWsPacketStatus Create(const std::string& FH, const std::string& E) {
        CreateDownloadWsPacketStatus Status = CreateDownloadWsPacketStatus::OK;
        QueryMethod = true;
        if (FH.size() != 8) Status |= CreateDownloadWsPacketStatus::FilenameB32Not8;
        if (E.size() > 31) Status |= CreateDownloadWsPacketStatus::ExtensionMoreThan31;
        if (Status != CreateDownloadWsPacketStatus::OK) return Status;

        Data.Base32Timestamp.FilenameB32 = FH;
        Data.Base32Timestamp.Extension.resize(0); // Ensure all characters are then replaced with '\0'
        Data.Base32Timestamp.Extension.resize(31, '\0');
        std::memcpy(Data.Base32Timestamp.Extension.data(), E.data(), E.size());
        //Base32Timestamp.Extension = E;
        return Status;
    }
    CreateDownloadWsPacketStatus Create(const std::string& F) {
        CreateDownloadWsPacketStatus Status = CreateDownloadWsPacketStatus::OK;
        QueryMethod = false;
        if (F.empty()) Status |= CreateDownloadWsPacketStatus::FilenameEmpty;
        // Do the check after because we can access the first character since it's not empty
        if (F[0] == '\0') Status |= CreateDownloadWsPacketStatus::FilenameEmpty;
        if (Status != CreateDownloadWsPacketStatus::OK) return Status;

        Data.RawFilename.FilenameSize = F.size();
        Data.RawFilename.Filename = F;
        return Status;
    }

    std::string Serialize() const {
        std::string Out(0, '\0'); // Make it empty because we don't know what to choose yet
        if (QueryMethod) { // Base32 timestamp
            Out.resize(2 + 8 + 31, '\0');
            std::memcpy(Out.data() + 2, Data.Base32Timestamp.FilenameB32.data(), 8);
            std::memcpy(Out.data() + 10, Data.Base32Timestamp.Extension.data(), 31);
        } else {
            Out.resize(2 + 8 + Data.RawFilename.FilenameSize, '\0');
            std::memcpy(Out.data() + 2, &Data.RawFilename.FilenameSize, 8);
            std::memcpy(Out.data() + 10, Data.RawFilename.Filename.data(), Data.RawFilename.FilenameSize);
        }
        std::memcpy(Out.data(), this, 2); // Copy the first two bytes
        return Out;
    }
};

class DownloadResponseWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::DownloadResponse;
    bool QueryMethod = true;
    std::uint64_t FileSize = 0;
    std::uint64_t ChunkCount = 0;
    std::uint64_t FileID = 0;

    union Payload {
        struct Base32Payload { // Active if `QueryMethod == true`
            std::string FilenameB32;
            std::string Extension;
            Base32Payload() = default;
            ~Base32Payload() = default;
        } Base32Timestamp;
        struct RawPayload { // Active if `QueryMethod == false`
            std::uint64_t FilenameSize = 0;
            std::string Filename;
            RawPayload() = default;
            ~RawPayload() = default;
        } RawFilename;

        Payload() : Base32Timestamp() {}
        ~Payload() {}
    } Data;

    DownloadResponseWsPacket() = default;
    ~DownloadResponseWsPacket() = default;

    CreateDownloadWsPacketStatus Create(std::uint64_t FS, std::uint64_t CC, std::uint64_t FI,
            const std::string& FH, const std::string& E) {

        CreateDownloadWsPacketStatus Status = CreateDownloadWsPacketStatus::OK;
        QueryMethod = true;
        if (FH.size() != 8) Status |= CreateDownloadWsPacketStatus::FilenameB32Not8;
        if (E.size() > 31) Status |= CreateDownloadWsPacketStatus::ExtensionMoreThan31;
        if (Status != CreateDownloadWsPacketStatus::OK) return Status;

        FileSize = FS;
        ChunkCount = CC;
        FileID = FI;
        Data.Base32Timestamp.FilenameB32 = FH;
        Data.Base32Timestamp.Extension.resize(0); // Ensure all characters are then replaced with '\0'
        Data.Base32Timestamp.Extension.resize(31, '\0');
        std::memcpy(Data.Base32Timestamp.Extension.data(), E.data(), E.size()); // Should be safe if E.size() == 0
        //Base32Timestamp.Extension = E;
        return Status;
    }
    CreateDownloadWsPacketStatus Create(std::uint64_t FS, std::uint64_t CC, std::uint64_t FI,
            const std::string& F) {

        CreateDownloadWsPacketStatus Status = CreateDownloadWsPacketStatus::OK;
        QueryMethod = false;
        if (F.empty()) Status |= CreateDownloadWsPacketStatus::FilenameEmpty;
        // Do the check after because we can access the first character since it's not empty
        if (F[0] == '\0') Status |= CreateDownloadWsPacketStatus::FilenameEmpty;
        if (Status != CreateDownloadWsPacketStatus::OK) return Status;

        FileSize = FS;
        ChunkCount = CC;
        FileID = FI;
        Data.RawFilename.FilenameSize = F.size();
        Data.RawFilename.Filename = F;
        return Status;
    }

    std::string Serialize() const {
        std::string Out(0, '\0'); // Make it empty because we don't know what to choose yet
        if (QueryMethod) { // Base32 timestamp
            Out.resize(2 + 8 + 8 + 8 + 8 + 31, '\0');
            std::memcpy(Out.data() + 2 + 8 + 8 + 8, Data.Base32Timestamp.FilenameB32.data(), 8);
            std::memcpy(Out.data() + 10 + 8 + 8 + 8, Data.Base32Timestamp.Extension.data(), 31);
        } else {
            Out.resize(2 + 8 + 8 + 8 + 8 + Data.RawFilename.FilenameSize, '\0');
            std::memcpy(Out.data() + 2 + 8 + 8 + 8, &Data.RawFilename.FilenameSize, 8);
            std::memcpy(Out.data() + 10 + 8 + 8 + 8, Data.RawFilename.Filename.data(), Data.RawFilename.FilenameSize);
        }
        std::memcpy(Out.data(), this, 2 + 8 + 8 + 8); // Copy the first bytes
        return Out;
    }

};

enum class CreateDownloadChunkWsPacketStatus : std::uint8_t {
    OK, NullptrData
};

class DownloadChunkWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::DownloadChunk;
    std::uint64_t FileID = 0;
    std::uint64_t ChunkIndex = 0;
    std::uint16_t DataSize = 0; // Maximum value of CONTROLZ_WSENVELOPE_CHUNKSIZE (4096)
    char* Data = nullptr; // Must point to CONTROLZ_WSENVELOPE_CHUNKSIZE (4096) bytes of memory

    DownloadChunkWsPacket() = default;
    ~DownloadChunkWsPacket() = default;

    CreateDownloadChunkWsPacketStatus Create(std::uint64_t FI, std::uint64_t CI, std::uint16_t DS, char* D) {
        if (D == nullptr) return CreateDownloadChunkWsPacketStatus::NullptrData;
        FileID = FI;
        ChunkIndex = CI;
        DataSize = DS;
        Data = D;
        return CreateDownloadChunkWsPacketStatus::OK;
    }

    std::string Serialize() const {
        if (Data == nullptr) throw std::runtime_error("DownloadChunkWsPacket.Serialize(): data pointer is nullptr");
        std::string Out(1 + 8 + 8 + 8 + CONTROLZ_WSENVELOPE_CHUNKSIZE, '\0');
        std::memcpy(Out.data(), this, 1 + 8 + 8 + 8);
        std::memcpy(Out.data() + 1 + 8 + 8 + 8, Data, CONTROLZ_WSENVELOPE_CHUNKSIZE);
        return Out;
    }

};

enum class CreateDMsWsPacketStatus : std::uint8_t {
    OK, DataEmpty
};

class DMsWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::DM;
    static constexpr const char Magic[3] = {'\0', 'D', 'M'};
    std::uint32_t UserID = 0;
    std::string Data;

    DMsWsPacket() = default;
    ~DMsWsPacket() = default;

    CreateDMsWsPacketStatus Create(std::uint32_t UI, const std::string& D) {
        if (D.empty()) return CreateDMsWsPacketStatus::DataEmpty;
        UserID = UI;
        Data = D;
        return CreateDMsWsPacketStatus::OK;
    }
    CreateDMsWsPacketStatus Create(std::uint32_t UI, std::string&& D) {
        if (D.empty()) return CreateDMsWsPacketStatus::DataEmpty;
        UserID = UI;
        Data = std::move(D);
        return CreateDMsWsPacketStatus::OK;
    }

    std::string Serialize() const {
        std::string Out(4 + 4 + Data.size(), '\0');
        std::memcpy(Out.data(), this, 4 + 4);
        std::memcpy(Out.data() + 4 + 4, Data.data(), Data.size());
        return Out;
    }

};

enum class CreateInfoUserWsPacketStatus : std::uint8_t {
    OK
};

class InfoUserWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::InfoUser;
    bool ResponseFormat = false;
    std::uint32_t UserID = 0;

    InfoUserWsPacket() = default;
    ~InfoUserWsPacket() = default;

    CreateInfoUserWsPacketStatus Create(bool RF, std::uint32_t UI) {
        ResponseFormat = RF;
        UserID = UI;
    }

    std::string Serialize() const {
        std::string Out(2 + 4, '\0');
        std::memcpy(Out.data(), this, sizeof(InfoUserWsPacket));
        return Out;
    }

};

enum class CreateInfoUserResponseWsPacketStatus : std::uint8_t {
    OK, ResponseSizeZero, NullptrData, Both
};

CreateInfoUserResponseWsPacketStatus operator|=(
        const CreateInfoUserResponseWsPacketStatus& Lhs,
        const CreateInfoUserResponseWsPacketStatus& Rhs) {

    return static_cast<CreateInfoUserResponseWsPacketStatus>(
        static_cast<std::uint8_t>(Lhs) | static_cast<std::uint8_t>(Rhs)
    );
}

class InfoUserResponseWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::InfoUserResponse;
    bool ResponseFormat = false;
    std::uint64_t ResponseSize = 0;
    char* Data = nullptr; // Must point to ResponseSize bytes of memory

    InfoUserResponseWsPacket() = default;
    ~InfoUserResponseWsPacket() = default;

    CreateInfoUserResponseWsPacketStatus Create(bool RF, std::uint64_t RS, char* D) {
        CreateInfoUserResponseWsPacketStatus Status = CreateInfoUserResponseWsPacketStatus::OK;
        if (RS == 0) Status |= CreateInfoUserResponseWsPacketStatus::ResponseSizeZero;
        if (D == nullptr) Status |= CreateInfoUserResponseWsPacketStatus::NullptrData;
        if (Status != CreateInfoUserResponseWsPacketStatus::OK) return Status;

        ResponseFormat = RF;
        ResponseSize = RS;
        Data = D;
        return Status;
    }

    std::string Serialize() const {
        if (Data == nullptr) throw std::runtime_error("InfoUserResponseWsPacket.Serialize(): data pointer is nullptr");
        // Mayb also ensure ResponseSize is not 0?
        std::string Out(2 + 8 + ResponseSize, '\0');
        std::memcpy(Out.data(), this, 2 + 8);
        std::memcpy(Out.data() + 2 + 8, Data, ResponseSize);
        return Out;
    }

};

enum class CreateInfoChatWsPacketStatus : std::uint8_t {
    OK
};

class InfoChatWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::InfoChat;
    bool ResponseFormat = false;
    std::uint32_t ChatID = 0;

    InfoChatWsPacket() = default;
    ~InfoChatWsPacket() = default;

    CreateInfoChatWsPacketStatus Create(bool RF, std::uint32_t CI) {
        ResponseFormat = RF;
        ChatID = CI;
    }

    std::string Serialize() const {
        std::string Out(2 + 4, '\0');
        std::memcpy(Out.data(), this, sizeof(InfoUserWsPacket));
        return Out;
    }
};

enum class CreateInfoChatResponseWsPacketStatus : std::uint8_t {
    OK, ResponseSizeZero, NullptrData, Both
};

CreateInfoChatResponseWsPacketStatus operator|=(
        const CreateInfoChatResponseWsPacketStatus& Lhs,
        const CreateInfoChatResponseWsPacketStatus& Rhs) {

    return static_cast<CreateInfoChatResponseWsPacketStatus>(
        static_cast<std::uint8_t>(Lhs) | static_cast<std::uint8_t>(Rhs)
    );
}

class InfoChatResponseWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::InfoChatResponse;
    bool ResponseFormat = false;
    std::uint64_t ResponseSize = 0;
    char* Data = nullptr; // Must point to ResponseSize bytes of memory

    InfoChatResponseWsPacket() = default;
    ~InfoChatResponseWsPacket() = default;

    CreateInfoChatResponseWsPacketStatus Create(bool RF, std::uint64_t RS, char* D) {
        CreateInfoChatResponseWsPacketStatus Status = CreateInfoChatResponseWsPacketStatus::OK;
        if (RS == 0) Status |= CreateInfoChatResponseWsPacketStatus::ResponseSizeZero;
        if (D == nullptr) Status |= CreateInfoChatResponseWsPacketStatus::NullptrData;
        if (Status != CreateInfoChatResponseWsPacketStatus::OK) return Status;

        ResponseFormat = RF;
        ResponseSize = RS;
        Data = D;
        return Status;
    }

    std::string Serialize() const {
        if (Data == nullptr) throw std::runtime_error("InfoUserResponseWsPacket.Serialize(): data pointer is nullptr");
        // Mayb also ensure ResponseSize is not 0?
        std::string Out(2 + 8 + ResponseSize, '\0');
        std::memcpy(Out.data(), this, 2 + 8);
        std::memcpy(Out.data() + 2 + 8, Data, ResponseSize);
        return Out;
    }
};

class NackWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::Nack;
    static constexpr const char Magic[3] = {'N', 'A', 'K'};

    NackWsPacket() = default;
    ~NackWsPacket() = default;

    void Create() { return; }

    std::string Serialize() const {
        std::string Out(sizeof(NackWsPacket), '\0');
        std::memcpy(Out.data(), this, sizeof(NackWsPacket));
        return Out;
    }
};

enum class CreateSendWsPacketStatus : std::uint8_t {
    OK, DataEmpty
};

class SendWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::Send;
    static constexpr const char Magic[3] = {'S', 'N', 'D'};
    std::uint32_t ChatID = 0;
    std::string Data;

    SendWsPacket() = default;
    ~SendWsPacket() = default;

    CreateSendWsPacketStatus Create(std::uint32_t CI, const std::string& D) {
        if (D.empty()) return CreateSendWsPacketStatus::DataEmpty;
        ChatID = CI;
        Data = D;
        return CreateSendWsPacketStatus::OK;
    }
    CreateSendWsPacketStatus Create(std::uint32_t CI, std::string&& D) {
        if (D.empty()) return CreateSendWsPacketStatus::DataEmpty;
        ChatID = CI;
        Data = std::move(D);
        return CreateSendWsPacketStatus::OK;
    }

    std::string Serialize() {
        std::string Out(4 + 4 + Data.size(), '\0');
        std::memcpy(Out.data(), this, 4 + 4);
        std::memcpy(Out.data() + 4 + 4, Data.data(), Data.size());
        return Out;
    }
};

// !! MISSING Update PACKET !!

// Bitmask-like
enum class CreateUploadWsPacketStatus : std::uint8_t {
    OK = 0, // OK :)
    // Bit 0                   Bit 1                    Bits 0 and 1
    FilenameB32Not8 = 1,       ExtensionMoreThan31 = 2, BothTimestamp = 3,
    // Bit 2
    FilenameEmpty = 4,
    // Bits 0, 1, and 2
    AllErrors
};

CreateUploadWsPacketStatus operator|=(const CreateUploadWsPacketStatus& Lhs, const CreateUploadWsPacketStatus& Rhs) {
    return static_cast<CreateUploadWsPacketStatus>(
        static_cast<std::uint8_t>(Lhs) | static_cast<std::uint8_t>(Rhs)
    );
}

class UploadWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::Upload;
    std::uint64_t FileSize = 0;
    std::uint64_t ChunkCount = 0;
    std::uint64_t FilenameSize = 0;
    std::string Filename;
    std::string FilenameB32;
    std::string Extension;

    UploadWsPacket() = default;
    ~UploadWsPacket() = default;

    CreateUploadWsPacketStatus Create(std::uint64_t FS, std::uint64_t CC, const std::string& F,
            const std::string& FB32, const std::string& E) {

        CreateUploadWsPacketStatus Status = CreateUploadWsPacketStatus::OK;
        if (FB32.size() != 8) Status |= CreateUploadWsPacketStatus::FilenameB32Not8;
        if (E.size() > 31) Status |= CreateUploadWsPacketStatus::ExtensionMoreThan31;
        if (F.empty()) Status |= CreateUploadWsPacketStatus::FilenameEmpty;
        if (Status != CreateUploadWsPacketStatus::OK) return Status;

        FileSize = FS;
        ChunkCount = CC;
        FilenameSize = F.size();
        Filename = F;
        Extension.resize(0);
        Extension.resize(31, '\0');
        std::memcpy(Extension.data(), E.data(), E.size());
        return Status;
    }

    std::string Serialize() const {
        std::string Out(1 + 8 + 8 + 8 + FilenameSize + 8 + 31, '\0');
        std::memcpy(Out.data(), this, 1 + 8 + 8 + 8);
        std::memcpy(Out.data() + 1 + 8 + 8 + 8, Filename.data(), FilenameSize);
        std::memcpy(Out.data() + 1 + 8 + 8 + 8 + FilenameSize, FilenameB32.data(), 8);
        std::memcpy(Out.data() + 1 + 8 + 8 + 8 + FilenameSize + 8, Extension.data(), 31);
        return Out;
    }
};

class UploadResponseWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::UploadResponse;
    std::uint32_t TimeDelta = 0;
    std::uint64_t FileID = 0;

    UploadResponseWsPacket() = default;
    ~UploadResponseWsPacket() = default;

    CreateUploadWsPacketStatus Create(std::uint32_t TD, std::uint64_t FI) {
        TimeDelta = TD;
        FileID = FI;
        return CreateUploadWsPacketStatus::OK;
    }

    std::string Serialize() const {
        std::string Out(sizeof(UploadResponseWsPacket), '\0');
        std::memcpy(Out.data(), this, sizeof(UploadResponseWsPacket));
        return Out;
    }
};

enum class CreateUploadChunkWsPacketStatus : std::uint8_t {
    OK, NullptrData
};

class UploadChunkWsPacket {
public:
    static constexpr const WsPacketType Type = WsPacketType::UploadChunk;
    std::uint64_t FileID = 0;
    std::uint64_t ChunkIndex = 0;
    std::uint16_t DataSize = 0; // Maximum value of CONTROLZ_WSENVELOPE_CHUNKSIZE (4096)
    char* Data = nullptr; // Must point to CONTROLZ_WSENVELOPE_CHUNKSIZE (4096) bytes of memory

    UploadChunkWsPacket() = default;
    ~UploadChunkWsPacket() = default;

    CreateUploadChunkWsPacketStatus Create(std::uint64_t FI, std::uint64_t CI, std::uint16_t DS, char* D) {
        if (D == nullptr) return CreateUploadChunkWsPacketStatus::NullptrData;
        FileID = FI;
        ChunkIndex = CI;
        DataSize = DS;
        Data = D;
        return CreateUploadChunkWsPacketStatus::OK;
    }

    std::string Serialize() const {
        if (Data == nullptr) throw std::runtime_error("UploadChunkWsPacket.Serialize(): data pointer is nullptr");
        std::string Out(1 + 8 + 8 + 2 + DataSize, '\0');
        std::memcpy(Out.data(), this, 1 + 8 + 8 + 2);
        std::memcpy(Out.data() + 1 + 8 + 8 + 2, Data, DataSize);
        return Out;
    }
};

#pragma pack(pop)

template <typename T>
concept WsPacket = requires (T Obj) {
    { T::Type } -> std::convertible_to<WsPacketType>;
    { Obj.Create() };
    { Obj.Serialize() } -> std::same_as<std::string>;
};

// Lazy ahh helper function to serialize a packet
template <WsPacket T>
std::string SerializeWsPacket(const T& Obj) {
    return Obj.Serialize(); // So funny lol
}

} // namespace ControlZ

#endif // CONTROLZ_WSENVELOPE_HPP
