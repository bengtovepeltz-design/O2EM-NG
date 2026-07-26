O2EM-NG Patch 0015f v7 - Header Render Cleanup
================================================

Baslinje:
- O2EM-NG(12)
- Patch 0015f v3/v5/v6

Ändringar:
- Återställer projektets ursprungliga låga headerbild.
- Ingen ny headerbild har genererats.
- Tar bort endast den felaktiga skräptexten efter PRESERVATION vid rendering.
- Hela ordet PRESERVATION behålls.
- Maskeringen skalas proportionellt i både fönsterläge och fullscreen.
- Ingen annan UI-, databas-, import- eller emulatorlogik ändras.

Installation:
1. Packa upp ZIP-filen över projektroten.
2. Ersätt befintliga filer.
3. Kör x64 Debug -> Rebuild Solution.
