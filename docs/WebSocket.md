
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

| Name             | Description                                         |
| ---------------- | --------------------------------------------------- |
| `Auth`           | Authenticate a user                                 |
| `Deauth`         | Deauthenticate a user                               |
| `DM`             | Send a private message to a user                    |
| `Download`       | Download a file from the server                     |
| `DownloadChunks` | Download a specific series of chunks from a file    |
| `InfoChat`       | Request information about a public chat             |
| `InfoUser`       | Request information about a user                    |
| `ManageFriend`   | Manage a friend request                             |
| `Send`           | Send a public message in a chat                     |
| `Update`         | Send new session information like incoming messages |
| `Upload`         | Upload a file to the server                         |
| `UploadChunks`   | Upload a specific series of chunks from a file      |

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
A negative acknowledgement means that the file was not able to be written to disk.

#### Response

The response only contains an acknowledgement.

If a negative acknowledgement is sent, the sender of the request packet must resend
again the same packet, also keeping the same **sequence ID**.

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
  that the server must resend with a `Download` *chunk* packet

Again, the last chunk expects an acknowledgement, and if a negative one is sent, this
whole process repeats.

### `InfoChat`

```
0-1  :  UserID
2-5  :  ChatID
```

- **UserID**: the UserID of the user that is requesting the information
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
0-1  :  UserID
2-3  :  Target UserID
```

- **UserID**: the UserID of the user that is requesting the information
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
0-1  :  UserID
2-3  :  Target UserID
4    :  Action
5    :  Padding null byte
```

- **Sender UserID**: the `UserID` of the user sending this packet
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
again the same packet, also keeping the same **sequence ID**.

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
- **Friend requests count**: the number of new friend request data (accepted or sent)
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
  or it can be `1` if the target user sent a friend request to the user receiving this
  packet

The **message data** is divided in chunks like this:

```
0-3  :  ChatID
4-5  :  Message count
---  :  DMsMessage data
```

- **ChatID**: the `ChatID` of the chat the messages were sent in
- **Message count**: the number of messages in this chunk
- **DMsMessage data**: a variadic number of serialized `DMsMessage` objects

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

Chunk packets keep the same **sequence ID** field as their parent request packet.

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
