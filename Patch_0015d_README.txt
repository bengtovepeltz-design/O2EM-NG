O2EM-NG Patch 0015d - Media Polish
==================================
Base:
  O2EM-NG(10) + Patch 0015 + 0015a + 0015b + 0015c

Changes:
- Screenshot tab can browse every screenshot for the selected game.
- Added Previous and Next buttons when more than one screenshot exists.
- Page Up / Page Down browse screenshots while the Screenshot tab is active.
- Screenshot selection resets automatically when another game is selected.
- Import Center now displays media status for the selected game:
  ROM, Cover, Manual and Screenshot count.
- Collection Page Up / Page Down behaviour is unchanged outside Screenshot.

Install:
1. Close Visual Studio and O2EM-NG.
2. Extract this ZIP over the project root.
3. Allow files to be replaced.
4. Open O2EM-NG.slnx.
5. Select x64 Debug.
6. Rebuild Solution.

Suggested tests:
- Select American Football with two screenshots.
- Open Screenshot and click Previous / Next.
- Test Page Up / Page Down in Screenshot.
- Open Import Center and verify the media status row.
- Select a game without screenshots and verify the empty-state message.

Database note:
Homebrew/prototype catalogue IDs are intentionally not introduced in this
patch. That change will be handled as a separate, controlled database
migration after the current Beta 3 UI/import work is stable.
