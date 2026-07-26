#include "emulator_core.h"
#include "vdc_stub.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include "types.h"
#include "vmachine.h"
#include "config.h"
#include "crc32.h"
#include "cpu.h"
#include "keyboard.h"
#include <cstdint>
#include "audio.h"

static long FileSize(FILE* stream)
{
    long current = ftell(stream);
    fseek(stream, 0L, SEEK_END);
    long length = ftell(stream);
    fseek(stream, current, SEEK_SET);
    return length;
}

static bool LoadBios(const std::string& biosPath, unsigned long& biosCrc)
{
    FILE* fn = fopen(biosPath.c_str(), "rb");

    if (!fn)
        return false;

    if (fread(rom_table[0], 1024, 1, fn) != 1)
    {
        fclose(fn);
        return false;
    }

    fclose(fn);

    for (int i = 1; i < 8; i++)
        memcpy(rom_table[i], rom_table[0], 1024);

    biosCrc = crc32_buf(rom_table[0], 1024);

    if (biosCrc == 0x8016A315)
    {
        app_data.vpp = 0;
        app_data.bios = ROM_O2;
    }
    else
    {
        app_data.vpp = 0;
        app_data.bios = ROM_UNKNOWN;
    }

    return true;
}

static bool LoadCart(const std::string& file)
{
    FILE* fn = fopen(file.c_str(), "rb");

    if (!fn)
        return false;

    app_data.crc = crc32_file(file.c_str());

    long size = FileSize(fn);

    if ((size % 1024) != 0)
    {
        fclose(fn);
        return false;
    }

    int nb = 0;

    if ((size == 32768) || (size == 65536) || (size == 131072) ||
        (size == 262144) || (size == 524288) || (size == 1048576))
    {
        app_data.megaxrom = 1;
        app_data.bank = 1;

        megarom = (Byte*)malloc(1048576);

        if (!megarom)
        {
            fclose(fn);
            return false;
        }

        if (fread(megarom, size, 1, fn) != 1)
        {
            fclose(fn);
            return false;
        }

        if (size < 65536)
            memcpy(megarom + 32768, megarom, 32768);
        if (size < 131072)
            memcpy(megarom + 65536, megarom, 65536);
        if (size < 262144)
            memcpy(megarom + 131072, megarom, 131072);
        if (size < 524288)
            memcpy(megarom + 262144, megarom, 262144);
        if (size < 1048576)
            memcpy(megarom + 524288, megarom, 524288);

        memcpy(&rom_table[0][1024], megarom + 4096 * 255 + 1024, 3072);

        nb = 1;
    }
    else if ((size % 3072) == 0)
    {
        app_data.three_k = 1;
        nb = static_cast<int>(size / 3072);

        for (int i = nb - 1; i >= 0; i--)
        {
            if (fread(&rom_table[i][1024], 3072, 1, fn) != 1)
            {
                fclose(fn);
                return false;
            }
        }
    }
    else
    {
        nb = static_cast<int>(size / 2048);

        if ((nb == 2) && (app_data.exrom))
        {
            if (fread(&extROM[0], 1024, 1, fn) != 1)
            {
                fclose(fn);
                return false;
            }

            if (fread(&rom_table[0][1024], 3072, 1, fn) != 1)
            {
                fclose(fn);
                return false;
            }
        }
        else
        {
            for (int i = nb - 1; i >= 0; i--)
            {
                if (fread(&rom_table[i][1024], 2048, 1, fn) != 1)
                {
                    fclose(fn);
                    return false;
                }

                memcpy(&rom_table[i][3072], &rom_table[i][2048], 1024);
            }
        }
    }

    fclose(fn);

    rom = rom_table[0];

    if (nb == 1)
        app_data.bank = 1;
    else if (nb == 2)
        app_data.bank = app_data.exrom ? 1 : 2;
    else if (nb == 4)
        app_data.bank = 3;
    else
        app_data.bank = 4;

    if ((rom_table[nb - 1][1024 + 12] == 'O') &&
        (rom_table[nb - 1][1024 + 13] == 'P') &&
        (rom_table[nb - 1][1024 + 14] == 'N') &&
        (rom_table[nb - 1][1024 + 15] == 'B'))
    {
        app_data.openb = 1;
    }

    printf(
        "ROM loaded: %s\n"
        "  size=%ld\n"
        "  crc=%08lX\n"
        "  nb=%d\n"
        "  bank=%d\n"
        "  three_k=%d\n"
        "  megaxrom=%d\n"
        "  exrom=%d\n"
        "  openb=%d\n",
        file.c_str(),
        size,
        app_data.crc,
        nb,
        app_data.bank,
        app_data.three_k,
        app_data.megaxrom,
        app_data.exrom,
        app_data.openb
    );


    return true;
}

bool EmulatorCore_StartRom(const std::string& romPath, RegionMode regionMode, const std::string& biosFile, bool scanlines)
{
    app_data.bank = 0;
    app_data.limit = 0;
    app_data.sound_en = 1;
    app_data.speed = 100;
    app_data.voice = 0;
    app_data.exrom = 0;
    app_data.three_k = 0;
    app_data.crc = 0;
    app_data.euro = 0;
    app_data.openb = 0;
    app_data.vpp = 0;
    app_data.bios = 0;
    app_data.megaxrom = 0;
    app_data.stick[0] = 1;
    app_data.stick[1] = 1;
    app_data.sticknumber[0] = 0;
    app_data.sticknumber[1] = 0;
    app_data.debug = 0;
    app_data.wsize = 2;
    app_data.fullscreen = 0;
    app_data.scanlines = scanlines ? 1 : 0;
    app_data.svolume = 100;
    app_data.vvolume = 100;
    app_data.filter = 0;
    app_data.scoretype = 0;
    app_data.scoreaddress = 0;
    app_data.default_highscore = 0;
    app_data.breakpoint = 65535;

    std::string baseFolder = SDL_GetBasePath();
    std::string biosPath = baseFolder + "BIOS\\" + biosFile;

    unsigned long biosCrc = 0;

    if (!LoadBios(biosPath, biosCrc))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "O2EM-NG Core",
            ("Failed to load BIOS:\n" + biosPath).c_str(),
            nullptr);

        return false;
    }

    printf("O2EM-NG: BIOS loaded: %s  CRC=%08lX\n",
        biosPath.c_str(),
        biosCrc);

    if (!LoadCart(romPath))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "O2EM-NG Core",
            "Failed to load ROM.",
            nullptr);

        return false;
    }

    key_done = 0;
    key_debug = 0;

    if (regionMode == RegionMode::PAL)
    {
        printf("O2EM-NG: Region setting: PAL\n");
        O2EM_SetRegionModeOverride(1);
    }
    else if (regionMode == RegionMode::NTSC)
    {
        printf("O2EM-NG: Region setting: NTSC\n");
        O2EM_SetRegionModeOverride(0);
    }
    else
    {
        printf("O2EM-NG: Region setting: AUTO\n");
        O2EM_SetRegionModeOverride(-1);
    }

    init_cpu();
    init_system();
    init_audio();

    run();

    return true;
}