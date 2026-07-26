#include "game_library.h"

#include "../../rom_browser.h"

void GameLibrary::Clear()
{
    games_.clear();
    currentIndex_ = 0;
}

void GameLibrary::Add(const GameInfo& game)
{
    games_.push_back(game);
}

void GameLibrary::SetGames(const std::vector<RomEntry>& roms)
{
    Clear();
    games_.reserve(roms.size());

    for (const RomEntry& rom : roms)
        games_.push_back(MakeGameInfo(rom));
}

bool GameLibrary::Empty() const noexcept
{
    return games_.empty();
}

std::size_t GameLibrary::Count() const noexcept
{
    return games_.size();
}

std::size_t GameLibrary::CurrentIndex() const noexcept
{
    return currentIndex_;
}

GameInfo* GameLibrary::Current() noexcept
{
    return Get(currentIndex_);
}

const GameInfo* GameLibrary::Current() const noexcept
{
    return Get(currentIndex_);
}

GameInfo* GameLibrary::Get(std::size_t index) noexcept
{
    if (index >= games_.size())
        return nullptr;

    return &games_[index];
}

const GameInfo* GameLibrary::Get(std::size_t index) const noexcept
{
    if (index >= games_.size())
        return nullptr;

    return &games_[index];
}

std::vector<GameInfo>& GameLibrary::Games() noexcept
{
    return games_;
}

const std::vector<GameInfo>& GameLibrary::Games() const noexcept
{
    return games_;
}

void GameLibrary::Select(std::size_t index) noexcept
{
    if (index < games_.size())
        currentIndex_ = index;
}

void GameLibrary::Next() noexcept
{
    if (games_.empty())
        return;

    currentIndex_ = (currentIndex_ + 1) % games_.size();
}

void GameLibrary::Previous() noexcept
{
    if (games_.empty())
        return;

    currentIndex_ =
        currentIndex_ == 0
            ? games_.size() - 1
            : currentIndex_ - 1;
}

void GameLibrary::Move(int direction) noexcept
{
    if (direction > 0)
        Next();
    else if (direction < 0)
        Previous();
}
