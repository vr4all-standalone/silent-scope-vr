# Rejected SHARC recompilation experiments

These patches are evidence for further investigation, **not applied by the default build**. Main graphics DSP recompilation remains Off. The current engine uses patches0001–0008; its live-SRAM patch0008 is separate from the rejected invalidation patch here. The later0.0.3 candidate uses only the separately tested scope DSP recompiler, with the main DSP still interpreted.

0008 detects changed PM SRAM writes that bypass generated recompiler accessors.0009 moves pending-invalidation checks onto taken branch/call and hardware-loop paths, and discards stale frontend loop metadata on device reset. Both address source-supported invalidation gaps, but neither combination resolved the observed native runtime failures.

Windows/x64 both-DSP DRC crashed at native55 seconds before these changes; with0008 it crashed68; with0008+0009 it again crashed55. Main-only DRC soft-reset the native game around78 seconds. With0008 and the portable C DRC backend, both DSPs avoided the host access violation through115 seconds but still caused the native reset around78. Successful exit status after a soft reset is not stable gameplay.

Logs and private captures are recorded under `overseer/changes/2026-11-01-silent-scope-quest/` and the project's ignored `private/scene-*` directories. No comparable Quest DRC runtime result exists. Further work should compare actual DSP instruction/data flow and timing rather than enabling DRC by default or treating missing crashes as output correctness.
