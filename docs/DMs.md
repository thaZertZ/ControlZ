
# DMs format specification

The DMs implementation is found in the `DMs.hpp` file under `src/headers`

The DMs binary format is how ControlZ stores multiple messages in one
file.  
To make everything streamlined and easy to process for even less
powerful or embedded hardware, it was decided not to use JSON or any other
kind of text serialization.

This format is essentially an on-disk linked list of dynamically-sized data,
and requires data to always be aligned to 2 bytes.

There are two versions of this format, the forward-linked version (default
for clients) and the backward-linked one (default for servers).

Currently this format does not mandate any insertion operations, but it
requires an append operation, a node linking type conversion (chaging
the file to link nodes in the opposite direction that they do currently),
and also a deserialization operation, which should include decryption.

## Format header

The file header for the DMs binary format is 14 bytes in size:

```
0-1  :  Magic bytes ['D', 'M']
2    :  Version number
3    :  Global configuration
4-5  :  Node count
6-13 :  Last node address
```

- **Magic bytes**: a simple identifier for the format
- **Version number**: gets incremented on every revision of the format
- **Global configuration**: a bitmask for information about the whole file:
```
7 6 5 4 3 2 1 0
---------------
x x x x x x l k

k  :  Zstd compression flag (not yet implemented)
l  :  Linking type (0 = forward and 1 = backward)
x  :  Reserved for future use
```
- **Node count**: a 16bit number of nodes (or messages) stored in this file
- **Last node address**: the 64bit absolute byte address of the last node in
  this file

## Node header

Every node's size is 12 bytes:

```
0-7  :  Encryption seed
8-11 :  Next/previous node offset
```

- **Encryption seed**: the 64bit seed used for the ***light*** message
  encryption process (see [encryption](#encryption))
- **Next/previous node offset**: when the file uses forward node linking,
  this value holds the byte offset to the next node, when backward node
  linking is used instead this holds the byte offset to the previous node

## Encryption

The kind of encryption implemented by this format ***is not safe for
production-grade systems*** as it is symmetrical and even a simple look at
the source code should be enough to write a decryption function.

The one and only reason that encryption is implemented is to provide even
a small protection against frequency-based data analysis.

Here the whole encryption process will be covered:

- Generate a random seed
- Generate a [context string](#context-strings)
- Derive a key from the two
- Encrypt data using the key

Generating a random seed can be done in a lot of ways, but the `Common.hpp`
header provides a `random_number` function to do that easily.  
The random seed is the only component of the encryption process that
is different for each node.

Generating a [context string](#context-strings) will be covered later, for
now just know that the `derive_context_string` function in `DMs.hpp` accepts
a variadic number of `const std::string&`s.

Deriving a key from the two can be easily done by calling `derive_key`
and passing the seed and the context string as arguments.

Encrypting data with it is also done with a simple function, called
`encrypt_decrypt` (signifying that it can be used for both actions) that
accepts the data to encrypt and the key to use as arguments.

## Context strings

A context string, used in the encryption process, is a combination
of text data related to the context in which the encrypted data will be
used.  
Usually this data is a list of usernames separated by a non-breaking
space ` `.

The server where the master DMs file is stored ***is required*** to hold
a copy of the context string, only if the file holds a public chat,
otherwise the two clients that are messaging each other should retain a
copy of the key.  
**Note**: when the server holds the file and the context string, clients
are also allowed to do so, but not the opposite.
