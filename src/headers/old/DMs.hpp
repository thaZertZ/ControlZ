#ifndef CONTROLZ_DMS_HPP
#define CONTROLZ_DMS_HPP

#include <cstdint>
#include <string>
#include <fstream>
#include <random>
#include <chrono>
#include <vector>
#include <algorithm> // std::reverse()

namespace ControlZ {

// Maybe make the header sizes either compile-time constants or even macros
#pragma pack(push, 1)

/// @brief Error status (`std::uint8_t`) returned when creating a DMs file
/// @note `FileFatalError` is `std::ios::badbit`, `FileOtherFail` is `std::ios::failbit` and
///       `Exception` is a non-rethrown exception
enum class CreateDMsFileStatus : std::uint8_t {
    OK, InvalidVersion, FileFatalError, FileOtherFail, Exception
};

/// @brief Error status (`std::uint8_t`) returned when decrypting a DMs file
/// @note `FileFatalError` is `std::ios::badbit`, `FileOtherFail` is `std::ios::failbit` and
///       `Exception` is a non-rethrown exception
enum class DecryptDMsFileStatus : std::uint8_t {
    OK,
    FileTooSmall, InvalidVersion, InvalidMagic, NonZeroPadding, ZeroNodeCount, InvalidLastNodeAddr,
    InvalidStartNode, InvalidNodeRange, // Used for checking StartNode and MaxNodes
    FileFatalError, FileOtherFail, Exception
};

/// @brief Error status (`std::uint8_t`) returned when appending to a DMs file
/// @note `FileFatalError` is `std::ios::badbit`, `FileOtherFail` is `std::ios::failbit` and
///       `Exception` is a non-rethrown exception
enum class AppendDMsFileStatus : std::uint8_t {
    OK,
    FileTooSmall, InvalidVersion, InvalidMagic, NonZeroPadding, ZeroNodeCount, InvalidLastNodeAddr,
    FileFatalError, FileOtherFail, Exception
};

/// @brief Error status (`std::uint8_t`) returned when converting a DMs file
/// @note `FileFatalError` is `std::ios::badbit`, `FileOtherFail` is `std::ios::failbit` and
///       `Exception` is a non-rethrown exception
/// @note This `enum class` has the same values as `AppendDMsFileStatus`, so it could be replaced
///            by it in future changes
enum class ConvertDMsFileStatus : std::uint8_t {
    OK,
    FileTooSmall, InvalidVersion, InvalidMagic, NonZeroPadding, ZeroNodeCount, InvalidLastNodeAddr,
    FileFatalError, FileOtherFail, Exception
};

/// @brief Error status returned by the `verify()` method of `DMsHeader`
enum class DMsHeaderVerifyStatus : std::uint8_t {
    OK, InvalidMagic, NonZeroPadding, ZeroNodeCount, InvalidLastNodeAddr
};

/// @brief `struct` returned by `DecryptDMsFile()` containing the decrypted file's format version,
///        the `ErrorStatus` (`DecryptDMsFileStatus`) of the decryption process, and a vector of
///        un-deserialized message data as `std::string`s
struct DecryptDMsFileResponse {
    std::uint8_t Version = 0; // If ErrorStatus != DecryptDMsFileStatus::OK then Version = 0, otherwise either 1 or 2
    DecryptDMsFileStatus ErrorStatus = DecryptDMsFileStatus::OK; // Error status set while decrypting
    std::vector<std::string> Data; // Heap allocated data buffer, order determined by the protocol version
};

/// @brief `struct` that gets serialized in a DMs file containing the necessary data for decrypting the
///        message associated with a `DMsNode`
struct DMsNode {
public:

    std::uint32_t NextNodeOffset = 0; // The byte offset to the next node
    std::uint64_t Seed = 0; // The seed used as part of the encryption process for this node's data

    /// @brief Default constructor, used when `read()`ing from a DMs file
    DMsNode() = default;
    /// @brief Construct a node with a seed
    /// @param Sd The seed used as part of the encryption process of the message associated with this node
    DMsNode(const std::uint64_t& Sd) : Seed(Sd) {}

};

/// @brief More complex `class` used once per DMs file, containing metadata and format data
class DMsHeader {
public:

    char Magic[2]; // Magic bytes `"DM"` (should we have a zero-initialization with `{}`?)
    std::uint8_t Version = 0; // Format version: `1` = backward-linked-list, `2` = forward-linked-list
    std::uint8_t Padding = 0; // Padding null byte, must be 0
    std::uint16_t NodeCount = 0; // The number of nodes contained in a file, must be non-zero
    std::uint32_t LastNodeAddr = 0; // The absolute address of the last node
    // 0 is an allowed value since it means that there are no more nodes, V1 only
    DMsNode Node; // The first node in the file

    /// @brief Default constructor, used when `read()`ing from a DMs file
    DMsHeader() = default;
    /// @brief Construct the header with the format version and the first node's seed
    /// @param Ver The format version (either `1` or `2`)
    /// @param Sd The first node's seed
    DMsHeader(const std::uint8_t Ver, const std::uint64_t& Sd) : Version(Ver), Padding(0), LastNodeAddr(0), Node(Sd) {
        Magic[0] = 'D';
        Magic[1] = 'M';
    }

    DMsHeader(DMsHeader&) = delete;
    DMsHeader(const DMsHeader&) = delete;
    // Maybe un-delete the two move constructors
    DMsHeader(DMsHeader&&) = delete;
    DMsHeader(const DMsHeader&&) = delete;

    /// @brief Verify a header's contents
    /// @return `DMsHeaderVerifyStatus ErrorStatus`
    [[nodiscard]] inline DMsHeaderVerifyStatus verify() const {
        if (Magic[0] != 'D' || Magic[1] != 'M') return DMsHeaderVerifyStatus::InvalidMagic;
        if (Padding != 0) return DMsHeaderVerifyStatus::NonZeroPadding;
        if (NodeCount == 0) return DMsHeaderVerifyStatus::ZeroNodeCount;
        if (LastNodeAddr < sizeof(DMsHeader) + 1 && LastNodeAddr != 0)
            return DMsHeaderVerifyStatus::InvalidLastNodeAddr;
        // This is because the two node-associated value can't mismatch:
        // if NodeCount is 1, LastNodeAddr must be 0 since there is only the first node
        // This situation is also safe in V2 since there is a special `NodeCount == 1` case
        if (LastNodeAddr == 0 && NodeCount == 0) return DMsHeaderVerifyStatus::InvalidLastNodeAddr;
        return DMsHeaderVerifyStatus::OK;
    }

};

#pragma pack(pop)

/// @brief Generate a random seed used as part of a DMs file message's encryption process
/// @return `std::uint64_t Seed`
/// @note When mapped and available, `std::random_device` is used, else the memory address of a variable
///       and the current nanoseconds time are `XOR`ed
std::uint64_t RandomSeed() {

    try {
        std::random_device Rd;
        return (static_cast<std::uint64_t>(Rd()) << 32) | Rd();
    } catch (...) {
        auto Now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::uint64_t MemAddr = reinterpret_cast<std::uint64_t>(&Now);
        return static_cast<std::uint64_t>(Now) ^ (MemAddr << 16);
    }

    return 0;
}

/// @brief "Scramble" a `std::uint64_t`, used in dynamic encryption processes
/// @param X The number to "scramble"
inline void AvalancheScramble(std::uint64_t& X) {
    X = (X ^ (X >> 30)) * 0xbf58476d1ce4e5b9ULL;
    X = (X ^ (X >> 27)) * 0x94d049bb133111ebULL;
    X = X ^ (X >> 31);
}

// The two input strings MUST respectively be SenderUserID and RecipientUserID

/// @brief Generate a context string used as part of a DMs file encryption process
/// @param StrA The sender's `UserID`
/// @param StrB The recipient's `UserID`
/// @return `std::string ContextStr`
/// @note The context string is used as a constant in the encryption process, while the seed
///       is node-independent
inline std::string DeriveContextStr(const std::string& StrA, const std::string& StrB) {
    return '%' + StrA + '%' + StrB + '%';
}

/// @brief Derive an encryption key to use in a DMs file message's encryption process
/// @param Seed The dynamic seed for the node
/// @param ContextStr The constant context string
/// @return `std::uint64_t Key`
std::uint64_t DeriveKey(const std::uint64_t& Seed, const std::string& ContextStr) {
    // NOTE: if not taking params by reference (not recommended) avoid copying so use Seed as hash
    //       and use `for (char& c : ContextStr)` instead

    std::uint64_t Hash = Seed;

    for (char c : ContextStr) {
        Hash ^= static_cast<std::uint64_t>(c);
        AvalancheScramble(Hash);
    }

    AvalancheScramble(Hash);
    return Hash;
}

/// @brief Encrypt or decrypt with a dynamic `XOR` loop to prevent frequency-based analysis on encrypted data
/// @param Data The serialization-independent data to cypher
/// @param Key The key derived from a `DeriveKey()` call
inline void EncryptDecrypt(std::string& Data, std::uint64_t Key) {
    for (char& c : Data) {
        AvalancheScramble(Key);
        c ^= static_cast<char>(Key >> 56);
    }
}

/// @brief Create a new DMs file
/// @param Path The path for the new file
/// @param Data The message data for the first message (serialized)
/// @param Seed The seed used as part of the encryption process for the first message
/// @param ContextStr The context string used as part of the encryption process for the first message
/// @param Version The DMs format version to create the file with
/// @return `CreateDMsFileStatus ErrorStatus`
/// @note The version with which the file is created doesn't impact the behaviour of this function, but rather
///       the behaviour of the `AppendDMsFile()` and `DecryptDMsFile()` functions if ever used on the new file
[[nodiscard]] CreateDMsFileStatus CreateDMsFile(
        const std::string& Path, std::string& Data, const std::uint64_t& Seed, const std::string& ContextStr,
        const std::uint8_t Version) {

    // NOTE: maybe avoid taking Data by reference even though it's efficient, just so that the caller
    //       doesn't get encrypted data after returning

    std::uint64_t Key = DeriveKey(Seed, ContextStr);
    if (Version - 1 > 1) return CreateDMsFileStatus::InvalidVersion; // Can only be 1 or 2

    // IMPORTANT: there is no need for a V2 version of this function since it only outputs one node
    DMsHeader Header(Version, Seed);
    Header.NodeCount = 1;

    std::ofstream File(Path, std::ios::binary);
    if (!File) return CreateDMsFileStatus::FileFatalError;
    File.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        File.seekp(std::ios::beg);
        File.write(reinterpret_cast<char*>(&Header), sizeof(Header));
        EncryptDecrypt(Data, Key);
        File.write(Data.data(), Data.size());
        File.close();
        return CreateDMsFileStatus::OK;

    } catch (const std::ios_base::failure& E) {
        if (File.bad()) {
            File.close();
            return CreateDMsFileStatus::FileFatalError;
        } else {
            File.close();
            return CreateDMsFileStatus::FileOtherFail;
        }
    } catch (...) {
        File.close();
        return CreateDMsFileStatus::Exception;
    }
}

// REMOVED DecryptDMsFileEz

/// @brief Append a message to an existing DMs file
/// @param Path The path to the file
/// @param Data The message data to append (after serialization)
/// @param Seed The seed used as part of the encryption process for the message
/// @param ContextStr The context string used as part og the encryption process for the message
/// @return `AppendDMsFileStatus ErrorStatus`
/// @note The returned `AppendDMsFileStatus` has the same values as `DecryptDMsFileStatus`
[[nodiscard]] AppendDMsFileStatus AppendDMsFile(
    const std::string& Path, std::string& Data, const std::uint64_t& Seed, const std::string& ContextStr) {

    std::fstream File(Path, std::ios::out | std::ios::in | std::ios::binary | std::ios::ate);
    if (!File) return AppendDMsFileStatus::FileFatalError;
    File.exceptions(std::ios::badbit | std::ios::failbit);
    DMsHeader Header;
        
    try {
        // Calculate filesize in bytes with error checking
        std::streampos TempSize = File.tellg();

        if (TempSize == std::streampos(-1)) return AppendDMsFileStatus::FileOtherFail;
        std::uint64_t ByteSize = static_cast<std::uint64_t>(TempSize);
        // The `+ 1` is added because there can be a header but there must also be data

        if (ByteSize < sizeof(Header) + 1) return AppendDMsFileStatus::FileTooSmall;

        File.seekg(0, std::ios::beg); // Reset cursor position

        File.read(reinterpret_cast<char*>(&Header), sizeof(Header));
        switch (Header.verify()) { // Finally verify the header integrity
            case DMsHeaderVerifyStatus::InvalidMagic:
                return AppendDMsFileStatus::InvalidMagic;
            case DMsHeaderVerifyStatus::NonZeroPadding:
                return AppendDMsFileStatus::NonZeroPadding;
            case DMsHeaderVerifyStatus::ZeroNodeCount:
                return AppendDMsFileStatus::ZeroNodeCount;
            case DMsHeaderVerifyStatus::InvalidLastNodeAddr:
                return AppendDMsFileStatus::InvalidLastNodeAddr;
        }
        // The version check is done externally because technically the version could even reach 255
        // Check for versions 1 or 2 in one operation
        if (Header.Version - 1 > 1) return AppendDMsFileStatus::InvalidVersion;

        if (Header.Version == 1) {
            if (Header.NodeCount == 1) { // Done only if there is the first header
                Header.Node.NextNodeOffset = ByteSize - sizeof(Header) + sizeof(DMsNode);
                File.seekp(0, std::ios::beg);
            } else {
                File.seekg(Header.LastNodeAddr, std::ios::beg); // Only seekg() is needed
                DMsNode LastNode;
                File.read(reinterpret_cast<char*>(&LastNode), sizeof(LastNode)); // Read the last node
                //LastNode.NextNodeOffset = ByteSize; // The next node will be written at ByteSize
                LastNode.NextNodeOffset = ByteSize - sizeof(LastNode) - File.tellg();

                File.seekp(Header.LastNodeAddr, std::ios::beg);
                File.write(reinterpret_cast<char*>(&LastNode), sizeof(LastNode)); // Write it back
                File.seekp(0, std::ios::beg);
            }
            Header.NodeCount++;
            Header.LastNodeAddr = ByteSize; // Update the last node address, it is this case every time
            File.write(reinterpret_cast<char*>(&Header), sizeof(Header));
            File.seekp(0, std::ios::end);
            DMsNode Node(Seed);
            File.write(reinterpret_cast<char*>(&Node), sizeof(Node)); // Write the new node to the end
            std::uint64_t Key = DeriveKey(Seed, ContextStr);
            EncryptDecrypt(Data, Key);
            File.write(reinterpret_cast<char*>(Data.data()), Data.size()); // Write the new data at the very end

            File.close();
            return AppendDMsFileStatus::OK;
        } else { // Version 2
            if (Header.NodeCount == 1) { // Handle very special case
                File.seekp(0, std::ios::beg);

                Header.NodeCount++;
                // No need to save an ExLastNodeAddr right?
                Header.LastNodeAddr = ByteSize;
                File.write(reinterpret_cast<char*>(&Header), sizeof(Header));
                File.seekp(0, std::ios::end);
                DMsNode Node(Seed);
                // Don't be fooled it's a PrevNodeOffset in V2, if this doesn't work it's probably just pluses
                Node.NextNodeOffset = ByteSize - sizeof(Header) + sizeof(Node);
                // Offset from Node2 back to Node1 position: ByteSize - sizeof(Header)
                //Node.NextNodeOffset = ByteSize - sizeof(Header);
                File.write(reinterpret_cast<char*>(&Node), sizeof(Node));
                //Header.LastNodeAddr = ByteSize; // Not Node.NextNodeOffset due to it being backwards
                //File.seekp(0, std::ios::beg);
                // Should be already at the end from writing the Node
            } else {
                /*
                File.seekg(Header.LastNodeAddr, std::ios::beg);
                DMsNode LastNode;
                File.read(reinterpret_cast<char*>(&LastNode), sizeof(LastNode)); // Read last node
                // LastNode.NextNodeOffset, aka PrevNodeOffset in V2, shouldn't change?
                File.seekp(Header.LastNodeAddr, std::ios::beg);
                File.write(reinterpret_cast<char*>(&LastNode), sizeof(LastNode)); // Write it back
                */
                File.seekp(0, std::ios::beg);

                // Probably everything from here is 1-node-or-not-specific
                Header.NodeCount++;
                std::uint32_t ExLastNodeAddr = Header.LastNodeAddr; // Save it for linking the node later
                Header.LastNodeAddr = ByteSize; // Still valid right?
                File.write(reinterpret_cast<char*>(&Header), sizeof(Header));
                File.seekp(0, std::ios::end);
                DMsNode Node(Seed);
                //Node.NextNodeOffset = ExLastNodeAddr; // Aka PrevNodeOffset
                Node.NextNodeOffset = ByteSize - ExLastNodeAddr; // Offset to previous node
                File.write(reinterpret_cast<char*>(&Node), sizeof(Node));
            }
            std::uint64_t Key = DeriveKey(Seed, ContextStr);
            EncryptDecrypt(Data, Key);
            File.write(Data.data(), Data.size());

            File.close();
            return AppendDMsFileStatus::OK;
        }
    } catch (const std::ios_base::failure& E) {
        if (File.bad()) {
            File.close();
            return AppendDMsFileStatus::FileFatalError;
        } else {
            File.close();
            return AppendDMsFileStatus::FileOtherFail;
        }
    } catch (...) {
        File.close();
        return AppendDMsFileStatus::Exception;
    }
}

/// @brief Decrypt a DMs file's contents
/// @param Path The path to the file
/// @param ContextStr The context string used as part of the encryption process
/// @param MaxNodes The amount of messages to decrypt. If `0` or greater than the actual
///                 amount of messages in the file, every one is decrypted
/// @return `DecryptDMsFileResponse Response`
/// @note If the decryption process is interrupted due to integrity issues, `std::fstream` exceptions
///       or other exceptions (not rethrown at the moment), every other member of the returned struct is
///       zero-initialized except `ErrorStatus`
[[nodiscard]] DecryptDMsFileResponse DecryptDMsFile(const std::string& Path, const std::string& ContextStr,
    const std::uint16_t StartNode, const std::uint16_t MaxNodes) {

    std::ifstream File(Path, std::ios::binary | std::ios::ate);

    if (!File) return { 0, DecryptDMsFileStatus::FileFatalError, {} };
    File.exceptions(std::ios::badbit | std::ios::failbit);
    DMsHeader Header;

    try {
        // Calculate filesize in bytes with error checking
        std::streampos TempSize = File.tellg();

        if (TempSize == std::streampos(-1)) return { 0, DecryptDMsFileStatus::FileOtherFail, {} };
        std::uint64_t ByteSize = static_cast<std::uint64_t>(TempSize);
        // The `+ 1` is added because there can be a header but there must also be data
        if (ByteSize < sizeof(Header) + 1) return { 0, DecryptDMsFileStatus::FileTooSmall, {} };

        File.seekg(0, std::ios::beg); // Reset cursor position

        File.read(reinterpret_cast<char*>(&Header), sizeof(Header));
        switch (Header.verify()) { // Finally verify the header integrity
            case DMsHeaderVerifyStatus::InvalidMagic:
                return { 0, DecryptDMsFileStatus::InvalidMagic, {} };
            case DMsHeaderVerifyStatus::NonZeroPadding:
                return { 0, DecryptDMsFileStatus::NonZeroPadding, {} };
            case DMsHeaderVerifyStatus::ZeroNodeCount:
                return { 0, DecryptDMsFileStatus::ZeroNodeCount, {} };
            case DMsHeaderVerifyStatus::InvalidLastNodeAddr:
                return { 0, DecryptDMsFileStatus::InvalidLastNodeAddr, {} };
        }
        // The version check is done externally because technically the version could even reach 255
        // Check for versions 1 or 2 in one operation
        if (Header.Version - 1 > 1) return { 0, DecryptDMsFileStatus::InvalidVersion, {} };

        // Check node ranges
        if (StartNode > Header.NodeCount - 1) return { 0, DecryptDMsFileStatus::InvalidStartNode, {} };
        if (StartNode + MaxNodes > Header.NodeCount) return { 0, DecryptDMsFileStatus::InvalidNodeRange, {} };
        // Start decrypting
        DecryptDMsFileResponse Response = { Header.Version, DecryptDMsFileStatus::OK, {} };
        Response.Data.resize(MaxNodes == 0 || MaxNodes > Header.NodeCount ?
            Header.NodeCount - StartNode : MaxNodes); // Should also work?

        if (Header.Version == 1) {
            File.seekg(sizeof(DMsHeader) - sizeof(DMsNode), std::ios::beg);
            std::uint32_t DataSize = 0;
            DMsNode CurrentNode;
            std::uint64_t Key = 0;
            // Placed oldest-first in the vector
            for (std::uint16_t i = 0; i < (MaxNodes == 0 || MaxNodes > Header.NodeCount
                    ? Header.NodeCount : MaxNodes + StartNode); ++i) { // Should work?
                    // Maybe no `+StartNode` here and no `-StartNode` in indexes?
                File.read(reinterpret_cast<char*>(&CurrentNode), sizeof(CurrentNode));
                std::uint64_t DataStart = File.tellg(); // Position right after reading the node
                if (i < StartNode) {
                    File.seekg(DataStart + CurrentNode.NextNodeOffset - sizeof(CurrentNode), std::ios::beg);
                    continue;
                }

                if (CurrentNode.NextNodeOffset == 0)
                    DataSize = ByteSize - DataStart; // From here to EOF
                else
                    DataSize = CurrentNode.NextNodeOffset - sizeof(CurrentNode); // From here to next node

                Response.Data[i - StartNode].resize(DataSize);
                File.read(Response.Data[i - StartNode].data(), DataSize);
                Key = DeriveKey(CurrentNode.Seed, ContextStr);
                EncryptDecrypt(Response.Data[i - StartNode], Key);

                if (CurrentNode.NextNodeOffset == 0) break;
                // Seeking isn't needed because the cursor is already at the next node after reading data
            }
        } else { // Version 2
            // Special case: if only one node, read from another offset
            if (Header.NodeCount == 1) {
                std::uint32_t DataSize = ByteSize - sizeof(Header);
                Response.Data[0].resize(DataSize);
                File.seekg(sizeof(Header), std::ios::beg);
                File.read(Response.Data[0].data(), DataSize);
                std::uint64_t Key = DeriveKey(Header.Node.Seed, ContextStr);
                EncryptDecrypt(Response.Data[0], Key);
            } else {
                File.seekg(Header.LastNodeAddr, std::ios::beg);
                std::uint32_t CurrentNodeAddr = 0;
                std::uint32_t DataSize = 0;
                std::uint32_t PrevNodeAddr = ByteSize;
                DMsNode CurrentNode;
                std::uint64_t Key = 0;
                // Placed newest-first in the vector
                for (std::uint16_t i = 0; i < (MaxNodes == 0 || MaxNodes > Header.NodeCount ?
                        Header.NodeCount : MaxNodes + StartNode); ++i) {
                        // Maybe no `+StartNode` here and no `-StartNode` in indexes?
                    CurrentNodeAddr = File.tellg();
                    File.read(reinterpret_cast<char*>(&CurrentNode), sizeof(CurrentNode));

                    if (i < StartNode) { // Skip iteration
                        PrevNodeAddr = CurrentNodeAddr;
                        File.seekg(CurrentNodeAddr - CurrentNode.NextNodeOffset, std::ios::beg);
                        continue;
                    }

                    // There should be no CurrentNode.NextNodeOffset == 0 special case
                    DataSize = PrevNodeAddr - File.tellg();

                    Response.Data[i - StartNode].resize(DataSize);
                    File.read(Response.Data[i - StartNode].data(), DataSize);
                    Key = DeriveKey(CurrentNode.Seed, ContextStr);
                    EncryptDecrypt(Response.Data[i - StartNode], Key);

                    // Still valid because the only node with a PrevNodeOffset value of 0 is the header
                    if (CurrentNode.NextNodeOffset == 0) break;
                    PrevNodeAddr = CurrentNodeAddr;
                    // Go to next node
                    File.seekg(CurrentNodeAddr - CurrentNode.NextNodeOffset, std::ios::beg);
                }
            }
        }

        File.close();
        return Response; // The caller MUST use std::move()

    } catch (const std::ios_base::failure& E) { // Every exception just returns Version = 0 instead of Response
        if (File.bad()) {
            File.close();
            return { 0, DecryptDMsFileStatus::FileFatalError, {} };
        } else {
            File.close();
            return { 0, DecryptDMsFileStatus::FileOtherFail, {} };
        }
    } catch (...) {
        File.close();
        return { 0, DecryptDMsFileStatus::Exception, {} };
    }
}

[[nodiscard]] ConvertDMsFileStatus ConvertDMsFile(std::string& Path, std::uint8_t TargetVersion) {

    std::fstream File(Path, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);

    if (!File) return ConvertDMsFileStatus::FileFatalError;
    File.exceptions(std::ios::badbit | std::ios::failbit);
    DMsHeader Header;

    try {
        // Calculate filesize in bytes with error checking
        std::streampos TempSize = File.tellg();

        if (TempSize == std::streampos(-1)) return ConvertDMsFileStatus::FileOtherFail;
        std::uint64_t ByteSize = static_cast<std::uint64_t>(TempSize);
        // The `+ 1` is added because there can be a header but there must also be data
        if (ByteSize < sizeof(Header) + 1) return ConvertDMsFileStatus::FileTooSmall;

        File.seekg(0, std::ios::beg); // Reset cursor position

        File.read(reinterpret_cast<char*>(&Header), sizeof(Header));
        // Just return if the version is correct right away
        if (Header.Version == TargetVersion) return ConvertDMsFileStatus::OK;
        switch (Header.verify()) { // Finally verify the header integrity
            case DMsHeaderVerifyStatus::InvalidMagic:
                return ConvertDMsFileStatus::InvalidMagic;
            case DMsHeaderVerifyStatus::NonZeroPadding:
                return ConvertDMsFileStatus::NonZeroPadding;
            case DMsHeaderVerifyStatus::ZeroNodeCount:
                return ConvertDMsFileStatus::ZeroNodeCount;
            case DMsHeaderVerifyStatus::InvalidLastNodeAddr:
                return ConvertDMsFileStatus::InvalidLastNodeAddr;
        }
        // The version check is done externally because technically the version could even reach 255
        // Check for versions 1 or 2 in one operation
        if (Header.Version - 1 > 1) return ConvertDMsFileStatus::InvalidVersion;

        std::vector<std::uint32_t> NodeAddrs;
        NodeAddrs.reserve(Header.NodeCount);

        DMsNode CurrentNode;
        std::uint32_t CurrentAddr = Header.Version == 1 ? sizeof(Header) - sizeof(CurrentNode) : Header.LastNodeAddr;
        //if (Header.Version == 1)
        //else
        //CurrentAddr = Header.LastNodeAddr;
        for (std::uint16_t i = 0; i < Header.NodeCount; ++i) {
            NodeAddrs.push_back(CurrentAddr);
            File.seekg(CurrentAddr, std::ios::beg);
            File.read(reinterpret_cast<char*>(&CurrentNode), sizeof(CurrentNode));
            if (CurrentNode.NextNodeOffset == 0) break;
            if (Header.Version == 1)
                CurrentAddr += CurrentNode.NextNodeOffset;
            else
                CurrentAddr -= CurrentNode.NextNodeOffset;
        }

        // When converting to V2, set LastNodeAddr to the last node (which is NodeAddrs.back() after collecting)
        // When converting to V1, keep it as-is (will be set later or is 0 for single-node)
        if (TargetVersion == 2 && Header.NodeCount > 1) {
            Header.LastNodeAddr = NodeAddrs.back();
        }

        // Reverse NodeAddrs if converting from V2 to V1 for proper oldest-first ordering
        if (Header.Version == 2) {
            std::reverse(NodeAddrs.begin(), NodeAddrs.end());
        }

        // Update version
        Header.Version = TargetVersion;
        // CurrentNode is still alive
        for (std::uint16_t i = 0; i < Header.NodeCount; ++i) {
            File.seekg(NodeAddrs[i], std::ios::beg);
            File.read(reinterpret_cast<char*>(&CurrentNode), sizeof(CurrentNode));

            if (TargetVersion == 1)
                CurrentNode.NextNodeOffset = (i == Header.NodeCount - 1) ? 0 : (NodeAddrs[i + 1] - NodeAddrs[i]);
            else
                CurrentNode.NextNodeOffset = (i == 0) ? 0 : (NodeAddrs[i] - NodeAddrs[i - 1]);

            File.seekp(NodeAddrs[i], std::ios::beg);
            File.write(reinterpret_cast<char*>(&CurrentNode), sizeof(CurrentNode));
        }
        File.seekp(0, std::ios::beg);
        // For V1 multi-node, LastNodeAddr must point to the last node; for single-node it should be 0
        //if (TargetVersion == 1) {
        //    Header.LastNodeAddr = (Header.NodeCount == 1) ? 0 : NodeAddrs.back();
        //}
        File.write(reinterpret_cast<char*>(&Header), sizeof(Header) - sizeof(CurrentNode));

        File.close();
        return ConvertDMsFileStatus::OK;

    } catch (const std::ios_base::failure& E) {
        if (File.bad()) {
            File.close();
            return ConvertDMsFileStatus::FileFatalError;
        } else {
            File.close();
            return ConvertDMsFileStatus::FileOtherFail;
        }
    } catch (...) {
        File.close();
        return ConvertDMsFileStatus::Exception;
    }

}

} // namespace ControlZ

#endif // CONTROLZ_DMS_HPP
