#pragma once

#include <cstddef>
#include <string>
#include <vector>

class GameLibrary;
struct GameInfo;

enum class CollectionView
{
    AllGames = 0,
    Favorites,
    RecentlyPlayed,
    MostPlayed
};

class CollectionManager
{
public:
    void Attach(GameLibrary* library);
    void Rebuild();
    void SetView(CollectionView view);
    void CycleView(int direction);
    void SetShowUninstalled(bool show);

    CollectionView View() const noexcept;
    const char* ViewName() const noexcept;
    std::size_t Count() const noexcept;
    std::size_t CurrentPosition() const noexcept;
    const GameInfo* Current() const noexcept;
    GameInfo* Current() noexcept;
    const GameInfo* Get(std::size_t position) const noexcept;
    void Move(int direction);
    void SelectPosition(std::size_t position);
    bool SelectFilename(const std::string& filename);

private:
    GameLibrary* library_ = nullptr;
    CollectionView view_ = CollectionView::AllGames;
    std::vector<std::size_t> indices_;
    std::size_t currentPosition_ = 0;
    bool showUninstalled_ = false;
};
