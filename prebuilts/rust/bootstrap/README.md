This directory documents the procedure used to produce our rustc-1.34.2.

1. Go into mrustc-bootstrap and run bootstrap.bash. Saving the output is
   reccomended in case something goes wrong. If the final command shows a diff,
   remove all generated files and try again.
2. In the base of the bootstrap kit, adjust `version_sequence` in `chain.py`
   to contain the sequence of rustc versions leading to the one you want.
   Patchlevels can be skipped here (e.g. 1.33.0 -> 1.34.2, no need to build
   1.34.1 or 1.34.0).
3. Run chain.py. Again, I reccomend saving the output to a log. If you want
   assurance that you are using the same rustc tarballs we were, verify the
   dumped hashes at the end of the build against the included logfile.
4. From your final compiler, grab the contents of
   
   * `build/x86_64-unknown-linux-gnu/stage3` (bin and lib)
   * `build/x86_64-unknown-linux-gnu/stage3-tools-bin/` (place in lib)
