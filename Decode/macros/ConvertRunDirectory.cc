//# Copyright 2020 Adrian Irles IJCLab (CNRS/IN2P3)
// Unified directory converter for 2026 commissioning runs.

#include "ConvertFile.cc"

#include "TSystem.h"
#include "TSystemDirectory.h"
#include "TSystemFile.h"
#include "TFileMerger.h"
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

TString TemporaryOutputDir(TString output_dir, TString run_prefix) {
  gSystem->mkdir(output_dir, true);
  return JoinPath(output_dir, ".tmp_convert_" + run_prefix);
}

TString MergedOutputName(TString output_dir, TString run_prefix) {
  gSystem->mkdir(output_dir, true);
  return JoinPath(output_dir, "converted_" + run_prefix + ".root");
}

bool MergeRootFiles(const std::vector<TString> &input_files,
                    TString output_file,
                    bool overwrite) {
  if (input_files.empty()) {
    std::cerr << "No ROOT files to merge into " << output_file << std::endl;
    return false;
  }

  TFileMerger merger;
  if (!merger.OutputFile(output_file, overwrite ? "RECREATE" : "CREATE")) {
    std::cerr << "Cannot create merged ROOT file: " << output_file << std::endl;
    return false;
  }

  for (const auto &file : input_files) {
    if (!merger.AddFile(file)) {
      std::cerr << "Cannot add ROOT file to merger: " << file << std::endl;
      return false;
    }
  }

  if (!merger.Merge()) {
    std::cerr << "Failed to merge ROOT files into " << output_file << std::endl;
    return false;
  }

  return true;
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

  const TString temp_output_dir = TemporaryOutputDir(output_dir, run_prefix);
  const TString merged_output_file = MergedOutputName(output_dir, run_prefix);
  std::vector<TString> converted_files;

  std::cout << "Found " << files.size() << " input file(s)." << std::endl;
  for (const auto &file : files) {
    const TString converted_file = OutputName(temp_output_dir, file);
    ConvertFile(file, converted_file, mode,
                zerosuppression, overwrite, getbadbcid);
    converted_files.push_back(converted_file);
  }

  std::cout << "Merging " << converted_files.size()
            << " ROOT file(s) into " << merged_output_file << std::endl;
  if (!MergeRootFiles(converted_files, merged_output_file, overwrite)) {
    gSystem->Exit(3);
  }

  for (const auto &converted_file : converted_files) {
    gSystem->Unlink(converted_file);
  }
  gSystem->Exec(TString::Format("rmdir %s >/dev/null 2>&1",
                                temp_output_dir.Data()));
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
