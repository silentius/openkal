---
name: existing-project-conventions
description: Resolve OpenKal naming mismatches and interpret master as the existing main/default branch.
---

# Existing project conventions

## Main branch alias

In branch and delivery requests, `master` always means the repository's existing
main/default branch, currently `main`. Apply this project alias without asking for clarification or creating or renaming a
branch to match `master`. Follow the normal project delivery workflow.

## Other naming discrepancies

When a requested path, filename, identifier or spelling differs from an existing
project name, check the actual name and its current uses first. Small differences
such as a missing letter, singular versus plural, or capitalization can be human
typing mistakes rather than an intentional change.

Point out the discrepancy and ask a concise question before creating an
alternative, renaming the existing name, or adapting code and packaging to support
the alternative. Do not silently copy the discrepancy into the implementation,
and do not silently reinterpret a meaningful naming choice either. Continue work
that does not depend on the answer while clarification is pending.

For example, when the project already uses `RuntimeData/sounds` and a request
says `RuntimeData/sound`, ask whether the user means the existing `sounds`
directory. An explicit, confirmed rename remains authoritative and needs no
repeated confirmation. Ordinary prose typos that do not affect implementation
names do not require clarification.
