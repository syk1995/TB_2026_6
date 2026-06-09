#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace siwecal {

struct CellShapingConfig {
  double mipValueGeV = 0.0001472;
  double mipThreshold = 0.5;
  double delayNs = 160.0;
  double tauFastNs = 30.0;
  double tauSlowNs = 180.0;
  int orderFast = 2;
  int orderSlow = 2;
  double fastWindowNs = 200.0;
  double slowWindowNs = 500.0;
  double fastNoiseMIP = 1.0 / 30.0;
  double slowNoiseMIP = 1.0 / 12.0;
  int peakSearchBins = 64;
  int refineIterations = 48;
  int triggerSearchBins = 64;
};

struct CellShapingResult {
  bool passThreshold = false;
  double energyGeV = 0.0;
  double timeNs = -1.0;
  double rawEnergyGeV = 0.0;
  double rawFirstTimeNs = -1.0;
  double slowSignalMIP = 0.0;
  double fastMaxMIP = 0.0;
  double slowMaxMIP = 0.0;
  double tMaxFastNs = -1.0;
  double tMaxSlowNs = -1.0;
};

namespace detail {

inline double factorial(int n) {
  double out = 1.0;
  for (int i = 2; i <= n; ++i) {
    out *= i;
  }
  return out;
}

inline double crRcResponse(double timeNs,
                           const std::vector<double> &stepEnergyGeV,
                           const std::vector<double> &stepTimeNs,
                           double mipValueGeV, double tauNs, int order) {
  if (order <= 0) {
    throw std::invalid_argument("CR-RC order must be positive");
  }
  const double tau = tauNs / static_cast<double>(order);
  const double norm = 4.0 / factorial(order);
  double signal = 0.0;

  for (std::size_t i = 0; i < stepEnergyGeV.size(); ++i) {
    const double stepAmplitudeMIP = stepEnergyGeV[i] / mipValueGeV;
    const double t = (timeNs - stepTimeNs[i]) / tau;
    if (t > 0.0) {
      signal += norm * stepAmplitudeMIP * std::pow(t, order) * std::exp(-t);
    }
  }
  return signal;
}

template <typename URBG> inline double sampleNoise(double sigmaMIP, URBG &rng) {
  if (sigmaMIP == 0.0) {
    return 0.0;
  }
  std::normal_distribution<double> noise(0.0, sigmaMIP);
  return noise(rng);
}

inline void validateInput(const std::vector<double> &stepEnergyGeV,
                          const std::vector<double> &stepTimeNs,
                          const CellShapingConfig &cfg) {
  if (stepEnergyGeV.size() != stepTimeNs.size()) {
    throw std::invalid_argument("stepEnergyGeV and stepTimeNs size mismatch");
  }
  if (cfg.mipValueGeV <= 0.0) {
    throw std::invalid_argument("mipValueGeV must be positive");
  }
  if (cfg.fastNoiseMIP < 0.0 || cfg.slowNoiseMIP < 0.0) {
    throw std::invalid_argument("noise sigma must be non-negative");
  }
  if (cfg.fastWindowNs <= 0.0 || cfg.slowWindowNs <= 0.0) {
    throw std::invalid_argument("shaping windows must be positive");
  }
  if (cfg.peakSearchBins <= 0 || cfg.refineIterations <= 0 ||
      cfg.triggerSearchBins <= 0) {
    throw std::invalid_argument("search bin and iteration counts must be positive");
  }
}

inline void fillRawResult(CellShapingResult &result,
                          const std::vector<double> &stepEnergyGeV,
                          const std::vector<double> &stepTimeNs) {
  result.rawEnergyGeV =
      std::accumulate(stepEnergyGeV.begin(), stepEnergyGeV.end(), 0.0);
  result.rawFirstTimeNs =
      *std::min_element(stepTimeNs.begin(), stepTimeNs.end());
}

inline bool rawEnergyBelowThreshold(const std::vector<double> &stepEnergyGeV,
                                    const CellShapingConfig &cfg) {
  const double rawEnergyGeV =
      std::accumulate(stepEnergyGeV.begin(), stepEnergyGeV.end(), 0.0);
  return rawEnergyGeV < cfg.mipThreshold * cfg.mipValueGeV;
}

struct ShapingPoint {
  double timeNs = -1.0;
  double signalMIP = 0.0;
};

template <typename Response>
inline ShapingPoint findPeak(Response &&response, double windowNs,
                             int searchBins, int refineIterations) {
  double bestTime = 0.0;
  double bestSignal = response(0.0);
  int bestIndex = 0;

  for (int i = 1; i <= searchBins; ++i) {
    const double time = windowNs * static_cast<double>(i) / searchBins;
    const double signal = response(time);
    if (signal > bestSignal) {
      bestSignal = signal;
      bestTime = time;
      bestIndex = i;
    }
  }

  if (bestIndex == 0 || bestIndex == searchBins) {
    return {bestTime, bestSignal};
  }

  double left =
      windowNs * static_cast<double>(bestIndex - 1) / searchBins;
  double right =
      windowNs * static_cast<double>(bestIndex + 1) / searchBins;
  constexpr double invPhi = 0.6180339887498948482;

  double c = right - invPhi * (right - left);
  double d = left + invPhi * (right - left);
  double fc = response(c);
  double fd = response(d);

  for (int i = 0; i < refineIterations; ++i) {
    if (fc < fd) {
      left = c;
      c = d;
      fc = fd;
      d = left + invPhi * (right - left);
      fd = response(d);
    } else {
      right = d;
      d = c;
      fd = fc;
      c = right - invPhi * (right - left);
      fc = response(c);
    }
  }

  const double peakTime = 0.5 * (left + right);
  return {peakTime, response(peakTime)};
}

template <typename Response>
inline double findFirstThresholdCrossing(Response &&response, double threshold,
                                         double stopTimeNs, int searchBins,
                                         int refineIterations) {
  if (stopTimeNs < 0.0) {
    return -1.0;
  }

  double leftTime = 0.0;
  double leftValue = response(leftTime) - threshold;
  if (leftValue >= 0.0) {
    return leftTime;
  }

  for (int i = 1; i <= searchBins; ++i) {
    const double rightTime = stopTimeNs * static_cast<double>(i) / searchBins;
    const double rightValue = response(rightTime) - threshold;
    if (rightValue >= 0.0) {
      double low = leftTime;
      double high = rightTime;
      for (int j = 0; j < refineIterations; ++j) {
        const double mid = 0.5 * (low + high);
        if (response(mid) >= threshold) {
          high = mid;
        } else {
          low = mid;
        }
      }
      return high;
    }
    leftTime = rightTime;
    leftValue = rightValue;
  }

  return -1.0;
}

template <typename URBG>
inline CellShapingResult
computeCellShaping(const std::vector<double> &stepEnergyGeV,
                   const std::vector<double> &stepTimeNs,
                   const CellShapingConfig &cfg, URBG &rng) {
  validateInput(stepEnergyGeV, stepTimeNs, cfg);

  CellShapingResult result;
  if (stepEnergyGeV.empty()) {
    return result;
  }
  fillRawResult(result, stepEnergyGeV, stepTimeNs);
  if (result.rawEnergyGeV < cfg.mipThreshold * cfg.mipValueGeV) {
    return result;
  }

  const auto fastClean = [&](double timeNs) {
    return crRcResponse(timeNs, stepEnergyGeV, stepTimeNs, cfg.mipValueGeV,
                        cfg.tauFastNs, cfg.orderFast);
  };
  const auto slowClean = [&](double timeNs) {
    return crRcResponse(timeNs, stepEnergyGeV, stepTimeNs, cfg.mipValueGeV,
                        cfg.tauSlowNs, cfg.orderSlow);
  };

  ShapingPoint fastPeak =
      findPeak(fastClean, cfg.fastWindowNs, cfg.peakSearchBins,
               cfg.refineIterations);
  fastPeak.signalMIP += sampleNoise(cfg.fastNoiseMIP, rng);
  ShapingPoint slowPeak =
      findPeak(slowClean, cfg.slowWindowNs, cfg.peakSearchBins,
               cfg.refineIterations);
  slowPeak.signalMIP += sampleNoise(cfg.slowNoiseMIP, rng);

  result.tMaxFastNs = fastPeak.timeNs;
  result.fastMaxMIP = fastPeak.signalMIP;
  result.tMaxSlowNs = slowPeak.timeNs;
  result.slowMaxMIP = slowPeak.signalMIP;

  if (result.fastMaxMIP < cfg.mipThreshold) {
    return result;
  }

  const double triggerTime =
      findFirstThresholdCrossing(fastClean, cfg.mipThreshold, fastPeak.timeNs,
                                 cfg.triggerSearchBins, cfg.refineIterations);
  if (triggerTime < 0.0) {
    return result;
  }
  result.timeNs = triggerTime;

  result.slowSignalMIP =
      slowClean(result.timeNs + cfg.delayNs) + sampleNoise(cfg.slowNoiseMIP, rng);
  if (result.slowSignalMIP < cfg.mipThreshold) {
    return result;
  }

  result.passThreshold = true;
  result.energyGeV = result.slowSignalMIP * cfg.mipValueGeV;
  return result;
}

} // namespace detail

template <typename URBG>
inline CellShapingResult
shapeCellSteps(const std::vector<double> &stepEnergyGeV,
               const std::vector<double> &stepTimeNs,
               const CellShapingConfig &cfg, URBG &rng) {
  return detail::computeCellShaping(stepEnergyGeV, stepTimeNs, cfg, rng);
}

inline CellShapingResult
shapeCellSteps(const std::vector<double> &stepEnergyGeV,
               const std::vector<double> &stepTimeNs,
               const CellShapingConfig &cfg = {}) {
  thread_local std::mt19937_64 rng(std::random_device{}());
  return shapeCellSteps(stepEnergyGeV, stepTimeNs, cfg, rng);
}

template <typename URBG>
inline CellShapingResult
shapeCellSteps(const std::vector<std::pair<double, double>> &stepEnergyTimeGeVNs,
               const CellShapingConfig &cfg, URBG &rng) {
  std::vector<double> energies;
  std::vector<double> times;
  energies.reserve(stepEnergyTimeGeVNs.size());
  times.reserve(stepEnergyTimeGeVNs.size());
  for (const auto &[energyGeV, timeNs] : stepEnergyTimeGeVNs) {
    energies.push_back(energyGeV);
    times.push_back(timeNs);
  }
  return shapeCellSteps(energies, times, cfg, rng);
}

inline CellShapingResult
shapeCellSteps(const std::vector<std::pair<double, double>> &stepEnergyTimeGeVNs,
               const CellShapingConfig &cfg = {}) {
  thread_local std::mt19937_64 rng(std::random_device{}());
  return shapeCellSteps(stepEnergyTimeGeVNs, cfg, rng);
}

} // namespace siwecal
