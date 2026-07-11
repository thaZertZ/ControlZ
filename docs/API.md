
# API layout

```
/api/
    /ack
    /auth
    /deauth
    /dload
    /dm
    /info/
         /chat
         /user
    /send
    /upd
    /upl
```

## POST `/api/ack`

Acknowledgement request for risky operations

## POST `/api/auth`

Authentication endpoint used on session login.

The client has to supply the `UserID`, `Username` and an Argon2
hash of the input password, which can also be stored locally (on
the client's machine) to avoid login every time.

## POST `/api/deauth`

Deauthentication endpoint used on session logoff.

The client has to supply the `UserID` and `Username`.

## GET `/api/dload`

File download endpoint use *only* for attachments.

## POST `/api/dm`

Send a Direct Message to another user.

The client has to supply the `UserID` (aka `SenderID`) and
`RecipientID` (the `UserID` of the recipient).

## GET `/info/chat`

Retrieve metadata of a chat in JSON format.

The client has to supply the `ChatID` of the chat to get info about.

## GET `/info/user`

Retrieve metadata about a user in JSON format.

The client has to supply the `UserID` of the user to get info about.

## POST `/send`

Send a message into a public chat.

The client has to supply the `UserID`, `Username`, and `ChatID`.

## GET `/upd`

Receive user-specific session updates through polling.

The client has to supply the `UserID` and `Username`.

## POST `/upl`

Upload a file, used *only* for attachments.

The client has to supply the `UserID` and `Username`.

# Per-session server connections

Each user session will have a maximum of 2 connections to the server
simultaneously, one of which will always be WebSockets.  
The second connection will be used either for semi-permanent
WebSockets data exchange or regular HTTP requests, like the ones shown
above.

The main WebSockets connection is used by the server to send session
update information (preferred instead of using `/api/upd`) and by the
client to send messages into public chats or forward DMs to other users.

The second connection is opened as WebSockets if the load of incoming
data from the server is too much to keep a stable balance between
server-to-client and client-to-server data exchange on the first
connection.  
In that case, the second connection is used for server-to-client data
and the first is restricted to client-to-server data *or* requests
instead.

If the second connection is available and the client needs to perform
GET requests, that connection will be used with regular HTTP.
