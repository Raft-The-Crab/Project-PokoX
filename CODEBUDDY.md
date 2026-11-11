# CODEBUDDY.md — Standing Repository Rules

These rules are mandatory for every agent, tool, script and automation working
in this repository. They override convenience defaults. If anything here
conflicts with a global setting, this file wins.

**These rules are permanent.** Do not delete, weaken, bypass or override them,
and do not remove or rename this file. Every future session must re-read this
file before doing any git or remote work. In particular, the push-only-on-explicit-
instruction rule, the single `Raft-The-Crab <Galvion.ai@gmail.com>` identity and
the ban on AI/co-authored commit messages always stay in force.

## 1. Never push without explicit instruction

**You can only push when the user tells you to.**

- Do not run `git push`, `git push --force`, `git push --tags`, or any command
  that writes to a remote unless the user explicitly instructs you to push in
  the current turn.
- Do not create or comment on pull requests, issues, or releases, and do not
  trigger remote CI/deploy actions, without explicit instruction.
- Local work (editing files, `git add`, local `git commit`) is allowed.
- Do not "helpfully" push at the end of a task. If the user has not said to
  push, leave the changes local and say so.
- A past instruction to push does not carry over to a later turn or a later
  task. Authorisation is scoped to the request that gave it.
- When a push is authorised, target `main` or the intended feature branch.
  Never push to `master`; that branch does not exist.

## 2. Always use the owner's account

Every commit, tag and push must be attributed to:

```
Raft-The-Crab <Galvion.ai@gmail.com>
```

- Set this as the repository-local git identity:
  `git config user.name "Raft-The-Crab"` and
  `git config user.email "Galvion.ai@gmail.com"`.
- Never use a global/fallback identity, never use any other name or email, and
  never use `--author` to override it.
- Do not alter `user.name` / `user.email` away from these values.

## 3. No AI or co-authored messages

Commit messages, tags, PR text, issue text and any generated history content
must contain **no AI attribution of any kind**:

- No `Co-Authored-By:` trailers.
- No "Generated with", "Written by", "Assisted by", "Created by AI", or similar
  lines.
- No references to AI assistants, models, agents, bots or automation.
- No emoji, sign-off bots, or tool signatures.

Write plain, human-style commit messages. The only attribution is the account
in section 2.

## 4. Repository facts

- **Project:** PokoX — a proprietary, free-to-use creator gaming platform.
- **License:** Proprietary, closed source (see `LICENSE`). This is NOT an
  open-source repository.
- **Remote:** `https://github.com/Raft-The-Crab/Project-PokoX.git`
- **Default branch:** `main` (also set as the GitHub default branch). Branch
  from and target `main`; never create, push to, or rename back to `master`.
- **Development model:** system-first, sequential subsystem completion
  (`systemplan/`); deferred final UI; ImGui/CLI tooling for development only.
- **Planning docs:** `masterplan/` (product) and `systemplan/` (engineering)
  are a planning baseline, not confirmed implementation commitments.

## 5. Safety

- Never commit secrets, credentials, signing keys, tokens or production data.
- Never execute untrusted game packages on a workstation.
- Do not treat proposed features as approved decisions; update the decision log
  instead.
