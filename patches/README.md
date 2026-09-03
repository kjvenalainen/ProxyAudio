# Local submodule patches

This directory records small, project-specific changes to git submodules without maintaining a fork. The ProxyAudio CMake build invokes [`apply-patches.sh`](apply-patches.sh) for libASPL before it is built.

The script processes every `*.patch` file in the matching subdirectory. It applies a patch when possible, skips a patch that is already present, and warns if the patch no longer matches the checked-out submodule.

## Layout

```text
patches/
├── apply-patches.sh
└── libASPL/
    ├── README.md
    └── *.patch
```

## Add or update a patch

Work from a clean submodule checkout so the patch contains only the intended change:

```bash
cd libASPL
git status --short
# make the focused change
git diff --binary > ../patches/libASPL/descriptive-name.patch
git diff --check
```

Review the patch and document its purpose in `patches/libASPL/README.md`. Do not discard unrelated or uncommitted submodule work to generate a patch.

To test one patch before a full build:

```bash
cd libASPL
git apply --check ../patches/libASPL/patch-name.patch
```

If that check succeeds, `git apply ../patches/libASPL/patch-name.patch` applies it manually. To check whether it is already applied, use `git apply --check --reverse` with the same patch.

## When libASPL changes

After updating the submodule revision, run a build. A warning that a patch cannot be applied means its context no longer matches. Recreate the patch against the new revision, verify it with `git apply --check`, and update the patch-specific README. Prefer upstreaming generally useful fixes; remove the local patch only after the submodule revision contains the upstream change.

## Troubleshooting

`git status --short` inside the submodule is the first check when patch behavior is unexpected. Existing local changes can overlap a patch, while a patch that fails both forward and reverse checks is incompatible with the current submodule revision.
