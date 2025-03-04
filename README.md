# Consistent hashing library
## Usage
Create `MakeWeightUpdater` that uses `DefaultConfig` or provide custom config to
`BasicWeightUpdater`.
Pass start of lookup array to `InitLookup` method.
Call `UpdateLookup` to update weights.