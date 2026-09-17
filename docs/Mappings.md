
# Mappings specification

ControlZ uses a custom binary format to map any kind of linearly-distributed
numerical input to a custom data structure.

This format is currently used to store the original filenames of attachments
based on their `AttachmentID`.

Fundamentally, it works by allocating pages of dynamic structures and
filling entries in nested tables with absolute addresses to these structures

## Format header

The header of this binary format is 4 bytes in size:

```
0-2  :  Magic bytes ['M', 'A', 'P']
3    :  Configuration bitmask
```

- **Magic bytes**: a simple identifier for the format
- **Configuration bimask**: a bitmask containing this information:
```
7 6 5 4 3 2 1 0
---------------
s s s s s u v v

v  :  Version number
u  :  Full flag
s  :  Last available slot of master table
```
- **Version number**: incremented every revision of the format
- **Full flag**: flag that gets turned on if the whole file's tables
  are full
- **Last available slot of master table**: a value holding the index into
  the top-level table which has free slots

## Tables

The master table is a table of 32 64bit addresses, adding up to
a total of 256 bytes.

Each slot in the master table contains the address of the page
corresponding to its index.

Each page holds a table of 256 64bit addresses, adding up to a
total of 2048 bytes.

Before the table, every page contains 2 bytes of information:

```
0  :  Magic byte 'T'
1  :  Last available slot of table
```

- **Magic byte**: the identifier for a page
- **Last available slot of table**: a value holding the index into the
  table which has free slots

Each address into this table refers to the object (which could be dynamic)
containing the mapping.
