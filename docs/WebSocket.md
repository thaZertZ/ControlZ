
# WebSocket

The use of the WebSocket protocol in ControlZ internally is crucial:
it is used as the main data exchange protocol between each client and
the server.

## The ControlZ WsPacket subprotocol

ControlZ uses a custom WebSocket subprotocol called WsPacket, which 
contains the packet type in the first byte:

- `Invalid`: Invalid state (*not* used when sending data)
- `Ack`: Acknowledgement
- `Auth`: Authentication of a user
- `Deauth`: Deauthentication of a user
- `Download`: Download a file
- `DownloadResponse`: Server response to a `Download` packet
- `DownloadChunk`: A chunk of data to download to a file
- `DM`: Send a Direct Message to a user (using the DMs format)
- `InfoUser`: Get information about a user
- `InfoUserResponse`: Server response to an `InfoUser` packet
- `InfoChat`: Get information about a chat
- `InfoChatResponse`: Server response to an `InfoChat` packet
- `Nack`: Negative acknowledgement
- `Send`: Send a message in a public chat
- `Update`: Receive live update information (like messages)
- `UpdateResponse`: Server response to an `Update` packet
- `Upload`: Upload a file
- `UploadResponse`: Server response to an `Upload` packet
- `UploadChunk`: A chunk of data to upload to a file

### `Ack` packet

An `Ack` packet is made up by 4 bytes, the first byte is the `Ack` packet
code itself, while the next three bytes are the ASCII values of the
characters `ACK`.

### `Auth` packet

An `Auth` packet is made up by 69 bytes, the first byte is the `Auth` packet
code itself, while the next 68 bytes contain the following data:

| Bytes | Type       | Data         | Description                               |
| ----- | ---------- | ------------ | ----------------------------------------- |
| 4     | `uint32_t` | UserID       | The ID of the user trying to authenticate |
| 32    | `char[]`   | UsernameHash | An Argon2id hash of the username          |
| 32    | `char[]`   | PswdHash     | An Argon2id hash of the password          |

### `Deauth` packet

A `Deauth` packet is made up by 45 bytes, the first byte is the `Deauth` packet
code itself, while the next 44 bytes contain the following data:

| Bytes | Type       | Data         | Description                                   |
| ----- | ---------- | ------------ | --------------------------------------------- |
| 4     | `uint32_t` | UserID       | The ID of the user trying to authenticate     |
| 32    | `char[]`   | UsernameHash | An Argon2id hash of the username              |
| 8     | `char[]`   | UserToken    | A token associated with the session to deauth |

### `Download` packet

<!--
the filename with which the file will be downloaded depends on the client, but
the server should keep a map of the original filenames + the timestamps

the Base32 alphabet is "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"

when doing Upload and UploadResponse packets, remember to add a field for a delta
uint32_t or uint16_t containing the number of milliseconds by which the upload
timestamp was nudged to tell the client
-->

| Bytes  | Type       | Data         | Description                                                              |
| ------ | ---------- | ------------ | ------------------------------------------------------------------------ |
| 1      | `bool`     | QueryMethod  | `true` if the filename provided is the timestamp, else the real filename |
|        |            |              | `QueryMethod = true`                                                     |
| 8      | `char[]`   | FilenameB32  | A Base32 value used to represent the filename timestamp in milliseconds  |
| < 32   | `char[]`   | Extension    | The file extension in ASCII, 31 characters maximum                       |
|        |            |              | `QueryMethod = false`                                                    |
| 8      | `uint64_t` | FilenameSize | The number of characters of the original filename+extension (can't be 0) |
| < 2^64 | `char[]`   | Filename     | The original filename+extension (can't be empty)                         |

### `DownloadResponse` packet

| Bytes  | Type       | Data         | Description                                                              |
| ------ | ---------- | ------------ | ------------------------------------------------------------------------ |
| 1      | `bool`     | QueryMethod  | `true` if the filename provided is the timestamp, else the real filename |
| 8      | `uint64_t` | FileSize     | The size of the file to download in bytes                                |
| 8      | `uint64_t` | ChunkCount   | The number of chunks in which the file's data will be split and sent     |
| 8      | `uint64_t` | FileID       | A randomly generated number used to indentify the file's chunks          |
|        |            |              | `QueryMethod = true`                                                     |
| 8      | `char[]`   | FilenameB32  | A Base32 value used to represent the filename timestamp in milliseconds  |
| < 32   | `char[]`   | Extension    | The file extension in ASCII, 31 characters maximum                       |
|        |            |              | `QueryMethod = false`                                                    |
| 8      | `uint64_t` | FilenameSize | The number of characters of the original filename+extension (can't be 0) |
| < 2^64 | `char[]`   | Filename     | The original filename+extension (can't be empty)                         |

The `FileID` field is used when there are multiple files to download, and packets can arrive
at different times due to multithreading, so multiple files could be downloading simultaneously
and multiple chunks of the same file could not arrive in order, so we also have `ChunkCount` and
later we will see `ChunkIndex`.

### `DownloadChunk` packet

| Bytes    | Type       | Data       | Description                                                               |
| -------- | ---------- | ---------- | ------------------------------------------------------------------------- |
| 8        | `uint64_t` | FileID     | A randomly generate number used to identify the file's chunks             |
| 8        | `uint64_t` | ChunkIndex | The number of the file chunk based on the `ChunkCount` field used earlier |
| 8        | `uint16_t` | DataSize   | The number of bytes occupied by the data sent after this field            |
| DataSize | `char[]`   | Data       | Raw binary data beloging to the file chunk                                |

### `DM` packet

A `DM` packet is made up by an indefinite amount of bytes, the first one is the `DM` packet
code itself, the next three bytes are respectively a padding byte and the two `DM` ASCII
characters, while 4 more bytes are used to hold the UserID of the user to send the message to.

After them, a serialized and unencrypted DMsMessageRAM object is transferred.

**Note:** The serialized object to concatenate is created through the use of the `Envelope.hpp`
header's API.

### `InfoUser` packet

An `InfoUser` packet is made up by 6 bytes, the first one is the `InfoUser` packet code
itself, while the following 5 bytes contain the following data:

| Bytes | Type       | Data           | Description                                                           |
| ----- | ---------- | -------------- | --------------------------------------------------------------------- |
| 1     | `bool`     | ResponseFormat | `true` if the response should contain JSON, `false` if binary is used |
| 4     | `uint32_t` | UserID         | The UserID of the user to get info about                              |

### `InfoUserResponse` packet

| Bytes        | Type       | Data           | Description                                                             |
| ------------ | ---------- | -------------- | ----------------------------------------------------------------------- |
| 1            | `bool`     | ResponseFormat | `true` if the response contains JSON, `false` if binary is used instead |
|              |            |                | `ResponseFormat = true`                                                 |
| 8            | `uint64_t` | ResponseSize   | Specify the size in bytes of the following JSON data                    |
| ResponseSize | `char[]`   | Data           | JSON data containing info about the requested user                      |
|              |            |                | `ResponseFormat = false`                                                |
| 8            | `uint64_t` | ResponseSize   | Specify the size in bytes of the following binary data                  |
| ResponseSize | `char[]`   | Data           | Raw binary data containing info about the requested user                |

### `InfoChat` packet

An `InfoChat` packet is made up by 6 bytes, the first one is the `InfoChat` packet code
itself, while the following 5 bytes contain the following data:

| Bytes | Type       | Data           | Description                                                           |
| ----- | ---------- | -------------- | --------------------------------------------------------------------- |
| 1     | `bool`     | ResponseFormat | `true` if the response should contain JSON, `false` if binary is used |
| 4     | `uint32_t` | ChatID         | The ChatID of the chat to get info about                              |

### `InfoChatResponse` packet

| Bytes        | Type       | Data           | Description                                                             |
| ------------ | ---------- | -------------- | ----------------------------------------------------------------------- |
| 1            | `bool`     | ResponseFormat | `true` if the response contains JSON, `false` if binary is used instead |
|              |            |                | `ResponseFormat = true`                                                 |
| 8            | `uint64_t` | ResponseSize   | Specify the size in bytes of the following JSON data                    |
| ResponseSize | `char[]`   | Data           | JSON data containing info about the requested chat                      |
|              |            |                | `ResponseFormat = false`                                                |
| 8            | `uint64_t` | ResponseSize   | Specify the size in bytes of the following binary data                  |
| ResponseSize | `char[]`   | Data           | Raw binary data containing info about the requested chat                |

### `Send` packet

A `Send` packet is made up by an indefinite amount of bytes, the first one is the `Send` packet
code itself, the next three bytes contain the three `SND` ASCII characters, while 4 more bytes
are used to hold the UserID of the user to send the message to.

After them, a serialized and unencrypted DMsMessageRAM object is transferred.

**Note:** The serialized object to concatenate is created through the use of the `Envelope.hpp`
header's API.

### `Update` packet

An `Update` is too complex to implement right now since most of the data and its format, that this
packet should carry, has not been defined well yet.

### `UpdateResponse` packet

Same as `Update` for now.

### `Upload` packet

| Bytes        | Type       | Data         | Description                                                                    |
| ------------ | ---------- | ------------ | ------------------------------------------------------------------------------ |
| 8            | `uint64_t` | FileSize     | The size of the file to upload in bytes                                        |
| 8            | `uint64_t` | ChunkCount   | The number of chunks in which the file to upload will be split                 |
| 8            | `uint64_t` | FilenameSize | The number of bytes that follow this field used to store the original filename |
| FilenameSize | `char[]`   | Filename     | The original filename, *without* the extension, stored in the Extension field  |
| 8            | `char[]`   | FilenameB32  | A Base32 value used to represent the filename timestamp in milliseconds        |
| < 32         | `char[]`   | Extension    | The file extension in ASCII, 31 characters maximum                             |

### `UploadResponse` packet

| Bytes  | Type       | Data         | Description                                                                   |
| ------ | ---------- | ------------ | ----------------------------------------------------------------------------- |
| 4      | `uint32_t` | TimeDelta    | A value in milliseconds by which the originally provided timestamp was nudged |
| 8      | `uint64_t` | FileID       | A randomly generated number to indetify the file during the transfer process  |

### `UploadChunk` packet

| Bytes    | Type       | Data       | Description                                                               |
| -------- | ---------- | ---------- | ------------------------------------------------------------------------- |
| 8        | `uint64_t` | FileID     | A randomly generate number used to identify the file's chunks             |
| 8        | `uint64_t` | ChunkIndex | The number of the file chunk based on the `ChunkCount` field used earlier |
| 8        | `uint16_t` | DataSize   | The number of bytes occupied by the data sent after this field            |
| DataSize | `char[]`   | Data       | Raw binary data beloging to the file chunk                                |
