#include "import_manager.h"

#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>

#include "../library/game_info.h"

namespace
{
    using Path = std::filesystem::path;

    const wchar_t* FilterFor(ImportAssetType type);


    const wchar_t* AssetName(ImportAssetType type)
    {
        switch (type)
        {
        case ImportAssetType::Rom: return L"ROM";
        case ImportAssetType::Bios: return L"BIOS";
        case ImportAssetType::Manual: return L"manual";
        case ImportAssetType::Cover: return L"cover";
        case ImportAssetType::Screenshot: return L"screenshot";
        }
        return L"file";
    }

    bool IsInsideFolder(const Path& path, const Path& folder)
    {
        std::error_code error;
        const Path canonicalPath = std::filesystem::weakly_canonical(path, error);
        if (error) return false;
        const Path canonicalFolder = std::filesystem::weakly_canonical(folder, error);
        if (error) return false;

        auto pathIt = canonicalPath.begin();
        auto folderIt = canonicalFolder.begin();
        for (; folderIt != canonicalFolder.end(); ++folderIt, ++pathIt)
        {
            if (pathIt == canonicalPath.end() || *pathIt != *folderIt)
                return false;
        }
        return true;
    }

    Path SelectInstalledBios(const Path& biosFolder, bool& cancelled)
    {
        wchar_t filename[32768] = {};
        const std::wstring initialFolder = biosFolder.wstring();

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFile = filename;
        dialog.nMaxFile = static_cast<DWORD>(std::size(filename));
        dialog.lpstrFilter = FilterFor(ImportAssetType::Bios);
        dialog.nFilterIndex = 1;
        dialog.lpstrInitialDir = initialFolder.c_str();
        dialog.lpstrTitle = L"Choose an installed BIOS file to remove";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
            OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&dialog))
        {
            cancelled = CommDlgExtendedError() == 0;
            return {};
        }

        cancelled = false;
        const Path selected(filename);
        return IsInsideFolder(selected, biosFolder) ? selected : Path{};
    }

    ImportResult MoveToRecycleBin(ImportAssetType type, const Path& path)
    {
        ImportResult result;
        std::error_code error;
        if (path.empty() || !std::filesystem::is_regular_file(path, error) || error)
        {
            result.message = "No matching file is available to delete.";
            return result;
        }

        const std::wstring question = L"Move this " + std::wstring(AssetName(type)) +
            L" to the Recycle Bin?\n\n" + path.filename().wstring();
        if (MessageBoxW(nullptr, question.c_str(), L"O2EM-NG - Confirm delete",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        {
            result.cancelled = true;
            result.message = "Delete cancelled.";
            return result;
        }

        std::wstring from = path.wstring();
        from.push_back(L'\0');
        from.push_back(L'\0');

        SHFILEOPSTRUCTW operation{};
        operation.wFunc = FO_DELETE;
        operation.pFrom = from.c_str();
        operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION |
            FOF_NOERRORUI | FOF_SILENT;

        const int status = SHFileOperationW(&operation);
        if (status != 0 || operation.fAnyOperationsAborted)
        {
            result.message = "O2EM-NG could not move the file to the Recycle Bin.";
            return result;
        }

        result.success = true;
        result.destination = path;
        result.message = "Moved to Recycle Bin: " + path.filename().string();
        return result;
    }

    const wchar_t* FilterFor(ImportAssetType type)
    {
        switch (type)
        {
        case ImportAssetType::Rom:
        case ImportAssetType::Bios:
            return L"Videopac binary files (*.bin;*.rom)\0*.bin;*.rom\0All files (*.*)\0*.*\0\0";
        case ImportAssetType::Manual:
            return L"Manuals (*.pdf;*.txt;*.html)\0*.pdf;*.txt;*.html;*.htm\0All files (*.*)\0*.*\0\0";
        case ImportAssetType::Cover:
        case ImportAssetType::Screenshot:
            return L"Images (*.jpg;*.jpeg;*.png;*.bmp;*.webp)\0*.jpg;*.jpeg;*.png;*.bmp;*.webp\0All files (*.*)\0*.*\0\0";
        }
        return L"All files (*.*)\0*.*\0\0";
    }

    const wchar_t* DialogTitle(ImportAssetType type)
    {
        switch (type)
        {
        case ImportAssetType::Rom: return L"Add ROM to O2EM-NG";
        case ImportAssetType::Bios: return L"Add BIOS to O2EM-NG";
        case ImportAssetType::Manual: return L"Add Manual to selected game";
        case ImportAssetType::Cover: return L"Add Cover to selected game";
        case ImportAssetType::Screenshot: return L"Add Screenshot to selected game";
        }
        return L"Add file to O2EM-NG";
    }

    Path SelectSourceFile(ImportAssetType type, bool& cancelled)
    {
        wchar_t filename[32768] = {};
        const wchar_t* filter = FilterFor(type);

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFile = filename;
        dialog.nMaxFile = static_cast<DWORD>(std::size(filename));
        dialog.lpstrFilter = filter;
        dialog.nFilterIndex = 1;
        dialog.lpstrTitle = DialogTitle(type);
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
            OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&dialog))
        {
            cancelled = CommDlgExtendedError() == 0;
            return {};
        }

        cancelled = false;
        return Path(filename);
    }

    std::string LowerExtension(const Path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        return extension;
    }

    bool IsAllowedExtension(ImportAssetType type, const Path& source)
    {
        const std::string extension = LowerExtension(source);
        switch (type)
        {
        case ImportAssetType::Rom:
        case ImportAssetType::Bios:
            return extension == ".bin" || extension == ".rom";
        case ImportAssetType::Manual:
            return extension == ".pdf" || extension == ".txt" ||
                extension == ".html" || extension == ".htm";
        case ImportAssetType::Cover:
        case ImportAssetType::Screenshot:
            return extension == ".jpg" || extension == ".jpeg" ||
                extension == ".png" || extension == ".bmp" ||
                extension == ".webp";
        }
        return false;
    }

    std::string TwoDigitId(int id)
    {
        std::ostringstream stream;
        stream << std::setfill('0') << std::setw(2) << id;
        return stream.str();
    }

    Path NextScreenshotPath(const Path& folder, const std::string& id,
        const std::string& extension)
    {
        for (int index = 1; index <= 999; ++index)
        {
            const Path candidate = folder /
                (id + "_screenshot_" + std::to_string(index) + extension);
            std::error_code error;
            if (!std::filesystem::exists(candidate, error))
                return candidate;
        }
        return {};
    }

    Path DestinationFor(ImportAssetType type, const GameInfo& game,
        const Path& source, const Path& basePath)
    {
        const std::string extension = LowerExtension(source);
        const std::string id = TwoDigitId(game.videopacNumber);

        switch (type)
        {
        case ImportAssetType::Rom:
        {
            // Patch 0022d: use the selected catalogue record as the stable
            // suggested ROM name. This prevents the save dialog from reusing
            // the previously imported game's filename. Keep the selected
            // source extension so both .bin and .rom imports remain valid.
            Path catalogueName = Path(game.filename).filename();
            std::string stem = catalogueName.stem().string();
            if (stem.empty())
                stem = "vp_" + id;
            return basePath / "ROMS" / (stem + extension);
        }
        case ImportAssetType::Bios:
            return basePath / "BIOS" / source.filename();
        case ImportAssetType::Manual:
            return basePath / "MANUALS" / (id + "_imported_manual" + extension);
        case ImportAssetType::Cover:
            return basePath / "BOXART" / (id + "_plastic_front" + extension);
        case ImportAssetType::Screenshot:
            return NextScreenshotPath(basePath / "SCREENSHOTS", id, extension);
        }
        return {};
    }

    Path AskForDestinationName(ImportAssetType type, const Path& suggestedDestination,
        bool& cancelled)
    {
        wchar_t filename[32768] = {};
        const std::wstring suggested = suggestedDestination.filename().wstring();
        wcsncpy_s(filename, suggested.c_str(), _TRUNCATE);

        const std::wstring initialFolder = suggestedDestination.parent_path().wstring();
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFile = filename;
        dialog.nMaxFile = static_cast<DWORD>(std::size(filename));
        dialog.lpstrFilter = FilterFor(type);
        dialog.nFilterIndex = 1;
        dialog.lpstrInitialDir = initialFolder.c_str();
        dialog.lpstrTitle = L"Choose the filename used inside O2EM-NG";
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

        if (!GetSaveFileNameW(&dialog))
        {
            cancelled = CommDlgExtendedError() == 0;
            return {};
        }

        cancelled = false;
        Path chosen(filename);
        Path cleanName = chosen.filename();
        if (cleanName.empty() || cleanName == L"." || cleanName == L"..")
            return {};

        if (cleanName.extension().empty())
            cleanName += suggestedDestination.extension();

        return suggestedDestination.parent_path() / cleanName;
    }

    ImportResult CopySelectedFile(ImportAssetType type, const Path& source,
        const Path& suggestedDestination)
    {
        ImportResult result;
        bool cancelled = false;
        const Path destination = AskForDestinationName(type, suggestedDestination, cancelled);
        if (destination.empty())
        {
            result.cancelled = cancelled;
            result.message = cancelled ? "Import cancelled." : "A valid destination filename was not entered.";
            return result;
        }

        if (!IsAllowedExtension(type, destination))
        {
            result.message = "The destination filename has an unsupported file extension.";
            return result;
        }

        std::error_code error;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error)
        {
            result.message = "O2EM-NG could not create the destination folder.";
            return result;
        }

        std::filesystem::copy_file(source, destination,
            std::filesystem::copy_options::overwrite_existing, error);
        if (error)
        {
            result.message = "The selected file could not be copied into O2EM-NG.";
            return result;
        }

        result.success = true;
        result.destination = destination;
        result.message = "Import complete: " + destination.filename().string();
        return result;
    }
}

ImportManager::ImportManager(const std::filesystem::path& basePath)
{
    SetBasePath(basePath);
}

void ImportManager::SetBasePath(const std::filesystem::path& basePath)
{
    basePath_ = basePath.empty() ? std::filesystem::current_path() : basePath;
}

const std::filesystem::path& ImportManager::BasePath() const noexcept
{
    return basePath_;
}

ImportResult ImportManager::ImportForGame(
    ImportAssetType type, const GameInfo& game) const
{
    ImportResult result;
    if (game.videopacNumber <= 0 || game.videopacNumber > 99)
    {
        result.message = "The selected game does not have a valid Videopac ID.";
        return result;
    }

    bool cancelled = false;
    const Path source = SelectSourceFile(type, cancelled);
    if (source.empty())
    {
        result.cancelled = cancelled;
        result.message = cancelled ? "Import cancelled." : "The file dialog could not be opened.";
        return result;
    }

    if (!IsAllowedExtension(type, source))
    {
        result.message = "The selected file type is not supported for this import.";
        return result;
    }

    const Path suggestedDestination = DestinationFor(type, game, source, basePath_);
    if (suggestedDestination.empty())
    {
        result.message = "O2EM-NG could not create a destination filename.";
        return result;
    }

    return CopySelectedFile(type, source, suggestedDestination);
}

ImportResult ImportManager::ImportBios() const
{
    ImportResult result;
    bool cancelled = false;
    const Path source = SelectSourceFile(ImportAssetType::Bios, cancelled);
    if (source.empty())
    {
        result.cancelled = cancelled;
        result.message = cancelled ? "Import cancelled." : "The file dialog could not be opened.";
        return result;
    }

    if (!IsAllowedExtension(ImportAssetType::Bios, source))
    {
        result.message = "The selected BIOS file type is not supported.";
        return result;
    }

    const Path suggestedDestination = basePath_ / "BIOS" / source.filename();
    return CopySelectedFile(ImportAssetType::Bios, source, suggestedDestination);
}


ImportResult ImportManager::DeleteForGame(ImportAssetType type,
    const GameInfo& game, const std::filesystem::path& screenshotPath) const
{
    Path target;
    switch (type)
    {
    case ImportAssetType::Rom: target = game.romPath; break;
    case ImportAssetType::Manual: target = game.manual; break;
    case ImportAssetType::Cover: target = game.boxArt; break;
    case ImportAssetType::Screenshot: target = screenshotPath; break;
    case ImportAssetType::Bios:
        return DeleteBios();
    }
    return MoveToRecycleBin(type, target);
}

ImportResult ImportManager::DeleteBios() const
{
    ImportResult result;
    const Path biosFolder = basePath_ / "BIOS";
    std::error_code error;
    if (!std::filesystem::is_directory(biosFolder, error) || error)
    {
        result.message = "No BIOS folder is available.";
        return result;
    }

    bool cancelled = false;
    const Path selected = SelectInstalledBios(biosFolder, cancelled);
    if (selected.empty())
    {
        result.cancelled = cancelled;
        result.message = cancelled
            ? "Delete cancelled."
            : "Choose a BIOS file from the O2EM-NG BIOS folder.";
        return result;
    }
    return MoveToRecycleBin(ImportAssetType::Bios, selected);
}
