//# Copyright 2020 Adrian Irles IJCLab (CNRS/IN2P3)
// Thin 2026 wrapper around the legacy single-slab converters.

#include "SLBdecoded2ROOT.cc"
#undef BCIDTHRES
#include "SLBraw2ROOT.cc"

#include <iostream>

enum class DecodeMode {
  kAsciiDat,
  kRawBinary
};

DecodeMode ParseDecodeMode(TString mode) {
  mode.ToLower();
  if (mode == "raw" || mode == "binary" || mode == "rawbin") {
    return DecodeMode::kRawBinary;
  }
  return DecodeMode::kAsciiDat;
}

void ConvertFile(TString input,
                 TString output = "default",
                 TString mode = "ascii",
                 bool zerosuppression = false,
                 bool overwrite = true,
                 bool getbadbcid = true) {
  const DecodeMode decode_mode = ParseDecodeMode(mode);

  std::cout << "Decode input: " << input << std::endl;
  std::cout << "Decode mode : " << mode << std::endl;
  std::cout << "Output file : " << output << std::endl;

  if (decode_mode == DecodeMode::kRawBinary) {
    SLBraw2ROOT converter;
    converter._maxReadOutCycleJump = 3;
    bool ok = false;
    while (!ok) {
      std::cout << "Trying _maxReadOutCycleJump="
                << converter._maxReadOutCycleJump << std::endl;
      ok = converter.ReadFile(input, overwrite, output, zerosuppression, getbadbcid);
      converter._maxReadOutCycleJump += 3;
    }
    return;
  }

  SLBdecoded2ROOT converter;
  converter.ReadFile(input, overwrite, output, zerosuppression);
}
