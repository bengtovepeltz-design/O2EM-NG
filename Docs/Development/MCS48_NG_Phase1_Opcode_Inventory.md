# Static opcode inventory

Counts only: not a 256-opcode semantic conformance test. O2EM counts exclude interrupt entry and peripherals.

| Hex | O2EM mnemonic | Cycles O2EM / MAME | MAME handler | Notes |
|---|---|---|---|---|
| 00 | NOP | 1 / 1 | nop |  |
| 01 | ILL | 1 / 1 | illegal | illegal / silent |
| 02 | OUTL BUS,A | 2 / 2 | outl_bus_a | legal but stubbed |
| 03 | ADD A,#data | 2 / 2 | add_a_n |  |
| 04 | JMP | 2 / 2 | jmp_0 |  |
| 05 | EN I | 1 / 1 | en_i |  |
| 06 | ILL | 1 / 1 | illegal | illegal / silent |
| 07 | DEC A | 1 / 1 | dec_a |  |
| 08 | INS A,BUS | 2 / 2 | ins_a_bus |  |
| 09 | IN A,Pp | 2 / 2 | in_a_p1 |  |
| 0A | IN A,Pp | 2 / 2 | in_a_p2 |  |
| 0B | ILL | 1 / 1 | illegal | illegal / silent |
| 0C | MOVD A,P4 | 2 / 2 | movd_a_p4 |  |
| 0D | MOVD A,P5 | 2 / 2 | movd_a_p5 |  |
| 0E | MOVD A,P6 | 2 / 2 | movd_a_p6 |  |
| 0F | MOVD A,P7 | 2 / 2 | movd_a_p7 |  |
| 10 | INC @Ri | 1 / 1 | inc_xr0 |  |
| 11 | INC @Ri | 1 / 1 | inc_xr1 |  |
| 12 | JBb address | 2 / 2 | jb_0 |  |
| 13 | ADDC A,#data | 2 / 2 | adc_a_n |  |
| 14 | CALL | 2 / 2 | call_0 |  |
| 15 | DIS I | 1 / 1 | dis_i |  |
| 16 | JTF | 2 / 2 | jtf |  |
| 17 | INC A | 1 / 1 | inc_a |  |
| 18 | INC Rr | 1 / 1 | inc_r0 |  |
| 19 | INC Rr | 1 / 1 | inc_r1 |  |
| 1A | INC Rr | 1 / 1 | inc_r2 |  |
| 1B | INC Rr | 1 / 1 | inc_r3 |  |
| 1C | INC Rr | 1 / 1 | inc_r4 |  |
| 1D | INC Rr | 1 / 1 | inc_r5 |  |
| 1E | INC Rr | 1 / 1 | inc_r6 |  |
| 1F | INC Rr | 1 / 1 | inc_r7 |  |
| 20 | XCH A,@Ri | 1 / 1 | xch_a_xr0 |  |
| 21 | XCH A,@Ri | 1 / 1 | xch_a_xr1 |  |
| 22 | ILL | 1 / 1 | illegal | illegal / silent |
| 23 | MOV a,#data | 2 / 2 | mov_a_n |  |
| 24 | JMP | 2 / 2 | jmp_1 |  |
| 25 | EN TCNTI | 1 / 1 | en_tcnti |  |
| 26 | JNT0 | 2 / 2 | jnt_0 |  |
| 27 | CLR A | 1 / 1 | clr_a |  |
| 28 | XCH A,Rr | 1 / 1 | xch_a_r0 |  |
| 29 | XCH A,Rr | 1 / 1 | xch_a_r1 |  |
| 2A | XCH A,Rr | 1 / 1 | xch_a_r2 |  |
| 2B | XCH A,Rr | 1 / 1 | xch_a_r3 |  |
| 2C | XCH A,Rr | 1 / 1 | xch_a_r4 |  |
| 2D | XCH A,Rr | 1 / 1 | xch_a_r5 |  |
| 2E | XCH A,Rr | 1 / 1 | xch_a_r6 |  |
| 2F | XCH A,Rr | 1 / 1 | xch_a_r7 |  |
| 30 | XCHD A,@Ri | 1 / 1 | xchd_a_xr0 |  |
| 31 | XCHD A,@Ri | 1 / 1 | xchd_a_xr1 |  |
| 32 | JBb address | 2 / 2 | jb_1 |  |
| 33 | ILL | 1 / 1 | illegal | illegal / silent |
| 34 | CALL | 2 / 2 | call_1 |  |
| 35 | DIS TCNTI | 1 / 1 | dis_tcnti |  |
| 36 | JT0 | 2 / 2 | jt_0 |  |
| 37 | CPL A | 1 / 1 | cpl_a |  |
| 38 | ILL | 1 / 1 | illegal | illegal / silent |
| 39 | OUTL P1,A | 2 / 2 | outl_p1_a |  |
| 3A | OUTL P2,A | 2 / 2 | outl_p2_a |  |
| 3B | ILL | 1 / 1 | illegal | illegal / silent |
| 3C | MOVD P4,A | 2 / 2 | movd_p4_a |  |
| 3D | MOVD P5,A | 2 / 2 | movd_p5_a |  |
| 3E | MOVD P6,A | 2 / 2 | movd_p6_a |  |
| 3F | MOVD P7,A | 2 / 2 | movd_p7_a |  |
| 40 | ORL A,@Ri | 1 / 1 | orl_a_xr0 |  |
| 41 | ORL A,@Ri | 1 / 1 | orl_a_xr1 |  |
| 42 | MOV A,T | 1 / 1 | mov_a_t |  |
| 43 | ORL A,#data | 2 / 2 | orl_a_n |  |
| 44 | JMP | 2 / 2 | jmp_2 |  |
| 45 | STRT CNT | 1 / 1 | strt_cnt |  |
| 46 | JNT1 | 2 / 2 | jnt_1 |  |
| 47 | SWAP A | 1 / 1 | swap_a |  |
| 48 | ORL A,Rr | 1 / 1 | orl_a_r0 |  |
| 49 | ORL A,Rr | 1 / 1 | orl_a_r1 |  |
| 4A | ORL A,Rr | 1 / 1 | orl_a_r2 |  |
| 4B | ORL A,Rr | 1 / 1 | orl_a_r3 |  |
| 4C | ORL A,Rr | 1 / 1 | orl_a_r4 |  |
| 4D | ORL A,Rr | 1 / 1 | orl_a_r5 |  |
| 4E | ORL A,Rr | 1 / 1 | orl_a_r6 |  |
| 4F | ORL A,Rr | 1 / 1 | orl_a_r7 |  |
| 50 | ANL A,@Ri | 1 / 1 | anl_a_xr0 |  |
| 51 | ANL A,@Ri | 1 / 1 | anl_a_xr1 |  |
| 52 | JBb address | 2 / 2 | jb_2 |  |
| 53 | ANL A,#data | 2 / 2 | anl_a_n |  |
| 54 | CALL | 2 / 2 | call_2 |  |
| 55 | STRT T | 1 / 1 | strt_t |  |
| 56 | JT1 | 2 / 2 | jt_1 |  |
| 57 | DA A | 1 / 1 | da_a |  |
| 58 | ANL A,Rr | 1 / 1 | anl_a_r0 |  |
| 59 | ANL A,Rr | 1 / 1 | anl_a_r1 |  |
| 5A | ANL A,Rr | 1 / 1 | anl_a_r2 |  |
| 5B | ANL A,Rr | 1 / 1 | anl_a_r3 |  |
| 5C | ANL A,Rr | 1 / 1 | anl_a_r4 |  |
| 5D | ANL A,Rr | 1 / 1 | anl_a_r5 |  |
| 5E | ANL A,Rr | 1 / 1 | anl_a_r6 |  |
| 5F | ANL A,Rr | 1 / 1 | anl_a_r7 |  |
| 60 | ADD A,@Ri | 1 / 1 | add_a_xr0 |  |
| 61 | ADD A,@Ri | 1 / 1 | add_a_xr1 |  |
| 62 | MOV T,A | 1 / 1 | mov_t_a |  |
| 63 | ILL | 1 / 1 | illegal | illegal / silent |
| 64 | JMP | 2 / 2 | jmp_3 |  |
| 65 | STOP TCNT | 1 / 1 | stop_tcnt |  |
| 66 | ILL | 1 / 1 | illegal | illegal / silent |
| 67 | RRC A | 1 / 1 | rrc_a |  |
| 68 | ADD A,Rr | 1 / 1 | add_a_r0 |  |
| 69 | ADD A,Rr | 1 / 1 | add_a_r1 |  |
| 6A | ADD A,Rr | 1 / 1 | add_a_r2 |  |
| 6B | ADD A,Rr | 1 / 1 | add_a_r3 |  |
| 6C | ADD A,Rr | 1 / 1 | add_a_r4 |  |
| 6D | ADD A,Rr | 1 / 1 | add_a_r5 |  |
| 6E | ADD A,Rr | 1 / 1 | add_a_r6 |  |
| 6F | ADD A,Rr | 1 / 1 | add_a_r7 |  |
| 70 | ADDC A,@Ri | 1 / 1 | adc_a_xr0 |  |
| 71 | ADDC A,@Ri | 1 / 1 | adc_a_xr1 |  |
| 72 | JBb address | 2 / 2 | jb_3 |  |
| 73 | ILL | 1 / 1 | illegal | illegal / silent |
| 74 | CALL | 2 / 2 | call_3 |  |
| 75 | EN CLK | 1 / 1 | ent0_clk | legal but stubbed |
| 76 | JF1 address | 2 / 2 | jf1 |  |
| 77 | RR A | 1 / 1 | rr_a |  |
| 78 | ADDC A,Rr | 1 / 1 | adc_a_r0 |  |
| 79 | ADDC A,Rr | 1 / 1 | adc_a_r1 |  |
| 7A | ADDC A,Rr | 1 / 1 | adc_a_r2 |  |
| 7B | ADDC A,Rr | 1 / 1 | adc_a_r3 |  |
| 7C | ADDC A,Rr | 1 / 1 | adc_a_r4 |  |
| 7D | ADDC A,Rr | 1 / 1 | adc_a_r5 |  |
| 7E | ADDC A,Rr | 1 / 1 | adc_a_r6 |  |
| 7F | ADDC A,Rr | 1 / 1 | adc_a_r7 |  |
| 80 | MOVX  A,@Ri | 2 / 2 | movx_a_xr0 |  |
| 81 | MOVX A,@Ri | 2 / 2 | movx_a_xr1 |  |
| 82 | ILL | 1 / 1 | illegal | illegal / silent |
| 83 | RET | 2 / 2 | ret |  |
| 84 | JMP | 2 / 2 | jmp_4 |  |
| 85 | CLR F0 | 1 / 1 | clr_f0 |  |
| 86 | JNI address | 2 / 2 | jni |  |
| 87 | ILL | 1 / 1 | illegal | illegal / silent |
| 88 | BUS,#data | 2 / 2 | orl_bus_n | legal but stubbed |
| 89 | ORL Pp,#data | 2 / 2 | orl_p1_n |  |
| 8A | ORL Pp,#data | 2 / 2 | orl_p2_n |  |
| 8B | ILL | 1 / 1 | illegal | illegal / silent |
| 8C | ORLD P4,A | 2 / 2 | orld_p4_a |  |
| 8D | ORLD P5,A | 2 / 2 | orld_p5_a |  |
| 8E | ORLD P6,A | 2 / 2 | orld_p6_a |  |
| 8F | ORLD P7,A | 2 / 2 | orld_p7_a |  |
| 90 | MOVX @Ri,A | 2 / 2 | movx_xr0_a |  |
| 91 | MOVX @Ri,A | 2 / 2 | movx_xr1_a |  |
| 92 | JBb address | 2 / 2 | jb_4 |  |
| 93 | RETR | 2 / 2 | retr |  |
| 94 | CALL | 2 / 2 | call_4 |  |
| 95 | CPL F0 | 1 / 1 | cpl_f0 |  |
| 96 | JNZ address | 2 / 2 | jnz |  |
| 97 | CLR C | 1 / 1 | clr_c |  |
| 98 | ANL BUS,#data | 2 / 2 | anl_bus_n | legal but stubbed |
| 99 | ANL Pp,#data | 2 / 2 | anl_p1_n |  |
| 9A | ANL Pp,#data | 2 / 2 | anl_p2_n |  |
| 9B | ILL | 1 / 1 | illegal | illegal / silent |
| 9C | ANLD P4,A | 2 / 2 | anld_p4_a |  |
| 9D | ANLD P5,A | 2 / 2 | anld_p5_a |  |
| 9E | ANLD P6,A | 2 / 2 | anld_p6_a |  |
| 9F | ANLD P7,A | 2 / 2 | anld_p7_a |  |
| A0 | MOV @Ri,A | 1 / 1 | mov_xr0_a |  |
| A1 | MOV @Ri,A | 1 / 1 | mov_xr1_a |  |
| A2 | ILL | 1 / 1 | illegal | illegal / silent |
| A3 | MOVP A,@A | 2 / 2 | movp_a_xa |  |
| A4 | JMP | 2 / 2 | jmp_5 |  |
| A5 | CLR F1 | 1 / 1 | clr_f1 |  |
| A6 | ILL | 1 / 1 | illegal | illegal / silent |
| A7 | CPL C | 1 / 1 | cpl_c |  |
| A8 | MOV Rr,A | 1 / 1 | mov_r0_a |  |
| A9 | MOV Rr,A | 1 / 1 | mov_r1_a |  |
| AA | MOV Rr,A | 1 / 1 | mov_r2_a |  |
| AB | MOV Rr,A | 1 / 1 | mov_r3_a |  |
| AC | MOV Rr,A | 1 / 1 | mov_r4_a |  |
| AD | MOV Rr,A | 1 / 1 | mov_r5_a |  |
| AE | MOV Rr,A | 1 / 1 | mov_r6_a |  |
| AF | MOV Rr,A | 1 / 1 | mov_r7_a |  |
| B0 | MOV @Ri,#data | 2 / 2 | mov_xr0_n |  |
| B1 | MOV @Ri,#data | 2 / 2 | mov_xr1_n |  |
| B2 | JBb address | 2 / 2 | jb_5 |  |
| B3 | JMPP @A | 2 / 2 | jmpp_xa |  |
| B4 | CALL | 2 / 2 | call_5 |  |
| B5 | CPL F1 | 1 / 1 | cpl_f1 |  |
| B6 | JF0 address | 2 / 2 | jf0 |  |
| B7 | ILL | 1 / 1 | illegal | illegal / silent |
| B8 | MOV Rr,#data | 2 / 2 | mov_r0_n |  |
| B9 | MOV Rr,#data | 2 / 2 | mov_r1_n |  |
| BA | MOV Rr,#data | 2 / 2 | mov_r2_n |  |
| BB | MOV Rr,#data | 2 / 2 | mov_r3_n |  |
| BC | MOV Rr,#data | 2 / 2 | mov_r4_n |  |
| BD | MOV Rr,#data | 2 / 2 | mov_r5_n |  |
| BE | MOV Rr,#data | 2 / 2 | mov_r6_n |  |
| BF | MOV Rr,#data | 2 / 2 | mov_r7_n |  |
| C0 | ILL | 1 / 1 | illegal | illegal / silent |
| C1 | ILL | 1 / 1 | illegal | illegal / silent |
| C2 | ILL | 1 / 1 | illegal | illegal / silent |
| C3 | ILL | 1 / 1 | illegal | illegal / silent |
| C4 | JMP | 2 / 2 | jmp_6 |  |
| C5 | SEL RB0 | 1 / 1 | sel_rb0 |  |
| C6 | JZ address | 2 / 2 | jz |  |
| C7 | MOV A,PSW | 1 / 1 | mov_a_psw |  |
| C8 | DEC Rr | 1 / 1 | dec_r0 |  |
| C9 | DEC Rr | 1 / 1 | dec_r1 |  |
| CA | DEC Rr | 1 / 1 | dec_r2 |  |
| CB | DEC Rr | 1 / 1 | dec_r3 |  |
| CC | DEC Rr | 1 / 1 | dec_r4 |  |
| CD | DEC Rr | 1 / 1 | dec_r5 |  |
| CE | DEC Rr | 1 / 1 | dec_r6 |  |
| CF | DEC Rr | 1 / 1 | dec_r7 |  |
| D0 | XRL A,@Ri | 1 / 1 | xrl_a_xr0 |  |
| D1 | XRL A,@Ri | 1 / 1 | xrl_a_xr1 |  |
| D2 | JBb address | 2 / 2 | jb_6 |  |
| D3 | XRL A,#data | 2 / 2 | xrl_a_n |  |
| D4 | CALL | 2 / 2 | call_6 |  |
| D5 | SEL RB1 | 1 / 1 | sel_rb1 |  |
| D6 | ILL | 1 / 1 | illegal | illegal / silent |
| D7 | MOV PSW,A | 1 / 1 | mov_psw_a |  |
| D8 | XRL A,Rr | 1 / 1 | xrl_a_r0 |  |
| D9 | XRL A,Rr | 1 / 1 | xrl_a_r1 |  |
| DA | XRL A,Rr | 1 / 1 | xrl_a_r2 |  |
| DB | XRL A,Rr | 1 / 1 | xrl_a_r3 |  |
| DC | XRL A,Rr | 1 / 1 | xrl_a_r4 |  |
| DD | XRL A,Rr | 1 / 1 | xrl_a_r5 |  |
| DE | XRL A,Rr | 1 / 1 | xrl_a_r6 |  |
| DF | XRL A,Rr | 1 / 1 | xrl_a_r7 |  |
| E0 | ILL | 1 / 1 | illegal | illegal / silent |
| E1 | ILL | 1 / 1 | illegal | illegal / silent |
| E2 | ILL | 1 / 1 | illegal | illegal / silent |
| E3 | MOVP3 A,@A | 2 / 2 | movp3_a_xa |  |
| E4 | JMP | 2 / 2 | jmp_7 |  |
| E5 | SEL MB0 | 1 / 1 | sel_mb0 |  |
| E6 | JNC address | 2 / 2 | jnc |  |
| E7 | RL A | 1 / 1 | rl_a |  |
| E8 | DJNZ Rr,address | 2 / 2 | djnz_r0 |  |
| E9 | DJNZ Rr,address | 2 / 2 | djnz_r1 |  |
| EA | DJNZ Rr,address | 2 / 2 | djnz_r2 |  |
| EB | DJNZ Rr,address | 2 / 2 | djnz_r3 |  |
| EC | DJNZ Rr,address | 2 / 2 | djnz_r4 |  |
| ED | DJNZ Rr,address | 2 / 2 | djnz_r5 |  |
| EE | DJNZ Rr,address | 2 / 2 | djnz_r6 |  |
| EF | DJNZ Rr,address | 2 / 2 | djnz_r7 |  |
| F0 | MOV A,@Ri | 1 / 1 | mov_a_xr0 |  |
| F1 | MOV A,@Ri | 1 / 1 | mov_a_xr1 |  |
| F2 | JBb address | 2 / 2 | jb_7 |  |
| F3 | ILL | 1 / 1 | illegal | illegal / silent |
| F4 | CALL | 2 / 2 | call_7 |  |
| F5 | SEL MB1 | 1 / 1 | sel_mb1 |  |
| F6 | JC address | 2 / 2 | jc |  |
| F7 | RLC A | 1 / 1 | rlc_a |  |
| F8 | MOV A,Rr | 1 / 1 | mov_a_r0 |  |
| F9 | MOV A,Rr | 1 / 1 | mov_a_r1 |  |
| FA | MOV A,Rr | 1 / 1 | mov_a_r2 |  |
| FB | MOV A,Rr | 1 / 1 | mov_a_r3 |  |
| FC | MOV A,Rr | 1 / 1 | mov_a_r4 |  |
| FD | MOV A,Rr | 1 / 1 | mov_a_r5 |  |
| FE | MOV A,Rr | 1 / 1 | mov_a_r6 |  |
| FF | MOV A,Rr | 1 / 1 | mov_a_r7 |  |
