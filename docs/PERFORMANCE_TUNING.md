# Fable II performance tuning

- D3D12 skips host compositor work for frames rejected by the existing presentation gates.
- A 60 FPS launcher preset uses `fast_guest_vblank_hz = 360`; unlimited stays at 1000. To revert, launch with `--fast_guest_vblank_hz 1000`.
- Guest thread yield batching defaults to 4. Restore `[perf] hotfunc_yield_every = 1` in `fable2_config.toml` if responsiveness or scheduling changes.

These changes do not alter image resolution or the existing shader correctness fixes. Frame-time gains are not benchmarked.