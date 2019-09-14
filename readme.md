# rogitfs

Read-only FUSE filesystem for git repository exploration

## Requirements

* fuse3
* libgit2

## Mount

### Mount

```
./rogitfs mountpoint --repopath=/path/to/repository
```

### Unmount

```
fusermount3 -u mountpoint
```

## Structure

| Path     |    |
|----------|----|
| /commit  | Commit hash directories containing the commit directory structure |
| /obj     | Object hash files containing raw object data |
| /refs    | References to commits as symlinks |
| /inherit | Commit inheritance structure using symlinks |

## Licence

Copyright 2019, aw32

SPDX-License-Identifier: GPL-3.0-only
