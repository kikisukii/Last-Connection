# LAST CONNECTION working rules

Follow [docs/GIT_WORKFLOW.md](docs/GIT_WORKFLOW.md) for Git operations.

- Keep the repository in its existing project directory and continue on `main` unless the user chooses otherwise.
- Commit a verified, meaningful development stage, not every parameter experiment or typo. Do not manufacture earlier commits, dates, activity or playtest results.
- Before committing, inspect status, working/staged diffs, untracked candidates, secrets, machine-local configuration and build artifacts. Run relevant checks and distinguish automated results from user playtesting.
- Do not commit credentials, local-only configuration, caches or temporary binaries. Suspected secrets block commit/push until reported and resolved.
- Preserve all existing work and history. Never use `reset --hard`, force-push, or change repository visibility without explicit user authorization. Stop and report conflicts, rejected pushes or divergent histories.
- Use the existing remote without changing its URL. If no remote exists, ask the user for the repository URL; never create a repository unasked.
- Use Conventional Commits. After an authorized meaningful commit and passing checks, push when a verified remote is available. Report the commit hash/message, checks, push result, branch and working-tree state.
- Tags represent real milestones; propose/confirm a tag for the milestone, not individual edits.
- Record Problem / Hypothesis / Change / Result in `docs/ITERATIONS.md`. If human gameplay validation is outstanding, write **Pending playtest**.
- User phrases such as “这版可以了”, “这个阶段完成了” or “这个版本先保留” authorize checking, committing and syncing the completed stage. Temporary tuning remains uncommitted until a meaningful iteration is confirmed.
