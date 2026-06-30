# TB2026 Analysis Notes

Quick reference for decoded ROOT branch shapes used by notebooks under `Ana/`.

These notes are derived from the decoder implementation in
`Decode/macros/SLBraw2ROOT.cc`, so we do not need to re-read the C++ every time.

## Dimension Order

For the raw-decoded ROOT tree `siwecaldecoded`, the main array branches use:

- `bcid[slab][chip][sca]`
- `corrected_bcid[slab][chip][sca]`
- `badbcid[slab][chip][sca]`
- `adc_low[slab][chip][sca][channel]`
- `adc_high[slab][chip][sca][channel]`
- `hitbit_low[slab][chip][sca][channel]`
- `hitbit_high[slab][chip][sca][channel]`

In NumPy or uproot this means:

- `bcid.shape == (n_entries, 15, 16, 15)`
- `corrected_bcid.shape == (n_entries, 15, 16, 15)`
- `badbcid.shape == (n_entries, 15, 16, 15)`
- `adc_low.shape == (n_entries, 15, 16, 15, 64)`
- `adc_high.shape == (n_entries, 15, 16, 15, 64)`

The per-entry axis order is therefore:

1. `slab`
2. `chip`
3. `sca`
4. `channel` for ADC and hit branches

## Why This Order Is Correct

The decoder declares the arrays as:

- `_bcid[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC]`
- `_adc_low[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC][NB_OF_CHANNELS_IN_SKIROC]`
- `_adc_high[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC][NB_OF_CHANNELS_IN_SKIROC]`

and fills them as:

- `_bcid[slabAdd][chipId][isca] = bcid[isca]`
- `_corrected_bcid[slabAdd][chipId][isca] = ...`
- `_adc_low[slabAdd][chipId][isca][ichn] = ...`
- `_adc_high[slabAdd][chipId][isca][ichn] = ...`

So the actual storage order is `slab`, then `chip`, then `sca`, then `channel`.

Relevant code locations:

- `Decode/macros/SLBraw2ROOT.cc`: array declarations around lines 145 to 154
- `Decode/macros/SLBraw2ROOT.cc`: ROOT branch declarations around lines 743 to 771
- `Decode/macros/SLBraw2ROOT.cc`: branch filling around lines 833 to 856

## Meaning Of Each Index

- `slab`: taken from `slabAdd`, and in the raw decoder it is assigned from `slabIndex`
- `chip`: taken from `chipId`
- `sca`: loop index `isca` over SKIROC events stored in one frame
- `channel`: loop index `ichn`

The decoder does:

- `slabAdd = slabIndex`
- `_bcid[slabAdd][chipId][isca] = ...`

so `slab` is the slab address used by the decoder, not a remapped geometry index.

## BCID Branches

- `bcid` is the raw decoded BCID from the frame.
- `corrected_bcid` is derived from `bcid` with 4096-wrap unfolding.
- `badbcid` is a quality flag computed from the corrected BCID sequence.

The correction logic is:

- if BCID decreases while staying positive, increment a loop counter
- store `corrected_bcid = bcid + loopBCID * 4096`

So if you want the original BCID values, use `bcid`.
If you want a monotonic BCID sequence across wraps, use `corrected_bcid`.

## ADC Branches

- `adc_low` and `adc_high` are both stored as `int`
- shape per entry is `(15, 16, 15, 64)`
- axis order is `slab, chip, sca, channel`

Important detail:

- the decoder reverses the channel order when filling the ROOT arrays

The code writes:

- `_adc_low[slabAdd][chipId][isca][ichn] = adcvalue_low[isca][64 - ichn - 1]`
- `_adc_high[slabAdd][chipId][isca][ichn] = adcvalue_high[isca][64 - ichn - 1]`

So ROOT `channel = 0` corresponds to the last element of the internal decoded
channel buffer, and ROOT `channel = 63` corresponds to the first one.

If a plot looks mirrored in channel index, this reversal is the first thing to check.

## Missing Values

Before filling a tree entry, the decoder initializes these arrays to `-999`.

That means:

- `-999` is a sentinel for missing or unfilled content
- analysis code should usually mask `-999` before plotting or computing summaries

## Suggested Uproot Shapes

For notebook work, the expected shapes are:

```python
BCID_SHAPE = (15, 16, 15)
ADC_SHAPE = (15, 16, 15, 64)
HIT_SHAPE = (15, 16, 15, 64)
```

Example:

```python
arrays = tree.arrays(["bcid", "adc_high"], entry_start=0, entry_stop=1, library="np")

bcid = arrays["bcid"]          # shape: (1, 15, 16, 15)
adc_high = arrays["adc_high"]  # shape: (1, 15, 16, 15, 64)
```
