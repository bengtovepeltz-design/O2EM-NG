#include "collection_manager.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "../library/game_info.h"
#include "../library/game_library.h"


namespace
{
    struct CatalogSortKey
    {
        int group = 2;       // 0 = numbered Videopac, 1 = hardware IDs (C7010...), 2 = no ID
        int number = 0;
        int variant = 0;     // plain number before '+' variant
        std::string text;
    };

    CatalogSortKey MakeCatalogSortKey(const GameInfo& game)
    {
        std::string id = game.catalogId;
        if (id.empty() && game.videopacNumber > 0)
            id = std::to_string(game.videopacNumber);

        CatalogSortKey key;
        if (id.empty())
        {
            key.text = game.title;
            return key;
        }

        std::string upper = id;
        std::transform(upper.begin(), upper.end(), upper.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        std::size_t pos = 0;
        while (pos < upper.size() && std::isdigit(static_cast<unsigned char>(upper[pos])))
            ++pos;
        if (pos > 0)
        {
            key.group = 0;
            key.number = std::stoi(upper.substr(0, pos));
            key.variant = upper.find('+', pos) != std::string::npos ? 1 : 0;
            key.text = upper;
            return key;
        }

        if (upper.size() > 1 && upper[0] == 'C')
        {
            std::size_t digit = 1;
            while (digit < upper.size() && std::isdigit(static_cast<unsigned char>(upper[digit])))
                ++digit;
            if (digit > 1)
            {
                key.group = 1;
                key.number = std::stoi(upper.substr(1, digit - 1));
                key.text = upper;
                return key;
            }
        }

        key.text = upper;
        return key;
    }

    bool CatalogLess(const GameInfo& a, const GameInfo& b)
    {
        const CatalogSortKey ka = MakeCatalogSortKey(a);
        const CatalogSortKey kb = MakeCatalogSortKey(b);
        if (ka.group != kb.group) return ka.group < kb.group;
        if (ka.number != kb.number) return ka.number < kb.number;
        if (ka.variant != kb.variant) return ka.variant < kb.variant;
        if (ka.text != kb.text) return ka.text < kb.text;
        return a.title < b.title;
    }
}

void CollectionManager::Attach(GameLibrary* library)
{
    library_ = library;
    Rebuild();
}

void CollectionManager::Rebuild()
{
    std::string selectedFilename;
    if (const GameInfo* selected = Current())
        selectedFilename = selected->filename;

    indices_.clear();
    currentPosition_ = 0;
    if (!library_)
        return;

    const auto& games = library_->Games();
    for (std::size_t i = 0; i < games.size(); ++i)
    {
        const GameInfo& game = games[i];
        // Patch 0022a: the normal Game Library contains only games with an
        // installed ROM. Import Center temporarily exposes the complete
        // catalogue so the user can select a title before importing its ROM.
        const bool installed = !game.romPath.empty() || !game.rom.path.empty();
        bool include = false;
        if (showUninstalled_)
        {
            include = true;
        }
        else if (installed)
        {
            switch (view_)
            {
            case CollectionView::AllGames: include = true; break;
            case CollectionView::Favorites: include = game.favorite; break;
            case CollectionView::RecentlyPlayed: include = game.lastPlayed > 0; break;
            case CollectionView::MostPlayed: include = game.playCount > 0; break;
            }
        }
        if (include)
            indices_.push_back(i);
    }

    if (view_ == CollectionView::AllGames)
    {
        // Patch 0030C phase 2: user-edited European catalogue IDs drive
        // the visible library order. This keeps e.g. 54 and 54+ adjacent
        // without changing the ROM filename or database identity.
        std::stable_sort(indices_.begin(), indices_.end(), [&](std::size_t a, std::size_t b) {
            return CatalogLess(games[a], games[b]);
        });
    }
    else if (view_ == CollectionView::RecentlyPlayed)
    {
        std::stable_sort(indices_.begin(), indices_.end(), [&](std::size_t a, std::size_t b) {
            return games[a].lastPlayed > games[b].lastPlayed;
        });
    }
    else if (view_ == CollectionView::MostPlayed)
    {
        std::stable_sort(indices_.begin(), indices_.end(), [&](std::size_t a, std::size_t b) {
            if (games[a].playCount != games[b].playCount)
                return games[a].playCount > games[b].playCount;
            return games[a].title < games[b].title;
        });
    }

    if (!selectedFilename.empty())
    {
        for (std::size_t p = 0; p < indices_.size(); ++p)
        {
            if (games[indices_[p]].filename == selectedFilename)
            {
                currentPosition_ = p;
                break;
            }
        }
    }
}

void CollectionManager::SetView(CollectionView view)
{
    view_ = view;
    Rebuild();
}

void CollectionManager::SetShowUninstalled(bool show)
{
    if (showUninstalled_ == show)
        return;

    showUninstalled_ = show;
    Rebuild();
}

void CollectionManager::CycleView(int direction)
{
    int value = static_cast<int>(view_) + (direction >= 0 ? 1 : -1);
    if (value < 0) value = 3;
    if (value > 3) value = 0;
    SetView(static_cast<CollectionView>(value));
}

CollectionView CollectionManager::View() const noexcept { return view_; }
const char* CollectionManager::ViewName() const noexcept
{
    switch (view_)
    {
    case CollectionView::AllGames: return "ALL GAMES";
    case CollectionView::Favorites: return "FAVORITES";
    case CollectionView::RecentlyPlayed: return "RECENTLY PLAYED";
    case CollectionView::MostPlayed: return "MOST PLAYED";
    default: return "COLLECTION";
    }
}
std::size_t CollectionManager::Count() const noexcept { return indices_.size(); }
std::size_t CollectionManager::CurrentPosition() const noexcept { return currentPosition_; }
const GameInfo* CollectionManager::Current() const noexcept { return Get(currentPosition_); }
GameInfo* CollectionManager::Current() noexcept
{
    if (!library_ || currentPosition_ >= indices_.size()) return nullptr;
    return library_->Get(indices_[currentPosition_]);
}
const GameInfo* CollectionManager::Get(std::size_t position) const noexcept
{
    if (!library_ || position >= indices_.size()) return nullptr;
    return library_->Get(indices_[position]);
}
void CollectionManager::Move(int direction)
{
    if (indices_.empty()) return;
    if (direction > 0) currentPosition_ = (currentPosition_ + 1) % indices_.size();
    else if (direction < 0) currentPosition_ = currentPosition_ == 0 ? indices_.size() - 1 : currentPosition_ - 1;
}

void CollectionManager::SelectPosition(std::size_t position)
{
    if (position < indices_.size())
        currentPosition_ = position;
}


bool CollectionManager::SelectFilename(const std::string& filename)
{
    if (!library_ || filename.empty())
        return false;

    const auto& games = library_->Games();
    for (std::size_t position = 0; position < indices_.size(); ++position)
    {
        if (games[indices_[position]].filename == filename)
        {
            currentPosition_ = position;
            return true;
        }
    }

    // A favorite can be clicked while another filtered collection is active.
    // Return to All Games so the selected title is always visible in the library.
    SetView(CollectionView::AllGames);
    for (std::size_t position = 0; position < indices_.size(); ++position)
    {
        if (games[indices_[position]].filename == filename)
        {
            currentPosition_ = position;
            return true;
        }
    }
    return false;
}
