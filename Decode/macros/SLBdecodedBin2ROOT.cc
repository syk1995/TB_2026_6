#ifndef SLBdecodedBin2ROOT_CC
#define SLBdecodedBin2ROOT_CC

#include <TFile.h>
#include <TList.h>
#include <TString.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TTree.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#define NB_OF_SKIROCS_PER_ASU 16
#define NB_OF_CHANNELS_IN_SKIROC 64
#define NB_OF_SCAS_IN_SKIROC 15
#define SLBDEPTH 15

namespace decoded_bin {

constexpr unsigned char kMagic[4] = {0xee, 0xee, 0xee, 0xee};
constexpr size_t kFrameHeaderSize = 50;
constexpr size_t kSingleEventHeaderSize = 11;
constexpr size_t kChannelRecordSize = 9;
constexpr size_t kSingleEventSize =
    kSingleEventHeaderSize + NB_OF_CHANNELS_IN_SKIROC * kChannelRecordSize;

template <typename T>
T ReadLE(const std::vector<unsigned char> &buffer, size_t offset) {
  if (offset + sizeof(T) > buffer.size()) {
    throw std::out_of_range("ReadLE beyond buffer");
  }
  T value;
  std::memcpy(&value, buffer.data() + offset, sizeof(T));
  return value;
}

bool IsMagicAt(const std::vector<unsigned char> &buffer, size_t offset) {
  return offset + 4 <= buffer.size() &&
         std::equal(kMagic, kMagic + 4, buffer.begin() + offset);
}

struct FileBuffer {
  TString path;
  std::vector<unsigned char> data;
};

struct FrameRef {
  int file_index = -1;
  size_t offset = 0;
  int size = 0;
  int chip = -1;
  int slab_add = 0;
  int cycle_id = -1;
  unsigned int transmit_id = 0;
  unsigned int start_acq = 0;
  unsigned int raw_tsd = 0;
  unsigned int raw_avdd0 = 0;
  unsigned int raw_avdd1 = 0;
  float tsd = 0;
  float avdd0 = 0;
  float avdd1 = 0;
};

}  // namespace decoded_bin

class SLBdecodedBin2ROOT {
 public:
  bool ReadFiles(const std::vector<TString> &input_files,
                 TString output_file = "default",
                 bool overwrite = true,
                 bool zerosuppression = false);

 private:
  void Initialisation();
  void TreeInit();
  void FillFrame(const decoded_bin::FileBuffer &file,
                 const decoded_bin::FrameRef &frame,
                 bool zerosuppression);
  void FillCycle(const std::vector<decoded_bin::FileBuffer> &files,
                 const std::vector<decoded_bin::FrameRef> &frames,
                 int cycle_id,
                 bool zerosuppression);

  TFile *fout = nullptr;
  TTree *tree = nullptr;

  int _bcid[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC];
  int _corrected_bcid[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC];
  int _badbcid[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC];
  int _nhits[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC];
  int _adc_low[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC]
              [NB_OF_CHANNELS_IN_SKIROC];
  int _adc_high[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC]
               [NB_OF_CHANNELS_IN_SKIROC];
  int _autogainbit_low[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC]
                      [NB_OF_CHANNELS_IN_SKIROC];
  int _autogainbit_high[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC]
                       [NB_OF_CHANNELS_IN_SKIROC];
  int _hitbit_low[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC]
                 [NB_OF_CHANNELS_IN_SKIROC];
  int _hitbit_high[SLBDEPTH][NB_OF_SKIROCS_PER_ASU][NB_OF_SCAS_IN_SKIROC]
                  [NB_OF_CHANNELS_IN_SKIROC];
  int _numCol[SLBDEPTH][NB_OF_SKIROCS_PER_ASU];
  int _chipId[SLBDEPTH][NB_OF_SKIROCS_PER_ASU];
  int _slot[SLBDEPTH];
  int _slboard_id[SLBDEPTH];
  int _n_slboards;
  int _acqNumber;
  double _startACQ[SLBDEPTH];
  int _rawTSD[SLBDEPTH];
  float _TSD[SLBDEPTH];
  int _rawAVDD0[SLBDEPTH];
  int _rawAVDD1[SLBDEPTH];
  float _AVDD0[SLBDEPTH];
  float _AVDD1[SLBDEPTH];
};

void SLBdecodedBin2ROOT::Initialisation() {
  fout->cd();
  tree = new TTree("siwecaldecoded", "siwecaldecoded");

  TString name;
  tree->Branch("acqNumber", &_acqNumber, "acqNumber/I");
  tree->Branch("n_slboards", &_n_slboards, "n_slboards/I");

  name = TString::Format("slot[%i]/I", SLBDEPTH);
  tree->Branch("slot", _slot, name);
  name = TString::Format("slboard_id[%i]/I", SLBDEPTH);
  tree->Branch("slboard_id", _slboard_id, name);
  name = TString::Format("chipid[%i][%i]/I", SLBDEPTH, NB_OF_SKIROCS_PER_ASU);
  tree->Branch("chipid", _chipId, name);
  name = TString::Format("nColumns[%i][%i]/I", SLBDEPTH, NB_OF_SKIROCS_PER_ASU);
  tree->Branch("nColumns", _numCol, name);

  name = TString::Format("startACQ[%i]/F", SLBDEPTH);
  tree->Branch("startACQ", _startACQ, name);
  name = TString::Format("rawTSD[%i]/I", SLBDEPTH);
  tree->Branch("rawTSD", _rawTSD, name);
  name = TString::Format("TSD[%i]/F", SLBDEPTH);
  tree->Branch("TSD", _TSD, name);
  name = TString::Format("rawAVDD0[%i]/I", SLBDEPTH);
  tree->Branch("rawAVDD0", _rawAVDD0, name);
  name = TString::Format("rawAVDD1[%i]/I", SLBDEPTH);
  tree->Branch("rawAVDD1", _rawAVDD1, name);
  name = TString::Format("AVDD0[%i]/F", SLBDEPTH);
  tree->Branch("AVDD0", _AVDD0, name);
  name = TString::Format("AVDD1[%i]/F", SLBDEPTH);
  tree->Branch("AVDD1", _AVDD1, name);

  name = TString::Format("bcid[%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC);
  tree->Branch("bcid", _bcid, name);
  name = TString::Format("corrected_bcid[%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC);
  tree->Branch("corrected_bcid", _corrected_bcid, name);
  name = TString::Format("badbcid[%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC);
  tree->Branch("badbcid", _badbcid, name);
  name = TString::Format("nhits[%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC);
  tree->Branch("nhits", _nhits, name);

  name = TString::Format("adc_low[%i][%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC,
                         NB_OF_CHANNELS_IN_SKIROC);
  tree->Branch("adc_low", _adc_low, name);
  name = TString::Format("adc_high[%i][%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC,
                         NB_OF_CHANNELS_IN_SKIROC);
  tree->Branch("adc_high", _adc_high, name);
  name = TString::Format("autogainbit_low[%i][%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC,
                         NB_OF_CHANNELS_IN_SKIROC);
  tree->Branch("autogainbit_low", _autogainbit_low, name);
  name = TString::Format("autogainbit_high[%i][%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC,
                         NB_OF_CHANNELS_IN_SKIROC);
  tree->Branch("autogainbit_high", _autogainbit_high, name);
  name = TString::Format("hitbit_low[%i][%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC,
                         NB_OF_CHANNELS_IN_SKIROC);
  tree->Branch("hitbit_low", _hitbit_low, name);
  name = TString::Format("hitbit_high[%i][%i][%i][%i]/I", SLBDEPTH,
                         NB_OF_SKIROCS_PER_ASU, NB_OF_SCAS_IN_SKIROC,
                         NB_OF_CHANNELS_IN_SKIROC);
  tree->Branch("hitbit_high", _hitbit_high, name);
}

void SLBdecodedBin2ROOT::TreeInit() {
  for (int isl = 0; isl < SLBDEPTH; isl++) {
    _slot[isl] = -1;
    _slboard_id[isl] = -1;
    _startACQ[isl] = -1;
    _rawTSD[isl] = -1;
    _TSD[isl] = -1;
    _rawAVDD0[isl] = -1;
    _rawAVDD1[isl] = -1;
    _AVDD0[isl] = -1;
    _AVDD1[isl] = -1;
    for (int chip = 0; chip < NB_OF_SKIROCS_PER_ASU; chip++) {
      _chipId[isl][chip] = -999;
      _numCol[isl][chip] = 0;
      for (int sca = 0; sca < NB_OF_SCAS_IN_SKIROC; sca++) {
        _bcid[isl][chip][sca] = -999;
        _corrected_bcid[isl][chip][sca] = -999;
        _badbcid[isl][chip][sca] = -999;
        _nhits[isl][chip][sca] = -999;
        for (int ch = 0; ch < NB_OF_CHANNELS_IN_SKIROC; ch++) {
          _adc_low[isl][chip][sca][ch] = -999;
          _adc_high[isl][chip][sca][ch] = -999;
          _autogainbit_low[isl][chip][sca][ch] = -999;
          _autogainbit_high[isl][chip][sca][ch] = -999;
          _hitbit_low[isl][chip][sca][ch] = -999;
          _hitbit_high[isl][chip][sca][ch] = -999;
        }
      }
    }
  }
  _n_slboards = -1;
  _acqNumber = -1;
}

void SLBdecodedBin2ROOT::FillFrame(const decoded_bin::FileBuffer &file,
                                   const decoded_bin::FrameRef &frame,
                                   bool zerosuppression) {
  using namespace decoded_bin;
  const auto &data = file.data;
  const int slab = frame.slab_add;
  const int chip = frame.chip;

  _n_slboards = SLBDEPTH;
  _slot[slab] = -1;
  _slboard_id[slab] = slab;
  _chipId[slab][chip] = chip;
  _startACQ[slab] = frame.start_acq;
  _rawTSD[slab] = static_cast<int>(frame.raw_tsd);
  _TSD[slab] = frame.tsd;
  _rawAVDD0[slab] = static_cast<int>(frame.raw_avdd0);
  _rawAVDD1[slab] = static_cast<int>(frame.raw_avdd1);
  _AVDD0[slab] = frame.avdd0;
  _AVDD1[slab] = frame.avdd1;

  int previous_bcid = -1000;
  int loop_bcid = 0;

  const size_t events_base = frame.offset + kFrameHeaderSize;
  for (int event_index = 0; event_index < frame.size; event_index++) {
    const size_t event_base = events_base + event_index * kSingleEventSize;
    const uint16_t bcid = ReadLE<uint16_t>(data, event_base + 4);
    const int sca_file = data[event_base + 6];
    const int nhits = static_cast<int>(ReadLE<uint32_t>(data, event_base + 7));
    const int sca = frame.size - (sca_file + 1);
    if (sca < 0 || sca >= NB_OF_SCAS_IN_SKIROC) {
      continue;
    }

    _bcid[slab][chip][sca] = bcid;
    _nhits[slab][chip][sca] = nhits;
    _numCol[slab][chip] = std::max(_numCol[slab][chip], sca + 1);
    if (_bcid[slab][chip][sca] > 0 &&
        _bcid[slab][chip][sca] - previous_bcid < 0) {
      loop_bcid++;
    }
    if (_bcid[slab][chip][sca] > 0) {
      _corrected_bcid[slab][chip][sca] =
          _bcid[slab][chip][sca] + loop_bcid * 4096;
      _badbcid[slab][chip][sca] = 0;
    }
    previous_bcid = bcid;

    int n_channels = zerosuppression ? nhits : NB_OF_CHANNELS_IN_SKIROC;
    n_channels = std::min(n_channels, NB_OF_CHANNELS_IN_SKIROC);
    const size_t channel_base = event_base + kSingleEventHeaderSize;
    for (int ichn = 0; ichn < n_channels; ichn++) {
      const size_t record = channel_base + ichn * kChannelRecordSize;
      const int channel = data[record];
      if (channel < 0 || channel >= NB_OF_CHANNELS_IN_SKIROC) {
        continue;
      }
      _adc_low[slab][chip][sca][channel] = ReadLE<uint16_t>(data, record + 1);
      _hitbit_low[slab][chip][sca][channel] = data[record + 3];
      _autogainbit_low[slab][chip][sca][channel] = data[record + 4];
      _adc_high[slab][chip][sca][channel] = ReadLE<uint16_t>(data, record + 5);
      _hitbit_high[slab][chip][sca][channel] = data[record + 7];
      _autogainbit_high[slab][chip][sca][channel] = data[record + 8];
    }
  }
}

void SLBdecodedBin2ROOT::FillCycle(
    const std::vector<decoded_bin::FileBuffer> &files,
    const std::vector<decoded_bin::FrameRef> &frames,
    int cycle_id,
    bool zerosuppression) {
  TreeInit();
  _acqNumber = cycle_id;
  for (const auto &frame : frames) {
    FillFrame(files.at(frame.file_index), frame, zerosuppression);
  }
  tree->Fill();
}

bool SLBdecodedBin2ROOT::ReadFiles(const std::vector<TString> &input_files,
                                   TString output_file,
                                   bool overwrite,
                                   bool zerosuppression) {
  using namespace decoded_bin;
  if (input_files.empty()) {
    std::cerr << "No decoded binary input files." << std::endl;
    return false;
  }
  if (output_file == "default") {
    output_file = TString::Format("%s.decoded.root", input_files.front().Data());
  }

  fout = new TFile(output_file, overwrite ? "recreate" : "create");
  if (!fout || !fout->IsOpen()) {
    std::cerr << "Cannot open output ROOT file: " << output_file << std::endl;
    return false;
  }
  Initialisation();

  std::vector<FileBuffer> files;
  std::map<int, std::vector<FrameRef>> frames_by_cycle;
  int total_frames = 0;
  int total_single_skiroc_events = 0;

  for (const auto &input : input_files) {
    std::ifstream in(input.Data(), std::ios::binary);
    if (!in) {
      std::cerr << "Cannot open input file: " << input << std::endl;
      return false;
    }
    FileBuffer file;
    file.path = input;
    file.data.assign(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
    const int file_index = files.size();
    std::cout << "Read decoded binary: " << input
              << " bytes=" << file.data.size() << std::endl;

    size_t pos = 0;
    while (pos + 4 < file.data.size()) {
      while (pos + 4 < file.data.size() && !IsMagicAt(file.data, pos)) {
        pos++;
      }
      if (pos + 4 >= file.data.size()) {
        break;
      }

      const size_t frame_start = pos + 4;
      if (frame_start + kFrameHeaderSize > file.data.size()) {
        break;
      }
      const int size = ReadLE<int32_t>(file.data, frame_start + 4);
      const int chip = file.data[frame_start + 8];
      const int skiroc = file.data[frame_start + 13];
      if (size < 0 || size > NB_OF_SCAS_IN_SKIROC || chip < 0 ||
          chip >= NB_OF_SKIROCS_PER_ASU || skiroc != chip) {
        pos++;
        continue;
      }
      const size_t frame_end =
          frame_start + kFrameHeaderSize + size * kSingleEventSize;
      if (frame_end > file.data.size()) {
        break;
      }

      bool channels_ok = true;
      for (int event_index = 0; event_index < size && channels_ok;
           event_index++) {
        const size_t event_base =
            frame_start + kFrameHeaderSize + event_index * kSingleEventSize;
        const size_t channel_base = event_base + kSingleEventHeaderSize;
        for (int ch = 0; ch < NB_OF_CHANNELS_IN_SKIROC; ch++) {
          if (file.data[channel_base + ch * kChannelRecordSize] != ch) {
            channels_ok = false;
            break;
          }
        }
      }
      if (!channels_ok) {
        pos++;
        continue;
      }

      int8_t slab_add_raw = static_cast<int8_t>(file.data[frame_start + 11]);
      int slab_add = slab_add_raw < 0 ? 0 : slab_add_raw;
      if (slab_add < 0 || slab_add >= SLBDEPTH) {
        slab_add = 0;
      }

      FrameRef frame;
      frame.file_index = file_index;
      frame.offset = frame_start;
      frame.size = size;
      frame.chip = chip;
      frame.slab_add = slab_add;
      frame.transmit_id = ReadLE<uint32_t>(file.data, frame_start + 14);
      frame.cycle_id = static_cast<int>(ReadLE<uint32_t>(file.data, frame_start + 18));
      frame.start_acq = ReadLE<uint32_t>(file.data, frame_start + 22);
      frame.raw_tsd = ReadLE<uint32_t>(file.data, frame_start + 26);
      frame.raw_avdd0 = ReadLE<uint32_t>(file.data, frame_start + 30);
      frame.raw_avdd1 = ReadLE<uint32_t>(file.data, frame_start + 34);
      frame.tsd = ReadLE<float>(file.data, frame_start + 38);
      frame.avdd0 = ReadLE<float>(file.data, frame_start + 42);
      frame.avdd1 = ReadLE<float>(file.data, frame_start + 46);

      frames_by_cycle[frame.cycle_id].push_back(frame);
      total_frames++;
      total_single_skiroc_events += size;
      pos = frame_end;
    }

    files.push_back(std::move(file));
  }

  for (const auto &item : frames_by_cycle) {
    FillCycle(files, item.second, item.first, zerosuppression);
  }

  fout->cd();
  fout->Write(0);
  fout->Close();

  std::cout << "Decoded binary frames: " << total_frames
            << " single_skiroc_events=" << total_single_skiroc_events
            << " cycles=" << frames_by_cycle.size()
            << " output=" << output_file << std::endl;
  return true;
}

namespace {

TString JoinPath(TString directory, TString name) {
  if (!directory.EndsWith("/")) {
    directory += "/";
  }
  return directory + name;
}

std::vector<TString> ListDecodedBinFiles(TString run_dir) {
  std::vector<TString> files;
  TSystemDirectory dir(run_dir, run_dir);
  TList *items = dir.GetListOfFiles();
  if (!items) {
    return files;
  }
  TSystemFile *item = nullptr;
  TIter next(items);
  while ((item = static_cast<TSystemFile *>(next()))) {
    TString name = item->GetName();
    if (item->IsDirectory()) {
      continue;
    }
    if (name.Contains(".bin") && !name.EndsWith(".root") &&
        !name.BeginsWith("converted_")) {
      files.push_back(JoinPath(run_dir, name));
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

}  // namespace

void ConvertDecodedBinRunDirectory(TString run_dir,
                                   TString output = "default",
                                   bool overwrite = true,
                                   bool zerosuppression = false) {
  std::vector<TString> files = ListDecodedBinFiles(run_dir);
  if (files.empty()) {
    std::cerr << "No decoded .bin files found in " << run_dir << std::endl;
    gSystem->Exit(2);
  }
  if (output == "default") {
    output = JoinPath(run_dir, "converted_" + TString(gSystem->BaseName(run_dir)) +
                                   "_decoded_bin.root");
  }

  SLBdecodedBin2ROOT converter;
  if (!converter.ReadFiles(files, output, overwrite, zerosuppression)) {
    gSystem->Exit(1);
  }
}

void ConvertDecodedBinFile(TString input,
                           TString output = "default",
                           bool overwrite = true,
                           bool zerosuppression = false) {
  std::vector<TString> files;
  files.push_back(input);
  SLBdecodedBin2ROOT converter;
  if (!converter.ReadFiles(files, output, overwrite, zerosuppression)) {
    gSystem->Exit(1);
  }
}

#endif
