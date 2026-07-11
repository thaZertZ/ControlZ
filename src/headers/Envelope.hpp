#pragma once

#include <string>
#include <cstring>
#include <cstdint>
#include <vector>

// Define all magic numbers

#define ATTACHMENT_MEDIATYPE_MASK  224
#define ATTACHMENT_EXTSIZE_MASK    31
#define ATTACHMENT_MEDIATYPE_SHPOS 5
#define ATTACHMENT_FILENAME_MAX    42
#define AFNAME_MAX                 ATTACHMENT_FILENAME_MAX
#define REPLY_TIMESTAMP_BYTECOUNT  4
#define REPLY_PREVIEW_BYTECOUNT    38

#define MESSAGE_VERSION_MASK       57344
#define MESSAGE_TEXTFLAG_MASK      4096
#define MESSAGE_ATTACHFLAG_MASK    2048
#define MESSAGE_EDITEDFLAG_MASK    1024
#define MESSAGE_ATTACHCOUNT_MASK   896
#define MESSAGE_TEXTSIZE_MASK      127

#define MESSAGE_VERSION_SHPOS      13
#define MESSAGE_TEXTFLAG_SHPOS     12
#define MESSAGE_ATTACHFLAG_SHPOS   11
#define MESSAGE_EDITEDFLAG_SHPOS   10
#define MESSAGE_ATTACHCOUNT_SHPOS  7

namespace ControlZ {

#pragma pack(push, 1)

/// @brief Media type for the a `DMsAttachment`
enum class DMsAttachmentMediaType : std::uint8_t {
    Audio, Binary, Code, Docs, Image, Other, Video, Reply
};

/// @brief Error status returned when creating a `BitmaskMetadata` value for a `DMsAttachment`
enum class NewDMsAttachmentBmDataStatus : std::uint8_t {
    OK, ExtSizeTooBig
};

/// @brief Error status returned when creating a reply `DMsAttachment` object (special case)
enum class NewDMsAttachmentReplyStatus : std::uint8_t {
    OK, PreviewChopped, LookBehindToDeepSearch, Both
};

/// @brief Overload `operator|=` to allow `OR`ing two `NewDMsAttachmentReplyStatus`es
/// @param O The lvalue
/// @param V The rvalue (rvalue reference)
/// @return `NewDMsAttachmentReplyStatus& O`
inline NewDMsAttachmentReplyStatus& operator|=(
    NewDMsAttachmentReplyStatus& O, const NewDMsAttachmentReplyStatus&& V) {

    O = static_cast<NewDMsAttachmentReplyStatus> (
        static_cast<std::uint8_t>(O) | static_cast<std::uint8_t>(V)
    );
    return O;
}

/// @brief Error status returned when creating a `BitmaskMetadata` value for a `DMsMessageDisk` or `DMsMessageRAM`
enum class NewDMsMessageBmDataStatus : std::uint8_t {
    OK, TextTooLong, FlagVsDataMismatch, Both, InvalidVersion
};

/// @brief Overload of `operator|=` to allow `OR`ing two `NewDMsMessageBmDataStatus`es
/// @param O The lvalue
/// @param V The rvalue (rvalue reference)
/// @return `NewDMsMessageBmDataStatus O`
inline NewDMsMessageBmDataStatus& operator|=(
    NewDMsMessageBmDataStatus& O, const NewDMsMessageBmDataStatus&& V) {

    O = static_cast<NewDMsMessageBmDataStatus> (
        static_cast<std::uint8_t>(O) | static_cast<std::uint8_t>(V)
    );
    return O;
}

/// @brief `struct` returned when calling `NewDMsAttachmentBitmaskMetadata()`
struct NewDMsAttachmentBmDataResponse {
    // Error status set while creating the value
    NewDMsAttachmentBmDataStatus ErrorStatus = NewDMsAttachmentBmDataStatus::OK;
    std::uint8_t BitmaskMetadata = 0; // The created value
};

/// @brief `struct` returned when calling `GetDMsAttachmentBitmaskMetadata()`
struct GetDMsAttachmentBmDataResponse {
    DMsAttachmentMediaType MediaType; // The media type of the attachment
    std::uint8_t ExtSize = 0; // The extension size of the attachment's filename
};

/// @brief `struct` returned when calling `GetDMsAttachmentReply()`
struct GetDMsAttachmentReplyResponse {
    std::uint8_t LookBehind = 0; // The number of messages ago the original message was, `0` = deep search
    std::uint32_t Timestamp = 0; // The timestamp of the original message
    char MsgPreview[38]{0}; // A 38 non-null-terminated preview of the original message
};

/// @brief `struct` returned when calling `NewDMsMessageBitmaskMetadata()`
struct NewDMsMessageBmDataResponse {
    // Error status set while creating the value
    NewDMsMessageBmDataStatus ErrorStatus = NewDMsMessageBmDataStatus::OK;
    std::uint16_t BitmaskMetadata = 0; // The created value
};

/// @brief `struct` returned when calling `GetDMsMessageBitmaskMetadata()`
struct GetDMsMessageBmDataResponse {
    std::uint8_t Version = 0; // The version of the `DMsMessageDisk` or `DMsMessageRAM` format
    bool TextFlag = true; // Flag set to `true` if the message contains text
    bool AttachFlag = false; // Flag set to `true` if the message contains attachments
    bool EditedFlag = false; // Flag set to `true` if the message is an edit of an older message
    std::uint8_t AttachCount = 0; // The number of attachments in this message, can't be `0` while `Attachflag = true`
    std::uint16_t TextSize = 0; // The 8-byte-block-biased size of the message
};

/*
### `DMsAttachment.BitmaskMetadata` specification
```
7 6 5 4 3 2 1 0     [bit position]
m m m s s s s s     [data]
```
`m` = media type (`000-110` in sequence: audio, binary, code, docs, image, other, video;
`111` could be a new media type if added in the future)
`s` = extension size, goes up to 31 characters, in the case of these bits being `00000`, the `.`
character in the filename char array isn't considered as there is no extension
*/
struct DMsAttachment {
public:
    std::uint8_t BitmaskMetadata = 0; // The metadata for this `DMsAttachment`
    char Filename[AFNAME_MAX]{}; // The filename of the attachment

    /// @brief Default constructor
    DMsAttachment() = default;
    /// @brief Construct with a `BitmaskMetadata` value
    /// @param BmData The `BitmaskMetadata` for this `DMsAttachment`
    DMsAttachment(std::uint8_t BmData) : BitmaskMetadata(BmData) {
        // Now it's zero-initialized
        //for (std::uint8_t i = 0; i < AFNAME_MAX; ++i)
        //    Filename[i] = '\0';
    }
    /// @brief Construct with a `BitmaskMetadata` value and a `Filename` value
    /// @param BmData The `BitmaskMetadata` for this `DMsAttachment`
    /// @param Fname The filename to assign to this `DMsAttachment`
    /// @note If the provided `Filename` value is longer than `AFNAME_MAX` it will get truncated,
    ///       if it is shorter the rest of the `Filename` array is filled with null bytes
    DMsAttachment(const std::uint8_t BmData, const std::string& Fname) : BitmaskMetadata(BmData) {
        //std::uint8_t Len = (AFNAME_MAX > Fname.size() ? Fname.size() : AFNAME_MAX);    *
        for (std::uint8_t i = 0; i < (AFNAME_MAX > Fname.size() ? Fname.size() : AFNAME_MAX); ++i)
            Filename[i] = Fname[i];
        // If the filename becomes gibberish add this again                         *
        //for (std::uint8_t i = Len; i < AFNAME_MAX; ++i)
        //    Filename[i] = '\0';
    }
};

/// @brief `struct` returned when calling `NewDMsAttachmentReply()`
struct NewDMsAttachmentReplyResponse {
    // Error status set while creating the object
    NewDMsAttachmentReplyStatus ErrorStatus = NewDMsAttachmentReplyStatus::OK;
    DMsAttachment Attachment; // The `DMsAttachment` object
};

/*
### `DMsMessageDisk.BitmaskMetadata` specification
```
F E D C B A 9 8 7 6 5 4 3 2 1 0     [bit position]
v v v t q g z z z s s s s s s s     [data]
```
`v` = version number (max 8)
`t` = contains text (flag)
`q` = contains attachment/s (flag)
`g` = edited message (flag)
`z` = biased attachment/s count (max 8)
`s` = binary-biased text size (max `1024`):
    write formula: `((RealLength + 7) / 8) - 1`
    read formula: `(StoredSize + 1) * 8`

    `NOTE 1:` messages with length values that vary in numbers lower than the one
    in the chosen version will contain `NULL` characters from the last intended
    character to the end.
    `NOTE 2:` in the write formula, the `7` is used to nudge the result value into
    the next 8bit block, so that a `ceil()` function is not needed since integers
    always round down.
    `NOTE 3:` the `+ 1` and `- 1` are used for biasing the size value, since a `0`
    character message is impossible.
*/
struct DMsMessageDisk {
public:
    std::uint16_t BitmaskMetadata = 0; // The metadata of this message
    std::uint32_t Timestamp = 0; // The timestamp of this message
    std::uint32_t SenderID = 0; // The `UserID` of the message sender
    // Here should ALWAYS follow a DMsAttachment sequence of `z` elements on disk,
    // as well as the actual message data

    /// @brief Default constructor, used when `read()`ing from a DMs file
    DMsMessageDisk() = default;
    /// @brief Construct with a `BitmaskMetadata`, `Timestamp` and `SenderID`
    /// @param BmData The `BitmaskMetadata` value
    /// @param Tstamp The `Timestamp` value
    /// @param SdID The `SenderID` value
    DMsMessageDisk(const std::uint16_t BmData, const std::uint32_t Tstamp, const std::uint32_t SdID) :
        BitmaskMetadata(BmData), Timestamp(Tstamp), SenderID(SdID) {}
};

/*
### `DMsMessageRAM.BitmaskMetadata` specification
```
F E D C B A 9 8 7 6 5 4 3 2 1 0     [bit position]
v v v t q g z z z s s s s s s s     [data]
```
`v` = version number (max 8)
`t` = contains text (flag)
`q` = contains attachment/s (flag)
`g` = edited message (flag)
`z` = biased attachment/s count (max 8)
`s` = binary-biased text size (max `1024`):
    write formula: `((RealLength + 7) / 8) - 1`
    read formula: `(StoredSize + 1) * 8`

    `NOTE 1:` messages with length values that vary in numbers lower than the one
    in the chosen version will contain `NULL` characters from the last intended
    character to the end.
    `NOTE 2:` in the write formula, the `7` is used to nudge the result value into
    the next 8bit block, so that a `ceil()` function is not needed since integers
    always round down.
    `NOTE 3:` the `+ 1` and `- 1` are used for biasing the size value, since a `0`
    character message is impossible.
*/
struct DMsMessageRAM {
public:
    std::uint16_t BitmaskMetadata; // The metadata of this message
    std::uint32_t Timestamp; // The timestamp of this message
    std::uint32_t SenderID; // The `UserID` of the message sender
    DMsAttachment* Attachments = nullptr; // A pointer to a heap-allocated buffer of `DMsAttachment`s
    std::string MessageData; // The serialized message data

    /// @brief Default constructor, used when `read()`ing from a DMs file
    DMsMessageRAM() = default;
    /// @brief Construct with a `BitmaskMetadata`, `Timestamp`, `SenderID` and `MessageData`
    /// @param BmData The `BitmaskMetadata` value
    /// @param Tstamp The `Timestamp` value
    /// @param SdID The `SenderID` value
    /// @param Data The `MessageData` value
    DMsMessageRAM(const std::uint16_t BmData, const std::uint32_t Tstamp, const std::uint32_t SdID, const std::string& Data) :
        BitmaskMetadata(BmData), Timestamp(Tstamp), SenderID(SdID), MessageData(Data) {

        // Calculate the amount of `DMsAttachment`s to allocate
        if (BitmaskMetadata & MESSAGE_ATTACHFLAG_MASK) {
            std::uint8_t AttachCount = ((BitmaskMetadata & MESSAGE_ATTACHCOUNT_MASK) >> MESSAGE_ATTACHCOUNT_SHPOS) + 1;
            if (AttachCount > 0) Attachments = new DMsAttachment[AttachCount];
        }
    }
    ~DMsMessageRAM() {
        // Calculate the amount of `DMsAttachment`s to free
        if (BitmaskMetadata & MESSAGE_ATTACHFLAG_MASK) {
            std::uint8_t AttachCount = ((BitmaskMetadata & MESSAGE_ATTACHCOUNT_MASK) >> MESSAGE_ATTACHCOUNT_SHPOS) + 1;
            if (AttachCount != 0) delete[] Attachments;
        }
    }
    // Delete copy operations
    DMsMessageRAM(DMsMessageRAM&) = delete;
    DMsMessageRAM(const DMsMessageRAM&) = delete;
    DMsMessageRAM operator=(DMsMessageRAM&) = delete;
    DMsMessageRAM operator=(const DMsMessageRAM&) = delete;

    // Move constructor
    DMsMessageRAM(DMsMessageRAM&& Other) noexcept :
        BitmaskMetadata(Other.BitmaskMetadata), Timestamp(Other.Timestamp), SenderID(Other.SenderID),
        Attachments(Other.Attachments), MessageData(std::move(Other.MessageData)) {
        Other.Attachments = nullptr;
    }

    /// @brief Helper function to add or change a `MessageData` value later
    inline void ChangeData(const std::string& NewData) {
        MessageData = NewData;
    }
};

#pragma pack(pop)

/// @brief Generate a `BitmaskMetadata` value for a `DMsAttachment` struct
/// @param MediaType The media type of the attachment
/// @param ExtSize The extension size of the attachment's filename
/// @return `NewDMsAttachmentBmDataResponse Response`
NewDMsAttachmentBmDataResponse NewDMsAttachmentBitmaskMetadata(
    const DMsAttachmentMediaType MediaType, const std::uint8_t ExtSize) {

    NewDMsAttachmentBmDataStatus ErrorStatus;
    std::uint8_t BitmaskMetadata = 0;

    BitmaskMetadata |= (static_cast<std::uint8_t>(MediaType) << ATTACHMENT_MEDIATYPE_SHPOS);
    ErrorStatus = NewDMsAttachmentBmDataStatus::OK;
    
    // Even if MediaType is DMsAttachmentMediaType::Reply the ExtSize is still used for the search hint
    if (ExtSize > 31) ErrorStatus = NewDMsAttachmentBmDataStatus::ExtSizeTooBig;
    else BitmaskMetadata |= ExtSize;

    return { ErrorStatus, BitmaskMetadata };

}

/// @brief Get a `DMsAttachment.BitmaskMetadata` contents into integral types
/// @param BitmaskMetadata The `BitmaskMetadata` value to decompose
/// @return `GetDMsAttachmentBmDataResponse Response`
inline GetDMsAttachmentBmDataResponse GetBitmaskMetadata(const std::uint8_t BitmaskMetadata) {
    return {
        static_cast<DMsAttachmentMediaType>((BitmaskMetadata & ATTACHMENT_MEDIATYPE_MASK) >> 5),
        static_cast<std::uint8_t>(BitmaskMetadata & ATTACHMENT_EXTSIZE_MASK)
    };
}

/// @brief Generate a `BitmaskMetadata` value for a `DMsMessageDisk` or `DMsMessageRAM` struct
/// @param Version The DMs format version
/// @param TextFlag Flag specifying whether the message contains any text
/// @param AttachFlag Flag specifying whether the message contains any attachments
/// @param EditedFlag Flag specifying whether the message is an edit of an older message
/// @param AttachCount The number of attachments in the message
/// @param TextSize The 8-byte-block-biased size of the message
/// @return `NewDMsMessageBmDataResponse Response`
NewDMsMessageBmDataResponse NewDMsMessageBitmaskMetadata(
    const std::uint8_t Version, const bool TextFlag, const bool AttachFlag, const bool EditedFlag,
    const std::uint8_t AttachCount, /* non-const */ std::uint16_t TextSize) { // No references because smaller whan WORD size

    NewDMsMessageBmDataStatus ErrorStatus = NewDMsMessageBmDataStatus::OK;
    std::uint16_t BitmaskMetadata = 0;

    // Hard-set version to 0 if it is out of bounds
    if (Version == 0 || Version > 8) ErrorStatus = NewDMsMessageBmDataStatus::InvalidVersion;
    // Check mismatches
    if (TextFlag && TextSize == 0) ErrorStatus |= NewDMsMessageBmDataStatus::FlagVsDataMismatch;
    if (AttachFlag && AttachCount == 0) ErrorStatus |= NewDMsMessageBmDataStatus::FlagVsDataMismatch;

    if (TextSize > 1024) ErrorStatus |= NewDMsMessageBmDataStatus::TextTooLong;
    TextSize = ((TextSize + 7) / 8) - 1;

    BitmaskMetadata |= (Version - 1) << MESSAGE_VERSION_SHPOS;
    BitmaskMetadata |= (TextFlag & 1) << MESSAGE_TEXTFLAG_SHPOS;
    BitmaskMetadata |= (AttachFlag & 1) << MESSAGE_ATTACHFLAG_SHPOS;
    BitmaskMetadata |= (EditedFlag & 1) << MESSAGE_EDITEDFLAG_SHPOS;
    BitmaskMetadata |= (AttachCount - 1) << MESSAGE_ATTACHCOUNT_SHPOS;
    BitmaskMetadata |= TextSize & MESSAGE_TEXTSIZE_MASK;

    return { ErrorStatus, BitmaskMetadata };
}

/// @brief Get a `DMsMessageDisk.BitmaskMetadata` or `DMsMessageRAM.BitmaskMetadata` contents into integral types
/// @param BitmaskMetadata The `BitmaskMetadata` value to decompose
/// @return `GetDMsMessageBmDataResponse Response`
GetDMsMessageBmDataResponse GetBitmaskMetadata(const std::uint16_t BitmaskMetadata) {
    GetDMsMessageBmDataResponse Response;
    Response.Version = ((BitmaskMetadata & MESSAGE_VERSION_MASK) >> MESSAGE_VERSION_SHPOS) + 1;
    Response.TextFlag = (BitmaskMetadata & MESSAGE_TEXTFLAG_MASK) != 0;
    Response.AttachFlag = (BitmaskMetadata & MESSAGE_ATTACHFLAG_MASK) != 0;
    Response.EditedFlag = (BitmaskMetadata & MESSAGE_EDITEDFLAG_MASK) != 0;
    Response.AttachCount = ((BitmaskMetadata & MESSAGE_ATTACHCOUNT_MASK) >> MESSAGE_ATTACHCOUNT_SHPOS) + 1;
    Response.TextSize = ((BitmaskMetadata & MESSAGE_TEXTSIZE_MASK) + 1) * 8;
    return Response;
}

// Create a specialized `DMsAttachmentMediaType::Reply` `DMsAttachment` object

/// @brief Create a specialized `DMsAttachmentMediaType::Reply` `DMsAttachment` object
/// @param LookBehind The number of messages ago the original message was, `0` = deep search
/// @param Timestamp The timestamp of the original message
/// @param Preview A 38 non-null-terminated preview of the original message
/// @return `NewDMsAttachmentReplyResponse Response`
NewDMsAttachmentReplyResponse NewDMsAttachmentReply(std::uint8_t LookBehind, const std::uint32_t Timestamp,
    const std::string& Preview) {

    NewDMsAttachmentReplyStatus ErrorStatus = NewDMsAttachmentReplyStatus::OK;
    std::uint8_t BitmaskMetadata = 0;
    char MsgPrev[REPLY_PREVIEW_BYTECOUNT]{}; // Initialize to '\0'

    // Do some (error) checks
    if (Preview.size() > REPLY_PREVIEW_BYTECOUNT) {
        for (std::uint8_t i = 0; i < REPLY_PREVIEW_BYTECOUNT; ++i)
            MsgPrev[i] = Preview[i];
        ErrorStatus = NewDMsAttachmentReplyStatus::PreviewChopped;
    } else for (std::uint8_t i = 0; i < Preview.size(); ++i)
        MsgPrev[i] = Preview[i];

    if (LookBehind > 31) {
        LookBehind = 0;
        ErrorStatus |= NewDMsAttachmentReplyStatus::LookBehindToDeepSearch;
    }
    
    // Construct the Attachment already with a `BitmaskMetadata` member
    DMsAttachment Attachment(
        (static_cast<std::uint8_t>(DMsAttachmentMediaType::Reply) << ATTACHMENT_MEDIATYPE_SHPOS) |
        LookBehind
    );

    // Copy the `Timestamp` and the `Preview` into the `Filename[]` char buffer
    std::memcpy(Attachment.Filename, &Timestamp, REPLY_TIMESTAMP_BYTECOUNT);
    std::memcpy(&(Attachment.Filename[REPLY_TIMESTAMP_BYTECOUNT]), MsgPrev, REPLY_PREVIEW_BYTECOUNT);

    return { ErrorStatus, Attachment };
}

/// @brief Get a specialized `DMsAttachmentMediaType::Reply` `DMsAttachment` object into integral types
/// @param Attachment The `DMsAttachment` object to decompose
/// @return `GetDMsAttachmentReplyResponse Response`
GetDMsAttachmentReplyResponse GetDMsAttachmentReply(const DMsAttachment& Attachment) {
    GetDMsAttachmentReplyResponse Response;

    Response.LookBehind = (Attachment.BitmaskMetadata & ATTACHMENT_EXTSIZE_MASK);
    std::memcpy(&Response.Timestamp, Attachment.Filename, REPLY_TIMESTAMP_BYTECOUNT);
    std::memcpy(Response.MsgPreview, &(Attachment.Filename[REPLY_TIMESTAMP_BYTECOUNT]), REPLY_PREVIEW_BYTECOUNT);

    return Response;
}

// Turn a `DMsMessageRAM` object into an `std::string`

/// @brief Turn a `DMsMessageRAM` object into an `std::string` to output its data in a DMs file
/// @param RAM The `DMsMessageRAM` object to serialize
/// @return `std::string Buffer`
[[nodiscard]] std::string SerializeUnencryptedBuffer(const DMsMessageRAM& RAM) {
    // Get metadata for exact size
    GetDMsMessageBmDataResponse Metadata = GetBitmaskMetadata(RAM.BitmaskMetadata);

    // TotalSize = Header(10) + (Z * AttachSize(43)) + PaddedText(S * 8)
    std::uint16_t TotalSize = static_cast<std::uint16_t>(sizeof(DMsMessageDisk)) + 
                        (Metadata.AttachCount * sizeof(DMsAttachment)) + 
                        Metadata.TextSize;

    // Init with '\0' to handle 8-byte padding automatically
    std::string Buffer(TotalSize, '\0');
    char* DataPtr = Buffer.data();

    // Copy header
    DMsMessageDisk DiskData(RAM.BitmaskMetadata, RAM.Timestamp, RAM.SenderID);
    std::memcpy(DataPtr, &DiskData, sizeof(DMsMessageDisk));
    DataPtr += sizeof(DMsMessageDisk);

    // Copy attachments if any
    if (Metadata.AttachFlag && RAM.Attachments != nullptr) {
        std::uint16_t AttachBytes = Metadata.AttachCount * sizeof(DMsAttachment);
        std::memcpy(DataPtr, RAM.Attachments, AttachBytes);
        DataPtr += AttachBytes;
    }

    // Copy raw text (the rest of the buffer remains '\0')
    if (Metadata.TextFlag && !RAM.MessageData.empty()) {
        std::memcpy(DataPtr, RAM.MessageData.data(), RAM.MessageData.size());
    }

    return Buffer;
}

// Turn an `std::vector<std::string>` into an `std::vector<DMsMessageRAM>` object

/// @brief Turn a vector of `std::string` buffers into a vector of `DMsMessageRAM` objects
/// @param Buffer The vector of buffers to deserialize
/// @return `std::vector<DMsMessageRAM> Response`
/// @note The returned `DMsMessageRAM` objects contain heap-allocated `DMsAttachment` objects, so the
///       caller MUST use `std::move()` to avoid UB when their destructor gets called
[[nodiscard]] std::vector<DMsMessageRAM> ParseDecryptedBuffer(const std::vector<std::string>& Buffer) {

    std::vector<DMsMessageRAM> Response;
    Response.reserve(Buffer.size());
    std::uint16_t i = 0; // `std::uint16_t` and not `std::uint8_t` because `DMsHeader.NodeCount` is a `std::uint16_t`
    for (auto& Current : Buffer) {
        char* DataPtr = const_cast<char*>(Current.data()); // Should work

        // Map the DMsMessageDisk header
        DMsMessageDisk* DiskData = reinterpret_cast<DMsMessageDisk*>(DataPtr);
        DataPtr += sizeof(DMsMessageDisk);

        // Get Metadata for sizes
        GetDMsMessageBmDataResponse Metadata = GetBitmaskMetadata(DiskData->BitmaskMetadata);

        // Create the RAM object (start with empty string),
        // the constructor handles the Attachments array allocation
        DMsMessageRAM RAM(DiskData->BitmaskMetadata, DiskData->Timestamp, DiskData->SenderID, "");

        // Copy Attachments back into the heap-allocated array
        if (Metadata.AttachFlag) {
            std::uint16_t AttachBytes = Metadata.AttachCount * sizeof(DMsAttachment);
            std::memcpy(RAM.Attachments, DataPtr, AttachBytes);
            DataPtr += AttachBytes;
        }

        // Extract and trim text using the ChangeData helper method
        if (Metadata.TextFlag) {
            // Create a temporary view of the 8-byte padded block
            std::string TempStr(DataPtr, Metadata.TextSize);
            // Find first '\0' to strip padding
            std::size_t NullPos = TempStr.find('\0');
            std::string RealText = (NullPos != std::string::npos) ? TempStr.substr(0, NullPos) : TempStr;

            RAM.ChangeData(RealText);
        }
        Response.push_back(std::move(RAM));
        i++;
    }
    return Response;
}

} // namespace ControlZ
