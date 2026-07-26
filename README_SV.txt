O2EM-NG - Collection Engine 01
==============================

Detta är första steget i den centrala GameLibrary-arkitekturen.

Nya filer:
  src/library/game_info.h
  src/library/game_library.h
  src/library/game_library.cpp

Ändrade filer:
  src/frontend/frontend_app.h
  src/frontend/frontend_app.cpp
  O2EM-NG.vcxproj
  O2EM-NG.vcxproj.filters

Vad som har ändrats
-------------------
- FrontendApp har inte längre ett eget selected_-index.
- GameLibrary äger nu listan över GameInfo-objekt.
- GameLibrary är den enda källan för aktuell markering.
- Tangentbord och gamepad flyttar markeringen genom GameLibrary.
- Spelstart och BoxArt använder fortfarande RomEntry genom en tillfällig
  kompatibilitetsbrygga i GameInfo. Detta gör refaktoreringen säker och liten.
- Programmet ska se ut och fungera exakt som före ändringen.

Kopiering
---------
1. Stäng Visual Studio.
2. Ta en backup eller Git-commit.
3. Kopiera innehållet i denna ZIP till projektets rot och skriv över.
4. Öppna lösningen igen.
5. Bygg Debug | x64.

Testlista
---------
1. Programmet startar utan fel.
2. Library visar samma ROM-lista som tidigare.
3. Upp/ned och gamepad ändrar vald ROM.
4. Markeringen wrappar från första till sista och tvärtom.
5. BoxArt följer vald ROM.
6. Enter/A startar rätt spel.
7. Frontenden återkommer efter avslutat spel.
8. Flikar och Settings fungerar som tidigare.

Teknisk kontroll
----------------
- game_library.cpp har syntaxkompilerats separat med C++20.
- Båda Visual Studio-projektfilerna har validerats som korrekt XML.
- Full SDL3/Visual Studio-kompilering kan endast verifieras i din lokala miljö.
