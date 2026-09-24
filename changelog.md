
# Changelog

## `src/headers/Envelope.hpp`:

- Changed the `timestamps` member of the `DMsMessageReplies` struct to be
  an `std::vector` instead of a manuall RAII heap-allocated array
