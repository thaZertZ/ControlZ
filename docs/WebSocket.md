
# WebSocket

In ControlZ, the WebSocket protocol is used as the main data exchange
method between clients and the server, and a custom ControlZ subprotocol
has been ideated specifically for the case.

Here the specification of this subprotocol will be covered.

## Protocol header

The first 10 bytes of each packet represent the same data:

```
0    :  Packet type and version
1    :  Flags
2-3  :  Packet ID
4-5  :  Response ID
6-9  :  Payload length
```

- **Packet type and version**: the kind of packet the payload represents and
  the subprotocol version in a bitmask
```
7 6 5 4 3 2 1 0
---------------
v v v t t t t t

t  :  Packet type
v  :  Version
```
- **Flags**: a bitmask for packet metadata
- **Packet ID**: a unique number associated with this packet (fragmented
  and chunked packets don't override this, instead they keep the one of
  the first packet in the chunk/fragment sequence)
- **Response ID**: the **packet ID** field of the packet being responded to
- **Payload length**: the length of the payload following this header

The packet metadata is structured like this:

```
7 6 5 4 3 2 1 0
---------------
x c r f a n b m

m  :  Request NACK
b  :  Request ACK
n  :  Respond with NACK
a  :  Respond with ACK
f  :  Fragment flag
r  :  Response flag
c  :  Chunk flag
x  :  Reserved for future use
```

- **Request NACK**: signal that the response packet must respond with NACK
- **Request ACK**: signal that the response packet must respond with ACK
- **Respond with NACK**: respond with NACK if the previous packet requested it
- **Respond with ACK**: respond with ACK if the previous packet requested it
- **Fragment flag**: set to `1` if more fragments of the same packet are coming,
  set to `0` if this is the last or only fragment. Every fragment packet except
  the first one must turn on the **chunk flag** bit
- **Response flag**: signal that this packet is a response version of the type
  of the last received packet (used when performing one-way requests)
- **Chunk flag**: signal that this packet is a chunk of a previously sent packet
  that initiated a chunk sequence by setting the **fragment flag** bit. The
  last chunk packet of a sequence must turn the **fragment flag** bit off to
  signal the end of it

## Packet types

| Name       | Description                                         |
| ---------- | --------------------------------------------------- |
| `Auth`     | Authenticate a user                                 |
| `Deauth`   | Deauthenticate a user                               |
| `DM`       | Send a private message to a user                    |
| `Download` | Download a file from the server                     |
| `InfoChat` | Request information about a public chat             |
| `InfoUser` | Request information about a user                    |
| `Send`     | Send a public message in a chat                     |
| `Update`   | Send new session information like incoming messages |
| `Upload`   | Upload a file to the server                         |

Here the individual packet payloads will be covered.

### `Auth`

```
0-1  :  UserID
x-y  :  Username hash
z-w  :  Password hash
```

- **UserID**: the UserID associated with the user to authenticate
- **Username hash**: an Argon2id hash of the username
- **Password hash**: an Argon2id hash of the password

#### Response

```
0-15  :  Session token
```

- **Session token**: a unique 16-character identifier for a user session.
  This must be used when deauthenticating

### `Deauth`

```
0-1   :  UserID
2-17  :  Session token
```

- **User ID**: the UserID associated with the user to deauthenticate
- **Session token**: the unique session identifier received upon authentication

This packet expects an acknowledgement.

#### Response

The response only contains an acknowledgement.

### `DM`

```
0-1  :  Recipient UserID
---  :  Raw DMsMessage data
```

**Recipient UserID**: the UserID of the user receiving the message
**Raw DMsMessage data**: message data serialized directly from a DMsMessage object

This packet expects an acknowledgement.

#### Response

The response only contains an acknowledgement.

If a negative acknowledgement is sent, the sender of the request packet must resend
again the same packet, also conserving the same **sequence ID**.

### `Download`

```
0-3  :  AttachmentID
```

- **AttachmentID**: the AttachmentID associated with the file to download

This packet expects a fragmented/chunked response.

#### Response

```
0-3  :  Chunk size
4-7  :  Chunk count
---  :  Original filename
```

- **Chunk size**: indicates how many bytes of file data each chunk will contain
- **Chunk count**: indicates how many chunks will follow this packet
- **Original filename**: a string containing the original name of the file to download

After this packet, chunks will follow.

#### Chunk

```
0-3  :  Chunk index
---  :  Raw file data
```

- **Chunk index**: the number of the chunk in the sequence. This is used to ensure that
  all chunks have arrived and helps sorting them
- **Raw file data**: the raw data of the file to download. Note that the last chunk
  may not contain the same amount of data bytes as all the other ones, because the file
  size is not a multiple of the chosen chunk size

Chunk packets keep the same **sequence ID** field as their parent response packet.

The last chunk packet will expect an acknowledgement signifying that the client was able
to receive and reconstruct the file contents. If a negative acknowledgement is received,
the packet transmitting it must be a `DownloadChunks` packet, asking for one or more
file chunks to be transmitted again.

### `DownloadChunks`

```
---  :  Chunk indeces
```

- **Chunk indeces**: a variadic number of chunk indeces (32bit unsigned integers)
  that the server must resend with a `Download` chunk packet

Again, the last chunk expects an acknowledgement, and if a negative one is sent, this
whole process repeats.

### `InfoChat`

```
0-1  :  UserID
2-3  :  ChatID
```

- **UserID**: the UserID of the user that is requesting the information
- **ChatID**: the ChatID of the chat to get information about

This packet expects an acknowledgement attached to a response, if a negative
acknowledgement is received, the response should not hold any data.

### `InfoUser`

```
0-1  :  UserID
2-3  :  Target UserID
```

- **UserID**: the UserID of the user that is requesting the information
- **Target UserID**: the UserID to get information about

This packet expects an acknowledgement attached to a response, if a negative
acknowledgement is received, the response should not hold any data.

#### Response

<!--
make this when the user format will be done,
only allow some information to be transmitted
-->

```
WIP
```

- **WIP**: WIP
