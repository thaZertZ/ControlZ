
# `SequenceID`s

**Packet ID**s correlate packets that belong to the same request, response, retry, or
fragment/chunk sequence. IDs are scoped to one WebSocket connection and one sending
direction. The client allocates IDs for client-to-server sequences; the server
independently allocates IDs for server-to-client sequences. Both directions may use
the same numeric ID at the same time without colliding.

## Sender allocation

Each endpoint owns an allocator for the IDs it sends. Its state is a 65536-bit
availability bitset:

```
0-65535  :  Packet ID availability bitset
```

At bit index `i`, `0` means ID `i` is available and `1` means it is reserved by an
active outgoing sequence. ID `0` is permanently reserved for `None`, `Explode`, and
`Heartbeat`, and its initialized to `1`. There are therefore 65535 allocatable IDs.

To start a sequence, the sender selects the smallest available nonzero ID, marks it
reserved, records the sequence in its pending-outgoing table, and only then queues the
packet for transmission. Reserving and recording must be one atomic operation.  
A single connection event loop is the preferred way to serialize these state changes.  
If multiple threads can send on one connection, protect allocation and registration
with a lock. Do not allow two threads to reserve the same bit.

The sender keeps an ID reserved until its sequence receives a terminal response:

- A definitive ACK completes the sequence and releases its ID.
- A NACK that reports final failure completes the sequence and releases its ID.
- A NACK that explicitly requests a retry is nonterminal. Keep the ID reserved and
  retry the same logical packet/sequence with the same ID as specified by its packet type.
- For a fragmented or chunked transfer, keep the ID reserved until the transfer's
  defined terminal ACK or final NACK. All fragments/chunks in that sequence retain
  the sequence's ID.

An endpoint must not retransmit a request merely because a local timer expired.  
If the expected terminal response does not arrive, the implementation may report a
timeout and close the connection. Closing resets the ID tracking bitset, so a late response
from the old connection cannot be mistaken for a response on the new one.  
Retry is performed only when a received NACK explicitly requests it.

## Receiver tracking and response matching

The receiver does **not** mirror or modify the sender's availability bitset. It keeps
separate inbound transaction state for packets from the peer.  
A request that expects a response, or starts a fragment/chunk sequence, is identified
by its sending direction and `Packet ID`.  
The receiver records its type, relevant flags, and request contents
while that sequence is active.  
A one-way, unfragmented packet with no expected response is processed once and does
not remain in the inbound transaction table.

Use `Response ID` to match an ACK or NACK to the corresponding outgoing sequence.  
Do not decide whether an incoming packet is a new request or a response by checking
whether its `Packet ID` bit is set in a shared bitset.

When the receiver sends a retry-requesting NACK, it retains the inbound transaction
and marks it as waiting for the requested retry. The retry must use the same packet
ID and represent the same logical operation and payload.  
The receiver may retry the operation, but must not execute it twice concurrently.  
A reuse of an active ID that was not requested by a retry NACK, or a retry with
different packet contents, is a protocol error, and the receiver should report it
and close the connection.

When the receiver sends a terminal ACK or NACK, it completes and removes the inbound
transaction. The sender releases its corresponding outgoing ID when it receives that
terminal response.  
A terminal response is sent once for each attempt, and the sender does not independently
retry on timeout. This rule, together with ordered WebSocket delivery, prevents an old
attempt's response from being confused with a later reuse of the same ID.

All state transitions for one connection must be processed in WebSocket message order.  
If application work is dispatched to other threads, completion results must
be returned to the connection's serialized state handler before a response is sent.

## Untracked packets and response fields

`None`, `Explode`, and `Heartbeat` always use **packet ID** `0` and do not reserve an ID.
They do not start a tracked sequence. A `None` ACK/NACK may still carry a nonzero
`Response ID` naming the request it answers.  
For a packet that is not responding to or otherwise correlated with an earlier packet,
`Response ID` is `0`. When ACK/NACK or the response flag refers to an earlier packet,
`Response ID` is that packet's nonzero ID.

Other packets use a nonzero **packet ID**. A response with a payload has its own packet
ID in the responder's sending-direction namespace, and its `Response ID` names the
request it answers.  
If that response begins a chunked/fragmented sequence, its own **packet ID** remains
reserved for that sequence.  
An unfragmented response that starts no further acknowledged
sequence releases its own ID after the complete frame has been accepted by the WebSocket
send operation. The same release point applies to a one-way, unfragmented packet that
expects no response.

## Connection reset and exhaustion

On connection close, discard both local allocator state and all inbound transaction
state for that connection. A newly established WebSocket connection starts with a
fresh allocator, and IDs from the old connection have no meaning on it.

If an endpoint has no available nonzero IDs, it must not send another ordinary
packet using an active ID. It will report an ID-exhaustion error to its higher-level
caller and will send `Explode` with **packet ID** `0` to notify the peer. `Explode` is
not acknowledged. The endpoint then closes and reopens the connection before
starting new sequences. The receiver also reports the peer's exhaustion notification to
its higher-level caller and follows the connection-close rule.
