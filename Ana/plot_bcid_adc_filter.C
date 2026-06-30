#include "TCanvas.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TLegend.h"
#include "TPaveText.h"
#include "TPad.h"
#include "TString.h"
#include "TSystem.h"
#include "TTree.h"
#include "TStyle.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kNSlab = 15;
constexpr int kNChip = 16;
constexpr int kNSca = 15;
constexpr int kNChannel = 64;
constexpr int kBcidBins = 512;
constexpr int kAdcBins = 512;
constexpr int kAdcMin = -1024;
constexpr int kAdcMax = 4096;
constexpr int kEventThreshold100 = 100;
constexpr int kEventThreshold150 = 150;
constexpr int kEventThreshold200 = 200;
constexpr int kLayerHitBins = 1025;
constexpr Long64_t kProgressEvery = 1000;
const char *kInputDataRoot = "/eos/experiment/drdcalo/siw-ecal/TB2026-06/Data/rundata_converted";
const char *kOutputDataRoot = "/afs/cern.ch/user/s/shiy/eos/TB2026-06/rundata_converted";

struct Histograms {
  TH1D *bcidBefore = nullptr;
  TH1D *bcidAfter = nullptr;
  TH1D *adcHighBefore = nullptr;
  TH1D *adcHighAfter = nullptr;
  TH1D *adcLowBefore = nullptr;
  TH1D *adcLowAfter = nullptr;
  TH2D *layerHit2D = nullptr;
  std::vector<TH1D *> layerAdcHitbit0;
  std::vector<TH1D *> layerAdcHitbit1;
  TH2D *eventSummary = nullptr;
};

struct Summary {
  Long64_t entriesRead = 0;
  Long64_t totalEntries = 0;
  long long totalScaSamples = 0;
  long long skippedScaSamples = 0;
  long long bcidBefore = 0;
  long long bcidAfter = 0;
  long long adcHighBefore = 0;
  long long adcHighAfter = 0;
  long long adcLowBefore = 0;
  long long adcLowAfter = 0;
  long long eventSummaryScaCount = 0;
  long long eventSummaryAbove100 = 0;
  long long eventSummaryAbove150 = 0;
  long long eventSummaryAbove200 = 0;
};

TString BuildOutputPdfPath(const TString &inputPath) {
  const TString canonicalInputRoot = gSystem->ExpandPathName(kInputDataRoot);
  const TString canonicalOutputRoot = gSystem->ExpandPathName(kOutputDataRoot);
  if (inputPath.BeginsWith(canonicalInputRoot)) {
    TString runStem = gSystem->BaseName(inputPath);
    runStem.ReplaceAll(".root", "");
    TString runTag = runStem;
    if (runTag.BeginsWith("raw_siwecal_")) {
      runTag.Remove(0, TString("raw_siwecal_").Length());
    }
    TString outputDir = canonicalOutputRoot + "/run" + runTag;
    gSystem->mkdir(outputDir, true);
    return outputDir + "/bcid_adc_filter.pdf";
  }
  return TString(gSystem->DirName(inputPath)) + "/bcid_adc_filter.pdf";
}

Histograms BookHistograms() {
  Histograms h;
  h.bcidBefore = new TH1D("bcid_before", "Raw BCID, all entries combined;bcid;Counts", kBcidBins, -1000.5, 4095.5);
  h.bcidAfter = new TH1D("bcid_after", "Raw BCID, all entries combined;bcid;Counts", kBcidBins, -1000.5, 4095.5);
  h.adcHighBefore = new TH1D("adc_high_before", "ADC High, all entries combined;adc high;Counts", kAdcBins, kAdcMin, kAdcMax);
  h.adcHighAfter = new TH1D("adc_high_after", "ADC High, all entries combined;adc high;Counts", kAdcBins, kAdcMin, kAdcMax);
  h.adcLowBefore = new TH1D("adc_low_before", "ADC Low, all entries combined;adc low;Counts", kAdcBins, kAdcMin, kAdcMax);
  h.adcLowAfter = new TH1D("adc_low_after", "ADC Low, all entries combined;adc low;Counts", kAdcBins, kAdcMin, kAdcMax);
  h.layerHit2D = new TH2D("layer_hit_number",
                          "Layer-by-layer hit number for bcid >= 0;Layer;Hit number (hitbit high = 1);Counts",
                          kNSlab,
                          -0.5,
                          kNSlab - 0.5,
                          kLayerHitBins,
                          -0.5,
                          kLayerHitBins - 0.5);
  h.eventSummary = new TH2D("event_total_hit_vs_weighted_layer",
                            "Event total hit number vs weighted layer position;sum(layer * N_{layer}) / N_{total};Total hit number",
                            100,
                            -0.5,
                            kNSlab - 0.5,
                            1000,
                            -0.5,
                            999.5);

  std::vector<TH1D *> all = {
      h.bcidBefore, h.bcidAfter, h.adcHighBefore, h.adcHighAfter, h.adcLowBefore, h.adcLowAfter};
  for (TH1D *hist : all) {
    hist->SetDirectory(nullptr);
    hist->SetLineWidth(2);
  }
  h.bcidBefore->SetLineColor(kBlue + 1);
  h.bcidAfter->SetLineColor(kRed + 1);
  h.adcHighBefore->SetLineColor(kBlue + 1);
  h.adcHighAfter->SetLineColor(kRed + 1);
  h.adcLowBefore->SetLineColor(kBlue + 1);
  h.adcLowAfter->SetLineColor(kRed + 1);
  h.layerHit2D->SetDirectory(nullptr);
  h.eventSummary->SetDirectory(nullptr);

  h.layerAdcHitbit0.reserve(kNSlab);
  h.layerAdcHitbit1.reserve(kNSlab);
  for (int slab = 0; slab < kNSlab; ++slab) {
    TH1D *h0 = new TH1D(Form("layer_%02d_hitbit0", slab),
                        Form("Layer %d: adc high by hitbit_high;adc high;Counts", slab),
                        kAdcBins,
                        kAdcMin,
                        kAdcMax);
    TH1D *h1 = new TH1D(Form("layer_%02d_hitbit1", slab),
                        Form("Layer %d: adc high by hitbit_high;adc high;Counts", slab),
                        kAdcBins,
                        kAdcMin,
                        kAdcMax);
    h0->SetDirectory(nullptr);
    h1->SetDirectory(nullptr);
    h0->SetLineWidth(2);
    h1->SetLineWidth(2);
    h0->SetLineColor(kBlue + 1);
    h1->SetLineColor(kRed + 1);
    h.layerAdcHitbit0.push_back(h0);
    h.layerAdcHitbit1.push_back(h1);
  }

  return h;
}

void DrawOverlay(TPad *pad, TH1D *before, TH1D *after) {
  pad->cd();
  pad->SetGrid();
  pad->SetLogy();
  double maxY = std::max(before->GetMaximum(), after->GetMaximum());
  if (maxY <= 0.0) {
    maxY = 1.0;
  }
  before->SetMinimum(0.5);
  before->SetMaximum(maxY * 5.0);
  before->Draw("hist");
  after->Draw("hist same");
  TLegend *legend = new TLegend(0.60, 0.75, 0.88, 0.88);
  legend->SetBorderSize(0);
  legend->SetFillStyle(0);
  legend->AddEntry(before, "Before removal", "l");
  legend->AddEntry(after, "After removal", "l");
  legend->Draw();
}

void DrawSummary(TPad *pad, const TString &inputPath, const TString &outputPath, const Summary &summary) {
  pad->cd();
  pad->SetGrid();
  TPaveText *box = new TPaveText(0.08, 0.08, 0.92, 0.92, "NDC");
  box->SetFillStyle(0);
  box->SetBorderSize(1);
  box->SetTextAlign(12);
  const double skippedFraction =
      summary.totalScaSamples > 0
          ? 100.0 * static_cast<double>(summary.skippedScaSamples) / static_cast<double>(summary.totalScaSamples)
          : 0.0;
  box->AddText("BCID / ADC filtering summary");
  box->AddText("");
  box->AddText(Form("Input ROOT : %s", inputPath.Data()));
  box->AddText(Form("Output PDF : %s", outputPath.Data()));
  box->AddText(Form("Entries read / total : %lld / %lld",
                    static_cast<long long>(summary.entriesRead),
                    static_cast<long long>(summary.totalEntries)));
  box->AddText(Form("Skipped slab-chip-sca samples : %lld / %lld (%.2f%%)",
                    summary.skippedScaSamples,
                    summary.totalScaSamples,
                    skippedFraction));
  box->AddText(Form("BCID entries before / after : %lld / %lld", summary.bcidBefore, summary.bcidAfter));
  box->AddText(Form("ADC high entries before / after : %lld / %lld", summary.adcHighBefore, summary.adcHighAfter));
  box->AddText(Form("ADC low entries before / after : %lld / %lld", summary.adcLowBefore, summary.adcLowAfter));
  box->AddText("SCA event table (Total hit number):");
  box->AddText(Form("  Total : %lld", summary.eventSummaryScaCount));
  box->AddText(Form("  > %d : %lld", kEventThreshold100, summary.eventSummaryAbove100));
  box->AddText(Form("  > %d : %lld", kEventThreshold150, summary.eventSummaryAbove150));
  box->AddText(Form("  > %d : %lld", kEventThreshold200, summary.eventSummaryAbove200));
  box->AddText("");
  box->AddText("Mask rule:");
  box->AddText("Skip the full slab-chip-sca sample when bcid < 0.");
  box->Draw();
}

void FillHistograms(TTree *tree, Long64_t maxEntries, Histograms &h, Summary &s) {
  std::cout << "[debug] FillHistograms: allocate buffers" << std::endl;
  std::vector<int> bcidBuffer(kNSlab * kNChip * kNSca);
  std::vector<int> adcHighBuffer(kNSlab * kNChip * kNSca * kNChannel);
  std::vector<int> adcLowBuffer(kNSlab * kNChip * kNSca * kNChannel);
  std::vector<int> hitbitHighBuffer(kNSlab * kNChip * kNSca * kNChannel);

  auto bcidAt = [&](int slab, int chip, int sca) -> int & {
    return bcidBuffer[((slab * kNChip) + chip) * kNSca + sca];
  };
  auto adcAt = [&](std::vector<int> &buffer, int slab, int chip, int sca, int channel) -> int & {
    return buffer[((((slab * kNChip) + chip) * kNSca) + sca) * kNChannel + channel];
  };

  std::cout << "[debug] FillHistograms: set branch addresses" << std::endl;
  tree->SetBranchAddress("bcid", bcidBuffer.data());
  tree->SetBranchAddress("adc_high", adcHighBuffer.data());
  tree->SetBranchAddress("adc_low", adcLowBuffer.data());
  tree->SetBranchAddress("hitbit_high", hitbitHighBuffer.data());

  std::cout << "[debug] FillHistograms: read entry count" << std::endl;
  s.totalEntries = tree->GetEntries();
  s.entriesRead = maxEntries > 0 ? std::min(s.totalEntries, maxEntries) : s.totalEntries;
  std::cout << "[debug] FillHistograms: start loop with " << s.entriesRead << " entries" << std::endl;

  for (Long64_t entry = 0; entry < s.entriesRead; ++entry) {
    if (entry % kProgressEvery == 0) {
      const double percent =
          s.entriesRead > 0 ? 100.0 * static_cast<double>(entry) / static_cast<double>(s.entriesRead) : 0.0;
      std::cout << "[progress] entry " << entry << " / " << s.entriesRead
                << " (" << percent << "%)" << std::endl;
    }
    tree->GetEntry(entry);

    for (int sca = 0; sca < kNSca; ++sca) {
      int layerHits[kNSlab] = {0};

      for (int slab = 0; slab < kNSlab; ++slab) {
        for (int chip = 0; chip < kNChip; ++chip) {
          const bool keep = bcidAt(slab, chip, sca) >= 0;

          ++s.totalScaSamples;
          if (!keep) {
            ++s.skippedScaSamples;
          }

          h.bcidBefore->Fill(bcidAt(slab, chip, sca));
          ++s.bcidBefore;
          if (keep) {
            h.bcidAfter->Fill(bcidAt(slab, chip, sca));
            ++s.bcidAfter;
          }

          for (int channel = 0; channel < kNChannel; ++channel) {
            const int high = adcAt(adcHighBuffer, slab, chip, sca, channel);
            const int low = adcAt(adcLowBuffer, slab, chip, sca, channel);

            h.adcHighBefore->Fill(high);
            h.adcLowBefore->Fill(low);
            ++s.adcHighBefore;
            ++s.adcLowBefore;

            if (!keep) {
              continue;
            }

            h.adcHighAfter->Fill(high);
            h.adcLowAfter->Fill(low);
            ++s.adcHighAfter;
            ++s.adcLowAfter;

            const int hitbit = adcAt(hitbitHighBuffer, slab, chip, sca, channel);
            if (hitbit == 1) {
              ++layerHits[slab];
              h.layerAdcHitbit1[slab]->Fill(high);
            } else if (hitbit == 0) {
              h.layerAdcHitbit0[slab]->Fill(high);
            }
          }
        }
      }

      int totalHits = 0;
      double weightedLayer = 0.0;
      for (int slab = 0; slab < kNSlab; ++slab) {
        h.layerHit2D->Fill(slab, layerHits[slab]);
        totalHits += layerHits[slab];
        weightedLayer += static_cast<double>(slab) * static_cast<double>(layerHits[slab]);
      }
      if (totalHits > 0) {
        ++s.eventSummaryScaCount;
        if (totalHits > kEventThreshold100) ++s.eventSummaryAbove100;
        if (totalHits > kEventThreshold150) ++s.eventSummaryAbove150;
        if (totalHits > kEventThreshold200) ++s.eventSummaryAbove200;
        h.eventSummary->Fill(weightedLayer / static_cast<double>(totalHits), totalHits);
      }
    }
  }
  if (s.entriesRead > 0) {
    std::cout << "[progress] entry " << s.entriesRead << " / " << s.entriesRead
              << " (100%)" << std::endl;
  }
}

}  // namespace

void plot_bcid_adc_filter(const char *input_root, Long64_t max_entries = -1) {
  std::cout << "[debug] plot_bcid_adc_filter: entered" << std::endl;
  if (input_root == nullptr || std::string(input_root).empty()) {
    throw std::runtime_error("Input ROOT path is empty.");
  }

  std::cout << "[debug] plot_bcid_adc_filter: opening file" << std::endl;
  TString inputPath = input_root;
  TFile *file = TFile::Open(inputPath, "READ");
  if (file == nullptr || file->IsZombie()) {
    throw std::runtime_error(Form("Cannot open ROOT file: %s", inputPath.Data()));
  }

  std::cout << "[debug] plot_bcid_adc_filter: getting tree" << std::endl;
  TTree *tree = dynamic_cast<TTree *>(file->Get("siwecaldecoded"));
  if (tree == nullptr) {
    file->Close();
    throw std::runtime_error("Cannot find tree 'siwecaldecoded' in input file.");
  }

  gStyle->SetOptStat(0);
  Histograms h = BookHistograms();
  Summary s;
  FillHistograms(tree, max_entries, h, s);
  std::cout << "[pre-draw] Entries read / total           : " << s.entriesRead << " / " << s.totalEntries << std::endl;
  std::cout << "[pre-draw] Skipped slab-chip-sca samples  : " << s.skippedScaSamples << " / " << s.totalScaSamples << std::endl;
  std::cout << "[pre-draw] SCA event table (Total hit number)" << std::endl;
  std::cout << "[pre-draw]   Total : " << s.eventSummaryScaCount << std::endl;
  std::cout << "[pre-draw]   > " << kEventThreshold100 << " : " << s.eventSummaryAbove100 << std::endl;
  std::cout << "[pre-draw]   > " << kEventThreshold150 << " : " << s.eventSummaryAbove150 << std::endl;
  std::cout << "[pre-draw]   > " << kEventThreshold200 << " : " << s.eventSummaryAbove200 << std::endl;
  const TString outputPdf = BuildOutputPdfPath(inputPath);

  TCanvas *canvasOverview = new TCanvas("canvas_bcid_adc_filter", "BCID ADC Filter", 1400, 1000);
  canvasOverview->Divide(2, 2);
  DrawOverlay(static_cast<TPad *>(canvasOverview->cd(1)), h.bcidBefore, h.bcidAfter);
  DrawOverlay(static_cast<TPad *>(canvasOverview->cd(2)), h.adcHighBefore, h.adcHighAfter);
  DrawOverlay(static_cast<TPad *>(canvasOverview->cd(3)), h.adcLowBefore, h.adcLowAfter);
  DrawSummary(static_cast<TPad *>(canvasOverview->cd(4)), inputPath, outputPdf, s);

  TCanvas *canvasLayerHits = new TCanvas("canvas_layer_hits", "Layer Hit Number", 1200, 900);
  canvasLayerHits->SetGrid();
  canvasLayerHits->SetLogz();
  h.layerHit2D->Draw("colz");

  TCanvas *canvasLayerHit1D = new TCanvas("canvas_layer_hit_1d", "Layer ADC High By Hitbit", 1600, 1000);
  canvasLayerHit1D->Divide(5, 3);
  for (int slab = 0; slab < kNSlab; ++slab) {
    TPad *pad = static_cast<TPad *>(canvasLayerHit1D->cd(slab + 1));
    pad->SetGrid();
    pad->SetLogy();
    double maxY = std::max(h.layerAdcHitbit0[slab]->GetMaximum(), h.layerAdcHitbit1[slab]->GetMaximum());
    if (maxY <= 0.0) {
      maxY = 1.0;
    }
    h.layerAdcHitbit0[slab]->SetMinimum(0.5);
    h.layerAdcHitbit0[slab]->SetMaximum(maxY * 5.0);
    h.layerAdcHitbit0[slab]->Draw("hist");
    h.layerAdcHitbit1[slab]->Draw("hist same");
    TLegend *legend = new TLegend(0.55, 0.75, 0.88, 0.88);
    legend->SetBorderSize(0);
    legend->SetFillStyle(0);
    legend->AddEntry(h.layerAdcHitbit0[slab], "hitbit_high = 0", "l");
    legend->AddEntry(h.layerAdcHitbit1[slab], "hitbit_high = 1", "l");
    legend->Draw();
  }

  TCanvas *canvasEventSummary = new TCanvas("canvas_event_summary", "Event Hit Summary", 1200, 900);
  canvasEventSummary->SetGrid();
  canvasEventSummary->SetLogz();
  h.eventSummary->Draw("colz");

  canvasOverview->Print(outputPdf + "(");
  canvasLayerHits->Print(outputPdf);
  canvasLayerHit1D->Print(outputPdf);
  canvasEventSummary->Print(outputPdf + ")");

  std::cout << "Wrote " << outputPdf << std::endl;
  std::cout << "Entries read / total           : " << s.entriesRead << " / " << s.totalEntries << std::endl;
  std::cout << "Skipped slab-chip-sca samples  : " << s.skippedScaSamples << " / " << s.totalScaSamples << std::endl;
  std::cout << "BCID entries before / after    : " << s.bcidBefore << " / " << s.bcidAfter << std::endl;
  std::cout << "ADC high entries before / after: " << s.adcHighBefore << " / " << s.adcHighAfter << std::endl;
  std::cout << "ADC low entries before / after : " << s.adcLowBefore << " / " << s.adcLowAfter << std::endl;
  std::cout << "SCA event table (Total hit number)" << std::endl;
  std::cout << "  Total : " << s.eventSummaryScaCount << std::endl;
  std::cout << "  > " << kEventThreshold100 << " : " << s.eventSummaryAbove100 << std::endl;
  std::cout << "  > " << kEventThreshold150 << " : " << s.eventSummaryAbove150 << std::endl;
  std::cout << "  > " << kEventThreshold200 << " : " << s.eventSummaryAbove200 << std::endl;

  file->Close();
}
