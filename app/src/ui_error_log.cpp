// Logs the text behind BO2's "UI Error <number>" (bo2-dev, diagnostics).
//
// The title shows a Lua/UI error only as a hash of its message (EXE_UI_ERROR "%u"), then raises
// the error that relaunches the title. Title update 18's handler is sub_8270BBD0 (it loads the
// EXE_UI_ERROR format at 0x82099AFC): this detour writes its message and the first arguments that
// read as text to mp_uierror.txt before the title hashes them.

#include <cstdio>
#include <cstring>
#include <string>

#include <rex/memory/mapped_ptr.h>
#include <rex/ppc/hook.h>

namespace {

// Text at a guest address, empty when it is not a plausible printable C string.
std::string GuestText(uint8_t* base, uint32_t address) {
  if (address < 0x40000000u || address >= 0xC0000000u) {
    return {};
  }
  const char* text = rex::memory::GuestPtr<const char*>(base, address);
  std::string out;
  for (size_t i = 0; i < 512 && text[i]; ++i) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if ((c < 0x20 || c > 0x7E) && c != '\n' && c != '\t') {
      return out.size() >= 3 ? out : std::string{};
    }
    out.push_back(static_cast<char>(c));
  }
  return out;
}

}  // namespace

REX_DETOUR(sub_8270BBD0, u32,
           (PPCContext & ctx, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8)) {
  u8* base = ctx.detour_frame->base;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  if (std::FILE* log = std::fopen("mp_uierror.txt", "a")) {
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    const uint32_t args[] = {r3, r4, r5, r6, r7, r8};
    std::fprintf(log, "UI error handler:");
    for (int i = 0; i < 6; ++i) {
      const std::string text = GuestText(base, args[i]);
      if (text.empty()) {
        std::fprintf(log, " r%d=%08X", 3 + i, args[i]);
      } else {
        std::fprintf(log, " r%d=\"%s\"", 3 + i, text.c_str());
      }
    }
    std::fprintf(log, "\n");
    std::fclose(log);
  }
  return original(ctx, r3, r4, r5, r6, r7, r8);
}

// HavokScript's runtime error (luaG_runerror: r3 the Lua state, r4 the format, then its
// arguments), which the "attempt to index a %s value" style errors call: logs the format and the
// arguments that read as text, so the script error behind "Error processing event" is visible.
REX_DETOUR(sub_826E7908, u32, (PPCContext & ctx, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8)) {
  u8* base = ctx.detour_frame->base;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  if (std::FILE* log = std::fopen("mp_uierror.txt", "a")) {
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    const uint32_t args[] = {r4, r5, r6, r7, r8};
    std::fprintf(log, "Lua runtime error:");
    for (int i = 0; i < 5; ++i) {
      const std::string text = GuestText(base, args[i]);
      if (text.empty()) {
        std::fprintf(log, " r%d=%08X", 4 + i, args[i]);
      } else {
        std::fprintf(log, " r%d=\"%s\"", 4 + i, text.c_str());
      }
    }
    // The Lua state's first words, to find the running function (its saved pc points into the
    // function's code).
    if (r3 >= 0x40000000u && r3 < 0xC0000000u) {
      const uint8_t* l = rex::memory::GuestPtr<const uint8_t*>(base, r3);
      std::fprintf(log, "\n  L %08X:", r3);
      for (int i = 0; i < 32; ++i) {
        const uint32_t w = (uint32_t{l[i * 4]} << 24) | (uint32_t{l[i * 4 + 1]} << 16) |
                           (uint32_t{l[i * 4 + 2]} << 8) | l[i * 4 + 3];
        std::fprintf(log, " %08X", w);
      }
    }
    std::fprintf(log, "\n");
    std::fclose(log);
  }
  return original(ctx, r3, r4, r5, r6, r7, r8);
}
