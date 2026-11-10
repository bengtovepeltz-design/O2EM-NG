#include "sdl_ng_host.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "src/core_ng/input/adapter.h"
#include "src/core_ng/machine.h"
#include "src/core_ng/memory/cartridge.h"
#include "src/core_ng/video/vdc_state.h"
#include "c7010.h"
#include "c7010_special.h"
#include "vdc_stub.h"

#include "host_commands.h"
#include "input_manager.h"
#include "input_sdl.h"
#include "o2em_keymap.h"
#include "o2em_keys.h"

namespace o2em::ng::host {

namespace {

constexpr SDL_PixelFormat kFormat = SDL_PIXELFORMAT_ARGB8888;

class SdlHost final : public MachineHost {
public:
    SdlHost(SDL_Renderer* renderer, int width, int height)
        : renderer_(renderer), width_(width), height_(height) {
        if (renderer_) {
            texture_ = SDL_CreateTexture(renderer_, kFormat,
                                         SDL_TEXTUREACCESS_STREAMING, width_,
                                         height_);
            if (texture_)
                SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
        }
        argb_.resize(static_cast<std::size_t>(width_) * height_);
        // Host presentation only. The emulation device owns hardware time;
        // SDL just buffers already-generated canonical PCM.
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_S16;
        spec.channels = 1;
        spec.freq = 48000;
        audio_stream_ = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (audio_stream_) {
            // MAME routes the i824x device at 0.25. Keep the canonical device
            // amplitude (0.5 logical full scale) and apply the presentation
            // gain here, outside machine state.
            SDL_SetAudioStreamGain(audio_stream_, 0.25f);
            SDL_ResumeAudioStreamDevice(audio_stream_);
        }
    }

    ~SdlHost() override {
        if (audio_stream_) SDL_DestroyAudioStream(audio_stream_);
        if (texture_) SDL_DestroyTexture(texture_);
    }

    bool Ready() const { return texture_ != nullptr; }
    bool QuitRequested() const { return commands_.QuitRequested(); }
    bool TakeReset() { return commands_.TakeReset(); }

    void HandleEvent(const SDL_Event& e) {
        // Command handling and input handling see the same event, exactly as
        // the legacy run loop did. InputManager owns the existing bindings.
        commands_.HandleEvent(e);
        InputManager_HandleEvent(e);
    }

    void BuildInput(MachineInput& out) {
        bool pressed[kO2emKeyMax + 1] = {};
        // Existing O2EM logical key ids; the adapter maps them to the
        // hardware matrix. No new user bindings are introduced.
        for (int k = 1; k <= kO2emKeyMax; ++k)
            pressed[k] = O2EMKey_IsPressed(k);

        const JoyState j1 = InputManager_GetPlayer1State();
        const JoyState j2 = InputManager_GetPlayer2State();
        ControllerState c1;
        c1.up = j1.up; c1.down = j1.down; c1.left = j1.left;
        c1.right = j1.right; c1.fire = j1.fire;
        ControllerState c2;
        c2.up = j2.up; c2.down = j2.down; c2.left = j2.left;
        c2.right = j2.right; c2.fire = j2.fire;
        out = BuildMachineInput(pressed, kO2emKeyMax + 1, c1, c2, false);
        if (const char* env = std::getenv("O2EM_INPUT_TRACE")) {
            if (env && *env && *env != '0') {
                static int last = -1;
                const int bits = ControllerBits(c1);
                const int bits2 = ControllerBits(c2);
                const int both = bits | (bits2 << 8);
                if (both != last) {
                    last = both;
                    std::fprintf(stderr, "INPUT_MACHINE p1=%02X p2=%02X\n",
                                 bits, bits2);
                }
            }
        }
    }

    void PresentFrame(const Framebuffer& fb) override {
        if (!texture_ || fb.width <= 0 || fb.height <= 0) return;
        const int w = fb.width < width_ ? fb.width : width_;
        const int h = fb.height < height_ ? fb.height : height_;
        for (int y = 0; y < h; ++y) {
            const std::uint8_t* row = fb.pixels.data() +
                                      static_cast<std::size_t>(y) * fb.stride;
            std::uint32_t* dst = argb_.data() +
                                 static_cast<std::size_t>(y) * width_;
            for (int x = 0; x < w; ++x) {
                const VdcRgb& c = kI8244PaletteRgb[row[x] & 0x0F];
                dst[x] = 0xFF000000u | (std::uint32_t(c.r) << 16) |
                         (std::uint32_t(c.g) << 8) | std::uint32_t(c.b);
            }
            for (int x = w; x < width_; ++x) dst[x] = 0xFF000000u;
        }
        SDL_UpdateTexture(texture_, nullptr, argb_.data(),
                          width_ * static_cast<int>(sizeof(std::uint32_t)));
    }

    void PushAudio(const std::int16_t* samples, std::size_t count) override {
        if (!audio_stream_ || !samples || count == 0) return;
        // Presentation policy only: during fast-forward/pause the host queue
        // may outrun playback. Drop stale host samples rather than changing the
        // emulated sound clock. The machine has already generated them.
        if (SDL_GetAudioStreamQueued(audio_stream_) > 96000)
            SDL_ClearAudioStream(audio_stream_);
        SDL_PutAudioStreamData(audio_stream_, samples,
                               static_cast<int>(count * sizeof(std::int16_t)));
    }

    void OnFrameComplete(std::uint64_t) override {
        if (!renderer_ || !texture_) return;
        int ow = 0, oh = 0;
        SDL_GetRenderOutputSize(renderer_, &ow, &oh);
        SDL_FRect dst;
        const float scale = SDL_min(static_cast<float>(ow) / width_,
                                    static_cast<float>(oh) / height_);
        const float dw = width_ * scale;
        const float dh = height_ * scale;
        dst.x = (ow - dw) * 0.5f;
        dst.y = (oh - dh) * 0.5f;
        dst.w = dw;
        dst.h = dh;
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, texture_, nullptr, &dst);
        // Reuse the existing frontend/debug overlay. Port-swap notifications
        // and the Debug active-core indicator are stored by VDCStub_ShowMessage
        // and must be composited by whichever host presents the frame.
        VDCStub_DrawOverlay(renderer_, dst);
        SDL_RenderPresent(renderer_);
    }

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    SDL_AudioStream* audio_stream_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint32_t> argb_;
    HostCommandState commands_;
};

} // namespace

bool RunSdl(const std::string& cartPath, const std::string& biosPath, bool pal) {
    SDL_Renderer* renderer = VDCStub_GetRenderer();
    if (!renderer) {
        std::printf("O2EM-NG: NG core requires the shared renderer\n");
        return false;
    }

    MachineConfig config;
    config.bios_path = biosPath;
    config.cartridge_path = cartPath;
    config.region = pal ? Region::Pal : Region::Ntsc;
    // Machine family is selected from the BIOS firmware identity, never from
    // the cartridge title. G7400 / Jopac+ BIOSes enable the EF934x/VPP device
    // and the G7400 clock profile; standard G7000/Odyssey2 BIOSes do not.
    {
        std::uint8_t bios_bytes[1024] = {};
        std::size_t got = 0;
        if (std::FILE* f = std::fopen(biosPath.c_str(), "rb")) {
            got = std::fread(bios_bytes, 1, sizeof(bios_bytes), f);
            std::fclose(f);
        }
        const std::uint32_t bios_crc =
            got == sizeof(bios_bytes) ? Crc32(bios_bytes, got) : 0;
        config.vpp_enabled = bios_crc == 0xE20A9F41u ||  // G7400 / Videopac+
                             bios_crc == 0x11647CA5u;    // Brandt Jopac (FR)
        config.g7400 = config.vpp_enabled;
    }

    const int w = 360, h = 243;
    SdlHost host(renderer, w, h);
    if (!host.Ready()) {
        std::printf("O2EM-NG: NG core texture creation failed: %s\n",
                    SDL_GetError());
        return false;
    }

    auto machine = Machine::Create(config, host);
    if (!machine->Load()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "O2EM-NG NG core",
                                 "Failed to load BIOS or ROM.", nullptr);
        return false;
    }

    // C7010 is cartridge-required special hardware, not a legacy-machine
    // fallback. Configure the existing NSC800 module and attach it to the NG
    // machine before power-on, exactly like the mapper is attached per cart.
    if (C7010_ConfigureForCartridge(machine->Cart().info.crc32)) {
        const std::filesystem::path firmware_path =
            std::filesystem::path(biosPath).parent_path() / "C7010" /
            "c7010_z80.bin";
        C7010_LoadFirmware(firmware_path.string().c_str());
        machine->AttachSpecialHardware(
            std::make_unique<C7010SpecialHardware>());
        std::printf("O2EM-NG: [NG core] C7010 NSC800 special hardware "
                    "attached\n");
        std::fflush(stdout);
    }

    machine->PowerOn();

    // The NG host may start after the frontend has already consumed SDL's
    // gamepad-added events. Open any controller that is already present before
    // the first frame. This only ensures handles exist; mappings are unchanged.
    InputSDL_EnsureGamepadsOpen();

    std::printf("O2EM-NG: [NG core] cart=%s bios=%s region=%s\n",
                cartPath.c_str(), biosPath.c_str(), pal ? "PAL" : "NTSC");
    std::printf("O2EM-NG: [NG core] existing O2EM-NG controls preserved; "
                "ESC/Back returns to the frontend, F5/East resets\n");
    std::fflush(stdout);

#if defined(_DEBUG)
    // Visible Debug-only core identification through the existing overlay.
    // Release keeps its normal UI.
    VDCStub_ShowMessage("ACTIVE CORE: NG");
#endif

    const std::uint64_t frame_ns = pal ? 20000000ull : 16666667ull;
    std::uint64_t next = SDL_GetTicksNS();
    while (!host.QuitRequested()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) host.HandleEvent(e);

        if (host.TakeReset()) {
            // Frontend F5 / gamepad East: reset the emulated machine, not the
            // host. Clear stale held input so a key held across the reset
            // cannot stick.
            InputSDL_ClearHeldState();
            machine->Reset();
            std::printf("O2EM-NG: [NG host] reset\n");
            std::fflush(stdout);
            next = SDL_GetTicksNS();
        }

        // Lossless per-frame refresh of the existing O2EM-NG controller state.
        // This is a host integration fallback; it does not define mappings or
        // touch the emulated machine directly.
        InputSDL_RefreshGamepadStates();
        MachineInput input;
        host.BuildInput(input);
        machine->SetMachineInput(input);
        machine->RunFrame();

        next += frame_ns;
        const std::uint64_t now = SDL_GetTicksNS();
        if (next > now) SDL_DelayNS(next - now);
        else next = now;
    }
    return true;
}

} // namespace o2em::ng::host
