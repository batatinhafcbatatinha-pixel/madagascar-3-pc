// Madagascar SDK graphics entry points recovered from its own decoded image.
// CPU bookkeeping remains AOT; draws/resolve/clear/present go to host D3D11.
#include "native_gpu.h"
#include "generated/default/madagascar3_init.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <rex/system/xthread.h>
#include <rex/thread.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <cstdlib>

namespace {
std::atomic<uint32_t> device{0};
std::atomic<bool> running{false};
rex::system::object_ref<rex::system::XHostThread> interrupt_thread;
void* window_handle = nullptr;
bool renderer_ready = false;
std::mutex callbacks_mutex;
struct Callback { uint32_t function, data; };
std::deque<Callback> callbacks;
thread_local m3gpu::UpDraw pending;
thread_local uint32_t pending_device = 0;
thread_local uint32_t suppress_draws = 0;
struct SuppressDraws { SuppressDraws() { ++suppress_draws; } ~SuppressDraws() { --suppress_draws; } };

uint8_t* Memory() { return rex::system::kernel_state()->memory()->virtual_membase(); }
uint32_t Load(uint32_t a) {
  return rex::memory::load_and_swap<uint32_t>(rex::memory::GuestPtr(Memory(), a));
}
void Store(uint32_t a, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(rex::memory::GuestPtr(Memory(), a), value);
}
void EnsureRenderer() {
  if (!renderer_ready) {
    renderer_ready = m3gpu::RendererInit(window_handle);
    if (!renderer_ready) throw std::runtime_error("Madagascar native hardware D3D11 renderer unavailable");
  }
}
uint32_t DisplayRead(void*, void*, uint32_t address) {
  switch ((address & 0xFFFF) / 4) {
    case 0x0F00: return 0x08100748;
    case 0x0F01: return 0x0000200E;
    case 0x194C: {
      const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count();
      return uint32_t((us % 16667) * 750 / 16667);
    }
    case 0x1951: return 1;  // handler 0x822D3668 tests vblank bit0
    case 0x1961: return (1280u << 16) | 720u;
    default: {
      static std::atomic<uint32_t> unknown{0};
      if (unknown.fetch_add(1) < 16) REXLOG_WARN("[m3native] display read not established: {:08X}", address);
      return 0;
    }
  }
}
void DisplayWrite(void*, void*, uint32_t address, uint32_t value) {
  static std::atomic<uint32_t> count{0};
  if (count.fetch_add(1) < 16) REXLOG_DEBUG("[m3native] display write {:08X}={:08X}", address, value);
}
void StartInterrupts() {
  if (running.exchange(true)) return;
  auto* ks = rex::system::kernel_state();
  interrupt_thread = rex::system::object_ref<rex::system::XHostThread>(
      new rex::system::XHostThread(ks, 128 * 1024, 0, []() -> int {
        auto* self = rex::system::XThread::GetCurrentThread();
        self->SetActiveCpu(2);
        auto* dispatcher = rex::system::kernel_state()->function_dispatcher();
        auto next = std::chrono::steady_clock::now();
        while (running.load()) {
          const uint32_t d = device.load();
          if (d) {
            for (;;) {
              Callback cb;
              {
                std::lock_guard guard(callbacks_mutex);
                if (callbacks.empty()) break;
                cb = callbacks.front(); callbacks.pop_front();
              }
              const uint32_t block = Load(d + 0x2B14);
              if (block) {
                Store(block + 0x10, cb.function);
                Store(block + 0x14, cb.data);
                uint64_t args[] = {1, d};
                dispatcher->ExecuteInterrupt(self->thread_state(), 0x822D3668, args, 2);
              }
            }
            auto now = std::chrono::steady_clock::now();
            if (now >= next) {
              next = now + std::chrono::microseconds(16667);
              uint64_t args[] = {0, d};
              dispatcher->ExecuteInterrupt(self->thread_state(), 0x822D3668, args, 2);
            }
          }
          rex::thread::Sleep(std::chrono::milliseconds(1));
        }
        return 0;
      }));
  interrupt_thread->set_name("Madagascar native VBlank");
  interrupt_thread->Create();
}
// Waits until `interval` vblank periods (1/60 s) have passed since the previous flip; a late
// frame restarts the schedule instead of rushing to catch up.
void PaceFlip(uint32_t interval) {
  using clock = std::chrono::steady_clock;
  static clock::time_point next{};
  const auto period = std::chrono::nanoseconds(16666667) * interval;
  auto now = clock::now();
  if (next.time_since_epoch().count() == 0 || now > next + period) next = now;
  while ((now = clock::now()) < next) {
    const auto left = next - now;
    if (left > std::chrono::milliseconds(2)) rex::thread::Sleep(std::chrono::milliseconds(1));
    else std::this_thread::yield();
  }
  next += period;
}
void FlushVertices(uint32_t d) {
  if (!pending.pending || pending_device != d) return;
  pending.pending = false;
  if (!suppress_draws) { EnsureRenderer(); m3gpu::RendererDrawUp(d, pending); }
}
}

namespace m3gpu {
void NativeGpuInstallMmio() {
  if (!rex::system::kernel_state()->memory()->AddVirtualMappedRange(
          0x7FC80000, 0xFFFF0000, 0x0000FFFF, nullptr, DisplayRead, DisplayWrite))
    throw std::runtime_error("Native display-register mapping failed");
}
void NativeGpuSetWindow(void* hwnd) { window_handle = hwnd; }
void NativeGpuShutdown() {
  running = false;
  if (interrupt_thread) {
    uint64_t timeout = uint64_t(-5000000);
    if (interrupt_thread->Wait(0, 0, 0, &timeout) != 0) interrupt_thread->Terminate(0);
    interrupt_thread.reset();
  }
}
}

REX_HOOK_RAW(sub_822D8788) {
  const uint32_t d = ctx.r3.u32;
  device = d;
  // +2B3D bit20 skips submission; bit02 writes CPU fences and bypasses waits.
  // These are established independently in 49D0/3C30/4280/4D10, not BF2 offsets.
  Memory()[d + 0x2B3D] |= 0x22;
  __imp__sub_822D8788(ctx, base);
  Memory()[d + 0x2B3D] |= 0x22;
  const uint32_t scratch = Load(d + 0x42F0);
  if (!scratch) throw std::runtime_error("Native command scratch buffer was not allocated");
  Store(d + 0x30, scratch);
  Store(d + 0x34, scratch + 0x12C0);
  Store(d + 0x38, scratch + 0x1220);
  EnsureRenderer();
  StartInterrupts();
}

// InsertCallback(device, command_pointer, flags, function, data): "run function(data) in the GPU
// interrupt once the GPU gets here". Hooked here rather than in its wrapper sub_822D5038 because
// Swap (0x822CF298) calls it directly for the flip callback 0x822CE880.
REX_HOOK_RAW(sub_822D43C0) {
  const Callback cb{ctx.r6.u32, ctx.r7.u32};
  __imp__sub_822D43C0(ctx, base);
  if (cb.function) { std::lock_guard guard(callbacks_mutex); callbacks.push_back(cb); }
}

REX_HOOK_RAW(sub_822DCC28) {
  const uint32_t d = ctx.r3.u32, shader = ctx.r5.u32, declaration = ctx.r7.u32;
  const uint32_t variant = ctx.r10.u32;
  __imp__sub_822DCC28(ctx, base);
  // This helper emits exactly one patched IM_LOAD_IMMEDIATE plus invalidate.
  const uint32_t meta = shader + Load(shader + 0x380 + variant * 8) + 0x368;
  const uint32_t bytes = Load(meta + 4), end = Load(d + 0x30);
  if (!bytes || bytes > 0x10000 || bytes & 3 || end < bytes + 8) return;
  const uint32_t begin = end - bytes + 4;
  const uint32_t header = Load(begin - 12);
  if (header >> 30 != 3 || ((header >> 8) & 0x7F) != 0x2B || Load(begin - 4) != bytes / 4)
    throw std::runtime_error("Patched native shader packet does not match recovered helper layout");
  std::lock_guard guard(m3gpu::ShaderCodeMutex());
  auto& captured = m3gpu::LoadedImmediateVertexShader();
  captured.device = d; captured.shader = shader; captured.declaration = declaration;
  const auto* source = rex::memory::GuestPtr(base, begin);
  captured.code.assign(source, source + bytes);
}

REX_HOOK_RAW(sub_822D0E20) {
  const uint32_t d = ctx.r3.u32, primitive = ctx.r4.u32, first = ctx.r5.u32, count = ctx.r6.u32;
  __imp__sub_822D0E20(ctx, base);
  if (!suppress_draws) { EnsureRenderer(); m3gpu::RendererDraw(d, primitive & 63, int32_t(first), 0, count, false); }
}
REX_HOOK_RAW(sub_822D1238) {
  const uint32_t d = ctx.r3.u32, primitive = ctx.r4.u32, first = ctx.r6.u32, count = ctx.r7.u32;
  const int32_t base_vertex = ctx.r5.s32;
  __imp__sub_822D1238(ctx, base);
  if (!suppress_draws) { EnsureRenderer(); m3gpu::RendererDraw(d, primitive & 63, base_vertex, first, count, true); }
}
REX_HOOK_RAW(sub_822D0960) {
  pending = m3gpu::UpDraw{}; pending_device = ctx.r3.u32;
  pending.prim = ctx.r4.u32 & 63; pending.vertex_count = ctx.r5.u32; pending.stride = ctx.r6.u32;
  __imp__sub_822D0960(ctx, base);
  pending.vertices = ctx.r3.u32; pending.pending = pending.vertices != 0;
}
REX_HOOK_RAW(sub_822D0950) {
  const uint32_t d = ctx.r3.u32;
  __imp__sub_822D0950(ctx, base);
  FlushVertices(d);
}
// Clear(device, rect_count, rects*, flags, color(ARGB), z(f1), stencil); flags bits 0-3 render
// targets, 0x10 depth, 0x20 stencil. Located from Copa 2014's 0x82468830 (same XDK library).
// The library flushes a newly set render target into its register shadow (RB_COLOR_INFO) only
// inside this call, so the host clear runs after it, on the targets the clear really addresses.
REX_HOOK_RAW(sub_822CE5C8) {
  const uint32_t d = ctx.r3.u32, count = ctx.r4.u32, rects = ctx.r5.u32, flags = ctx.r6.u32;
  const uint32_t color = ctx.r7.u32, stencil = ctx.r9.u32;
  const float z = float(ctx.f1.f64);
  FlushVertices(d);
  EnsureRenderer();
  {
    SuppressDraws suppress;
    __imp__sub_822CE5C8(ctx, base);
  }
  m3gpu::RendererClear(d, count, rects, flags, color, z, stencil);
}
// BeginTiling(device, flags, tile_count, tile_rects*, clear_color(float4*), clear_z(f1),
// clear_stencil): the tiles map to one host surface of their union, cleared once.
REX_HOOK_RAW(sub_822D0128) {
  const uint32_t d = ctx.r3.u32, tiles = ctx.r5.u32, rects = ctx.r6.u32, color = ctx.r7.u32;
  const uint32_t stencil = ctx.r9.u32;
  const float z = float(ctx.f1.f64);
  FlushVertices(d);
  EnsureRenderer();
  uint32_t w = 0, h = 0;
  for (uint32_t i = 0; i < std::min<uint32_t>(tiles, 16) && rects; ++i) {
    w = std::max(w, Load(rects + i * 16 + 8));
    h = std::max(h, Load(rects + i * 16 + 12));
  }
  if (w > 8192 || h > 8192) w = h = 0;
  m3gpu::RendererSetTiling(w && h, w, h);
  {
    SuppressDraws suppress;
    __imp__sub_822D0128(ctx, base);
  }
  if (!color) return;
  uint32_t argb = 0;
  for (int i = 0; i < 4; ++i) {
    const uint32_t bits = Load(color + i * 4);
    float v;
    std::memcpy(&v, &bits, 4);
    const uint32_t c = uint32_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    argb |= c << (i == 3 ? 24 : 16 - 8 * i);
  }
  m3gpu::RendererClear(d, 0, 0, 0x31, argb, z, stencil);
}
// EndTiling(device, flags, resolve_rect*, dest_texture*, ...): resolves each tile through
// sub_822D3238 (hooked), then the host surfaces go back to the surfaces' own size.
REX_HOOK_RAW(sub_822D0498) {
  const uint32_t d = ctx.r3.u32;
  FlushVertices(d);
  EnsureRenderer();
  {
    SuppressDraws suppress;
    __imp__sub_822D0498(ctx, base);
  }
  m3gpu::RendererSetTiling(false, 0, 0);
}
REX_HOOK_RAW(sub_822D3238) {
  const uint32_t d = ctx.r3.u32, stencil = Load(ctx.r1.u32 + 0x5C);
  EnsureRenderer();
  m3gpu::RendererResolve(d, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32,
                         ctx.r8.u32, ctx.r9.u32, ctx.r10.u32, float(ctx.f1.f64), stencil);
  SuppressDraws suppress;
  __imp__sub_822D3238(ctx, base);
}
// Swap(device, front_texture). The library schedules each flip for vblank [device+0x4240] (it adds
// the present interval, 2 = 30 fps for this game) while [device+0x4238] counts vblanks; the GPU
// held the flip until then. Without a GPU nothing waited and the game ran twice as fast, so the
// flip is paced here to interval x 1/60 s.
REX_HOOK_RAW(sub_822CF618) {
  const uint32_t d = ctx.r3.u32, front = ctx.r4.u32;
  __imp__sub_822CF618(ctx, base);
  EnsureRenderer();
  // Interval = growth of the flip target per swap ([device+0x4248] counts swaps); both are also
  // updated by the flip callback, so compare against the previous Swap.
  static uint32_t last_target = 0, last_swaps = 0, interval = 1;
  const uint32_t target = Load(d + 0x4240), swaps = Load(d + 0x4248);
  if (swaps != last_swaps && last_swaps) {
    const uint32_t ratio = (target - last_target + (swaps - last_swaps) / 2) / (swaps - last_swaps);
    if (ratio >= 1 && ratio <= 4) interval = ratio;
  }
  last_target = target;
  last_swaps = swaps;
  static const bool unpaced = std::getenv("MADAGASCAR3_NO_PACING") != nullptr;  // diagnostics
  if (!unpaced) PaceFlip(interval);
  m3gpu::RendererSwap(d, front);
}

// D3D tiling kicks its per-hardware-thread replay workers through a GPU callback whose data is the
// secondary command buffer (device+0x35C0). With submission disabled (+2B3D bit 0x20) the buffer
// allocator sub_822D3B48 always fails, so the data is 0 and the worker would dereference it
// (read of guest 0x4 in sub_822E6DA8). The host renderer already drew the clear; nothing to replay.
REX_HOOK_RAW(sub_822E6220) {
  if (!ctx.r3.u32) {
    static std::atomic<uint32_t> n{0};
    if (n.fetch_add(1) < 4) REXLOG_INFO("[m3native] tiling replay callback without command buffer skipped");
    return;
  }
  __imp__sub_822E6220(ctx, base);
}
