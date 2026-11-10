# O2EM-NG ↔ MAME 0.289 compatibility matrix (Checkpoint A)

Generated from `tests/compat/matrix/compat_matrix.csv`.

Automated triage only. PIXEL-PERFECT PASS is not claimed; palette, animation phase, input timing and BIOS selection are separate. ORANGE/UNKNOWN entries are probes for the architecture audit.

## Severity summary

| Severity | Count |
|---|---:|
| GREEN | 18 |
| YELLOW | 39 |
| ORANGE | 9 |
| UNKNOWN | 19 |
| **Total** | **85** |

## GREEN

| VP | Title | ROM | CRC | System | Region | MAME | O2EM | Geometry metric |
|---|---|---|---|---|---|---|---|---|
| VP02 | Pairs/Space Rendezvous/Logic | vp_02.bin | A69C2C93 | G7000 | PAL | 3prspclg | YES | iou=0,813<br> rowcorr=0,935<br> colcorr=0,912<br> litO2=0,274<br> litM=0,252 |
| VP04 | Air-Sea War/Battle | vp_04.bin | 5AE57A8C | G7000 | PAL | 2aswbat | YES | iou=0,913<br> rowcorr=0,935<br> colcorr=0,688<br> litO2=0,521<br> litM=0,487 |
| VP05 | Blackjack | vp_05.bin | 6F66B1F7 | G7000 | PAL | blackjck | YES | iou=0,583<br> rowcorr=0,892<br> colcorr=0,815<br> litO2=0,036<br> litM=0,028 |
| VP06 | Tenpin Bowling/Basketball | vp_06.bin | 084EE035 | G7000 | PAL | 2bwlbskt | YES | iou=0,580<br> rowcorr=0,904<br> colcorr=0,822<br> litO2=0,072<br> litM=0,060 |
| VP10 | Golf | vp_10.bin | 14B6CC76 | G7000 | PAL | golf | YES | iou=0,846<br> rowcorr=0,944<br> colcorr=0,891<br> litO2=0,348<br> litM=0,323 |
| VP11 | Cosmic Conflict | vp_11.bin | F4C2075E | G7000 | PAL | cosmic | YES | iou=0,820<br> rowcorr=0,916<br> colcorr=0,732<br> litO2=0,418<br> litM=0,375 |
| VP14 | Gunfighter | vp_14.bin | ABE368BF | G7000 | PAL | gunfight | YES | iou=0,741<br> rowcorr=0,892<br> colcorr=0,749<br> litO2=0,463<br> litM=0,428 |
| VP25 | Skiing | vp_25.bin | 4663C4B6 | G7000 | PAL | ski | YES | iou=0,787<br> rowcorr=0,978<br> colcorr=0,720<br> litO2=0,380<br> litM=0,373 |
| VP35 | Electronic Billiards | vp_35.bin | 24D53E2C | G7000 | PAL | billiard | YES | iou=0,875<br> rowcorr=0,946<br> colcorr=0,683<br> litO2=0,537<br> litM=0,498 |
| VP36 | Electronic Soccer/Electronic Ice Hockey | vp_36.bin | 39989464 | G7000 | PAL | 2hckyscr | YES | iou=0,779<br> rowcorr=0,909<br> colcorr=0,678<br> litO2=0,495<br> litM=0,445 |
| VP45 | Morse | vp_45.bin | 2122012C | G7000 | PAL | morse | YES | iou=0,665<br> rowcorr=0,823<br> colcorr=0,679<br> litO2=0,243<br> litM=0,183 |
| VP48 | Backgammon | vp_48.bin | 9D2C45DC | G7000 | PAL | backgamm | YES | iou=0,780<br> rowcorr=0,757<br> colcorr=0,684<br> litO2=0,625<br> litM=0,618 |
| VP61 | Interpol (Europe, prototype) | vp_61.bin | 69D21F8F | G7000 | PAL | interpol | YES | iou=0,650<br> rowcorr=0,864<br> colcorr=0,854<br> litO2=0,048<br> litM=0,035 |
| VP64 | Shark Hunter (Europe, prototype) | vp_64.bin | DF36683F | G7000 | PAL | shark | YES | iou=0,857<br> rowcorr=0,987<br> colcorr=0,644<br> litO2=0,650<br> litM=0,625 |
| VP65 | Spider-Man (Europe, prototype, set 1) | vp_65.bin | 35ACE4E8 | G7000 | PAL | spider | YES | iou=0,902<br> rowcorr=0,971<br> colcorr=0,888<br> litO2=0,548<br> litM=0,531 |
| — | AMOK! | Amok (1998)(Dondzilla, John).bin | 2247405E | G7000 | PAL | amok | YES | iou=0,615<br> rowcorr=0,817<br> colcorr=0,861<br> litO2=0,180<br> litM=0,135 |
| — | Atlantis (Europe, USA) | Atlantis (1983)(Imagic)(EU-US).bin | A7344D1F | G7000 | PAL | atlantis | YES | iou=0,945<br> rowcorr=0,999<br> colcorr=0,723<br> litO2=0,427<br> litM=0,424 |
| — | written by Ted (Foolery) Szczypiorski | new_planet-lander.bin | 6CB1DE16 | G7000 | PAL | plander | YES | iou=0,586<br> rowcorr=0,881<br> colcorr=0,652<br> litO2=0,308<br> litM=0,274 |

## YELLOW

| VP | Title | ROM | CRC | System | Region | MAME | O2EM | Geometry metric |
|---|---|---|---|---|---|---|---|---|
| VP01 | Race/Spin-out/Cryptogram | vp_01.bin | 4EDA4917 | G7000 | PAL | 3rcsocpt | YES | iou=0,469<br> rowcorr=0,956<br> colcorr=0,484<br> litO2=0,098<br> litM=0,100 |
| VP03 | American Football | vp_03.bin | F9084AA3 | G7000 | PAL | football | YES | iou=0,432<br> rowcorr=0,777<br> colcorr=0,110<br> litO2=0,420<br> litM=0,383 |
| VP08 | Baseball | vp_08.bin | B07748E7 | G7000 | PAL | baseball | YES | iou=0,416<br> rowcorr=0,722<br> colcorr=0,303<br> litO2=0,165<br> litM=0,130 |
| VP09 | Computer Programmer | vp_09.bin | B590CF14 | G7000 | PAL | compprog | YES | iou=0,298<br> rowcorr=0,802<br> colcorr=0,385<br> litO2=0,023<br> litM=0,028 |
| VP12 | Take the Money and Run | vp_12.bin | 11FF50AD | G7000 | PAL | moneyrun | YES | iou=0,430<br> rowcorr=0,597<br> colcorr=0,158<br> litO2=0,457<br> litM=0,358 |
| VP13 | Playschool Maths | vp_13.bin | 1C8368E7 | G7000 | PAL | psmaths | YES | iou=0,432<br> rowcorr=0,696<br> colcorr=0,263<br> litO2=0,330<br> litM=0,248 |
| VP17 | Chinese Logic | vp_17.bin | 11EDF25B | G7000 | PAL | chinese | YES | iou=0,455<br> rowcorr=0,643<br> colcorr=0,287<br> litO2=0,364<br> litM=0,246 |
| VP18 | Laser War | vp_18.bin | 17BE749B | G7000 | PAL | laser | YES | iou=0,509<br> rowcorr=0,827<br> colcorr=0,402<br> litO2=0,161<br> litM=0,131 |
| VP20 | Stone Sling | vp_20.bin | 44D1A8A5 | G7000 | PAL | stone | YES | iou=0,788<br> rowcorr=0,918<br> colcorr=0,518<br> litO2=0,213<br> litM=0,237 |
| VP21 | Secret of the Pharaohs | vp_21.bin | 7394E82C | G7000 | PAL | pharaohs | YES | iou=0,518<br> rowcorr=0,675<br> colcorr=0,723<br> litO2=0,381<br> litM=0,257 |
| VP22 | Space Monster | vp_22.bin | 71AE8533 | G7000 | PAL | spacemon | YES | iou=0,420<br> rowcorr=0,761<br> colcorr=0,523<br> litO2=0,157<br> litM=0,145 |
| VP23 | Las Vegas Gambling | vp_23.bin | BB6E7135 | G7000 | PAL | lasvegas | YES | iou=0,555<br> rowcorr=0,710<br> colcorr=0,503<br> litO2=0,486<br> litM=0,397 |
| VP24 | Flipper Game | vp_24.bin | 2C9D1715 | G7000 | PAL | flipper | YES | iou=0,470<br> rowcorr=0,722<br> colcorr=0,290<br> litO2=0,275<br> litM=0,218 |
| VP26 | Basket Game | vp_26.bin | 239DF97D | G7000 | PAL | baskgame | YES | iou=0,509<br> rowcorr=0,663<br> colcorr=0,208<br> litO2=0,381<br> litM=0,301 |
| VP27 | Electronic Table Football | vp_27.bin | 89C6DE2C | G7000 | PAL | tablefb | YES | iou=0,391<br> rowcorr=0,666<br> colcorr=0,206<br> litO2=0,275<br> litM=0,191 |
| VP28 | Electronic Volleyball | vp_28.bin | D3B09FEC | G7000 | PAL | volley | YES | iou=0,395<br> rowcorr=0,677<br> colcorr=0,361<br> litO2=0,271<br> litM=0,221 |
| VP29 | Dam Buster | vp_29.bin | FB83171E | G7000 | PAL | dambust | YES | iou=0,629<br> rowcorr=0,878<br> colcorr=0,081<br> litO2=0,427<br> litM=0,413 |
| VP30 | Battlefield | vp_30.bin | 58FA6766 | G7000 | PAL | battlef | YES | iou=0,428<br> rowcorr=0,671<br> colcorr=0,278<br> litO2=0,314<br> litM=0,256 |
| VP32 | A Labyrinth Game/Supermind | vp_32.bin | D158EEBA | G7000 | PAL | 2lbsprmd | YES | iou=0,396<br> rowcorr=0,489<br> colcorr=0,098<br> litO2=0,519<br> litM=0,359 |
| VP33 | Jumping Acrobats | vp_33.bin | 6CEBAB74 | G7000 | PAL | acrobat | YES | iou=0,380<br> rowcorr=0,728<br> colcorr=0,251<br> litO2=0,210<br> litM=0,274 |
| VP34 | Satellite Attack | vp_34.bin | 395629E1 | G7000 | PAL | satellit | YES | iou=0,476<br> rowcorr=0,974<br> colcorr=0,491<br> litO2=0,094<br> litM=0,082 |
| VP37 | Monkeyshines | vp_37.bin | FEE664F3 | G7000 | PAL | monkey | YES | iou=0,380<br> rowcorr=0,609<br> colcorr=0,431<br> litO2=0,401<br> litM=0,316 |
| VP38 | Munchkin | vp_38.bin | 4A3E2DC8 | G7000 | PAL | munchkin | YES | iou=0,449<br> rowcorr=0,565<br> colcorr=0,101<br> litO2=0,456<br> litM=0,356 |
| VP39 | Freedom Fighters | vp_39.bin | 54736B74 | G7000 | PAL | freedom | YES | iou=0,488<br> rowcorr=0,970<br> colcorr=0,401<br> litO2=0,082<br> litM=0,076 |
| VP40 | 4-in-1 Row | vp_40.bin | 3BFEF56B | G7000 | PAL | 4in1 | YES | iou=0,753<br> rowcorr=1,000<br> colcorr=0,178<br> litO2=0,562<br> litM=0,560 |
| VP41 | Conquest of the World | vp_41.bin | AFC093FD | G7000 | PAL | conquest | YES | iou=0,632<br> rowcorr=0,901<br> colcorr=0,267<br> litO2=0,138<br> litM=0,110 |
| VP42 | Quest for the Rings | vp_42.bin | 9451BE1E | G7000 | PAL | quest | YES | iou=0,436<br> rowcorr=0,612<br> colcorr=0,030<br> litO2=0,526<br> litM=0,388 |
| VP43 | Pick Axe Pete | vp_43.bin | D62814A3 | G7000 | PAL | pickaxe | YES | iou=0,545<br> rowcorr=0,621<br> colcorr=0,164<br> litO2=0,588<br> litM=0,393 |
| VP44 | Crazy Chase | vp_44.bin | A6909A8B | G7000 | PAL | crazyc | YES | iou=0,431<br> rowcorr=0,591<br> colcorr=0,193<br> litO2=0,441<br> litM=0,328 |
| VP46 | The Great Wall Street Fortune Hunt | vp_46.bin | 881CEAE4 | G7000 | PAL | gtwallst | YES | iou=0,800<br> rowcorr=0,870<br> colcorr=0,329<br> litO2=0,461<br> litM=0,388 |
| VP47 | The Mousing Cat | vp_47.bin | F390BFEC | G7000 | PAL | mousing | YES | iou=0,478<br> rowcorr=0,896<br> colcorr=0,835<br> litO2=0,032<br> litM=0,025 |
| VP49 | Turtles | vp_49.bin | 1C750349 | G7000 | PAL | turtles | YES | iou=0,488<br> rowcorr=0,612<br> colcorr=0,094<br> litO2=0,522<br> litM=0,401 |
| VP50 | Super Bee | vp_50.bin | 9585D511 | G7000 | PAL | superbee | YES | iou=0,446<br> rowcorr=0,682<br> colcorr=0,296<br> litO2=0,230<br> litM=0,181 |
| VP51 | Terrahawks (Europe, plus) | vp_51.bin | 764894A1 | G7000 | PAL | hawks | YES | iou=0,585<br> rowcorr=0,880<br> colcorr=0,346<br> litO2=0,172<br> litM=0,147 |
| VP52 | Killer Bees (Europe, plus) | vp_52.bin | DEE70B33 | G7000 | PAL | beespl | YES | iou=0,442<br> rowcorr=0,781<br> colcorr=0,402<br> litO2=0,278<br> litM=0,215 |
| VP54 | Loony Balloon (no plus graphics, unreleased) | vp_54.bin | CA23F40C | G7000 | PAL | loonyp | YES | iou=0,473<br> rowcorr=0,678<br> colcorr=0,211<br> litO2=0,368<br> litM=0,278 |
| — | Exojet+ (France) | Exojet (198x)(Philips)(FR).bin | CFCA6F39 | G7000 | PAL | exojet | YES | iou=0,571<br> rowcorr=0,917<br> colcorr=0,469<br> litO2=0,177<br> litM=0,208 |
| — | Popeye (Europe) | Popeye (1983)(Parker Brothers)(EU).bin | 5216771A | G7000 | PAL | popeye | YES | iou=0,750<br> rowcorr=0,773<br> colcorr=0,599<br> litO2=0,669<br> litM=0,569 |
| — | Super Cobra (Brazil) | Super Cobra (1983)(Parker Brothers)(BR).bin | E58AA0AC | G7000 | NTSC | cobrab | YES | iou=0,407<br> rowcorr=0,644<br> colcorr=0,286<br> litO2=0,548<br> litM=0,281 |

## ORANGE

| VP | Title | ROM | CRC | System | Region | MAME | O2EM | Geometry metric |
|---|---|---|---|---|---|---|---|---|
| VP07 | Mathematician/Echo | vp_07.bin | DC5AFAA2 | G7000 | PAL | 2mthecho | YES | iou=0,108<br> rowcorr=0,137<br> colcorr=0,458<br> litO2=0,036<br> litM=0,050 |
| VP15 | Samurai | vp_15.bin | 7810BAD5 | G7000 | PAL | samurai | YES | iou=0,185<br> rowcorr=0,464<br> colcorr=0,201<br> litO2=0,121<br> litM=0,268 |
| VP16 | Depth Charge/Marksman | vp_16.bin | 2DCB77F0 | G7000 | PAL | 2dpthmrk | YES | iou=0,080<br> rowcorr=-0,320<br> colcorr=0,471<br> litO2=0,316<br> litM=0,313 |
| VP19 | Catch the Ball/Noughts and Crosses | vp_19.bin | A57E1724 | G7000 | PAL | 2bllcrss | YES | iou=0,193<br> rowcorr=-0,162<br> colcorr=0,316<br> litO2=0,489<br> litM=0,263 |
| VP31 | Musician | vp_31.bin | AFB23F89 | G7000 | PAL | musician | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,290 |
| VP57 | Blobbers | vp_57.bin | 9D72D4E9 | G7000 | PAL | blobbers | YES | iou=0,017<br> rowcorr=-0,196<br> colcorr=-0,218<br> litO2=0,048<br> litM=0,461 |
| VP60 | Trans American Rally (Europe) | vp_60.bin | 0D2D721D | G7000 | PAL | rally | YES | iou=0,375<br> rowcorr=0,727<br> colcorr=0,558<br> litO2=0,004<br> litM=0,005 |
| VP60 | Trans American Rally + (works only on G7400 and Jopac) | vp_60_16.bin | 0D2D721D | G7000 | PAL | rally | YES | iou=0,375<br> rowcorr=0,727<br> colcorr=0,558<br> litO2=0,004<br> litM=0,005 |
| — | Moto-Crash+ (France) | Moto-Crash (France).bin | 68560DC7 | G7000 | PAL | motocras | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,396 |

## UNKNOWN

| VP | Title | ROM | CRC | System | Region | MAME | O2EM | Geometry metric |
|---|---|---|---|---|---|---|---|---|
| VP01+ | Race / Spin-Out / Cryptogram (Europe, plus) | vp_01+.bin | EE3EE642 | G7400/VPP | PAL | 3rcsocptpl | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP06+ | Ten Pin Bowling / Basketball (Europe, plus, prototype) | vp_06+.bin | E5FB4260 | G7400/VPP | PAL | 2bwlbsktpl | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP11+ | Cosmic Conflict (Europe, plus) | vp_11+.bin | A8DBED92 | G7400/VPP | PAL | cosmicpl | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP20+ | Stone Sling (Europe, plus) | vp_20+.bin | 2391C2FB | G7400/VPP | PAL | stonepl | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP43+ | Pickaxe Pete (Europe, plus) | vp_43+.bin | B2FFB353 | G7400/VPP | PAL | pickaxepl | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP53+ | Phantom House (Europe, prototype) | vp_53+.bin | 313547EB | G7400/VPP | PAL | phantom | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP54+ | Loony Balloon (Europe, plus) | vp_54+.bin | B932D584 | G7400/VPP | PAL | loony | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP55+ | Neutron Star (Europe, prototype) | vp_55+.bin | D03EFA86 | G7400/VPP | PAL | neutronp | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP56+ | Norseman (Europe) | vp_56+.bin | A75C42F8 | G7400/VPP | PAL | norseman | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP58+ | Air Battle (Europe, prototype, set 1) | vp_58+.bin | 1EEB88F5 | G7400/VPP | PAL | airbattlp | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP59+ | Helicopter Rescue (Europe) | vp_59+.bin | C4134DF8 | G7400/VPP | PAL | helicopt | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP60+ | Trans American Rally (Europe) | vp_60+.bin | 0D2D721D | G7400/VPP | PAL | rally | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| VP62+ | Clay Pigeon (Europe, plus, prototype, set 1) | vp_62+.bin | 6FABCB7E | G7400/VPP | PAL | claypigpl | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| — |  | Bird Hunt (Ahnl66).bin | 5EC65414 | G7000 | PAL | — | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| — |  | Kill the Attacking Aliens — demo 2.bin | 877B4109 | G7000 | PAL | — | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| — | written by Rene van den Enden | new_pong.bin | 245D3533 | G7000 | PAL | — | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| — |  | twobv.bin | B25F6E1D | G7000 | PAL | — | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| vp_C7010 | Chess (not complete, game has extra microprocessor module) | vp_C7010.bin | 77066338 | G7000 | PAL | chess | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |
| vp_Tutankham+ | Tutankham (Europe, prototype) | vp_Tutankham+.bin | 0B2DEB61 | G7400/VPP | PAL | tutank | YES | iou=0,000<br> rowcorr=0,000<br> colcorr=0,000<br> litO2=0,000<br> litM=0,000 |

## Notes on the ORANGE/UNKNOWN probes

- ORANGE can be caused by a different animation/game state (input and BIOS frame timing differ between MAME and O2EM-NG), not only by a renderer defect. Always inspect `tests/compat/diff/<rom>_side.png` before promoting an ORANGE item to a core defect.
- UNKNOWN is expected where MAME requires the G7400/VPP BIOS/charset (not present locally), where the ROM is a multi-game cart requiring a specific in-cart menu key, or where the default `1` + `Enter` BIOS sequence does not select the game.
- Moto-Crash (France) and similar Jopac/G7400-only variants need the correct system/BIOS, not the G7000 default.
- Palette differences are deliberately excluded from the geometry metric (lit-mask IoU + row/column profile correlation).

## Reproduce

```text
python tests/compat/scripts/inventory_roms.py
powershell -NoProfile -ExecutionPolicy Bypass -File tests/compat/scripts/run_matrix.ps1 -All
python tests/compat/scripts/summarize_matrix.py
```