
# Todo

## Global:

- Change the `append_*` functions for binary formats to align
  data before writing to follow the serialization boundary sanitization
  mentioned earlier
- Make `normalize()` methods on all binary structure formats to allow
  not serializing to a string but still fixing internal state, then
  call that method when serializing to same time
- Give better defaults to metadata fields
