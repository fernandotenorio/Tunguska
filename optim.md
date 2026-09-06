# Tunguska 2.1: code optimization opportunities

Reviewed 2026-09-05. Ranked by expected potential to improve playing strength through faster execution of the existing search. This is a code and executable inspection, not a measured Elo ranking. No candidate has been implemented or A/B tested; percentages and Elo gains would be guesses. For single-thread play, start with NNUE work and move selection. For SMP, removing shared hash-statistics increments may deserve first place.

Scope: engine code in `Tunguska/includes/Engine`, `Tunguska/includes/NNUE`, and `Tunguska/src`, plus the Visual Studio build settings and supplied executable. No `data_pipeline` code or NNUE weight headers were read. Search-policy fixes, network changes, and UCI compliance are outside this review. Locations below use the current source's line numbers or function names.

## 1. Fuse the NNUE feature updates into one pass per perspective (Done)

**Potential: high. Effort: small to medium. Preserves search and evaluation.**

Evidence: `src/NNUE/nnue_loader.cpp:55` (`update`) and `:85` (`updateUndo`). `HL_SIZE` is 1024. Each removed or added feature causes a separate traversal of a 2048-byte perspective accumulator. A normal quiet move performs four traversals across the two perspectives; undo repeats all four. Captures perform six each way, and castling eight.

Dispatch once on the change shape, then perform a single loop over neurons:

```cpp
// Conceptual quiet-move kernel, independently for each perspective:
a[j] = a[j] - removed[j] + added[j];
// Capture: subtract two rows, add one. Castling: subtract two, add two.
```

Use AVX2 wrapping 16-bit addition/subtraction, with feature-row pointers resolved outside the loop. Keep a generic fallback if needed. Fuse undo similarly as an initial standalone change.

The supplied executable **already auto-vectorizes** the current update loops: disassembly of `NNUEState::update` contains unrolled `vpsubw`/`vpaddw` and repeated accumulator loads/stores. Merely translating the existing loops to intrinsics misses the main opportunity. The benefit is fewer passes, stores, loop setups, and redundant accumulator loads.

For a quiet move plus undo, nominal array traffic falls from 48 KiB to 32 KiB: count accumulator reads, weight reads, and accumulator writes for both perspectives. This is instruction-level traffic, largely served by caches, **not** a claim of equivalent DRAM traffic or a 33% engine speedup. Validate every accumulator lane and the final evaluation, including promotions, en passant, and castling. Preserve wrapping arithmetic; do not introduce saturation.

## 2. Keep accumulators by ply; eliminate inverse updates, then materialize lazily (Done: pt1, pt2 regression not merged)

**Potential: high, but overlaps #1 and #3. Effort: medium. Preserves search with careful state handling.**

Evidence: `NNUEState` owns one accumulator (`includes/NNUE/nnue_loader.h`); both search loops apply `update` before recursion and `updateUndo` afterwards (`Search.cpp:320,410,664,669`).

Give each search thread an aligned accumulator stack. Compute a child directly from its parent with the fused kernel in #1. Undo then restores the parent index without reading weight rows or modifying accumulator lanes. Avoid a separate `memcpy` followed by updates: load the parent and store the completed child in one pass.

A 64-position stack occupies 256 KiB per thread, excluding metadata; account explicitly for root, maximum ply, and null-move indexing. This increases the working set relative to the current 4 KiB accumulator and can affect cache behavior, so benchmark the actual engine.

Next, record feature deltas and a computed/dirty marker, materializing only when `evaluate` needs the child. Currently a child may return through repetition, a TT cutoff, or another path without evaluating, although the parent has already updated both accumulators. Pending chains must be applied from a valid ancestor; branching and re-search must never consume an accumulator belonging to an earlier sibling. Null moves change the evaluation perspective but not piece features.

Implement direct child construction first and lazy materialization separately. Bitwise compare against full feature reconstruction through random legal sequences and complete undo sequences. Measure update counts and evaluated nodes as well as time.

## 3. Move NNUE updates below the existing move-pruning tests (Done)

**Potential: medium to high. Effort: small. Best early low-risk patch.**

Evidence: `Search.cpp:320` updates the accumulator before the LMP and futility checks at `:328` and `:347`. Both prune paths immediately run `updateUndo` and undo the board. Neither test uses the child accumulator.

Keep `makeMove`, the legality result, `legalMovesCount`, and every pruning condition exactly where they are semantically. Move `nnue_state.update(changes)` to just before the first recursive search. Pruned moves then need only board undo. This removes an entire update/undo pair for each such move without altering which moves are pruned.

Initially retain feature extraction before `makeMove`: it needs the parent board. Do not simply move `moveDiffFeatures` below `makeMove`, and do not move pruning ahead of legality counting. Preserve the current killer-table indices and conditions even if they look questionable; correcting them is a separate search change. Lazy accumulators in #2 eventually subsume this benefit.

## 4. Select the next best move on demand instead of fully sorting (Done)

**Potential: medium to high. Effort: small to medium. Can preserve the exact move order.**

Evidence: `Search.cpp:522` scores all moves, then selection-sorts the complete list before either search loop examines its first move. For 40 moves this performs 780 score comparisons even when the first move cuts off.

Keep the initial score snapshot and perform exactly one selection-sort step immediately before consuming each move. At a first-move cutoff, only 39 comparisons are needed. If every move is consumed, the comparison count remains unchanged. This saves sorting work, not initial scoring or SEE work.

Preserve the strict `>` comparison, first-maximum choice, and swap behavior. Selection sort is not stable; replacing it with a stable sort or another tie policy changes the searched tree. Preserve the consumed prefix because the history-penalty loop reads it. Do not rescore the suffix after recursive searches: histories and countermoves can change during recursion.

Workers currently rotate the fully sorted root prefix. The simplest exact implementation retains eager sorting at worker roots while using lazy selection elsewhere. A later implementation can materialize the required sorted prefix before applying the same rotation. Compare emitted move sequences, including tied scores, and fixed-depth single-thread nodes/PVs.

## 5. Remove shared atomic TT diagnostic counters from the search hot path (Done, not merged)

**Potential: medium on one thread, potentially high on many threads. Effort: very small.**

Evidence: `HashTable.cpp:71,127,129` increments `hit`, `newWrite`, or `overWrite` using `fetch_add`. In the inspected engine these statistics are not used to choose moves, replace entries, or print search information. The executable contains `lock inc` at the matching probe/store paths (for example, virtual addresses `0x14000D35C` and `0x14000798D`).

Compile the increments out of production builds, or collect diagnostics per thread and aggregate after search. Relaxed ordering does not remove the atomic read-modify-write or the shared cache-line ownership contention. Several adjacent counters are in the same small object, so different counters can contend too.

Do not remove atomics from actual TT entries. Their synchronization/checksum scheme is separate from these dispensable statistics. Compare at 1, 2, 4, and the intended tournament thread count; SMP tree differences prevent expecting identical node counts there.

## 6. Reuse SEE results between move scoring and quiescence pruning (Done, regression not merged)

**Potential: medium. Effort: small to medium.**

Evidence: `scoreMove` calls `isBadCapture` for risky captures (`Search.cpp:474` onward); the quiescence loop later calls `see` again for non-promoting, non-EP captures. Both operate on the same restored parent position and only need to know whether SEE is negative.

Store a per-move tri-state result alongside the score: unknown, negative, or nonnegative. Reuse a known result during quiescence; compute unknown results only after delta pruning allows the move to reach SEE. Carry this metadata through selection-sort swaps.

An inexpensive-looking capture was not evaluated by SEE during scoring, so its high move score is not sufficient evidence to skip the later SEE call. EP and promotion exemptions must remain exactly as today. Do not replace quiescence's SEE with the risky-capture shortcut: that would change pruning.

## 7. Remove duplicated slider calculations inside SEE (Done, apparent regression not merged)

**Potential: medium. Effort: small to medium.**

Evidence: `Search.cpp:719` initializes attackers by calling `MoveGen::attackers_to` for both colors. Each call performs a bishop and rook lookup with identical target and occupancy. `considerXrays` also reconstructs the same bishop/queen and rook/queen unions during the exchange sequence.

Build both-color attackers in one helper: use each color's pawn attack mask, combined knight and king sets, and one bishop plus one rook lookup intersected with combined slider sets. Hoist the slider unions outside the SEE exchange loop and pass them to the x-ray helper.

Preserve the existing occupancy filtering, least-valuable-attacker selection, and exchange termination. A direction-specific x-ray refresh is a further option: a diagonally aligned departing attacker cannot uncover a rook ray, and vice versa. Test this independently against the current SEE over a large capture corpus. A generic threshold-SEE rewrite is a larger follow-up and must reproduce the existing sign test, rather than quietly fixing/changing SEE behavior.

## 8. Train a profile-guided release build

**Potential: medium, uncertain until measured. Effort: small source impact, moderate build work.**

Evidence: `Tunguska.vcxproj` already requests maximum-speed optimization, AVX2, `NDEBUG`, and whole-program optimization for x64 Release. No PGO configuration is present in the project file; imported settings or the historical binary build could differ.

Train an instrumented build on representative searches, then produce a profile-optimized executable. Use openings, tactical middlegames, quiet middlegames, and endings, with separate held-out timing positions. Search profiles can inform inlining, branch layout, and code placement in the large move/search functions. Perft alone is not representative of NNUE, TT, or search-pruning costs.

Microsoft documents `/LTCG` with `/GENPROFILE` and `/USEPROFILE` for this workflow: [profile-guided optimizations](https://learn.microsoft.com/en-us/cpp/build/profile-guided-optimizations?view=msvc-170). Keep this separate from source patches so gains and regressions are attributable. Do not advertise enabling `/O2`, AVX2, or whole-program optimization as new wins: they are already requested.

## 9. Compact the magic attack tables without changing the magic algorithm

**Potential: medium; depends on caches and CPU. Effort: medium.**

Evidence: `includes/Engine/Magic.h:11` reserves `64 * 4096` rook entries and `64 * 512` bishop entries, although `Magic.cpp` uses per-square relevant-bit counts. The declared attack storage is 2.25 MiB.

Allocate each square exactly `1 << relevantBits` entries in flat arrays. With the current ordinary rook/bishop occupancy masks, the totals are 102400 rook entries and 5248 bishop entries: 841 KiB combined. Preserve existing multiplication, shifts, and attack values; each square needs an offset or pointer. Unused space in the rectangular layout is not all actively accessed, so the declared-size reduction overstates the reduction in the active cache working set. Packing still improves page footprint and placement.

Consider grouping mask, multiplier, shift, and base pointer per square, but measure the extra address load against cache/TLB savings. Exhaustively compare all masked occupancies to the original table and run perft. BMI2/PEXT is an optional hardware-specific experiment, not a prerequisite or universally faster replacement.

## 10. Avoid redundant NNUE evaluation for the same search position

**Potential: medium to low; depends on re-search frequency. Effort: medium.**

Evidence: `alphaBeta` and `quiescence` call the output layer whenever they need an evaluation. Aspiration retries, PVS re-searches, and null-move descendants can revisit an unchanged accumulator.

With #2's per-position state, cache the exact output evaluation and a validity flag. Reuse it only for the same position and perspective. A null move cannot assume `eval(opponent) == -eval(us)`: this network applies distinct output-weight halves and a bias. Cache the two perspectives separately if worthwhile. Reinitialize validity whenever a stack slot becomes a new sibling.

Keep this distinct from putting static evaluation in the TT, which would require changing the current packed entry layout. Measure cache hit rate; a validity check at every evaluation can lose if reuse is rare.

## 11. Improve output-layer instruction scheduling

**Potential: low to medium. Effort: small.**

Evidence: `nnue_loader.cpp:115` has a single `sum256`, updated for both perspectives every iteration. The binary retains that dependency chain: two `vpaddd` operations feed the same running sum in the loop near `0x140014344`.

Benchmark two independent sums for the perspectives and, separately, modest unrolling with multiple partial sums. Combine at the end. This may improve scheduling, but loads and multiply throughput may already dominate. Avoid enough unrolling to cause register spills or excessive code growth.

Preserve the integer result exactly. The existing division by constant `QA=255` is already lowered to multiply/shift arithmetic in the executable; manually replacing division or changing QA is not a speed opportunity of the same kind. Likewise, aligned versus unaligned load spelling alone is not evidence of a gain.

## 12. Remove material bookkeeping unused by the NNUE search

**Potential: low to medium, especially capture-heavy search. Effort: small.**

Evidence: `Board.cpp` updates `material[2]` on captures, EP, promotions, and undo. Neither active search nor NNUE evaluation reads this field. In the inspected engine, the consumers are initialization and perft's material restoration assertion; `Evaluation.cpp` contains piece values and an empty initializer, not a classical evaluator.

Remove the running totals from the production search representation, or make maintenance a compile-time diagnostic option. Preserve piece values: MVV-LVA, SEE, and pruning still use them. For debug verification, recompute material independently if desired rather than maintaining unused production state. Confirm any other build consumers before removing the public field; this review deliberately did not inspect training code.

## 13. Stop repeating alpha-beta entry bookkeeping at the quiescence boundary

**Potential: low to medium. Effort: small, with measurement care.**

Evidence: `Search.cpp:191-211` performs resource checks, draw detection, maximum-ply handling, and a node increment before dispatching to quiescence, which repeats much of that entry work at `:564` onward. Repetition scanning can therefore happen twice on the same position.

Factor the shared entry checks or provide an explicitly prechecked quiescence entry. Preserve the current handling of draw/max-ply cases and stopping. The current counters count an ordinary alpha-beta-to-quiescence transition twice. Removing one count can lower displayed NPS while making search faster, and it changes periodic time-check cadence if that cadence still uses the counter. For an isolated comparison, retain equivalent accounting/poll scheduling or compare elapsed time for identical completed searches and document the accounting change.

## 14. Make the common board move/undo path cheaper

**Potential: low to medium. Effort: small to medium.**

Evidence: `Board.cpp:129` and `undoMove` repeatedly clear the source bit and set the destination in the moving-piece and side bitboards; special-move branches precede the ordinary path. Castling hash keys are removed and added for every move even when rights are unchanged.

Candidate patches to measure separately:

- Compute `fromBit`, `toBit`, and their XOR once. For an ordinary move, XOR the combined mask into the moving-piece and own-side bitboards, where source occupancy and destination absence are established. Keep capture, promotion, EP, and castling cases explicit.
- Dispatch the common ordinary-move case before the rarer cases if profiling supports that layout. A promotion/EP/castle/pawn-jump classification can use one combined flag mask, but retain the current encodings and behavior.
- Save the old castle rights and update the castle hash only when the rights change, using the XOR of old/new castle keys. A branch may cost more than two hot loads on some CPUs; a no-rights fast path is another experiment.
- Replace redundant white-king/black-king branches with an equivalent piece-type test in the non-castling path.

The compiler can already simplify some expressions. Inspect generated code before crediting a source-level reduction. Require make/undo round-trip comparisons of mailbox, bitboards, key, counters, rights, and king squares, plus the full perft suite.

## 15. Share attacks between capture and quiet generation when generating both

**Potential: low to medium. Effort: medium if exact order is required.**

Evidence: `MoveGen::pseudoLegalMoves` calls separate capture and quiet generators. Both walk the same pieces and calculate the same slider attacks. Both recompute occupancy; pawn and leaper work is repeated too.

For full generation, calculate each piece's attacks once, emitting captures to one buffer and quiets to another, then concatenate in the original order. Keep the capture-only quiescence path specialized. The existing `rookMoves`/`bishopMoves`/etc. illustrate the combined attack calculation, but directly switching to them changes the generation sequence and therefore tie ordering in search.

This is an alternative direction to staged generation below: if captures often cut off, delaying quiet generation can be better than generating both together. Compare them independently, keeping move sets and order identical when claiming a pure execution-speed result. No move-generation rewrite is needed to pursue the higher-ranked items.

## 16. Add TT prefetch with enough useful work before the probe

**Potential: low to medium, hardware dependent. Effort: small.**

Evidence: the TT is directly indexed from `board.zKey` with a power-of-two mask; the default allocation is 256 MiB. `alphaBeta` probes after node-entry checks, and `makeMove` computes the child key before recursion.

Experiment with a read prefetch of the child's entry after the key is ready, ahead of NNUE work or other independent work. Avoid issuing one for every quiescence-only or pruned child; those paths may never probe the TT. A prefetch immediately before the load usually has little latency to hide. Lazy NNUE changes the available overlap, so retest after #2.

Keep entry contents, indexing, replacement rules, and synchronization unchanged. Measure full searches, including SMP where extra traffic can regress performance.

## 17. Simplify feature extraction and undo-state transport

**Potential: low individually. Effort: small to medium.**

Evidence: `FeatureExtractor.cpp:101` builds four feature arrays and counts before every attempted move, independently decoding information also needed by `Board::makeMove`. `get_indices` calls a switch mapping engine piece codes. `BoardState` has a handwritten copy constructor, and undo takes it by value.

Possible independent improvements:

- For known-valid piece codes, derive `nnue_piece = (piece >> 1) - 1 + 6 * (piece & 1)`, and flip its color directly, or use a compact lookup table. The compiler may already lower the switch well; compare assembly and timing.
- Produce compact deltas while making the board move, or retain a compact parent descriptor and expand only after legality/pruning succeeds. Castling encodes a flag/index in `from`, and EP captures on a different square; keep those cases explicit.
- Use a defaulted copy constructor where appropriate and pass undo state by `const BoardState&`, or fill a caller-owned undo slot. Check generated code: return-value optimization and whole-program optimization can already remove copies.
- Consider a smaller naturally aligned undo record after measuring its contribution. Preserve full halfmove-counter range and all restored fields; do not use packed, unaligned records just to save bytes.

The current hot-path feature arrays and move lists are already fixed-size stack storage. The vector-returning full feature extractor is not the per-node path; replacing its allocations will not improve search NPS.

## 18. Reuse check and pin information only where it actually saves work

**Potential: low for the simple patch; larger variants require care.**

Evidence: `MoveGen::isLegalMove` accepts `pinned` but calls `pinnedBB` again instead of using it. Its callers `legalMoves` and `HashTable::moveExists` already computed that mask. Use the argument. This mainly improves legal-move perft and PV validation: the recursive search uses `makeMove` legality checks, so this is not a major search-NPS opportunity by itself.

Separately, checked search nodes first call `isSquareAttacked`, then `getEvasions` calculates the checkers again through `attackers_to`. Passing a computed checker mask can avoid duplicate slider work. Computing the entire mask at every non-check node may cost more than the current short-circuit boolean query, so measure an appropriate interface.

An eventual fast legal-move path could skip post-move king-safety checks for established safe cases using pins, but it is beyond the simplest optimizations and needs exhaustive EP, king, and check-evasion validation. Do not assume the existing standalone legality helper is a drop-in replacement for the search's make-and-test path.

## 19. Reduce control/statistics cache contention beyond the TT counters

**Potential: low on one thread; potentially useful at larger thread counts.**

Evidence: `Search::totalNodes` is a shared atomic updated every 1024 local nodes, while `stopped` is read at node entry and after children. Batching is already present, so there is no atomic node increment on every node.

Place writable reporting counters on separate cache lines from frequently read stop/control state if their actual binary layout shares a line. Consider cache-line-separated per-thread reporting slots, aggregated by the reporting thread. Use sufficiently wide counters for long Windows runs: `long` is 32-bit here. That width change supports trustworthy measurement; it is not itself a speed gain.

Relaxed operations can be appropriate for independent reporting counters, but on x64 changing a stop load's memory-order spelling alone need not change its instruction. Do not remove synchronization or reduce stop responsiveness without an explicit design and validation. Benchmark after #5, which addresses the much more frequent shared writes.

## 20. Conditional or lower-return follow-ups

These are below the stronger candidates because their search-speed payoff is uncertain or their behavior-preserving implementation is less simple.

**Staged move generation/scoring.** Search a validated TT move before generating/scoring everything, then materialize later groups only if needed. This can save more than #4 but requires preserving promotion priority, good/bad capture placement, counter/killer duplicates, tie ordering, and history penalties. The existing split capture/quiet generators are useful building blocks. Conventional TT/capture/killer/quiet staging is not automatically equivalent to the current numeric scores. Treat order changes as search changes with separate match testing.

**Large-page TT allocation.** The 64–256 MiB tested/default TT makes TLB behavior a plausible target. Optional Windows large pages with ordinary-page fallback are worth measuring on the deployment machine. This entails OS privileges/allocation handling and is not the first simple patch. Keep the same byte budget and entry count when comparing. Do not change to clustered entries or a new replacement policy in the same experiment.

**Target-specific SIMD and instruction sets.** The binary already uses AVX2 evaluation and updates. Wider SIMD or a BMI2 attack implementation requires CPU-specific measurement and supported-CPU dispatch/builds. A smaller NNUE would change evaluation strength and is outside scope. Do not assume supported instructions are faster on every CPU.

**Inlining tiny helpers.** Move bitfield decoders, bit setters, Zobrist helpers, and magic accessors are defined in `.cpp` files. Header-visible definitions could help builds without effective link-time optimization. However, whole-program optimization is already requested, and inspected binary calls show NNUE routines remaining out of line while sampled tiny-helper names were absent. Verify actual residual calls before proposing widespread `forceinline`; forcing large search functions inline risks code bloat.

**Skip impossible slider tests.** `isSquareAttacked` invokes bishop/rook magic lookups even when the matching enemy slider union is empty. Testing the union first can help sparse endings, but adds branches in middlegames. The same idea applies to x-ray helpers and empty piece sets. Benchmark a representative mix.

**Thread lifecycle and PV extraction.** `UCI::parseGo` constructs workers and copies a board for each search, and delays worker `i` by `i * 5` ms. `getPVLine` regenerates/validates moves after each depth on every thread. A persistent worker pool and leaner PV validation can reduce per-move overhead, especially at short controls or many threads. At 40/15 these are lower priority than per-node work. Preserve heuristic reset behavior; changing the worker delay or root scheduling can change SMP search behavior. Worker PVs are used to choose their next iteration's bookkeeping, so deleting worker extraction is not an automatic safe equivalence.

## Evidence and validation plan

### Supplied binary observations

- Executable: `bin/x64/Release/Tunguska-2.1.exe`, 1,710,080 bytes.
- SHA-256: `B46C9A3746B2117BE1F2D87F5082739AE84C5F7768837BB1002D6D90BA56E7F9`.
- A fresh-process run with `Threads=1`, `Hash=64`, `position startpos`, `go depth 12` completed with `bestmove e2e4`, score `cp 59`, reported nodes `340992`, and reported time `254` ms. PV: `e2e4 e7e5 g1f3 b8c6 b1c3 g8f6 f1b5 f8d6 e1g1 e8g8 b5a4 c6e7`.
- That is roughly 1.34 million **reported** nodes/s in one short sanity run, not a representative benchmark or measured optimization gain. The final depth report omits the current unflushed local-node remainder; alpha-beta/quiescence boundary counting also needs the caution in #13.
- Read-only disassembly confirmed vectorized NNUE update loops, the output-layer running-sum chain, and locked TT-statistic increments. No sampling profile or candidate executable was produced. Hardware-specific rankings remain unverified.

### How to decide whether a patch is worth keeping

1. Build a baseline from the current source into a separate output directory before comparing source patches to the supplied executable. Keep the supplied binary intact and confirm the new baseline's single-thread search results. Record compiler version and effective compile/link flags. The project already requests `/Qvec-report:2`; use its diagnostics and assembly to verify kernel generation ([Microsoft's reporting documentation](https://learn.microsoft.com/en-us/cpp/build/reference/qvec-report-auto-vectorizer-reporting-level?view=msvc-170)).
2. Benchmark one change at a time on fixed-depth positions taking seconds, not milliseconds. Use a diverse position suite, identical hash/thread settings and cleared initial state, repeated interleaved baseline/candidate runs, and comparable CPU load/frequency conditions. Compare paired elapsed times and their variability. Do not parallelize competing timing runs.
3. For behavior-preserving changes, require identical completed single-thread score, best move, PV, and node counts under unchanged accounting. A difference is evidence to investigate, not proof of an improvement. Keep current search quirks unchanged during these comparisons.
4. Run the existing perft suite for board, legality, and attack-table changes. It is necessary but insufficient for NNUE: add accumulator/full-rebuild and make/undo state comparisons for the relevant patches. SEE and move-selection changes need direct differential checks of their results/order.
5. Profile representative search after the first changes. Count updates, evaluations, pruned moves, consumed/generated moves, and repeated SEE calls in an instrumented diagnostic build; use a normal release build for final timings. This establishes which proposed costs actually dominate.
6. Confirm speed benefits on the tournament CPU and thread count, then use controlled matches at a relevant time control to establish Elo impact. More displayed NPS alone does not establish stronger play, especially if node accounting or ordering changes. Do not add individual percentage gains together: the NNUE changes in particular remove overlapping work.

Suggested first patch sequence by ease of isolating results: **#5 (unused counters), #3 (pruned-move updates), #4 (lazy selection), #1 (fused updates), #6/#7 (SEE reuse), then #2 (accumulator stack/laziness)**. The opportunity ranking above puts larger expected strength potential first; this implementation sequence starts with small, readily attributable changes.
