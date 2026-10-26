// Madagascar 3 native imported audio exports; no game execution validation yet.
// Override only the seven audio imports actually present in this XEX.
// XMAPlayback context commands are handled by xma_commands.cpp.

#include <atomic>
#include <cstring>
#include <thread>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <rex/system/xtypes.h>

#include "generated/default/madagascar3_init.h"
#include "audio_system.h"
#include "xma_engine.h"
#include "xma_fields.h"

using namespace rex;
using m3audio::Ctx;
using m3audio::XmaEngine;
namespace xf = m3audio;

namespace {

inline uint32_t Load32(uint8_t* base, uint32_t a) { return rex::memory::load_and_swap<uint32_t>(rex::memory::GuestPtr(base, a)); }
inline void Store32(uint8_t* base, uint32_t a, uint32_t v) { rex::memory::store_and_swap<uint32_t>(rex::memory::GuestPtr(base, a), v); }
inline uint16_t Load16(uint8_t* base, uint32_t a) { return rex::memory::load_and_swap<uint16_t>(rex::memory::GuestPtr(base, a)); }
inline void Store16(uint8_t* base, uint32_t a, uint16_t v) { rex::memory::store_and_swap<uint16_t>(rex::memory::GuestPtr(base, a), v); }

}  // namespace

// ---------------------------------------------------------------------------------------------
// XMA*
// ---------------------------------------------------------------------------------------------

// XMACreateContext(u32* out_context)
REX_HOOK_RAW(__imp__XMACreateContext) {
  uint32_t context = XmaEngine::Get().Allocate();
  Store32(base, ctx.r3.u32, context);
  ctx.r3.u64 = context ? X_STATUS_SUCCESS : X_STATUS_NO_MEMORY;
}

REX_HOOK_RAW(__imp__XMAReleaseContext) {
  XmaEngine::Get().Release(ctx.r3.u32);
  ctx.r3.u64 = 0;
}

// XMAPlayback uses the context command adapter.

// XAudio render driver
// ---------------------------------------------------------------------------------------------


REX_HOOK_RAW(__imp__XAudioGetSpeakerConfig) {
  Store32(base, ctx.r3.u32, 0x00010001);
  ctx.r3.u64 = 0;
}

// XAudioRegisterRenderDriverClient(u32 callback_and_arg[2], u32* out_driver)
REX_HOOK_RAW(__imp__XAudioRegisterRenderDriverClient) {
  uint32_t callback_ptr = ctx.r3.u32, driver_ptr = ctx.r4.u32;
  auto* audio = m3audio::AudioSystem();
  uint32_t callback = callback_ptr ? Load32(base, callback_ptr) : 0;
  if (!audio || !callback) {
    ctx.r3.u64 = X_E_INVALIDARG;
    return;
  }
  uint32_t index = 0;
  X_STATUS status = audio->RegisterClient(callback, Load32(base, callback_ptr + 4), &index);
  if (status != X_STATUS_SUCCESS) {
    ctx.r3.u64 = status;
    return;
  }
  Store32(base, driver_ptr, 0x41550000 | index);
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XAudioUnregisterRenderDriverClient) {
  if (auto* audio = m3audio::AudioSystem()) audio->UnregisterClient(ctx.r3.u32 & 0xFFFF);
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XAudioSubmitRenderDriverFrame) {
  if (auto* audio = m3audio::AudioSystem()) audio->SubmitFrame(ctx.r3.u32 & 0xFFFF, ctx.r4.u32);
  ctx.r3.u64 = 0;
}

// No system-wide PC category gain: let the game mixer use full scale.
REX_HOOK_RAW(__imp__XAudioGetVoiceCategoryVolume) {
  float one = 1.0f;
  uint32_t bits;
  std::memcpy(&bits, &one, 4);
  Store32(base, ctx.r4.u32, bits);
  ctx.r3.u64 = 0;
}
