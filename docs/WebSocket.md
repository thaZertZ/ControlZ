
# WebSocket

In ControlZ, the WebSocket protocol is used as the main data exchange
method between clients and the server, and a custom ControlZ subprotocol
has been ideated specifically for the case.

This subprotocol always expects data in network byte order (big endian)
representation, both packets and their payloads.  
Here the specification of this subprotocol will be covered.

## Protocol header

The first 12 bytes of each packet represent the same data:

```
0     :  Packet type and version
1     :  Flags
2-3   :  Packet ID
4-5   :  Response ID
6-7   :  UserID
8-11  :  Payload length
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
- **Packet ID**: the ID of this packet's sequence. Fragments and chunks in one
  sequence retain the ID assigned to the first packet. IDs are scoped to one
  WebSocket connection and one sending direction. ID `0` is reserved for `None`,
  `Explode`, and `Heartbeat`, while all other packet types use a nonzero ID.
- **Response ID**: the nonzero **Packet ID** of the earlier packet this packet
  responds to. It is `0` when the packet is not a response and does not otherwise
  refer to an earlier packet. ACK/NACK-only `None` packets use **packet ID** `0`
  and put the responded-to packet's ID in **response ID**.
- **UserID**: the `UserID` of the user sending the packet
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
  of the specified packet type
- **Chunk flag**: signal that this packet is a chunk of a previously sent packet
  that initiated a chunk sequence by setting the **fragment flag** bit. The
  last chunk packet of a sequence must turn the **fragment flag** bit off to
  signal the end of it

Requesting or responding with both ACK and NACK is illegal.

Each sender allocates IDs independently from the other sender. The client and server
may therefore use the same numeric **packet ID** simultaneously in opposite
directions. A sender reserves an ID for an active sequence and releases it only on
the terminal response defined for that sequence. The receiver tracks active incoming
sequences separately, and it does not mirror the sender's allocation bitmap. Full
allocation, retry, response-matching, and reset rules are specified in
[`SequenceID.md`](./SequenceID.md).

An ACK or NACK is correlated to the request using **Response ID**. A retry-requesting
NACK is nonterminal: the sender retries the same logical packet with the same
**packet ID**, and the receiver retains the inbound sequence.  
A terminal ACK or NACK ends the sequence and permits the sender to release that ID.
The receiver must not execute a retry as a second independent operation. A timeout does
not authorize resending a request.  
If the response does not arrive, the implementation reports a timeout and closes the
connection, which resets that connection's ID state.

## Packet types

| Name             | Description                                                |
| ---------------- | ---------------------------------------------------------- |
| `None`           | Sending this packet won't do ***anything***                |
| `Auth`           | Authenticate a user                                        |
| `AuthToken`      | Reauthenticate a user with a session token                 |
| `Deauth`         | Deauthenticate a user                                      |
| `DM`             | Send a private message to a user                           |
| `Download`       | Download a file from the server                            |
| `DownloadChunks` | Download a specific series of chunks from a file           |
| `Explode`        | Tell the other end that we ran out of **packet ID** values |
| `GetFilename`    | Ask the server the name of a file via its `AttachmentID`   |
| `Heartbeat`      | Tell the other end that this end is alive                  |
| `InfoChat`       | Request information about a public chat                    |
| `InfoUser`       | Request information about a user                           |
| `ManageFriend`   | Manage a friend relationship                               |
| `Send`           | Send a public message in a chat                            |
| `Update`         | Send new session information like incoming messages        |
| `UpdateChat`     | Update a public chat's settings                            |
| `UpdateUser`     | Update a user's settings                                   |
| `Upload`         | Upload a file to the server                                |
| `UploadChunks`   | Upload a specific series of chunks from a file             |

Here the individual packet payloads will be covered.

### `None`

Sending a packet with this type will not do ***anything***, not even
update the user activity list like `Heartbeat`, and any payload will be
*ignored*.

The only rare case in which this packet would be sent is when responding with an
ACK or NACK without a payload. Set the corresponding **respond with ACK** or
**respond with NACK** flag, set **response ID** to the responded-to packet's nonzero
**packet ID**.  
This packet does not itself request an ACK/NACK.

This packet uses the reserved **packet ID** value of `0`.

### `Auth`

```
0-31   :  Username hash
32-63  :  Password hash
```

- **Username hash**: an Argon2id hash of the username
- **Password hash**: an Argon2id hash of the password

#### Response

```
0-15  :  Session token
```

- **Session token**: a unique 16-character identifier for a user session.
  This must be used when deauthenticating

### `AuthToken`

```
0-15  :  Session token
```

- **Session token**: a unique 16-character identifier for a user session

This packet expects an ackowledgement, if a negative one is received it
either means that the session token is invalid or that another session is
active with this token.

Internally, the server keeps a list of session tokens per user, together
with information about if the session is currently active (determined by
whether the user has sent at least one packet in the last minute).  
A session token remains in the list for 24 hours, after which it is
removed and therefore becomes invalid.

When the server checks if the session token is valid, it just checks if
it is in the list and is *also* inactive.

### `Deauth`

```
0-15
```

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
A negative acknowledgement means that the file was not able to be written to disk.

#### Response

The response only contains an acknowledgement.

If a negative acknowledgement is sent, the sender of the request packet must resend
again the same packet, also keeping the same **packet ID**.

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
  size may not be a multiple of the chosen chunk size

Chunk packets keep the same **packet ID** field as their parent response packet.

The last chunk packet will expect an acknowledgement signifying that the client was able
to receive and reconstruct the file contents. If a negative acknowledgement is received,
the packet transmitting it must be a `DownloadChunks` packet, asking for one or more
file chunks to be transmitted again.

### `DownloadChunks`

```
---  :  Chunk indeces
```

- **Chunk indeces**: a variadic number of chunk indeces (32bit unsigned integers)
  that the server must resend with a `Download` *chunk* packet

Again, the last chunk expects an acknowledgement, and if a negative one is sent, this
whole process repeats.

### `Explode`

The payload is *empty* and this packet expects no acknowledgements.  
Either endpoint may send this packet when its outgoing allocator has no free nonzero
**packet ID** values. Before sending it, the sender reports ID exhaustion to its
higher-level caller and stops starting ordinary sequences.  
The receiver reports the peer's exhaustion notification to its higher-level caller.  
`Explode` does not free, reserve, or acknowledge any ordinary packet ID.  
After notifying the peer, the sender closes the connection and opens a new one before
starting more sequences.

This packet uses **packet ID** `0` and **response ID** `0`.

### `GetFilename`

```
0-3  :  AttachmentID
```

- **AttachmentID**: the `AttachmentID` of the file to retrieve the original name of

This packet expects a response, if a negative acknowledgement is received, it means
that the `AttachmentID` was invalid or the server was not able to retrieve the filename
due to I/O errors.

#### Response

```
---  :  Filename
```

- **Filename**: the raw character data of the filename

### `Heartbeat`

The payload is *empty* and this packet expects no acknowledgements.  
This packet is used by the server to update the session activity list for each user,
fundamental for reauthenticating a user with a token.

This packet uses **packet ID** `0` and **response ID** `0`.

### `InfoChat`

```
0-3  :  ChatID
```

- **ChatID**: the ChatID of the chat to get information about

This packet expects an acknowledgement attached to a response, if a negative
acknowledgement is received, the response should not hold any data.  
The negative acknowledgement means that the **ChatID** field was invalid.

#### Response

```
0    :  Policy bitmask
1    :  Name length
2-3  :  Description length
---  :  Name
---  :  Description
---  :  Members list
```

- **Policy bitmask**: a bitmask containing information about which fields are
  allowed to be read. The ones that aren't will hold a value of `0`
- **Name length**: the length in bytes of the **name** field
- **Description length**: the length in bytes of the **description** field
- **Name**: the raw characters of the chat's name. Can be at most 40 characters
- **Description**: the raw characters of the chat's description. Can be at most
  4000 characters
- **Members list**: the list of members inside the chat

### `InfoUser`

```
0-1  :  Target UserID
```

- **Target UserID**: the UserID to get information about

This packet expects an acknowledgement attached to a response, if a negative
acknowledgement is received, the response should not hold any data.  
The negative acknowledgement means that the **Target UserID** field was invalid.

#### Response

```
0-3  :  Registration timestamp
4    :  Policy bitmask
5    :  Username length
6-7  :  Bio length
---  :  Username
---  :  Bio
---  :  Friends list
```

- **Registration timestamp**: a timestamp in seconds from a custom epoch on
  1/1/2027 at 00:00 UTC at which the user was registered
- **Policy bitmask**: a bitmask containing information about which fields
  are allowed to be read. The ones that aren't will hold a value of `0`
- **Username length**: the number of bytes occupied by the **username** field
- **Bio length**: the number of bytes occupied by the **bio** field
- **Username**: the raw characters of the user's username
- **Bio**: the raw characters of the user's biography
- **Friends list**: a variadic list of `UserID`s corresponding to that user's
  friends

This is the structure of the **policy bitmask** field:

```
7 6 5 4 3 2 1 0
---------------
x p p q q u b l

p  :  Message exchange policy
q  :  Friend request policy
u  :  Username flag
b  :  Bio flag
l  :  Friends list flag
x  :  Reserved for future use
```

- **Message exchange policy**: mirrors the **message exchange policy** field of the user
  binary file format, found [here](./User.md#format-header)
- **Friend request policy**: mirrors the **friend request policy** field of the user
  binary file format
- **Username flag**: whether the user can read the requested username
- **Bio flag**: whether the user can read the requested bio
- **Friends list flag**: whether the user can read the requested friends list

### `ManageFriend`

```
0-1  :  Target UserID
2    :  Action
3    :  Padding null byte
```

- **Target UserID**: the `UserID` of the friend
- **Action**: a value representing which action to perform on the specified user.
  `0` = request if it is a friend, `1` = send friend request,
  `2` = accept friend request, `3` = remove friend

This packet expects an acknowledgement, if a negative acknowledgement is received,
it means that the friend request could not be sent (if **action** was `1`)
because the user didn't allow friend requests, or because the **Target UserID**
field was invalid.  
If **action** was `0` it means that the target user is not a
friend of this user.  
Other values of **action** require an acknowledgement.

When a friend request is sent, the server writes it in a `.bin` file that holds
pairs of `UserID`s: the `UserID` of the sender and the target `UserID`.  
Once a friend request has been accepted, the pair is replaced with zeroes ready
to be used again for other requests.

### `Send`

```
0-3  :  ChatID
---  :  Raw DMsMessage data
```

- **ChatID**: the `ChatID` of the chat to send the message to
- **Raw DMsMessage data**: message data serialized directly from a DMsMessage object

This packet expects an acknowledgement.  
A negative acknowledgement means that the file was not able to be written to disk.

#### Response

The response only contains an acknowledgement.

If a negative acknowledgement is sent, the sender of the request packet must resend
again the same packet, also keeping the same **packet ID**.

### `Update`

```
0-1  :  DMs count
2-3  :  Friend requests count
4-5  :  New messages count
---  :  DMs data
---  :  Friend request data
---  :  Message data
```

- **DMs count**: the number of new DMs
- **Friend requests count**: the number of new friend request data
- **New messages count**: the number of *chunks* of new messages in public chats
- **DMs data**: a variadic number of serialized `DMsMessage` objects from different users.
  DMs from the same user are guaranteed to be on after the other, but not necessarily in
  chronological order or sorted among the different `UserID`s
- **Friend request data**: a variadic number of a structure which will be defined later
- **Message data**: a variadic number of chunks divided per public chat, whose data
  will be a series of serialized `DMsMessage` objects that might not be in chronological
  order

The **friend request data** is defined like this:

```
0-1  :  Target UserID
2    :  Action
3    :  Padding null byte
```

- **Target UserID**: the `UserID` of the user to which this action relates
- **Action**: can be `0` if the target user accepted a previously sent friend request,
  it can be `1` if the target user sent a friend request to the user receiving this
  packet, or it can be `2` if the target user removed this user as a friend

The **message data** is divided in chunks like this:

```
0-3  :  ChatID
4-5  :  Message count
---  :  DMsMessage data
```

- **ChatID**: the `ChatID` of the chat the messages were sent in
- **Message count**: the number of messages in this chunk
- **DMsMessage data**: a variadic number of serialized `DMsMessage` objects

### `UpdateChat`

```
---  :  Raw ChatFile data
```

- **Raw ChatFile data**: raw binary data from a serialized `ChatFile` object

This packet expects an acknowledgement, if a negative acknowledgement is received,
it means that the user did not have the required privileges to update the chat,
or the file was not able to be written to disk.

### `UpdateUser`

```
---  :  Raw UserFile data
```

- **Raw UserFile data**: raw binary data from a serialized `UserFile` object

This packet expects an acknowledgement, if a negative acknowledgement is received,
it means that the file was not able to be written to disk.

### `Upload`

```
0-3  :  Chunk size
4-7  :  Chunk count
---  :  Original filename
```

- **Chunk size**: indicates how many bytes of file data each chunk will contain
- **Chunk count**: indicates how many chunks will follow
- **Original filename**: a string containing the original name of the file to upload

After this packet, the server will respond with an `AttachmentID` that can be used
inside a `DMsMessage` payload, so that a message can be sent with an already registered
file and then the actual file contents are sent.

#### Response

```
0-3  :  AttachmentID
```

- **AttachmentID**: a new `AttachmentID` (provided by the server) associated with the
  file to upload

#### Chunk

```
0-3  :  Chunk index
---  :  Raw file data
```

- **Chunk index**: the number of the chunk in the sequence. This is used to ensure that
  all chunks have arrived and helps sorting them
- **Raw file data**: the raw data of the file to download. Note that the last chunk
  may not contain the same amount of data bytes as all the other ones, because the file
  size may not be a multiple of the chosen chunk size

Chunk packets keep the same **packet ID** field as their parent request packet.

The last chunk packet will expect an acknowledgement signifying that the server was able
to receive and reconstruct the file contents. If a negative acknowledgement is received,
the packet transmitting it must be a `DownloadChunks` packet, asking for one or more
file chunks to be transmitted again.

### `UploadChunks`

```
---  :  Chunk indeces
```

- **Chunk indeces**: a variadic number of chunk indeces (32bit unsigned integers)
  that the client must resend with a `Download` *chunk* packet

Again, the last chunk expects an acknowledgement, and if a negative one is sent, this
whole process repeats.
