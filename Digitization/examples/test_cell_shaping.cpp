#include "CellShaping.hh"

#include <iostream>
#include <vector>

int main() {
  // Input format: one cell, vectors of GeV deposits and ns times from steps.
  std::vector<double> stepEnergyGeV = {
      2.26747829e-02, 1.20633305e-02, 3.44056671e-02,
      2.40233228e-02, 2.55296692e-02};
  std::vector<double> stepTimeNs = {
      0.68680886, 0.68704597, 0.68719071, 0.68769231, 0.68813894};

  siwecal::CellShapingConfig cfg;
  auto result = siwecal::shapeCellSteps(stepEnergyGeV, stepTimeNs, cfg);

  std::cout << "passThreshold " << result.passThreshold << "\n"
            << "rawEnergyGeV " << result.rawEnergyGeV << "\n"
            << "rawFirstTimeNs " << result.rawFirstTimeNs << "\n"
            << "digitizedEnergyGeV " << result.energyGeV << "\n"
            << "digitizedTimeNs " << result.timeNs << "\n"
            << "slowSignalMIP " << result.slowSignalMIP << "\n"
            << "fastMaxMIP " << result.fastMaxMIP << "\n"
            << "slowMaxMIP " << result.slowMaxMIP << "\n";
}
