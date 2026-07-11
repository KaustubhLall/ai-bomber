# Autonomous overnight brick execution — 2026-07-11

Single consolidated log for the ~10-hour autonomous session starting 2026-07-11 ~09:35
UTC ("your 10 hours starts now"), covering execution of the post-audit brick plan
(KL-101 through KL-109, all children of KL-96). Read this file top-to-bottom for the
whole night's narrative; it links out to Linear issues and commits for detail rather
than duplicating them.

**Purpose of this file specifically:** the user asked to "document everything you
assume explicitly for review later tomorrow morning as a whole." Linear issues carry
the per-brick technical record (kept up to date as each brick lands); this file is the
narrative spine — what got worked in what order, every assumption/judgment call made
without asking, and why — for a single morning read rather than reconstructing intent
from scattered commits.

## Standing constraints (from the user, carried through the whole session)

- Preserve experimental integrity. Do not disturb an active training iteration. Never
  run two `bomber_alphazero_native.exe` processes concurrently (this crashed training
  twice earlier in the broader session — see `07-grokking-campaign.md`).
- Goal is **defensible combat competence**, not a "superhuman"/"champion" claim. No
  claim ships without clearing KL-100's checklist.
- Brick-by-brick, one focused commit per completed brick/slice. Don't mix semantic and
  performance-only changes.
- Verification vocabulary (from `General Preferences.md`): Diff hygiene / Compiled /
  CTest / Replay smoke-tested / Paired-evaluated / Live-tested / Deferred intentionally.
- Advisor tool: **confirmed unavailable this session** (errored both when first tried
  and again after the user's explicit "use... the advisor model" instruction — see
  timeline below). Proceeding on my own judgment plus the very detailed instructions
  already given in this conversation. Flagging this once, here, rather than repeating
  it at every decision point.

## Timeline

### Before this file existed (context carried in from the live conversation)

- Overnight campaign (previous sub-session, same calendar run): diagnosed and fixed
  the "crush-cheese" exploit (v5's reported superhuman result was 95-97% arena-crush
  wins, not bomb-kills). Reward reshaping + opponent league implemented, `v6-league`
  launched. Full writeup: `07-grokking-campaign.md` Part 2.
- KL-97 iter-100 gate run: **failed** (vs MCTS-256, 24W-6D-34L, bomb-kill 3.1% of
  games, WAIT 75.0% — worse than the iteration-50 read). v6-league stopped at
  iteration 102, `crush01` branched (reward lever 1: `arena-crush-win-value` 0.3→0.1).
- `crush01` trained to iteration 134 before being stopped for this brick plan (target
  was "first safe snapshot at/after 130"; `iteration_000130.pt` preserved intact — see
  KL-98 for the exact deviation and why: mandatory context-reading + a stop-mechanism
  back-and-forth took longer than the run kept training). A gate-eval run on
  `iteration_000130.pt` completed with an encouraging-looking score (0.4→0.6 vs MCTS)
  that **turned out to be invalid** — see next entry.
- User delivered the full 7-brick plan (KL-101–KL-109) with a specific root cause
  already identified: the gate-eval script never passed `--arena-crush-win-value`, so
  it silently evaluated crush01's 0.1-trained checkpoint at the trainer's struct
  default of 0.3. This is why the 0.6 score is not trustworthy.

### Brick 1, Part A+B — semantic-manifest inheritance + LR schedule pinning

Commit `7f69250`. Full detail in KL-101's "Progress 2026-07-11" section. Summary:

- Confirmed both root causes directly in code before fixing: `runtime_config_signature()`
  never included reward/league/mechanics fields at all (not even soft-warned);
  `learning_rate()` re-derives its cosine horizon from `iterations * train_steps`
  whenever `learning_rate_schedule_updates == 0`, so extending `--iterations` on any
  resume silently changes the horizon against already-accumulated `global_updates`.
- Fix: semantic manifest persisted in every checkpoint, inherited into the live config
  by default on load; explicit CLI overrides produce a logged fork, never a silent
  substitution. Schedule horizon resolved once, pinned thereafter. Pre-fix checkpoints
  fail closed unless `--legacy-accept-unverified-semantics` is passed.
- Verified: 2 new CTest regressions (chained, same pattern as the existing
  smoke/resume/gate chain) prove both fixes. Full native CTest 31/31, dependency-free
  CTest 22/22. Legacy fail-closed + opt-in-with-warning paths manually verified against
  a real pre-fix checkpoint (v6-league `iteration_000100.pt`).
- **Assumption made, not asked:** did not retroactively rewrite crush01's own
  provenance record (it was forked via ad-hoc copy, before Part C's explicit fork
  mechanism existed). Treated as a historical/legacy fork; the bugfix from Part C
  (below) still applies to it going forward since it's a pure code fix, not a
  run-dir-specific patch.

### Brick 1, Part C — fork/checkpoint provenance (in progress as this file is created)

Root cause of the "iter-110 fake initial promotion" bug found by reading the actual
promotion-gate code (`trainer.cpp` ~line 2091): `const bool has_incumbent =
std::filesystem::exists(best_path);` — a pure file-existence check on `best.pt`
specifically. crush01 only had `latest.pt` copied into its run directory (not
`best.pt`), so at its first evaluation-interval iteration (110), `has_incumbent` read
false despite the loaded checkpoint's own inherited `best_iteration`/`best_score`
correctly showing 20+ prior promotions worth of real lineage — a genuine logical
inconsistency between the numeric state (real champion exists) and the file-existence
check (no incumbent found), and the promotion gate believed the file check.

*(This section is being filled in live; see the rest of this file / KL-101 for the
completed fix, or if you're reading this before it's been updated further, the fix
was still in progress at last-save time — check `git log` on
`src/training/native/trainer.cpp` for the actual landed state.)*
