# MeterMaid Tests

The canonical calibration harness is
[`MeterMaid/tests/ReferenceToneTest.cpp`](../MeterMaid/tests/ReferenceToneTest.cpp)
— it drives the live `MeterEngine` with known-energy sine tones and
asserts every loudness / true-peak / correlation reading within
±0.5 LU / ±0.5 dBTP of th±0.5 LU / ±0.5 dBTP of th±0.5 LU / ±0.5 dBTP of th±0.5 LU / ±0.5 dBTP of B build -DJUCE_PATH=$PWD/JUCE -DMETERMAID_BUILD_TESTS=ON
cmake --build build --target MeterMaid_ReferenceTest -j
./build/MeterMaid_ReferenceTest
```

CI runs this harness as Gate 1 before the macOS universal-binary job
is allowed to execute.
