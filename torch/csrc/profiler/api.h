#pragma once

#include <ATen/ATen.h>
#include <ATen/record_function.h>
#include <torch/csrc/Export.h>

struct CUevent_st;

namespace torch {
namespace profiler {
namespace impl {

// ----------------------------------------------------------------------------
// -- Profiler Config ---------------------------------------------------------
// ----------------------------------------------------------------------------
enum class C10_API_ENUM ActivityType {
  CPU = 0,
  CUDA, // CUDA kernels, runtime
  NUM_KINETO_ACTIVITIES, // must be the last one
};

enum class C10_API_ENUM ProfilerState {
  Disabled = 0,
  CPU, // CPU-only profiling
  CUDA, // CPU + CUDA events
  NVTX, // only emit NVTX markers
  KINETO, // use libkineto
  KINETO_GPU_FALLBACK, // use CUDA events when CUPTI is not available
  NUM_PROFILER_STATES, // must be the last one
};

struct TORCH_API ProfilerConfig {
  explicit ProfilerConfig(
      ProfilerState state,
      bool report_input_shapes = false,
      bool profile_memory = false,
      bool with_stack = false,
      bool with_flops = false,
      bool with_modules = false)
      : state(state),
        report_input_shapes(report_input_shapes),
        profile_memory(profile_memory),
        with_stack(with_stack),
        with_flops(with_flops),
        with_modules(with_modules) {}
  ~ProfilerConfig() = default;
  ProfilerState state;
  bool report_input_shapes;
  bool profile_memory;
  bool with_stack;
  bool with_flops;
  bool with_modules;

  // Returns IValues corresponding to ProfilerConfig struct, to be used for
  // serialization.
  at::IValue toIValue() const;

  // Reconstructs a ProfilerConfig from IValues given by toIValue.
  static ProfilerConfig fromIValue(const at::IValue& profilerConfigIValue);
};

struct TORCH_API ProfilerThreadLocalStateBase
    : public c10::MemoryReportingInfoBase {
  explicit ProfilerThreadLocalStateBase(const ProfilerConfig& config)
      : c10::MemoryReportingInfoBase(), config_(config) {}
  ~ProfilerThreadLocalStateBase() override = default;

  const ProfilerConfig& config() const {
    return config_;
  }

  void setCallbackHandle(at::CallbackHandle handle) {
    handle_ = handle;
  }

  at::CallbackHandle callbackHandle() const {
    return handle_;
  }

  bool hasCallbackHandle() {
    return handle_ > 0;
  }

  bool memoryProfilingEnabled() const override {
    return config_.profile_memory;
  }

 protected:
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  std::mutex state_mutex_;
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  ProfilerConfig config_ = ProfilerConfig(ProfilerState::Disabled);
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  at::CallbackHandle handle_ = 0;
};

// Returns if the profiler is currently enabled in the current thread.
TORCH_API bool profilerEnabled();

// ----------------------------------------------------------------------------
// -- CUDA --------------------------------------------------------------------
// ----------------------------------------------------------------------------
using CUDAEventStub = std::shared_ptr<CUevent_st>;

struct TORCH_API CUDAStubs {
  virtual void record(int* device, CUDAEventStub* event, int64_t* cpu_ns)
      const = 0;
  virtual float elapsed(const CUDAEventStub* event, const CUDAEventStub* event2)
      const = 0;
  virtual void nvtxMarkA(const char* name) const = 0;
  virtual void nvtxRangePushA(const char* name) const = 0;
  virtual void nvtxRangePop() const = 0;
  virtual bool enabled() const {
    return false;
  }
  virtual void onEachDevice(std::function<void(int)> op) const = 0;
  virtual void synchronize() const = 0;
  virtual ~CUDAStubs();
};

TORCH_API void registerCUDAMethods(CUDAStubs* stubs);
TORCH_API const CUDAStubs* cudaStubs();

} // namespace impl
} // namespace profiler
} // namespace torch

// There are some components which use these symbols. Until we migrate them
// we have to mirror them in the old autograd namespace.
namespace torch {
namespace autograd {
namespace profiler {
using torch::profiler::impl::ActivityType;
using torch::profiler::impl::ProfilerConfig;
using torch::profiler::impl::ProfilerState;
using torch::profiler::impl::profilerEnabled;
} // namespace profiler
} // namespace autograd
} // namespace torch
