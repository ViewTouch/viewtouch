# Looking over a change before it ships

ViewTouch is worked on directly on `main`, so nobody has to approve a
change. That makes a second look before a release worth more, not less.
For a larger change (money, saving, syncing, updates, anything the root
helper does), ask a person or run `/code-review` in Claude Code on the
commits since the last release. Then go through this list.

## Money

- Is every amount in `Money` (whole cents), never a `double` past the
  screen? Percents in basis points or ppm, rounded once?
- Does a check still add up (`core::booksProblem`) after the change: a
  split, a merge, a refund, a void, a reopen?
- Is a closed check worked out with the rates it closed with (`closedRates`)?
- Does the day's report still equal its checks added up? (The property
  tests check this: `[properties]`.)

## Saving and syncing

- Is every change to the store's data saved through the sink (`s_->sink`),
  and does it survive a restart (a test that reloads it)?
- Does it reach the other screens (an `emit` of the right `...Changed`),
  and the standby?
- A new field: in `toJson` and `fromJson`, and read from an older store
  without it (`tests/fixtures/stores/`)?
- A database change: a new migration step, never an edit of an old one.

## Who may do it

- Does a new command check the permission on the store's computer
  (`require(perm::...)`), not just hide a button?
- Could a server do a manager's job with it? (The fuzzer tries:
  `[fuzz]`.)

## Threads

- Does anything run on a worker (`QThreadPool`, the writer, printers)?
  Then it takes copies, never `this` or references to the store's data,
  and reports back with a queued call. (ThreadSanitizer: Modern sanitizers.)

## The screens

- Every new phrase in `tr()`/`qsTr()`, translated in `i18n/` (CI checks).
- Does it fit at 1280 × 800 and on a phone, and once the words are longer
  (`[pseudo]`)?
- No QML warnings (the tests fail on one).

## Shipping

- The full suite passes here, and the CI jobs after the push.
- Each bug fixed has a test that fails without the fix.
- The manual and README say what changed, for the people using it.
- Before approving a release: the hardware checks in
  [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md).
