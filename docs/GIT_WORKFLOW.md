# Git / GitHub workflow

Established by the project owner's instructions on 2026-11-10. The aim is a truthful record of prototype → playtest → problem → change → verification, not contribution counts.

## Repository and history

Keep the existing repository location and simple `main` workflow. Inspect status, branches, remotes and recent history before Git actions. Preserve uncommitted files and existing commits. Do not reconstruct separate historical phase commits from the current combined snapshot, backdate commits, or describe untested tuning as accepted gameplay.

Use the existing remote URL and visibility. When no remote exists, obtain the owner's repository URL. Do not create repositories or change visibility without authorization. Never force-push or discard work with `reset --hard`. Stop and report merge conflicts, rejected pushes, divergent branches or mismatched remote history.

## Milestone checks

1. Complete one meaningful Gameplay, tuning, engineering or documentation stage.
2. Run relevant automated checks and the Native Demo; record actual outcomes and limitations. For Web stages, build and inspect the local browser version too.
3. Review `git status`, working diff, staged diff and untracked contents. Group only coherent changes. The first snapshot must be described as the current baseline, not as a fictional series of earlier implementations.
4. Check for secrets, accidental files, machine-specific settings, caches and generated binaries. If a suspected secret is found, stop commit/push and report its file/reason without revealing its value.
5. Record Problem / Hypothesis / Change / Result in `docs/ITERATIONS.md`. Automated correctness does not establish game feel; use **Pending playtest** until the owner confirms it.
6. Commit with a clear Conventional Commit message, verify status, then push the verified meaningful stage to the confirmed remote.

Avoid tiny parameter/typo commits and empty commits. Temporary experiments remain working-tree changes until a completed tuning iteration is confirmed. “这版可以了”, “这个阶段完成了” and “这个版本先保留” authorize this workflow. Report failed or unavailable verification instead of marking it PASS.

## Exclusions

Do not commit API keys, passwords, access tokens, cookies, credentials, `.env` files, private/local configuration, IDE state, downloaded toolchains, Emscripten caches, large temporary builds or unnecessary binaries. Formal deployable Web artifacts require a defined deployment plan; they are not automatically included by building locally.

The build scripts currently contain machine-specific tool paths; make those portable or move their overrides into ignored local configuration before the first source commit. Local environment inventories and path-bearing evidence need review/redaction before inclusion. Screenshots and sanitized test reports may be retained as purposeful evidence.

## Tags and reporting

Use tags only for real prototype milestones. Version examples in the owner's brief are suggestions, not instructions to fabricate missing versions. Propose a tag at a completed milestone; do not silently label a prototype as a portfolio release.

After a commit, report:

```text
【Git Commit】
Commit: <hash>
Message: <Conventional Commit message>
本次修改: <scope>
测试:
- Native: PASS / FAIL / unavailable, with evidence
- Web: PASS / FAIL / 未测试
- Gameplay: confirmed by owner / 待我试玩
GitHub:
- Push: 成功 / 失败 / 未执行, with reason
当前 Branch: <branch>
当前 Working Tree: clean / 有未提交修改
```

Before eventual public release, audit credentials/private paths/local configuration, code and docs, Native/Web builds, browser playability, deployment, history and tags. Public visibility remains the owner's decision.
