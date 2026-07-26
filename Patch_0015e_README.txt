O2EM-NG Patch 0015e - Safe Media Delete
=========================================
Base: O2EM-NG(11).ZIP

Changes
-------
- Adds a DELETE button beside every Import Center category:
  ROM, BIOS, Manual, Cover and Screenshot.
- ROM, Manual and Cover deletion targets the selected game's current file.
- Screenshot deletion targets the screenshot currently displayed.
- BIOS deletion opens directly in the O2EM-NG BIOS folder so the installed
  BIOS file can be selected.
- Every delete operation requires confirmation.
- Deleted files are moved to the Windows Recycle Bin, not permanently erased.
- Delete buttons are disabled when the selected game has no matching media.
- Media status and screenshot display refresh immediately after deletion.

Test
----
1. Open O2EM-NG.slnx.
2. Select x64 Debug.
3. Rebuild Solution.
4. Test with expendable/copied media first.
5. Verify that deleted files appear in the Windows Recycle Bin.

Note
----
BIOS deletion is always selectable because more than one BIOS file may exist.
The file chooser is restricted to the project's BIOS folder.

UPDATE 0015e v2
---------------
- Delete buttons are visible beside every Add action in Import Center.
- Screenshot tab now also has a visible DELETE button for the currently shown image.
- Clicking DELETE uses the same confirmation and Windows Recycle Bin workflow.
