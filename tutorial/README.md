# TB2026-06 Tutorials

Small, self-contained examples for checking the TB2026-06 decoding workflow.

## ASU Source Decoded-Binary Example

The `asu_source/` tutorial contains one small undecoded source-ASU `.bin` file
and scripts that run the full local workflow:

```bash
cd tutorial/asu_source
bash run_all.sh
```

The workflow decodes the sample `.bin` file into a ROOT file, then creates a
quick hitmap PDF. Generated files are written under `tutorial/asu_source/output/`
and are intentionally ignored by Git.
