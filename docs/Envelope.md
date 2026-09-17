
# Envelope specification

The ControlZ message binary format is implemented in the `Envelope.hpp`
header under `src/headers`.

This format requires all data to be aligned to at least 2 bytes.  
Here the whole specification of the format will be covered.

## Format header

The header of this binary format is 8 bytes long:

```
0-3  :  Message timestamp in seconds from a custom epoch on 1/1/2027 at 00:00
4-5  :  UserID of the sender
6-7  :  Message metadata
```

These are the bits of the metadata field in detail, written in big-endian
representation:

```
F E D C B A 9 8 7 6 5 4 3 2 1 0
-------------------------------
t v v k k k r x z z z z z z z z

t  :  Edit
v  :  Version number
k  :  Attachment count
r  :  Reply flag
x  :  Payload length encoding
z  :  Payload length
```

- **Edit**: a value of `0` means that the message is a new message,
  a `1` means it is an edited message, in this case, the timestamp field
  of the header matches the timestamp of the message to edit
- **Version number**: a biased value ranging from `1` to `4` (respectively
  represented as `00` and `11`)
- **Attachment count**: the number of attachments tied to this message,
  ranging from `0` to `7` (see [attachments](#attachments))
- **Reply flag**: a value of `1` signifies that before the attachment payload
  and the message payload sits a special reply payload
- **Payload length encoding**: stores the encoding used to encode the message
  payload length: `0` uses the short encoding and `1` uses the long encoding,
  more on it [here](#payload-encoding)
- **Payload length**: the length of the message payload encoded according to
  the specified encoding

One might wonder though how it would be possible to delete a message, in
that case we leverage an invariant present in this format: with a message
type of `1` (edit), and every field past the version number equal to `0`,
we signal a deletion state where the timestamp in the message header is
used to represent the *original* timestamp of the message to be deleted.  
Such a state implies no reply data, no attachment data, and no message
data either after this header.

## Payload encoding

Since a message could contain a very small number of characters but also a
very large one, we need a way to store its length without occupying eccessive
space in memory but also without having to give up flexibility.

The two length encodings used by this format are the *short encoding* and
the *long encoding*.

### Short encoding

The message payload is serialized into chunks of 4
bytes, which are filled with null bytes if the actual payload requires
less bytes than a multiple of 4.  
This allows us to *quadruple* the number
we can represent in the size field: normally we would be able to store
numbers from `0` to `255`, but thanks to this encoding we can range from
`0` to `1020`.  
This trades a maximum of 3 null bytes of padding for being able to store
much larger sizes.

Therefore the calculation for converting a real length into a
*short-encoded* one is: `encoded = (real + 3) / 4`.  
Here the `+ 3` is used to round up so that the encoded value never
truncates characters like integer division would.

To convert back, the operation would be `real = encoded * 4`, but we would
have extra padding null bytes in our payload, which are often not a problem
at all, especially if working with C strings.

### Long encoding

This encoding scheme works the same as the *short* one, but instead of
`4` bytes, each block stores `8`, allowing us to range from `0` to `2040`.

The conversion operations should be straightforward now:  
- `encoded = (real + 7) / 8`  
- `real = encoded * 8`

## Other payloads

In this format, we don't only store messages, but we also store attachments
and replies.

### Attachments

An attachment payload is a contiguous sequence of however many instances
we specify (in the header) of the structure we are going to define,
of 6 bytes in size:

```
0-3  :  Attachment ID
4-5  :  File extension
```

- **Attachment ID**: the unique identifier associated with the attachment,
  provided by the server after registering the attachment, which is done
  before sending any message data
- **File extension**: an enumeration of all major file extensions,
  with a value of `0` reserved for telling a client to ask the server the
  extension of the stored file

### Replies

A reply payload is actually able to store multiple replies at once with a
dynamic structure, with a minimum size of 6 bytes:

```
0    :  Reply count
1    :  Padding null byte
2-5  :  First reply message timestamp
...  :  More message timestamps if specified
```

- **Reply count**: the number of messages being replied to, biased by `1`,
  meaning that a value of `0` actually encodes `1` and `255` encodes `256`
- **Message timestamps**: the timestamps of the messages being replied to
  (at least one is required)

## Payload order

In the final serialized format this should be the layout of all payloads:

```
[header] [replies] [attachments] [message]
```

Every payload after the header is optional, but at least one between
attachments and message data is required, unless deletion is the case.
