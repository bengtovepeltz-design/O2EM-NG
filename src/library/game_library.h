#pragma once

#include <cstddef>
#include <vector>

#include "game_info.h"

class GameLibrary
{
public:
    void Clear();
    void Add(const GameInfo& game);
    void SetGames(const std::vector<RomEntry>& roms);

    bool Empty() const noexcept;
    std::size_t Count() const noexcept;
    std::size_t CurrentIndex() const noexcept;

    GameInfo* Current() noexcept;
    const GameInfo* Current() const noexcept;

    GameInfo* Get(std::size_t index) noexcept;
    const GameInfo* Get(std::size_t index) const noexcept;

    std::vector<GameInfo>& Games() noexcept;
    const std::vector<GameInfo>& Games() const noexcept;

    void Select(std::size_t index) noexcept;
    void Next() noexcept;
    void Previous() noexcept;
    void Move(int direction) noexcept;

private:
    std::vector<GameInfo> games_;
    std::size_t currentIndex_ = 0;
};
