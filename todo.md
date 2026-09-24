
# Todo

## Global:

- Update serialization logic across *all* binary formats to syncronize metadata
  across different child structs (like with `DMsMessage` or `UserFile`) to avoid
  broken invariants and allowing the caller to avoid doing extra complex encoding
  calculations
- Create tests for `User.hpp` and also `Time.hpp` if possible
- Create more intensive tests for every binary format

## `src/headers/Common.hpp`:

- Make `network_byte_order()` constrained on `std::integral` types and then specialize
  the template in *each* individual header that defines some binary format to allow
  converting individual struct fields while keeping a clean API

## `src/headers/DMs.hpp`:

- Make `derive_context_str()` accept a template or function parameter of type `char`
  that replaces the interpolation character between strings

## `docs/WebSocket.md`:

- Finish the specification with the rest of the packets
