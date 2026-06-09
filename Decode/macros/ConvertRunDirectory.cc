//# Copyright 2020 Adrian Irles IJCLab (CNRS/IN2P3)
// Unified directory converter for 2026 commissioning runs.

#include "ConvertFile.cc"

#include "TSystem.h"
#include "TSystemDirectory.h"
#include "TSystemFile.h"
#include "TList.h"
#include "TString.h"

#include <algorithm>
#include <iostream>
#include <vector>

namespace {

TString JoinPath(TString directory, TString name) {
  if (!directory.EndsWith("/")) {
    directory += "/";
  }
  return directory + name;
}

std::vector<TString> ListMatchingFiles(TString directory,
                                       TString prefix,
                                       TString mode) {
  mode.ToLower();
  std::vector<TString> files;
  TSystemDirectory dir(directory, directory);
  TList *items = dir.GetListOfFiles();
  if (!items) {
    std::cerr << "Cannot open directory: " << directory << std::endl;
    return files;
  }

  TSystemFile *item = nullptr;
  TIter next(items);
  while ((item = static_cast<TSystemFile *>(next()))) {
    TString name = item->GetName();
    if (item->IsDirectory()) {
      continue;
    }

    bool keep = false;
    if (mode == "raw" || mode == "binary" || mode == "rawbin") {
      keep = name.BeginsWith(prefix + "_raw.bin");
    } else {
      keep = (name == prefix + ".dat") || name.BeginsWith(prefix + ".dat_");
    }

    if (keep) {
      files.push_back(JoinPath(directory, name));
    }
  }

  std::sort(files.begin(), files.end());
  return files;
}

TString OutputName(TString output_dir, TString input_file) {
  gSystem->mkdir(output_dir, true);
  TString base = gSystem->BaseName(input_file);
  return JoinPath(output_dir, "converted_" + base + ".root");
}

}  // namespace

void ConvertRunDirectory(TString run_dir,
                         TString output_dir = "converted",
                         TString mode = "ascii",
                         TString run_prefix = "",
                         bool zerosuppression = false,
                         bool overwrite = true,
                         bool getbadbcid = true) {
  if (run_prefix == "") {
    run_prefix = gSystem->BaseName(run_dir);
  }

  std::vector<TString> files = ListMatchingFiles(run_dir, run_prefix, mode);
  if (files.empty()) {
    std::cerr << "No input files found in " << run_dir
              << " with prefix " << run_prefix << std::endl;
    gSystem->Exit(2);
  }

  std::cout << "Found " << files.size() << " input file(s)." << std::endl;
  for (const auto &file : files) {
    ConvertFile(file, OutputName(output_dir, file), mode,
                zerosuppression, overwrite, getbadbcid);
  }
}

void ConvertCommissionRun(int run_number,
                          TString data_root =
                              "/home/llr/ilc/shi/data/SiWECAL-Prototype/"
                              "TB2026-06/Comission/tdc",
                          TString output_root =
                              "/home/llr/ilc/shi/code/TB_2026_6/Decode/"
                              "converted",
                          TString mode = "ascii") {
  TString run_name = TString::Format("ilc_run_%06d", run_number);
  ConvertRunDirectory(JoinPath(data_root, run_name),
                      JoinPath(output_root, run_name),
                      mode,
                      run_name);
}
