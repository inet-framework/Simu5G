# Refactoring

## Workflow

- SMALL STEPS: For larger refactorings, I prefer to have a sequence of SMALL,
  safe, straightforward, but compiling and testable changes (as git commits)
  that are easy to review. If changes are too big (e.g. multiple steps combined
  into one), they are difficult to review, and also to troubleshoot/debug if
  something breaks. (The user can squash commits manually later, but slicing up
  commits later is much harder and often impossible.)

- WHEN UNSURE, ASK! Stopping and letting the user clarify or give clearance
  is 100 times better than pushing through with potentially false assumptions.
  Pushing through without having complete understanding is a big NO-NO!

- Do not change anything in the code, or do actions that were not explicitly
  requested without getting confirmation from the user first.

- EXTREME CAUTION: When seeing anything suspicious (something that suggests that
  initial assumptions or the refactoring plan might not be completely right),
  STOP and REPORT this to the user. Pushing through is a big NO-NO!!!

- In accordance with the above: Always break large refactorings into small,
  independently testable steps. Commit after each step (run `git commit`, without
  asking for permission). After commit, compile and run the fingerprint tests. If
  there are compile errors, fix all, then commit (!) and try again. Run
  fingerprint tests after each step, and only proceed if ALL fingerprints pass.
  If anything goes wrong, STOP immediately and do NOT push through!

## Git Commits

- **NEVER use `git add -A` or `git commit -a`.** The repo may contain unrelated
  untracked files that must not be included in commits.
- To commit, use: `git add -u` (stages only already-tracked changed files),
  then `git add <file>` for any new files YOU created, then `git commit -m "..."`.

## File Operations
- Use `git mv` for file renames to preserve history
- Update all `#include` statements when renaming files
- Update header guards to match new file names
- Check for all references using grep before renaming
