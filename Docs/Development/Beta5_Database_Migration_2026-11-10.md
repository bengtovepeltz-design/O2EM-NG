# Beta 5 database migration and project-page ownership

## Result

Beta 4 installations can be upgraded in place to Beta 5 without losing personal
data. The user `o2em-ng.db` and `mycollection.db` are preserved by the installer
and migrated by the application on first Beta 5 startup.

## Ownership model

### System-owned

- About
- Credits
- Special Thanks
- Roadmap
- Release Notes
- standard catalogue rows and approved system corrections
- the packaged migration catalogue (`GAMEDATA/catalogue.db`)

### User-owned

- `games.favorite`
- `games.play_count`
- `games.last_played`
- `games.user_rating`
- `games.rating`
- `suppressed_catalog_entries`
- all game fields written by Edit Game Data:
  `user_title`, `year`, `publisher`, `developer`, `genre`, `players`,
  `controls`, `voice_module`, `videopac_plus_text`, `rating`,
  `short_description`, `user_description`, `trivia`, `manual_path`,
  `user_catalog_id`
- the complete `mycollection.db`

`rating` is the legacy free-text personal rating and is never migrated over.
`user_rating` is the current personal 0-5 star rating.

## Project-page architecture

System project pages are compiled into the application through
`src/frontend/system_project_pages.cpp`. The application does not load current
pages from `project_pages` in the user database.

Legacy `project_pages` rows may remain in an old database for compatibility and
historical preservation, but they are ignored by the UI and can never override
the current Beta 5 pages.

The project-page editor was removed from the normal UI. System pages are
view-only in both Debug and Release; no Debug-only project editor is retained.
Editing of game/library metadata is unchanged.

## Schema versioning

- Beta 4 database: `PRAGMA user_version = 0`
- Fresh Beta 5 seed: `PRAGMA user_version = 5`
- Migrated Beta 5 database: `PRAGMA user_version = 5`

The schema migration is additive and idempotent:

- `games.user_rating INTEGER NOT NULL DEFAULT 0`
- `suppressed_catalog_entries(rom_filename TEXT PRIMARY KEY COLLATE NOCASE)`

Existing `user_rating` values are preserved. Missing columns/tables are added
without recreating the `games` table.

## Migration algorithm

On application startup:

1. Read `PRAGMA user_version`.
2. If the database exists and version is below 5, create safety backups of
   `o2em-ng.db` and, if present, `mycollection.db`.
3. Ensure the Beta 5 schema exists with additive `CREATE`/`ALTER` statements.
4. Run the existing Gamelist refresh.
5. In a `BEGIN IMMEDIATE` transaction, merge `GAMEDATA/catalogue.db`:
   - insert rows not present in the user database;
   - update system-owned catalogue fields on existing rows;
   - fill only empty user-editable metadata fields from the seed;
   - never overwrite favorite/play history/user rating/user metadata/rating/
     manual path/user catalog ID;
   - skip inserting a new row when the user has suppressed that filename;
   - apply the approved `vp_60.bin` correction
     `videopac_plus_text = Yes`.
6. Set `PRAGMA user_version = 5`.
7. Commit. On failure, roll back the catalogue merge transaction.

The three Beta 5 catalogue additions are:

- `Exojet (198x)(Philips)(FR).bin`
- `twobv.bin`
- `vp_60+.bin`

## Backups

Before an existing database is migrated:

```text
GAMEDATA/Backups/o2em-ng.pre-v5.0.0-<timestamp>.db
GAMEDATA/Backups/mycollection.pre-v5.0.0-<timestamp>.db
```

Backups are created with the SQLite backup API and unique names; existing
backups are not overwritten. If a required backup fails, migration is aborted
before modification.

## Upgrade behavior

- Fresh install: installer supplies the clean Beta 5 seed and
  `catalogue.db`; application creates an empty `mycollection.db`.
- Installer upgrade: existing `o2em-ng.db` and `mycollection.db` are preserved;
  `catalogue.db` and the application are updated; first startup migrates the
  preserved database.
- Portable/manual upgrade: same application startup migration; it does not
  depend on Inno Setup.
- Repair/reinstall: version 5 database is already migrated; startup merge is
  idempotent and does not create additional migration backups.
- Second launch: no migration backup and no duplicate catalogue rows.

## Verification summary

Tested with a real Beta 4 user database containing 231 games, 18 favorites,
101 play counts, 9 personal ratings, 10 suppressed entries, custom user
metadata and a populated 23-row `mycollection.db`:

- before: 231 games, user_version 0
- after: 234 games, user_version 5, no duplicates, integrity `ok`
- all favorites, play counts, last-played values, personal ratings, legacy
  rating, custom metadata and suppressed entries preserved
- `mycollection.db` remained byte-identical
- two migration backups created
- second run created no new backups and did not duplicate rows
- clean install: 234 games, zero personal data, empty My Collection

## Installer policy

The installer continues to preserve `GAMEDATA/o2em-ng.db` and
`GAMEDATA/mycollection.db` on upgrade. It always updates the system-owned
`GAMEDATA/catalogue.db`. Migration is performed by the application so it also
works for portable/manual upgrades.
