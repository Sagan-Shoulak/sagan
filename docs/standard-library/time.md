---
title: Time library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Time library

The independently versioned `sagan-time` package provides native clocks and
deterministic gameplay timing without making the rendering package responsible
for simulation time.

## Clock domains

`time.monotonic.Instant` is process-relative and never represents a civil or
Unix timestamp. Use it for frame deltas, deadlines, profiling, and elapsed
time. `time.utc.UtcInstant` is an absolute Unix-epoch timestamp intended for
save metadata, logs, and network protocols. The two types cannot be mixed.

`time.duration.Duration` stores a signed checked `Int64<nanosecond>`. Conversion
from `Float64<second>` rounds to the nearest nanosecond using ties-to-even.
Overflow, non-finite conversions, negative sleeps, and zero duration divisors
raise the existing typed runtime failures. The nominal nanosecond precision is
not a promise that the operating system clock or scheduler resolves one
nanosecond; `time.monotonic.resolution()` reports the clock's nominal period.

```sagan
import duration from time.duration
import now from time.monotonic
import sleep from time.monotonic

let started = now()
sleep(duration(10 millisecond))
let elapsed = now().since(started)
print(elapsed.seconds())
```

## Gameplay frames

`time.game.FrameClock` supplies an explicit equivalent of a conventional
engine's `deltaTime`. `FrameTime.delta()` is scaled gameplay time;
`unscaled_delta()` is monotonic elapsed time after maximum-delta clamping. The
first tick returns zero. Pausing produces zero scaled delta, and time scale must
be finite and non-negative.

```sagan
import frame_clock from time.game

let clock = frame_clock()
while running {
  let frame = clock.tick!()
  update(frame.delta_seconds())
}
```

The default maximum delta is 250 milliseconds. This prevents a debugger pause
or suspended window from injecting an unbounded gameplay step while preserving
the explicit unscaled value used by the frame.

## Fixed simulation steps

`FixedStepClock` accumulates display-frame deltas into a configured fixed step.
It returns a bounded step count, interpolation ratio, and any whole-step time
dropped after the catch-up limit. This makes overload visible and avoids the
spiral of death.

```sagan
let simulation = FixedStepClock(duration(1.0 / 60.0 second), 8)
let batch = simulation.advance!(frame.delta())
for ignored in batch.steps().times {
  update_physics(batch.step_seconds())
}
render(batch.interpolation())
```

## Timers and deterministic tests

`time.timer` provides `Stopwatch`, `Deadline`, `OneShotTimer`, and
`RepeatingTimer`. Timer polling accepts an explicit monotonic `Instant`, so a
`ManualClock` can test behavior without sleeping. Repeating timers perform
bounded catch-up and then skip older missed intervals; they never create an
unbounded burst.

## UTC timestamps

`UtcInstant` supports Unix nanoseconds plus strict UTC ISO-8601 strings in the
form `YYYY-MM-DDTHH:MM:SS[.fffffffff]Z`. Fractions contain one through nine
digits and formatting removes trailing fractional zeroes. The exact supported
UTC range is the range representable by signed Unix-epoch nanoseconds. Leap seconds,
offset-bearing input, local civil time, and IANA timezone rules are not silently
approximated.

IANA timezone support belongs in a later versioned data package. Async timers
also remain deferred until Sagan has an accepted concurrency and event-loop
model.
