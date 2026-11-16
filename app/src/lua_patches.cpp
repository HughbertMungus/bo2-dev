// Patches to the title's Lua (LUI menu) chunks as they load (bo2-dev).
//
// The menus are HavokScript bytecode inside encrypted fastfiles, so instead of editing the archives
// this detours the chunk block reader (luaZ_read, sub_82631320: r3 = the reader, whose remaining
// byte count is at +16 and read pointer at +20) and rewrites known constants or instructions in
// the unread bytes before the loader parses them. Each patch finds its bytes by hashes (byte_signature.h), so
// a different build of the scripts is simply left alone. Function layout: a u32 instruction count,
// padding to 4-byte alignment, 4-byte instructions (opcode first; 0x32 loads a constant, 0x39
// jumps to pc + 1 + offset), then a u32 constant count and the constants (3 = float, 4 = string).
//
// BO2MP_LMP_BOT_LIMIT=<n> (1-17): Local Multiplayer bot limits (details at the patches below).
// BO2MP_MENU_QUIT=1: the multiplayer main menu without Xbox LIVE and with Quit at the end.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <rex/memory/mapped_ptr.h>
#include <rex/ppc/hook.h>

#include "byte_signature.h"

namespace {

using bo2::BE32;
using bo2::ByteEdit;
using bo2::ByteSignature;
using bo2::FloatBE;

struct LuaPatch {
  const char* what;
  ByteSignature sig;
  std::vector<ByteEdit> edits;
  std::atomic<uint32_t> applied{0};
};

uint32_t LoadBE32(const uint8_t* p) {
  return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}

const char* Env(const char* name) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  return std::getenv(name);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

std::vector<LuaPatch*> BuildPatches() {
  std::vector<LuaPatch*> patches;
  auto add = [&](const char* what, ByteSignature sig, std::vector<ByteEdit> edits) {
    patches.push_back(new LuaPatch{what, sig, std::move(edits)});
  };
  const char* bots = Env("BO2MP_LMP_BOT_LIMIT");
  if (bots && bots[0]) {
    const long limit = std::strtol(bots, nullptr, 10);
    if (limit >= 1 && limit <= 17) {
      // The bot menu's constants: the bot_enemies dvar name, a total cap of 11, then
      // GameModeIsMode / CoD / GAMEMODE_LOCAL_SPLITSCREEN and the Local Multiplayer total cap of 9.
      // The menu's friends + enemies cap counts the selector values, not the bots that join, so
      // "Maximum" on one side would zero the other. The match script already stops at 18 players
      // and a TDM team at 9, so both caps are lifted out of the way: Maximum + Maximum fills both
      // teams.
      const std::string total_cap = FloatBE(2.0f * static_cast<float>(limit));
      add("local multiplayer bot limit", {84, 0xa0a8, 0xd1044655f183f65bull},
          {{14, total_cap}, {80, total_cap}});
      // Each side's selector (Button_Bots_AddChoices) also stops at 9 in Local Multiplayer: after
      // comparing m_dvarName with "bot_friends" it loads constant 21 (9; the pool holds the labels
      // MPUI_0..11_BOTS at 0-11 and the numbers 0-11 at 12-23), and just before that constant 22
      // (10) bounds both sides. Load constant 23 (11) in both places, the last label there is, so
      // the total cap above is what limits the bots.
      add("local multiplayer bots per side", {48, 0x1999, 0x53e5ddb479e2e3abull},
          {{43, "\x17"}, {19, "\x17"}});
      // That leaves the "10 bots" entry as each side's last choice: make it "Maximum" (the
      // MENU_MAXIMUM text key, the same length as the MPUI_10_BOTS key it replaces) worth the
      // limit, so it fills the match (the match script stops at 18 players and a team at 9). Its
      // label and value constants are used only by the choice tables now that the bound loads
      // constant 23.
      add("bots choice 10 -> Maximum label", {50, 0xc4af, 0x850d7615ccadb783ull},
          {{18, "MENU_MAXIMUM"}});
      add("bots choice 10 -> Maximum value", {33, 0x708c, 0x1135af58d344fd6aull},
          {{6, FloatBE(static_cast<float>(limit))}});
      // The lobby (PrivateGameLobby) clamps the bots for team based gametypes to 9 before the
      // match, so a TDM enemy team stopped at 9 bots (FFA, not team based, did not). A team holds
      // at most 13 players (scoreboard; a 14th ends the title), so team games clamp each side's
      // bots to 13 (or the limit, if lower).
      add("lobby team game bot clamp 9 -> 13", {40, 0xc400, 0xde3d166572b85c1cull},
          {{14, FloatBE(static_cast<float>(limit < 13 ? limit : 13))}});
      // The scoreboard shows 9 rows per team in Local Multiplayer (13 elsewhere). Use 13 there
      // too, the most a team supports: a 14th player on a team ended the title, and 18 rows ended
      // it at startup, so the lobby clamp above stops team games at 13 per team.
      add("split screen scoreboard rows 9 -> 13", {42, 0xc400, 0x7fc540bcc6176fc8ull},
          {{33, FloatBE(13.0f)}});
    }
  }
  const char* menu_quit = Env("BO2MP_MENU_QUIT");
  if (menu_quit && menu_quit[0] == '1') {
    // BO2MP_MENU_QUIT=1: the multiplayer main menu (CoD.MainMenu.New) reads System Link, Local,
    // Options, Quit. Instructions are 4 bytes, opcode first; a JMP lands on pc + 2 + offset
    // (counting the data words that follow field reads, which a jump must not land on).
    // 1. Skip adding the Xbox LIVE entry (563-579: mainLobbyButton = buttonList:addButton(...) and
    //    its setActionEventName; nothing else reads mainLobbyButton): at 563 (the addButton lookup;
    //    the field read at 560 owns data words) jump +15 to 580, the next statement.
    add("main menu: no Xbox LIVE entry", {48, 0xf2e1, 0x6d1d17c0bb62f284ull},
        {{3 * 4, BE32(0x39000f00)}});
    // 2. After Options, "if CoD.isPC" guards a spacer, Single Player, Multiplayer / Zombies, a
    //    spacer, Quit, a quit button prompt and a re-layout; off PC the jump at 767 (+138) goes past
    //    all of it to 907. Jump to the Quit button at 863 instead (+94).
    add("main menu: Quit after Options", {28, 0x7a2a, 0x1afd9950fe8b9e9full},
        {{5 * 4, BE32(0x39005e00)}});
    // 3. After Quit's setActionEventName (882), skip the PC quit button prompt and re-layout
    //    (883-906): at 883 (the addLeftButtonPrompt lookup) jump +22 to 907, where the original
    //    jump went. Quit opens the game's own quit popup (open_quit_popup -> OpenQuitPopup, whose
    //    handler is registered on every platform).
    add("main menu: skip PC quit prompt and layout", {16, 0x0de9, 0xe61e1bbf55146de8ull},
        {{2 * 4, BE32(0x39001600)}});
  }
  return patches;
}

const std::vector<LuaPatch*>& Patches() {
  static const std::vector<LuaPatch*> patches = BuildPatches();
  return patches;
}

void ApplyPatches(uint8_t* data, uint32_t size) {
  for (LuaPatch* patch : Patches()) {
    for (size_t i = bo2::Find(patch->sig, data, size); i != SIZE_MAX;
         i = bo2::Find(patch->sig, data, size, i + patch->sig.length)) {
      bo2::ApplyEdits(data + i, patch->edits);
      if (patch->applied.fetch_add(1) == 0) {
        if (std::FILE* log = std::fopen("mp_luapatch.txt", "a")) {
          std::fprintf(log, "patched %s\n", patch->what);
          std::fclose(log);
        }
      }
    }
  }
}

}  // namespace

// luaZ_read(reader, destination, size): copies size bytes of the chunk, refilling from the reader.
REX_DETOUR(sub_82631320, u32, (PPCContext & ctx, u32 reader, u32 destination, u32 size)) {
  if (!Patches().empty()) {
    u8* base = ctx.detour_frame->base;
    // Scan each buffered stretch of the chunk once, before any of it is copied out.
    thread_local uint32_t scanned_begin = 0;
    thread_local uint32_t scanned_end = 0;
    const uint8_t* reader_host = rex::memory::GuestPtr<const uint8_t*>(base, reader);
    const uint32_t remaining = LoadBE32(reader_host + 16);
    const uint32_t pointer = LoadBE32(reader_host + 20);
    if (remaining && pointer && !(pointer >= scanned_begin && pointer < scanned_end)) {
      scanned_begin = pointer;
      scanned_end = pointer + remaining;
      ApplyPatches(rex::memory::GuestPtr<uint8_t*>(base, pointer), remaining);
    }
  }
  return original(ctx, reader, destination, size);
}
