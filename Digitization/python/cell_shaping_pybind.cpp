#include "Shaping_FastVersion.hh"

#include <cstdint>
#include <random>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

py::dict resultToDict(const siwecal::CellShapingResult &result) {
  py::dict out;
  out["pass_threshold"] = result.passThreshold;
  out["energy_gev"] = result.energyGeV;
  out["time_ns"] = result.timeNs;
  out["raw_energy_gev"] = result.rawEnergyGeV;
  out["raw_first_time_ns"] = result.rawFirstTimeNs;
  out["slow_signal_mip"] = result.slowSignalMIP;
  out["fast_max_mip"] = result.fastMaxMIP;
  out["slow_max_mip"] = result.slowMaxMIP;
  out["t_max_fast_ns"] = result.tMaxFastNs;
  out["t_max_slow_ns"] = result.tMaxSlowNs;
  return out;
}

py::dict pointToDict(const siwecal::ShapingFastVersionPoint &point) {
  py::dict out;
  out["time_ns"] = point.timeNs;
  out["signal_mip"] = point.signalMIP;
  return out;
}

py::dict shapingFastVersionToDict(
    const siwecal::ShapingFastVersionResult &points) {
  py::dict out;
  out["fast_peak"] = pointToDict(points.fastPeak);
  out["slow_peak"] = pointToDict(points.slowPeak);
  out["trigger"] = pointToDict(points.trigger);
  out["slow_sample"] = pointToDict(points.slowSample);
  return out;
}

} // namespace

PYBIND11_MODULE(cell_shaping_cpp, m) {
  m.doc() = "Python bindings for the SiWECAL cell shaping helper";

  py::class_<siwecal::CellShapingConfig>(m, "CellShapingConfig")
      .def(py::init<>())
      .def_readwrite("mip_value_gev", &siwecal::CellShapingConfig::mipValueGeV)
      .def_readwrite("mip_threshold",
                     &siwecal::CellShapingConfig::mipThreshold)
      .def_readwrite("delay_ns", &siwecal::CellShapingConfig::delayNs)
      .def_readwrite("tau_fast_ns", &siwecal::CellShapingConfig::tauFastNs)
      .def_readwrite("tau_slow_ns", &siwecal::CellShapingConfig::tauSlowNs)
      .def_readwrite("order_fast", &siwecal::CellShapingConfig::orderFast)
      .def_readwrite("order_slow", &siwecal::CellShapingConfig::orderSlow)
      .def_readwrite("fast_window_ns",
                     &siwecal::CellShapingConfig::fastWindowNs)
      .def_readwrite("slow_window_ns",
                     &siwecal::CellShapingConfig::slowWindowNs)
      .def_readwrite("fast_noise_mip",
                     &siwecal::CellShapingConfig::fastNoiseMIP)
      .def_readwrite("slow_noise_mip",
                     &siwecal::CellShapingConfig::slowNoiseMIP)
      .def_readwrite("peak_search_bins",
                     &siwecal::CellShapingConfig::peakSearchBins)
      .def_readwrite("refine_iterations",
                     &siwecal::CellShapingConfig::refineIterations)
      .def_readwrite("trigger_search_bins",
                     &siwecal::CellShapingConfig::triggerSearchBins);

  m.def(
      "shaping_fast_version",
      [](const std::vector<double> &stepEnergyGeV,
         const std::vector<double> &stepTimeNs,
         const siwecal::CellShapingConfig &cfg, std::uint64_t seed) {
        std::mt19937_64 rng(seed);
        return shapingFastVersionToDict(
            siwecal::shapingFastVersion(stepEnergyGeV, stepTimeNs, cfg, rng));
      },
      py::arg("step_energy_gev"), py::arg("step_time_ns"), py::arg("cfg"),
      py::arg("seed") = 5489);

  m.def(
      "shape_cell_steps",
      [](const std::vector<double> &stepEnergyGeV,
         const std::vector<double> &stepTimeNs,
         const siwecal::CellShapingConfig &cfg, std::uint64_t seed) {
        std::mt19937_64 rng(seed);
        return resultToDict(
            siwecal::shapeCellSteps(stepEnergyGeV, stepTimeNs, cfg, rng));
      },
      py::arg("step_energy_gev"), py::arg("step_time_ns"), py::arg("cfg"),
      py::arg("seed") = 5489);
}
