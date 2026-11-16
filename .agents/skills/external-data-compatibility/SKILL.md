---
name: external-data-compatibility
description: Keep optional user-supplied game data separate from the OpenKal source distribution.
---

# External data compatibility

OpenKal must build and run its project-owned tests without a sibling data
repository. User-supplied game data is optional runtime input and is not part of
the source distribution.

When implementing a data reader:

1. Work from public documentation or an independently written format
   specification.
2. Add synthetic, project-owned fixtures that exercise the reader.
3. Do not copy names, comments, tables, or configuration values from external
   data packs into the repository unless their license explicitly permits it.
4. Keep local data locations in ignored runtime profiles. Never commit absolute
   paths or assume a particular sibling directory.
5. Record the source and license of any redistributable sample data.
