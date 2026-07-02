# CLAUDE.md — AI Context File for RTB Static-Inference Engine (Self-Loop Deferred)

> **Purpose:** Paste this file at the start of any AI conversation (Claude, GPT, Gemini, etc.)
> to restore full project context instantly. Keep this file updated as the project evolves.
> Last updated: 2026-06-30

---

## 1. WHO I AM

- Strong in C++ and Python, learning systems engineering deeply
- Competitive programming background: Codeforces ~1200 — strong algorithmic fundamentals,
  building systems/concurrency intuition from scratch on this project
- New to RTB domain but understand the math and ML
- Goal: build a portfolio project that demonstrates production-grade systems + ML knowledge
- Target roles: SWE / ML Engineer internships and full-time positions (2025-2026)
- Background: Read McMahan et al. 2013 (FTRL paper) — understand the algorithm at paper level
- Learning approach: implement piece by piece, hit real bugs/questions, then read the relevant
  paper section or theory to understand *why* — not theory-first, code-second

---

## 2. WHAT THIS PROJECT IS

**Name:** RTB (Real-Time Bidding) Static-Inference Engine
**Stack:** C++17, Python 3.9, CMake
**Status:** Architecture finalized for v1 scope — no code written yet

### Elevator Pitch
A production-grade Real-Time Bidding engine that predicts click-through rate using
FTRL-Proximal weights (trained offline on historical auction data) and serves bids
under a strict 5ms p99 latency budget. v1 is a static-inference engine: weights are
trained once offline and loaded at startup. Live online learning (self-loop) is a
deliberately deferred v2 extension — see Section 2b.

### What makes it non-trivial (resume differentiators, v1 scope)
1. **FTRL-Proximal weights** — per-coordinate adaptive learning rates, L1
   sparsity, handles high-cardinality sparse RTB features correctly (trained offline,
   served via static inference)
2. **Arena allocator** — per-request memory slab, pointer-bump allocation, zero GC
   pauses on critical path, targets <5ms p99 latency
3. **Flat hash map** — open-addressing, cache-friendly, ~3x faster than
   std::unordered_map for feature store lookups
4. **Feature hashing trick** — maps arbitrary high-cardinality string features
   (domain, URL, visitor_id) to fixed-size weight vector; handles unseen features gracefully
5. **Offline regret verification** — empirically measures O(√T) regret bound from the
   FTRL paper on historical replay, proving implementation correctness against the
   theoretical guarantee (done offline in Python, not live)

### What we are NOT building (v1)
- No Kafka, no Redis, no Docker (yet) — single-process, single-machine
- No microservices — library interface, not a web service
- No REST API
- No neural networks — FTRL is the deliberate choice; can defend why
- **No live/online weight updates** — see Section 2b for why this is deferred, not abandoned
- No lock-free ring buffer, no learner thread, no concurrency primitives in v1

---

## 2b. SELF-LOOP LEARNING — DEFERRED TO v2 (DECISION RECORD)

**Decision: build static-inference engine completely first. Self-loop is an optional
v2 extension once v1 is fully working, measured, and documented.**

### Why deferred (not just "later" — actual reasoning, don't re-open without new info)

1. **Risk concentration.** The lock-free SPSC ring buffer + learner thread is by far the
   highest-risk, highest-time-cost, easiest-to-get-subtly-wrong piece of the original
   design. Memory-ordering bugs (release/acquire) are the kind of bug that surfaces
   intermittently under load, not deterministically — exactly the failure mode that
   eats a week of debugging time with little guarantee of success.

2. **Differentiator math without the live mechanism is still strong.** FTRL math +
   arena allocator + flat hash map + feature hashing is already a legitimate systems
   project. We are not losing the ML story or the low-latency story — only the live
   concurrency story.

3. **Honesty about the dataset.** IPinYou is a static historical log, not a live
   auction stream. A ring buffer + learner thread replaying static logs demonstrates
   the *concurrency primitive* works, but the "self-improving in real time" narrative
   is partly cosmetic unless paired with an explicit replay simulator that feeds events
   asynchronously. That simulator is itself nontrivial scope. Better to be explicit
   about this limitation than hand-wave it in an interview.

4. **Sequencing reduces total risk.** Build Phase 1+2 (static inference, no
   concurrency) to a complete, working, measured, written-up state first. This
   guarantees a defensible portfolio piece exists even if the v2 concurrency work
   stalls or runs out of time. Self-loop becomes a bonus on top of a solid base,
   not a single point of failure for the whole project.

### What v2 would add, if/when we return to it
- RingBuffer.hpp (lock-free SPSC, std::atomic, release/acquire ordering)
- LearnerThread (background FTRL weight updater)
- ThreadSanitizer validation
- A live regret tracker (vs. the offline-only version in v1)
- Decision needed at that time: build a replay simulator that feeds events
  asynchronously, or scope it as "concurrency primitive proven in isolation" —
  these are different claims and should not be conflated in interview talking points.

**Do not silently re-add ring buffer / learner thread code without updating this
section.** If we revisit self-loop, update status here first.

---

## 3. DATASET

Source: IPinYou RTB dataset (Adobe DevCraft Hackathon version)
Copyright: IPinYou Inc. 2014 ACM 978-1-4503-2999-6/14/08

### Files
```
dataset/bid.06.txt  through  bid.12.txt     # All DSP bids submitted
dataset/imp.06.txt  through  imp.12.txt     # Bids that won (impression served)
dataset/clk.06.txt  through  clk.12.txt     # Impressions that were clicked
dataset/conv.06.txt through  conv.12.txt    # Clicks that converted
city.txt, region.txt, user.profile.tags.txt
```

### Log Format (24 tab-separated columns — VERIFIED against real dataset 2026-06-22)

> **Correction from earlier draft:** original spec had 23 columns and was missing
> `UserTag`. Verified directly against real `bid.06.txt` (21 fields) and `clk.06.txt`
> (24 fields) sample lines. The 24-column version below is canonical; `bid.txt` simply
> omits the three outcome-only columns (marked †) since they don't exist at bid time.

| Col | Name | Type | Notes |
|-----|------|------|-------|
| 1  | BidID | string | Unique auction ID |
| 2  | Timestamp | uint64 | yyyyMMddHHmmssSSS — needs 64-bit |
| 3† | Logtype | int | 1=impression, 2=click, 3=conversion — NOT at bid time |
| 4  | VisitorID | string | Hashed cookie (user identity) |
| 5  | User-Agent | string | Browser/OS/device |
| 6  | IP | string | Hashed IP (trailing octet masked, e.g. `180.127.189.*`) |
| 7  | Region | uint32 | Numeric region code |
| 8  | City | uint32 | Numeric city code |
| 9  | Adexchange | uint32 | Which exchange |
| 10 | Domain | string | Hashed domain |
| 11 | URL | string | Hashed URL |
| 12 | AnonymousURLID | string | **Empty string OR literal `"null"` when URL is known — parser must treat BOTH as absent. Verified inconsistent across files: empty in bid.txt sample, literal "null" in clk.txt sample.** |
| 13 | AdslotID | string | Specific ad slot |
| 14 | Adslotwidth | uint32 | Pixels |
| 15 | Adslotheight | uint32 | Pixels |
| 16 | Adslotvisibility | **uint32** | **VERIFIED NUMERIC CODE (e.g. 0,1,2), not string "FirstView" etc. as originally assumed. Exact code→meaning mapping not yet confirmed — treat as opaque categorical int for now.** |
| 17 | Adslotformat | **uint32** | **VERIFIED NUMERIC CODE (e.g. 0,1), not string "Fixed" etc. Same caveat as above.** |
| 18 | Adslotfloorprice | uint32 | Minimum bid — hard gate |
| 19 | CreativeID | string | Ad creative identifier |
| 20 | Biddingprice | uint32 | DSP's bid (training signal only) |
| 21†| Payingprice | uint32 | Market price — NOT at bid time (label only) |
| 22†| KeypageURL | string | Post-auction metadata — NOT at bid time |
| 23 | AdvertiserID | uint32 | One of 5 advertisers |
| 24 | UserTag | string | Comma-separated list of segment IDs (e.g. `10006,10063,...`), or `null` if none. **Missing from original spec — added after verification.** |

**† = available in logs ONLY (imp/clk/conv files), never in bid.txt / never at bid time.
Using these as input features = data leakage.**

### File-specific column counts (verified)
- `bid.06.txt`: **21 columns** (24 minus cols 3, 21, 22 — outcome columns don't exist pre-auction)
- `imp/clk/conv.06.txt`: **24 columns** (full set, since outcome is known)

Parser must handle both layouts — do not assume a fixed column count without
checking which file type is being read.

### 5 Advertisers
| ID | N | Industry | Bidding Implication |
|----|---|----------|---------------------|
| 1458 | 0  | Local e-commerce  | Clicks only — bid for CTR |
| 3358 | 2  | Software          | Conv = 2 clicks |
| 3386 | 0  | Global e-commerce | Clicks only |
| 3427 | 0  | Oil               | Clicks only |
| 3476 | 10 | Tire              | Conv = 10 clicks — bid for high-intent |

### Train / Validation Split (TEMPORAL — never random)
- Training: days 06–10
- Validation: days 11–12
- Reason: temporal autocorrelation; random split leaks future data into training

### Label Construction (join order matters)
```
bid logs (base)
  → inner join imp logs on BidID   (only winning bids have downstream data)
    → left join clk logs on BidID  (click_label = 1 if present)
      → left join conv logs on BidID (conv_label = 1 if present)
```
**Never join bid directly to click — non-winning bids have no click data.**

---

## 4. SCORING FUNCTION

```
Score = total_clicks + N * total_conversions
Subject to: fixed per-advertiser budget (hard constraint)
```

Auction type: **Second-price** — winner pays second-highest bid (market price).
Implication: bidding true expected value is the dominant strategy. Bid shading
is only for budget pacing, not auction strategy.

---

## 5. SYSTEM ARCHITECTURE (v1 — static inference)

### Two-Phase Design
```
OFFLINE (Python) — hours budget, run once (or periodically, manually)
  Raw logs → Feature Engineering → FTRL training → Serialize weights
                                                              ↓
                                                   trained_weights.bin
                                                   feature_config.json
                                                   budget_config.json
                                                              ↓ loaded at startup
ONLINE (C++) — 5ms budget per request, INFERENCE ONLY
  BidRequest → Parser → FloorGate → FeatureExtractor → FTRL Predict (read-only)
                                                              ↓
                                                    pCTR → BidCalculator
                                                              ↓
                                                    BudgetTracker → bid or -1
                                                              ↓
                                                    [log event for offline analysis]
```

Note: weights are **never modified** in the online path in v1. The only output of
a request, besides the bid, is an optional log line for later offline evaluation —
no ring buffer, no background thread.

### Key Design Decisions and Why

**Why C++ for online, Python for offline?**
5ms constraint eliminates Python for inference. Python is used where time budget
is unlimited (training). This is the universal production split in ad-tech.

**Why FTRL not SGD?**
RTB features are sparse and high-cardinality. FTRL uses per-coordinate learning
rates — rare features (seen 3 times) get larger updates than common features
(seen 10,000 times). SGD uses a global learning rate and either undertrains rare
features or overtrains common ones. FTRL also produces sparse weight vectors via
L1 regularization, which speeds up inference. (Note: in v1, this matters for
*training quality* offline — inference itself is a static dot product regardless
of which optimizer produced the weights.)

**Why arena allocator not malloc?**
malloc/free call system allocator which can stall unpredictably (free list
coalescing, mmap calls). Arena = pointer bump allocation on pre-allocated slab.
Reset = pointer = start. Zero kernel calls on critical path. Eliminates p99 spikes.

**Why flat hash map not std::unordered_map?**
std::unordered_map uses separate chaining (linked list per bucket) — cache
hostile, every probe is a pointer dereference. Flat hash map uses open addressing
— keys and values in contiguous array, cache-friendly probing. ~3x faster for
the feature store which is queried on every request.

**Why feature hashing not vocabulary lookup?**
RTB feature space is unbounded (new domains, URLs, user IDs appear constantly).
A fixed vocabulary can't handle unseen features. Feature hashing maps any string
to a fixed-size weight vector index via hash % vector_size. Collisions are
acceptable at large vector sizes. Handles cold-start naturally.

**Why no concurrency primitives in v1?**
There is nothing to coordinate yet — one thread reads immutable weights and
serves predictions. Concurrency only becomes necessary once there's a second
thread mutating shared state (the v2 self-loop learner). Adding ring buffers
or atomics now would be unjustified complexity with no corresponding need.
See Section 2b for the full reasoning on deferring self-loop.

---

## 6. C++ MODULE STRUCTURE (v1)

```
rtb_engine/
├── include/
│   ├── BidRequest.hpp          # 23-field typed struct
│   ├── FeatureExtractor.hpp    # Feature engineering interface
│   ├── FTRLModel.hpp           # FTRL weights + predict (NO update method in v1)
│   ├── ArenaAllocator.hpp      # Per-request memory slab
│   ├── FlatHashMap.hpp         # Open-addressing feature store
│   ├── BidCalculator.hpp       # EV → bid price + floor gate
│   ├── BudgetTracker.hpp       # Per-advertiser pacing state
│   └── BiddingEngine.hpp       # Top-level orchestrator
├── src/                        # Implementations
├── models/                     # Serialized artifacts from Python
│   ├── trained_weights.bin     # float32 array — FTRL weights (offline-trained)
│   ├── feature_config.json     # hash size, feature order, scaling params
│   └── budget_config.json      # per-advertiser budgets and N values
├── tests/                      # Unit + integration + latency tests
├── python/                     # Offline ML pipeline
│   ├── data_loader.py
│   ├── feature_engineering.py  # MUST match C++ FeatureExtractor exactly
│   ├── ftrl_train.py           # Full offline FTRL training (was ftrl_pretrain.py)
│   ├── evaluator.py            # Offline scoring function simulation
│   └── regret_tracker.py       # O(√T) regret bound verification (offline replay)
└── CMakeLists.txt
```

**Note:** `FTRLModel.hpp` exposes `predict()` only in v1. The `update()` method
(z, n accumulation per McMahan et al. Section 3-4) is implemented and unit-tested
in the **Python** training pipeline only. If/when v2 happens, the same update
logic gets ported to C++ and called from the learner thread — not redesigned
from scratch.

---

## 7. FEATURE VECTOR (canonical order — must match Python and C++)

> **Corrected 2026-06-24, after research into real-world handling of this
> exact dataset.** Original spec assumed `adslot_visibility`/`adslot_format`
> were strings with known small enumerations, and mapped them manually.
> We verified (Section 3) they're raw numeric codes in the real data, AND
> confirmed via published research + the dataset-specific FeatureHashing R
> package that the established practice for THIS dataset is to treat them
> (and most other fields) as categorical and HASH them — not pass raw
> codes through as if they were ordinal numbers. Passing a category code
> straight into a linear model implies a false ordinal relationship
> (format=4 is NOT "4x" of format=1) — this was a real correctness issue,
> not just a simplification choice.

```
Index | Feature          | Derived From           | Transform
------|------------------|------------------------|---------------------------
  0   | hour_of_day      | timestamp              | (ts / 10000000) % 100
  1   | day_of_week      | timestamp              | parse date, compute weekday
  2   | slot_area        | width * height         | integer multiply
  3   | visibility_hash  | adslot_visibility_code | hash("vis:" + code) % HASH_SIZE
                                                     (CHANGED from manual mapping —
                                                      code is categorical, not ordinal)
  4   | format_hash      | adslot_format_code     | hash("fmt:" + code) % HASH_SIZE
                                                     (CHANGED — same reasoning as above)
  5   | domain_hash      | domain                 | fnv1a(domain) % HASH_SIZE
  6   | url_hash         | url                    | fnv1a(url) % HASH_SIZE
  7   | visitor_hash     | visitor_id             | fnv1a(visitor_id) % HASH_SIZE
  8   | floor_ratio      | floor_price            | floor_price / GLOBAL_MEAN_FLOOR_PLACEHOLDER
                                                     (PLACEHOLDER constant for v1 — see note below)
  9   | is_mobile        | user_agent             | case-insensitive contains("mobile") ? 1 : 0
                                                     (CHANGED to case-insensitive — real samples
                                                      showed inconsistent casing: lowercase in one
                                                      bid.txt sample, capitalized in another)
  10  | exchange_id      | adexchange             | direct (uint32)
  11  | advertiser_id    | advertiser_id          | direct (uint32)
  12  | region           | region                 | direct (uint32)
  13  | city             | city                   | direct (uint32)
```

**HASH_SIZE = 2^18 = 262144** (tunable — larger reduces collisions, increases memory)

**GLOBAL_MEAN_FLOOR_PLACEHOLDER:** v1 uses a hardcoded placeholder constant
(exact value TBD when we write the code) instead of a real dataset-wide
mean. This is a known, deliberate stand-in — must be replaced with a value
actually computed from training data (days 06-10) once the offline Python
pipeline exists. Don't forget to revisit this — flagged here so it isn't
silently treated as final.

**Open question, not yet resolved:** `region`, `city`, `advertiser_id`,
`exchange_id` are currently "direct (uint32)" passthrough, same as the
OLD design for visibility/format. The same research that corrected
visibility/format (FeatureHashing R package, published papers on this
dataset) actually treats region/city/advertiser_id as categorical/hashed
too, not passed through raw. We have NOT yet revisited this for v1 — it's
a known simplification, not a verified-correct choice. Revisit if time
allows; flagged here so it isn't forgotten or assumed settled.

**CONFIRMED via real data, 2026-06-24 (10 real sample lines checked across
bid.txt and clk.txt):** visibility_code takes values {0,1,2}, format_code
takes values {0,1,5}. Format jumping straight to 5 (skipping 2,3,4 in our
small sample) strongly implies the true category count is at least 6,
of which we've only randomly seen 3. This definitively confirms neither
field is binary, and decisively settles the hashing decision above — keep
both hashed. Do not re-open this without new contradicting evidence.

---

## 8. FTRL-PROXIMAL ALGORITHM (McMahan et al. 2013)

> v1 note: this algorithm runs entirely **offline in Python** during training.
> The C++ engine only ever evaluates the resulting `w_i` at inference time
> (a static sparse dot product). The update rule below is documented here
> because the offline trainer implements it and because it's foundational if
> v2 self-loop is built later — not because C++ needs it in v1.

### Per-coordinate update rule
```
For each feature i with gradient g_i:

  z_i  +=  g_i  -  (1/η_i - 1/η_{i-1}) * w_i
  n_i  +=  g_i²

  η_i  =  (β + √n_i) / α          ← per-coordinate learning rate

  w_i  =  0                        if |z_i| ≤ λ1
        =  -(η_i / (λ2 + 1/η_i))  * (z_i - sign(z_i)*λ1)   otherwise
```

### Hyperparameters
| Param | Symbol | Typical Value | Role |
|-------|--------|---------------|------|
| Learning rate | α | 0.1 | Step size scale |
| Smoothing | β | 1.0 | Prevents division by zero in η |
| L1 regularization | λ1 | 1.0 | Sparsity — zeroes out unimportant features |
| L2 regularization | λ2 | 1.0 | Weight magnitude control |
| Hash size | — | 2^18 | Feature vector dimension |

### Per-request state (offline trainer; stored per feature index)
- `z[i]` — accumulated gradient signal (float32 array, size = HASH_SIZE)
- `n[i]` — sum of squared gradients (float32 array, size = HASH_SIZE)
- `w[i]` — current weight (derived on prediction, not stored explicitly)

### Memory estimate (offline trainer)
```
2 arrays × 262144 elements × 4 bytes = 2MB
Well within 512MB constraint.
```

### What the C++ engine actually stores (v1)
Only the final `w[i]` array (262144 float32s, ~1MB), loaded once from
`trained_weights.bin` at startup. No `z[i]`, no `n[i]` — those only exist
during offline training, not at inference time.

---

## 9. ARENA ALLOCATOR DESIGN

```cpp
class ArenaAllocator {
    void*    slab_;       // pre-allocated block (e.g. 64KB per request)
    size_t   offset_;     // current allocation pointer
    size_t   capacity_;   // total slab size

public:
    void* allocate(size_t size, size_t align = 8) {
        offset_ = align_up(offset_, align);
        void* ptr = (char*)slab_ + offset_;
        offset_ += size;
        return ptr;  // no bookkeeping, no free list
    }

    void reset() { offset_ = 0; }  // entire request done in one instruction
};
```

One arena instance per request (or reset per request).
Slab allocated once at engine startup via mmap (not malloc — bypasses allocator).

---

## 10. WHAT TO BUILD FIRST (Implementation Order, v1 scope)

```
Phase 1 — Foundation (C++)
  BidRequest.hpp + parser
  FeatureExtractor (matches Python exactly)
  ArenaAllocator
  FlatHashMap
  Unit tests for all above

Phase 2 — Core Intelligence
  FTRLModel (predict only — single struct, no z/n state in C++)
  Python offline pipeline (data_loader, feature_engineering, ftrl_train)
  Parity test: Python features == C++ features on 100 samples

Phase 3 — Bidding Logic
  BidCalculator (EV formula, floor gate, pacing multiplier)
  BudgetTracker (per-advertiser state, pacing algorithm)
  BiddingEngine (orchestrator, wires everything)

Phase 4 — Evaluation & Polish
  Offline scoring function simulator (Python)
  Regret tracker (O(√T) verification, offline replay on validation days 11-12)
  Latency benchmarks (p50, p95, p99)
  README, ARCHITECTURE.md, DESIGN_DECISIONS.md

Phase 5 — OPTIONAL v2 EXTENSION (only after Phase 1-4 complete and written up)
  Decide: build replay simulator first, or scope ring buffer as isolated primitive?
  RingBuffer (SPSC lock-free)
  LearnerThread (background weight updater)
  ThreadSanitizer validation
  Live regret tracker
```

---

## 11. INTERVIEW TALKING POINTS (defend every decision)

**"Why FTRL over SGD?"**
Per-coordinate learning rates. RTB data is sparse — most features are zero
for any given request. FTRL's η_i adapts per feature frequency. SGD's global
learning rate either undertrains rare features or overtrains common ones.

**"Why arena over malloc?"**
p99 latency. malloc can stall on free list coalescing or mmap calls.
Arena = pointer bump = single addition. Reset = pointer = base. Zero
system calls on critical path. Eliminates tail latency spikes.

**"Why flat hash map over unordered_map?"**
Cache locality. unordered_map = separate chaining = pointer per bucket =
cache miss per probe. Flat hash map = open addressing = contiguous array =
L1 cache hits on probe sequence. Memory access pattern is the difference.

**"How do you know your FTRL is correct?"**
Regret tracking. McMahan 2013 proves FTRL achieves O(√T) regret. I replay
the validation set offline and track cumulative regret — empirically
verifies the implementation matches the theoretical bound.

**"Why not a neural network?"**
Latency budget and interpretability. A shallow MLP adds 2-4ms inference.
FTRL inference is a sparse dot product — microseconds. Also: FTRL weights
are directly interpretable (large |w_i| = important feature), which is
valuable for debugging and explaining bids.

**"Why doesn't the model update in real time / why no online learning?"**
Deliberate scope decision, not a limitation I missed. Online learning via a
lock-free ring buffer + background learner thread is real, valuable
engineering — but it's also the highest-risk piece (memory-ordering bugs,
TSan validation) for a portfolio timeline, and the IPinYou dataset is a
static historical log, not a live stream, so a fully honest "self-improving"
story needs a replay simulator on top of the concurrency primitive. I scoped
v1 as a complete, correct, well-measured static-inference engine first, and
treated online learning as a v2 extension to build once the foundation is
solid — sequencing risk instead of taking it all at once.

---

## 12. PAPERS & REFERENCES

1. **McMahan et al. 2013** — "Ad Click Prediction: a View from the Trenches"
   Google. The FTRL-Proximal paper. Read sections 2-4 carefully.
   https://research.google/pubs/ad-click-prediction-a-view-from-the-trenches/

2. **Zhang et al. 2014** — "Optimal Real-Time Bidding for Display Advertising"
   IPinYou dataset paper. Describes the data we're using.

3. **Richardson et al. 2007** — "Predicting Clicks: Estimating the Click-Through
   Rate for New Ads" — foundational CTR prediction paper.

---

## 13. WHAT NOT TO DO (decisions already made — don't re-open without new info)

- No Kafka, Redis, Docker in core design — single-process, these solve
  multi-machine problems we don't have
- No microservices — library interface, not a web service
- No neural networks as primary model — FTRL is the deliberate choice
- No random train/val split — temporal only
- No Col 21 (payingprice) as input feature — data leakage
- No std::unordered_map for feature store — flat hash map instead
- **No ring buffer / learner thread / live weight updates in v1** — see
  Section 2b for full reasoning. This is deferred, not forgotten. Don't
  silently start building it; revisit the decision record first.

---

## 14. CURRENT STATUS & NEXT STEPS

**Done:**
- Full v1 (static-inference) architecture designed and documented
- Self-loop deferral decision made and recorded (Section 2b)
- Dataset format VERIFIED against real bid.06.txt and clk.06.txt sample lines
  (see Section 3 — corrected from 23 to 24 canonical columns; UserTag added;
  bid.txt confirmed at 21 columns; Adslotvisibility/Adslotformat confirmed
  numeric codes, not strings as originally assumed)
- **Phase 1, Step 1 COMPLETE: `BidRequest.hpp` + `BidRequest.cpp` (parser)**
  - Struct contains ONLY the 21 bid-time fields — zero outcome fields,
    by design (see decision record below)
  - `std::string_view` for all text fields, `std::optional<BidRequest>`
    return type for parse failures (no exceptions on hot path)
  - **`split_fields`, `parse_uint`, and `parse_bid_request` all WRITTEN BY
    USER from a skeleton, block by block, reviewed and corrected through
    discussion — not generated wholesale. User can explain the
    fail-fast/reject-malformed design tradeoffs in their own words.**
  - Full test suite (`tests/test_bid_request.cpp`) passing, including
    malformed-input edge cases (too few/too many fields, non-numeric
    values, empty bid_id, trailing-tab edge case)
  - Verified compiling and passing via CMake + ctest, both in sandbox
    and on local machine (Mac, Apple Clang 17, CMake 4.3.4)
  - `docs/LEARNING_LOG.md` created — separate from this file, personal
    learning journal updated by the user after each session
  - **Committed to git** — see git log for exact commit

**Decisions made during Phase 1 Step 1 (locked in, don't re-open without
new info):**
- `BidRequest` has NO `paying_price` / outcome fields at all, not even as
  `std::optional`. Confirmed explicitly after an earlier draft accidentally
  included `paying_price` as optional — rejected in favor of strict
  separation, matching the original architecture decision in Section 5.
- Raw categorical fields renamed with `_code` suffix
  (`adslot_visibility_code`, `adslot_format_code`) to signal at the type
  level that these need feature engineering before use in the model.
- `anonymous_url_id` and `user_tag` "absent" checks centralized as struct
  methods (`has_anonymous_url_id()`, `has_user_tag()`) because real data
  showed inconsistent null representation across files (empty string in
  one sample, literal `"null"` string in another) — don't want every
  caller to have to remember both forms.
- Field-count validation in the parser rejects BOTH too-few AND too-many
  fields (a genuine bug was caught and fixed here — see
  docs/LEARNING_LOG.md 2026-06-23 entry for the full story).

**Working mode (locked in, applies to all future phases):**
- Block by block, not whole-file generation. AI gives skeleton/signature
  + a short description of what each block needs to do; user writes the
  actual logic; AI reviews, points out bugs/tradeoffs, explains lines on
  request — but does not pre-write the implementation.
- This is why Step 1 took longer than just generating the file — that's
  expected and correct for this project's goal (learning, not just output).

**In Progress:**
- **Phase 1, Step 2: `FeatureExtractor`**
  - `FeatureExtractor.hpp` COMPLETE — `FeatureVector` type (`std::array<double,14>`),
    `FeatureIndex` enum naming each of the 14 box positions, `kHashSize`,
    `extract_features()` declared. No failure mode (unlike parsing) —
    every valid `BidRequest` always produces a feature vector.
  - `FeatureExtractor.cpp` skeleton delivered (TODOs only, not yet
    implemented): `fnv1a_hash`, `hash_to_bucket`, `is_mobile_user_agent`,
    and the 14-field body of `extract_features`.
  - User about to write `fnv1a_hash` (standard 32-bit FNV-1a recipe,
    given as spec — offset basis 2166136261, prime 16777619 — not
    designed from scratch, a known algorithm).
  - NOT yet compiled/tested — skeleton only, logic not yet written.

**Decisions made during Phase 1 Step 2 (locked in):**
- `visibility_code` and `format_code` (cols 15/16 in bid.txt) are HASHED
  into the same 262144-bucket space as domain/url/visitor_id — NOT passed
  through as raw numbers, and NOT one-hot encoded. Reasoning: passing a
  category code straight into a linear model implies false ordinality
  (format=5 is not "5x" format=1). Confirmed via research (FeatureHashing
  R package + published papers built specifically for this dataset both
  hash these fields) AND via direct evidence: checked 10 real sample lines
  across bid.txt/clk.txt, found visibility ∈ {0,1,2}, format ∈ {0,1,5} —
  format jumping straight to 5 strongly implies ≥6 true categories, of
  which we'd only randomly seen 3. Neither field is binary; the
  false-ordinality concern is real, not theoretical. Don't re-open this
  without new contradicting evidence — see Section 7 for full data table.
- `floor_ratio` (index 8) uses a hardcoded placeholder constant
  (`kGlobalMeanFloorPlaceholder`, currently 50.0, NOT yet verified against
  real data) instead of a true dataset-wide mean. MUST be replaced once
  the offline Python pipeline can compute the real mean from training
  days 06-10. Flagged in code comments too — don't forget this.
- `is_mobile` (index 9) does a case-INsensitive substring check for
  "mobile". Real sample user-agent strings showed inconsistent casing
  (lowercase in some bid.txt rows, capitalized "Mobile" in others) —
  a case-sensitive check would silently miss real mobile traffic.
- OPEN / not yet decided: `region`, `city`, `advertiser_id`, `exchange_id`
  still use direct (uint32) passthrough in the current design — the same
  research that corrected visibility/format suggests these might need the
  same hashing treatment, but this has NOT been investigated or decided
  yet. Flagged, not forgotten — revisit if time allows.

---

**Phase 1, Step 2 COMPLETE: `FeatureExtractor.cpp` implemented and reviewed.**

- `fnv1a_hash`: standard 32-bit FNV-1a (offset basis 2166136261, prime
  16777619), iterates over `unsigned char` to avoid signed-char overflow
  bugs. Verified correct.
- `hash_to_bucket`: `fnv1a_hash(data) % kHashSize`. Shared helper used by
  domain/url/visitor/visibility/format hashing.
- `is_mobile_user_agent`: builds a lowercase copy of the user-agent via
  `tolower` (cast to `unsigned char` first, same overflow reasoning as
  the hash), then does a substring `find` for "mobile". Known minor
  inefficiency: allocates a heap string per call, on the hot path. Not
  fixed yet — flagged as a profiling target, not urgent for v1. If it
  ever shows up in a profiler, switch to a manual case-insensitive
  comparison loop to avoid the allocation.
- `kHourOfDay`: `(timestamp / 10'000'000) % 100` — strips the 7-digit
  `mmssSSS` suffix, then isolates the 2-digit `HH`.
- `kDayOfWeek`: implemented via `std::tm` + `mktime`, NOT custom calendar
  math. Year/month/day sliced from the `yyyyMMdd` prefix of the
  timestamp (`tm_year = year-1900`, `tm_mon = month-1`), `mktime` called
  to normalise and populate `tm_wday` as a side effect.
  - **Convention fix (locked in):** `tm_wday` is C convention
    (Sun=0...Sat=6), but the project spec requires Python's `weekday()`
    convention (Mon=0...Sun=6) to keep C++ inference and Python training
    in exact parity. Conversion applied: `(tm_wday + 6) % 7`. Verified:
    Sun(0)→6, Mon(1)→0, ..., Sat(6)→5. This conversion must be applied
    wherever `tm_wday` is read — don't forget it if this logic is ever
    touched again.
  - **Known limitation, accepted for v1 (not a bug to fix now):**
    `mktime` interprets the broken-down time as LOCAL time, not UTC.
    IPinYou timestamps are presumed Beijing time (UTC+8); `mktime` does
    not know this and uses the OS timezone instead. This can
    mis-categorize `kDayOfWeek` by exactly one day, only for rows near
    midnight boundaries where the local-vs-source timezone offset
    crosses a calendar day. Decision: accepted as a documented
    assumption rather than fixed now, because (a) it only affects a
    small fraction of rows, (b) the model just learns slightly shifted
    day-of-week weights rather than failing, and (c) the correct fix
    (`timegm` or manual Zeller's-formula date math) is not portable
    (`timegm` is POSIX-only, unavailable on MSVC) and not worth the
    rabbit hole unless training/serving machines have inconsistent
    timezones. **If train and serve ever run on machines with different
    timezones, revisit this — that's the condition that would actually
    cause a problem.**
- `kVisibilityHash` / `kFormatHash`: numeric codes are stringified with a
  prefix (`"vis:" + to_string(code)`, `"fmt:" + to_string(code)`) before
  hashing, specifically to prevent visibility code N and format code N
  from colliding in the same bucket space.
- `kDomainHash` / `kUrlHash` / `kVisitorHash`: direct `hash_to_bucket()`
  calls on the respective string_view fields.
- `kFloorRatio`: `floor_price / kGlobalMeanFloorPlaceholder` as specified.
- `kIsMobile`: `is_mobile_user_agent(user_agent) ? 1.0 : 0.0`.
- `kExchangeId` / `kAdvertiserId` / `kRegion` / `kCity`: raw passthrough,
  matching the still-open decision noted above.
- **Architectural note for `FTRLModel` (when we get there, don't forget
  this):** hashed features (domain/url/visitor/visibility/format) are
  stored as `double` bucket IDs in the `FeatureVector`, but they are NOT
  meant to be used as raw numeric values in a standard dot product —
  they are lookup keys/indices into a weight array. This is correct as
  designed in the extractor; the constraint is on how `FTRLModel` must
  interpret them. Must be handled correctly when `FTRLModel` is built —
  flagged now so it isn't missed later.
- Status: code written and reviewed in detail (logically reviewed,
  walked through manually). **NOT yet compiled or unit tested.** Next
  concrete step before moving on: compile + run through `tests/` the
  same way `BidRequest` was, with edge cases (midnight timestamps,
  mobile/Mobile casing, max-size hash inputs).

---

**Phase 1, Step 3 IN PROGRESS: `ArenaAllocator`**

- Mental model established: per-request bump allocator. One heap slab
  allocated once at construction (`new char[capacity]`), a single
  `offset_` cursor that advances on each `allocate()`, and `reset()`
  which just sets `offset_ = 0` — O(1) "free everything," no
  individual-object bookkeeping, because every object allocated during
  one bid request shares the same lifetime (the request itself).
- Backing memory location decided: **heap**, not stack or static/global.
  Stack ruled out (size limits, compile-time-constant requirement, risk
  of overflow). Static/global ruled out (data race risk if v2 ever adds
  concurrency — see Section 2b). Heap allocation happens once in the
  constructor; the per-request cost that matters is just pointer
  arithmetic afterward.
- Overflow handling decided: **`assert`, not `nullptr` and not
  `bad_alloc`.** Reasoning: arena size is a known, fixed parameter
  chosen upfront based on expected per-request memory needs; overflow
  represents a sizing bug to catch in testing, not a runtime condition
  to gracefully handle on the hot path. `nullptr` requires every caller
  to remember to check (easy to forget, crashes later and far from the
  cause). `bad_alloc` adds exception-handling overhead on a 5ms-budget
  hot path. `assert` is zero-cost in release builds and fails loudly in
  debug/test builds — standard pattern in game engines / HFT systems for
  this exact bump-allocator use case.
- `.hpp` and `.cpp` written (deviated from strict block-by-block working
  mode this once — written in full, with a complete manual dry run
  walked through instead, covering: unaligned single-byte alloc, aligned
  8-byte double alloc showing padding bytes, aligned 4-byte alloc with
  no padding needed, `reset()` behavior (cursor only, memory NOT zeroed
  — callers must not assume clean memory after reset), and an overflow
  case triggering the assert). Class is non-copyable (`= delete` on copy
  ctor/assign) since two arenas sharing one slab would be a correctness
  bug.
- Key implementation detail to remember: alignment is done via bitmask
  rounding — `(addr + alignment - 1) & ~(alignment - 1)` — which only
  works correctly when `alignment` is a power of two (true for all
  realistic alignments: 1, 2, 4, 8, 16...).
- **Not yet compiled or tested.** Still need a
  `tests/test_arena_allocator.cpp` following the same pattern as
  `BidRequest`'s test suite before this is considered done.

**Next:**
- Compile + unit test `FeatureExtractor.cpp` (currently reviewed but
  untested).
- Compile + unit test `ArenaAllocator` (currently written but untested).
- After both are tested and passing: `FlatHashMap`.
- Still need: one `head -n 2` sample from `imp.06.txt`, for when we reach
  the offline Python label-joining pipeline (not blocking yet).

---

*Update this file after every significant decision or implementation milestone.*
*When starting a new AI chat: paste this entire file as your first message.*
*For personal learning notes (not project state), see docs/LEARNING_LOG.md
— that file is updated separately, after each session, in the user's own words.*