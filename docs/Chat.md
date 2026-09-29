
# Chat

Every chat file in ControlZ uses a custom binary format, whose specification will
be covered in this file.

## Format header

The file format header is composed of 14 bytes:

```
0-3    :  Magic bytes "CHAT"
4-7    :  ChatID
8      :  Metadata bitmask
9      :  Name length
10-11  :  Description length
12-13  :  Members list length
```

- **Magic bytes**: a simple identifier for the format
- **ChatID**: a 32bit unsigned integer holding a unique identifier for this chat
- **Metadata bitmask**: a bitmask holding metadata about the chat
- **Name length**: the length in bytes of the **name** field
- **Description length**: the length in bytes of the **description** field
- **Members list length**: the length in bytes of the **members list** field

After this header, a variadic data section follows:

```
---  :  Name
---  :  Description
---  :  Members list
-32  :  Argon2id password salt
-32  :  Argon2id password hash
```

- **Name**: the name of the chat. It can be 40 characters at most
- **Description**: the description of the chat. It can be 4000 characters at most
- **Members list**: a variadic number of `UserID`s corresponding to the members
  of the chat
- **Argon2id password salt**: the salt value used in the Argon2id hashing algorithm
  for the password
- **Argon2id password hash**: the password hashed with Argon2id. This and the
  previous fields are optional

This is the structure of the **metadata bitmask** field:

```
7 6 5 4 3 2 1 0
---------------
v v v x p n d m

v  :  Version number
p  :  Requires password
n  :  Name flag
d  :  Description flag
m  :  Members list flag
x  :  Reserved for future use
```

- **Version number**: gets incremented for every revision of the format
- **Requires password**: flags whether this chat requires a password to join
- **Name flag**: flags whether users that are not chat members can view this
  chat's name
- **Description flag**: flags whether users that are not chat members can view
  this chat's description
- **Members list flag**: flags whether users that are not chat members can
  view this chat's members list
