#pragma once

#include "CellShaping.hh"

#include <random>
#include <utility>
#include <vector>

namespace siwecal {

struct ShapingFastVersionPoint {
  double timeNs = -1.0;
  double signalMIP = 0.0;
};

struct ShapingFastVersionResult {
  ShapingFastVersionPoint fastPeak;
  ShapingFastVersionPoint slowPeak;
  ShapingFastVersionPoint trigger;
  ShapingFastVersionPoint slowSample;
};

namespace detail {

inline ShapingFastVersionPoint toShapingFastVersionPoint(
    const ShapingPoint &point) {
  return {point.timeNs, point.signalMIP};
}

} // namespace detail

template <typename URBG>
inline ShapingFastVersionResult
shapingFastVersion(const std::vector<double> &stepEnergyGeV,
                   const std::vector<double> &stepTimeNs,
                   const CellShapingConfig &cfg, URBG &rng) {
  detail::validateInput(stepEnergyGeV, stepTimeNs, cfg);

  ShapingFastVersionResult result;
  if (stepEnergyGeV.empty()) {
    return result;
  }
  if (detail::rawEnergyBelowThreshold(stepEnergyGeV, cfg)) {
    return result;
  }

  const auto fastClean = [&](double timeNs) {
    return detail::crRcResponse(timeNs, stepEnergyGeV, stepTimeNs,
                                cfg.mipValueGeV, cfg.tauFastNs,
                                cfg.orderFast);
  };
  const auto slowClean = [&](double timeNs) {
    return detail::crRcResponse(timeNs, stepEnergyGeV, stepTimeNs,
                                cfg.mipValueGeV, cfg.tauSlowNs,
                                cfg.orderSlow);
  };

  auto fastPeak = detail::findPeak(fastClean, cfg.fastWindowNs,
                                   cfg.peakSearchBins, cfg.refineIterations);
  fastPeak.signalMIP += detail::sampleNoise(cfg.fastNoiseMIP, rng);
  result.fastPeak = detail::toShapingFastVersionPoint(fastPeak);

  auto slowPeak = detail::findPeak(slowClean, cfg.slowWindowNs,
                                   cfg.peakSearchBins, cfg.refineIterations);
  slowPeak.signalMIP += detail::sampleNoise(cfg.slowNoiseMIP, rng);
  result.slowPeak = detail::toShapingFastVersionPoint(slowPeak);

  if (result.fastPeak.signalMIP < cfg.mipThreshold) {
    return result;
  }

  result.trigger.timeNs = detail::findFirstThresholdCrossing(
      fastClean, cfg.mipThreshold, result.fastPeak.timeNs,
      cfg.triggerSearchBins, cfg.refineIterations);
  if (result.trigger.timeNs < 0.0) {
    return result;
  }
  result.trigger.signalMIP = cfg.mipThreshold;

  result.slowSample.timeNs = result.trigger.timeNs + cfg.delayNs;
  result.slowSample.signalMIP =
      slowClean(result.slowSample.timeNs) +
      detail::sampleNoise(cfg.slowNoiseMIP, rng);
  return result;
}

inline ShapingFastVersionResult
shapingFastVersion(const std::vector<double> &stepEnergyGeV,
                   const std::vector<double> &stepTimeNs,
                   const CellShapingConfig &cfg = {}) {
  thread_local std::mt19937_64 rng(std::random_device{}());
  return shapingFastVersion(stepEnergyGeV, stepTimeNs, cfg, rng);
}

template <typename URBG>
inline ShapingFastVersionResult
shapingFastVersion(
    const std::vector<std::pair<double, double>> &stepEnergyTimeGeVNs,
    const CellShapingConfig &cfg, URBG &rng) {
  std::vector<double> energies;
  std::vector<double> times;
  energies.reserve(stepEnergyTimeGeVNs.size());
  times.reserve(stepEnergyTimeGeVNs.size());
  for (const auto &[energyGeV, timeNs] : stepEnergyTimeGeVNs) {
    energies.push_back(energyGeV);
    times.push_back(timeNs);
  }
  return shapingFastVersion(energies, times, cfg, rng);
}

inline ShapingFastVersionResult
shapingFastVersion(
    const std::vector<std::pair<double, double>> &stepEnergyTimeGeVNs,
    const CellShapingConfig &cfg = {}) {
  thread_local std::mt19937_64 rng(std::random_device{}());
  return shapingFastVersion(stepEnergyTimeGeVNs, cfg, rng);
}

} // namespace siwecal
