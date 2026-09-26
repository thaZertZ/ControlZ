
# User binary format

In ControlZ, user data is stored in a custom binary format, whose specification
will be covered here.

## Format header

The header is composed of 80 bytes:

```
0-2    :  Magic bytes "USR"
3      :  Padding null byte
4-5    :  User ID
6-7    :  User metadata and policies
8-11   :  Registration timestamp
12-43  :  Argon2id password salt
44-75  :  Argon2id password hash
76     :  Username length
77     :  Padding null byte
78-79  :  Bio length
```

And this variadic data follows:

```
---    :  Username
---    :  Bio
---    :  Friends list
```

- **Magic bytes**: an identifier for the binary format
- **User metadata and policies**: a bitmask containing information about
  the user and how other users are related to it
- **Registration timestamp**: a timestamp in seconds from a custom epoch on
  1/1/2027 at 00:00 UTC at which the user was registered
- **Argon2id password salt**: the salt value used in the Argon2id hashing algorithm
  for the password
- **Argon2id password hash**: the password hashed with Argon2id
- **Username length**: the number of bytes that the **username** payload occupies
- **Bio length**: the number of bytes that the **bio** payload occupies
- **Username**: a string containing the user's chosen name
- **Bio**: a string containing the user's biography
- **Friends list**: a variadic number of `UserID`s corresponding to the users
  that this user is friend of

This is the structure of the **user metadata and policies** field:

```
F E D C B A 9 8 7 6 5 4 3 2 1 0
-------------------------------
v v v m m i i r r s s x x x x x

v  :  Version number
m  :  Moderation level
i  :  Bio policy
r  :  Friends list policy
s  :  User status
x  :  Reserved for future use
```

- **Version number**: gets incremented on every new revision of the format
- **Moderation level**: indicate the level of privilege this user has. `00` =
  regular user, `01` = local moderator, `10` = global moderator, `11` = administrator
- **Bio policy**: indicate which users should be able to view this user's bio.
  `00` = no one, `01` = friends, `10` = mutual chat members, `11` = anyone
- **Friends list policy**: indicate which users should be able to view this user's
  friend list. The values are the same as the **bio policy** field
- **User status**: indicate the status of this user's account. `00` = active, `01` =
  warned, `10` = suspended, `11` = banned
