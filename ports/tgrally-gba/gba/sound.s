@ sound.s -- the sound: the game's module player (src/tgrally/audio/
@ music.c: BrModRowRead, BrModTick, BrMusicLoopSamples) on a module's channels
@ (up to 16: the race's has 6, the front end's 16), the game's six effect
@ voices (as the race drove them, tools/sound.py's trace a retrace at a time,
@ or started by the menu: BrSfxVoiceStart), and a mixer: 13379 Hz, the two FIFOs (A left,
@ B right) fed by DMA 1 and 2 on timer 0, 224 samples a retrace, double
@ buffered, mixed in the vertical blank.  In IWRAM, ARM.
@
@   void snd_init(void)       before the blank interrupt is on
@   void snd_vblank(void)     from it, every retrace
@   void snd_play(const Song *s)   the module from its start (BrMusicStart), or its recorded state
@   void snd_sfx(const SndSample *s, int rate, int left, int right)   an effect on a free voice
@
@ A voice's position and step are Q12 bytes from its sample's start.  The
@ samples are stored offset by 128 (unsigned), so a voice adds level times
@ (sample + 128) and the mix starts from -128 times the levels; each mix
@ word holds the left in its high half and the right in its low (the
@ game's effects mixer packs its two levels so), exact modulo 2^32.
        .section .iwram, "ax"
        .arm
        .global snd_init, snd_vblank, s_voice, s_chan, s_player, mix_chunk, mod_tick, row_read, sfx_frame
        .global snd_play, snd_sfx, s_song, s_sfx_trace, s_mus_gain

        .equ    MAXCH, 16               @ music channels at most
        .equ    NV, 6 + MAXCH           @ voices: 6 effects, then the music's (the mixer walks 6 + the module's)
        .equ    V_DATA, 0
        .equ    V_POS, 4
        .equ    V_RATE, 8               @ 0: silent
        .equ    V_LEN, 12
        .equ    V_LOOP, 16
        .equ    V_LOOPS, 20
        .equ    V_VOL, 24               @ left << 16 | right
        .equ    V_BASE, 28              @ a music voice's level before the music's own
        .equ    V_SIZE, 32
        .equ    C_NOTE, 0               @ BrModChan
        .equ    C_SMP, 1
        .equ    C_VOL, 2
        .equ    C_FX, 3
        .equ    C_PARAM, 4
        .equ    C_INST, 5
        .equ    C_ARP, 6                @ three notes, then the index (arp[3] reads it, as in the game)
        .equ    C_ARPIDX, 9
        .equ    C_ARPON, 10
        .equ    C_PORTA, 12
        .equ    C_SLIDE, 14
        .equ    C_TARGET, 16
        .equ    C_SIZE, 20
        .equ    P_SPEED, 0              @ BrModState
        .equ    P_TICKS, 4
        .equ    P_ROWS, 8
        .equ    P_ORDER, 12
        .equ    P_ROW, 16
        .equ    P_TACC, 20              @ hundredths of a sample since the last tick
        .equ    P_BUF, 24               @ the buffer playing
        .equ    P_SFX, 28               @ the effects' retrace
        .equ    S_DATA, 0               @ SndSample
        .equ    S_LEN, 4
        .equ    S_LOOP, 8
        .equ    S_VOL, 12
        .equ    S_LOOPS, 13
        .equ    S_REL, 14
        .equ    SG_SMP, 0               @ Song
        .equ    SG_PAT, 4
        .equ    SG_ORDER, 8
        .equ    SG_LEN, 12
        .equ    SG_RESTART, 16
        .equ    SG_SPEED, 20
        .equ    SG_CHANS, 24
        .equ    SG_LEVEL, 28
        .equ    SG_INIT, 32
        .equ    N, 224                  @ samples a retrace
        .equ    CHUNK, 56               @ mixed at a time: loops checked, the player ticked between
        .equ    TICK, 26758             @ 13379 / 50, in hundredths: the game ticks at 50 Hz
        .equ    SHIFT, 6                @ the mix to 8 bits

        .section .iwram_bss, "aw", %nobits
        .align  2
s_voice:  .space NV * V_SIZE
s_chan:   .space MAXCH * C_SIZE
s_player: .space 32
s_song:   .space 4                  @ the module playing
s_sfx_trace: .space 4               @ 1: the effects are the race's trace
s_mus_gain: .space 4                @ the music's fade, 0..256
s_acc:    .space CHUNK * 4
s_out:    .space 4 * N              @ left 0, left 1, right 0, right 1

        .section .iwram, "ax"
@ \d = the note rate of note \n (+ the sample's relative note), the table's last past its end
        .macro  NOTERATE d, n
        mov     \n, \n, lsl #16
        mov     \n, \n, lsr #16         @ & 0xffff, as the game indexes
        cmp     \n, #119
        movhi   \n, #119
        ldr     \d, =g_note_rate
        ldr     \d, [\d, \n, lsl #2]
        .endm

@ \h = the channel's sample header (\s: its instrument, 1..)
        .macro  SMPHDR h, s
        ldr     \h, =s_song
        ldr     \h, [\h]
        ldr     \h, [\h, #SG_SMP]
        sub     \h, \h, #16
        add     \h, \h, \s, lsl #4
        .endm

@ \d = the module's field at \off
        .macro  SONG d, off
        ldr     \d, =s_song
        ldr     \d, [\d]
        ldr     \d, [\d, #\off]
        .endm

@ r0 = BrModRowRead(r0): one packed row into the module's channels
row_read:
        stmfd   sp!, {r4-r11, lr}
        mov     r4, r0
        mov     r5, #0
        ldr     r6, =s_chan
        ldr     r7, =s_voice + 6 * V_SIZE
.Lch:
        mov     r8, #0                  @ note
        mov     r9, #0                  @ instrument
        mov     r10, #0                 @ effect
        mov     r11, #0                 @ parameter
        ldrb    r0, [r4], #1
        tst     r0, #0x80
        beq     .Lfull
        tst     r0, #1
        ldrbne  r8, [r4], #1
        strbne  r8, [r6, #C_NOTE]
        tst     r0, #2
        ldrbne  r9, [r4], #1
        strbne  r9, [r6, #C_INST]
        tst     r0, #4
        ldrbne  r1, [r4], #1
        strbne  r1, [r6, #C_VOL]
        bne     1f
        cmp     r8, #0
        movne   r1, #64
        strbne  r1, [r6, #C_VOL]
1:      tst     r0, #8
        ldrbne  r10, [r4], #1
        strbne  r10, [r6, #C_FX]
        tst     r0, #16
        ldrbne  r11, [r4], #1
        strbne  r11, [r6, #C_PARAM]
        b       .Lgot
.Lfull:
        mov     r8, r0
        strb    r8, [r6, #C_NOTE]
        ldrb    r9, [r4], #1
        strb    r9, [r6, #C_INST]
        ldrb    r1, [r4], #1
        strb    r1, [r6, #C_VOL]
        ldrb    r10, [r4], #1
        strb    r10, [r6, #C_FX]
        ldrb    r11, [r4], #1
        strb    r11, [r6, #C_PARAM]
.Lgot:
        cmp     r8, #0
        ldrbne  r1, [r6, #C_NOTE]
        subne   r1, r1, #1
        strbne  r1, [r6, #C_NOTE]
        cmp     r9, #0
        beq     2f
        ldrb    r1, [r6, #C_VOL]
        cmp     r1, #0
        moveq   r1, #64
        strbeq  r1, [r6, #C_VOL]
2:      mov     r1, #0
        strh    r1, [r6, #C_PORTA]
        strh    r1, [r6, #C_SLIDE]
        strb    r1, [r6, #C_ARPON]
        ldrb    r12, [r6, #C_SMP]
        cmp     r10, #0
        beq     .Lfx0
        cmp     r10, #1
        beq     .Lfx1
        cmp     r10, #2
        beq     .Lfx2
        cmp     r10, #3
        beq     .Lfx3
        cmp     r10, #6
        cmpne   r10, #10
        beq     .Lfx6
        cmp     r10, #12
        beq     .Lfx12
        cmp     r10, #13
        beq     .Lfx13
        cmp     r10, #15
        beq     .Lfx15
        b       .Lfxend
.Lfx0:                                  @ arpeggio
        cmp     r11, #0
        beq     .Lfxend
        ldrb    r1, [r6, #C_NOTE]
        mov     r2, r11, lsr #4
        add     r2, r1, r2
        strb    r2, [r6, #C_ARP + 1]
        and     r2, r11, #15
        add     r2, r1, r2
        strb    r2, [r6, #C_ARP + 2]
        mov     r2, #1
        strb    r2, [r6, #C_ARPON]
        strb    r1, [r6, #C_ARP]
        b       .Lfxend
.Lfx1:                                  @ portamento up
        ldrb    r1, [r6, #C_PARAM]
        strh    r1, [r6, #C_PORTA]
        mov     r1, #0
        str     r1, [r6, #C_TARGET]
        b       .Lfxend
.Lfx2:                                  @ down
        ldrb    r1, [r6, #C_PARAM]
        rsb     r1, r1, #0
        strh    r1, [r6, #C_PORTA]
        mov     r1, #0
        str     r1, [r6, #C_TARGET]
        b       .Lfxend
.Lfx3:                                  @ toward the note, no restart
        cmp     r12, #0
        beq     .Lfxend
        ldrb    r1, [r6, #C_PARAM]
        strh    r1, [r6, #C_PORTA]
        mov     r8, #0
        ldrb    r2, [r6, #C_NOTE]
        SMPHDR  r3, r12
        ldrsb   r3, [r3, #S_REL]
        add     r2, r2, r3
        NOTERATE r3, r2
        str     r3, [r6, #C_TARGET]
        ldr     r2, [r7, #V_RATE]
        cmp     r2, r3
        rsbhi   r1, r1, #0
        strhhi  r1, [r6, #C_PORTA]
        b       .Lfxend
.Lfx6:                                  @ volume slide
        ldrb    r1, [r6, #C_PARAM]
        tst     r1, #0xF0
        movne   r2, r1, lsr #4
        andeq   r2, r1, #15
        rsbeq   r2, r2, #0
        strh    r2, [r6, #C_SLIDE]
        b       .Lfxend
.Lfx12:                                 @ set volume
        strb    r11, [r6, #C_VOL]
        b       .Lfxend
.Lfx13:                                 @ pattern break
        mov     r1, #0
        ldr     r2, =s_player
        str     r1, [r2, #P_ROWS]
        b       .Lfxend
.Lfx15:                                 @ speed
        ldrb    r1, [r6, #C_PARAM]
        ldr     r2, =s_player
        str     r1, [r2, #P_SPEED]
        str     r1, [r2, #P_TICKS]
.Lfxend:
        cmp     r8, #0
        beq     3f
        mov     r1, #0                  @ a note: its instrument's sample from the start
        strb    r1, [r6, #C_ARPIDX]
        ldrb    r12, [r6, #C_INST]
        strb    r12, [r6, #C_SMP]
        cmp     r12, #0
        beq     4f
        SMPHDR  r3, r12
        ldr     r1, [r3, #S_DATA]
        cmp     r1, #0
        beq     4f                      @ (no sample: the game would play its table's junk)
        str     r1, [r7, #V_DATA]
        ldr     r1, [r3, #S_LEN]
        str     r1, [r7, #V_LEN]
        ldr     r1, [r3, #S_LOOP]
        str     r1, [r7, #V_LOOP]
        ldrb    r1, [r3, #S_LOOPS]
        str     r1, [r7, #V_LOOPS]
        mov     r1, #0
        str     r1, [r7, #V_POS]
        ldrb    r2, [r6, #C_NOTE]
        ldrsb   r1, [r3, #S_REL]
        add     r2, r2, r1
        NOTERATE r1, r2
        str     r1, [r7, #V_RATE]
        b       3f
4:      mov     r1, #0
        str     r1, [r7, #V_RATE]
3:      cmp     r12, #0                 @ the voice's level: the sample's times the channel's
        beq     5f
        SMPHDR  r3, r12
        ldr     r1, [r3, #S_DATA]
        cmp     r1, #0
        beq     5f
        ldrb    r1, [r3, #S_VOL]
        ldrb    r2, [r6, #C_VOL]
        mul     r0, r1, r2
        mov     r0, r0, lsr #6
        str     r0, [r7, #V_BASE]
5:      add     r6, r6, #C_SIZE
        add     r7, r7, #V_SIZE
        add     r5, r5, #1
        SONG    r0, SG_CHANS
        cmp     r5, r0
        blt     .Lch
        mov     r0, r4
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

@ BrModTick: the row when its ticks run out, then every channel's arpeggio,
@ portamento and volume slide
mod_tick:
        stmfd   sp!, {r4-r11, lr}
        ldr     r11, =s_player
        ldr     r0, [r11, #P_TICKS]
        sub     r0, r0, #1
        mov     r0, r0, lsl #16         @ an unsigned short, as the game's
        movs    r0, r0, lsr #16
        str     r0, [r11, #P_TICKS]
        bne     .Leffects
        ldr     r0, [r11, #P_SPEED]
        str     r0, [r11, #P_TICKS]
        ldr     r0, [r11, #P_ROWS]
        cmp     r0, #0
        bne     1f
        ldr     r1, [r11, #P_ORDER]     @ the next order's pattern
        add     r1, r1, #1
        and     r1, r1, #0xFF
        ldr     r3, =s_song
        ldr     r3, [r3]
        ldr     r2, [r3, #SG_LEN]
        cmp     r1, r2
        ldreq   r1, [r3, #SG_RESTART]
        str     r1, [r11, #P_ORDER]
        ldr     r2, [r3, #SG_ORDER]
        ldrb    r2, [r2, r1]
        ldr     r3, [r3, #SG_PAT]
        ldr     r3, [r3, r2, lsl #2]
        ldrb    r0, [r3, #5]
        add     r3, r3, #9
        str     r3, [r11, #P_ROW]
1:      sub     r0, r0, #1              @ rows left: an unsigned short
        mov     r0, r0, lsl #16
        mov     r0, r0, lsr #16
        str     r0, [r11, #P_ROWS]
        ldr     r0, [r11, #P_ROW]
        bl      row_read
        str     r0, [r11, #P_ROW]
.Leffects:
        ldr     r4, =s_chan
        ldr     r5, =s_voice + 6 * V_SIZE
        SONG    r6, SG_CHANS
.Lchan:
        ldrb    r7, [r4, #C_SMP]
        ldrb    r0, [r4, #C_ARPON]      @ arpeggio
        cmp     r0, #0
        cmpne   r7, #0
        ldrne   r1, [r5, #V_RATE]
        cmpne   r1, #0
        beq     2f
        ldrb    r0, [r4, #C_ARPIDX]
        add     r1, r4, #C_ARP
        ldrb    r0, [r1, r0]
        SMPHDR  r2, r7
        ldrsb   r2, [r2, #S_REL]
        add     r0, r0, r2
        NOTERATE r1, r0
        str     r1, [r5, #V_RATE]
        ldrb    r0, [r4, #C_ARPIDX]
        add     r0, r0, #1
        and     r0, r0, #3
        strb    r0, [r4, #C_ARPIDX]
2:      ldrsh   r8, [r4, #C_PORTA]      @ portamento, in period space: period = K / rate
        cmp     r8, #0
        ldrne   r1, [r5, #V_RATE]
        cmpne   r1, #0
        beq     3f
        ldr     r0, =g_porta_k
        ldr     r0, [r0]
        bl      udiv
        sub     r0, r0, r8
        cmp     r0, #1
        movlt   r0, #1
        mov     r1, r0
        ldr     r0, =g_porta_k
        ldr     r0, [r0]
        bl      udiv
        ldr     r1, [r4, #C_TARGET]
        cmp     r1, #0
        beq     21f
        cmp     r8, #0
        bge     22f
        cmp     r0, r1                  @ down to the target
        movlo   r0, r1
        b       21f
22:     cmp     r0, r1                  @ up to it
        movhi   r0, r1
21:     str     r0, [r5, #V_RATE]
3:      ldrsh   r0, [r4, #C_SLIDE]      @ volume slide, within 0..64
        cmp     r0, #0
        beq     4f
        ldrb    r1, [r4, #C_VOL]
        add     r1, r1, r0
        cmp     r1, #0
        movlt   r1, #0
        cmp     r1, #64
        movgt   r1, #64
        strb    r1, [r4, #C_VOL]
        cmp     r7, #0
        beq     4f
        SMPHDR  r2, r7
        ldr     r3, [r2, #S_DATA]
        cmp     r3, #0
        beq     4f
        ldrb    r2, [r2, #S_VOL]
        mul     r3, r2, r1
        mov     r3, r3, lsr #6
        str     r3, [r5, #V_BASE]
4:      add     r4, r4, #C_SIZE
        add     r5, r5, #V_SIZE
        subs    r6, r6, #1
        bne     .Lchan
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

@ the effect voices for this retrace, from the trace
sfx_frame:
        stmfd   sp!, {r4-r8, lr}
        ldr     r8, =s_player
        ldr     r0, [r8, #P_SFX]
        ldr     r4, =g_sfx_trace
        add     r1, r0, r0, lsl #1      @ 48 bytes a retrace
        add     r4, r4, r1, lsl #4
        add     r0, r0, #1
        ldr     r1, =g_sfx_frames
        ldr     r1, [r1]
        cmp     r0, r1
        movge   r0, #0
        str     r0, [r8, #P_SFX]
        ldr     r5, =s_voice
        mov     r6, #6
1:      ldrb    r0, [r4]
        cmp     r0, #0xFF
        moveq   r1, #0
        streq   r1, [r5, #V_RATE]
        beq     2f
        ldrb    r1, [r4, #3]
        cmp     r1, #0
        beq     3f
        ldr     r1, =g_sfx_smp          @ (re)started: the sample, from the recorded place
        add     r1, r1, r0, lsl #4
        ldr     r2, [r1, #S_DATA]
        str     r2, [r5, #V_DATA]
        ldr     r2, [r1, #S_LEN]
        str     r2, [r5, #V_LEN]
        ldr     r2, [r1, #S_LOOP]
        str     r2, [r5, #V_LOOP]
        ldrb    r2, [r1, #S_LOOPS]
        str     r2, [r5, #V_LOOPS]
        ldrh    r2, [r4, #6]
        mov     r2, r2, lsl #12
        str     r2, [r5, #V_POS]
3:      ldrh    r1, [r4, #4]
        str     r1, [r5, #V_RATE]
        ldrb    r1, [r4, #1]
        ldrb    r2, [r4, #2]
        orr     r1, r2, r1, lsl #16
        str     r1, [r5, #V_VOL]
2:      add     r4, r4, #8
        add     r5, r5, #V_SIZE
        subs    r6, r6, #1
        bne     1b
        ldmfd   sp!, {r4-r8, lr}
        bx      lr
        .ltorg

@ mix CHUNK samples to r0 (left) and r1 (right)
mix_chunk:
        stmfd   sp!, {r4-r11, lr}
        stmfd   sp!, {r0, r1}
        ldr     r4, =s_voice            @ samples run past their end: loop back, or stop
        SONG    r5, SG_CHANS
        add     r5, r5, #6
1:      ldr     r0, [r4, #V_RATE]
        cmp     r0, #0
        beq     2f
        ldr     r0, [r4, #V_POS]
        ldr     r1, [r4, #V_LEN]
        cmp     r1, r0, lsr #12
        bhi     2f
        ldr     r1, [r4, #V_LOOPS]
        cmp     r1, #0
        streq   r1, [r4, #V_RATE]
        ldrne   r1, [r4, #V_LOOP]
        subne   r0, r0, r1, lsl #12
        strne   r0, [r4, #V_POS]
2:      add     r4, r4, #V_SIZE
        subs    r5, r5, #1
        bne     1b
        ldr     r4, =s_player           @ the player's tick: 50 a second
        ldr     r0, [r4, #P_TACC]
        ldr     r1, =CHUNK * 100
        add     r0, r0, r1
        ldr     r1, =TICK
        cmp     r0, r1
        subge   r0, r0, r1
        str     r0, [r4, #P_TACC]
        blge    mod_tick
        ldr     r4, =s_voice + 6 * V_SIZE   @ the music voices' levels: the music's own (and its fade), left or right
        SONG    r6, SG_LEVEL
        ldr     r0, =s_mus_gain
        ldr     r0, [r0]
        mul     r6, r0, r6
        SONG    r2, SG_CHANS
        mov     r5, #0
3:      ldr     r0, [r4, #V_BASE]
        mul     r1, r0, r6
        mov     r1, r1, lsr #24
        tst     r5, #1
        moveq   r1, r1, lsl #16
        str     r1, [r4, #V_VOL]
        add     r4, r4, #V_SIZE
        add     r5, r5, #1
        cmp     r5, r2
        blt     3b
        ldr     r4, =s_voice            @ the bias: -128 times the playing voices' levels
        SONG    r5, SG_CHANS
        add     r5, r5, #6
        mov     r6, #0
4:      ldr     r0, [r4, #V_RATE]
        cmp     r0, #0
        ldrne   r0, [r4, #V_VOL]
        subne   r6, r6, r0, lsl #7
        add     r4, r4, #V_SIZE
        subs    r5, r5, #1
        bne     4b
        ldr     r0, =s_acc
        mov     r1, #CHUNK / 4
        mov     r2, r6
        mov     r3, r6
        mov     r7, r6
5:      stmia   r0!, {r2, r3, r6, r7}
        subs    r1, r1, #1
        bne     5b
        ldr     r11, =s_voice           @ each playing voice into the mix
        SONG    r10, SG_CHANS
        add     r10, r10, #6
.Lvoice:
        ldr     r4, [r11, #V_RATE]
        cmp     r4, #0
        beq     7f
        ldr     r2, [r11, #V_DATA]
        ldr     r3, [r11, #V_POS]
        ldr     r5, [r11, #V_VOL]
        cmp     r5, #0                  @ at no level: only its place moves on
        moveq   r0, #CHUNK
        mlaeq   r3, r4, r0, r3
        beq     61f
        ldr     r0, =s_acc
        mov     r1, #CHUNK / 4
6:      ldmia   r0, {r6-r9}
        add     r3, r3, r4
        ldrb    r12, [r2, r3, lsr #12]
        mla     r6, r5, r12, r6
        add     r3, r3, r4
        ldrb    r12, [r2, r3, lsr #12]
        mla     r7, r5, r12, r7
        add     r3, r3, r4
        ldrb    r12, [r2, r3, lsr #12]
        mla     r8, r5, r12, r8
        add     r3, r3, r4
        ldrb    r12, [r2, r3, lsr #12]
        mla     r9, r5, r12, r9
        stmia   r0!, {r6-r9}
        subs    r1, r1, #1
        bne     6b
61:     str     r3, [r11, #V_POS]
7:      add     r11, r11, #V_SIZE
        subs    r10, r10, #1
        bne     .Lvoice
        ldmfd   sp!, {r4, r5}           @ to 8 bits: left the high half, right the low
        ldr     r0, =s_acc
        mov     r1, #CHUNK
8:      ldr     r2, [r0], #4
        mov     r3, r2, lsl #16
        mov     r3, r3, asr #16         @ right
        sub     r2, r2, r3
        mov     r2, r2, asr #16         @ left
        mov     r2, r2, asr #SHIFT
        cmp     r2, #127
        movgt   r2, #127
        cmn     r2, #128
        mvnlt   r2, #127
        strb    r2, [r4], #1
        mov     r3, r3, asr #SHIFT
        cmp     r3, #127
        movgt   r3, #127
        cmn     r3, #128
        mvnlt   r3, #127
        strb    r3, [r5], #1
        subs    r1, r1, #1
        bne     8b
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

snd_init:
        stmfd   sp!, {r4-r8, lr}
        mov     r0, #0x04000000
        mov     r1, #0x80
        strh    r1, [r0, #0x84]         @ sound on
        ldr     r1, =0x9A0C             @ A left, B right, both on timer 0, full, reset
        strh    r1, [r0, #0x82]
        ldr     r1, =s_out
        str     r1, [r0, #0xBC]         @ DMA 1: left to FIFO A
        add     r2, r0, #0xA0
        str     r2, [r0, #0xC0]
        ldr     r3, =0xB640             @ on, the FIFO's timing, 32-bit, repeat, the destination fixed
        strh    r3, [r0, #0xC6]
        add     r1, r1, #2 * N          @ DMA 2: right to FIFO B
        str     r1, [r0, #0xC8]
        add     r2, r0, #0xA4
        str     r2, [r0, #0xCC]
        strh    r3, [r0, #0xD2]
        add     r0, r0, #0x100
        ldr     r1, =65536 - 1254       @ timer 0 at 13379 Hz: 224 samples a retrace exactly
        strh    r1, [r0]
        mov     r1, #0x80
        strh    r1, [r0, #2]
        ldmfd   sp!, {r4-r8, lr}
        bx      lr
        .ltorg

@ the module r0 from its start (BrMusicStart: BrModLoad's speed, BrModReset, every voice at
@ 0x20), or from the player's recorded state (its init: the race's, as the game had it at
@ the replay's start); the effects silenced, the trace from its start
snd_play:
        stmfd   sp!, {r4-r8, lr}
        mov     r3, #0x04000000
        add     r3, r3, #0x200
        ldrh    r8, [r3, #8]            @ (no blank in between: IME off)
        mov     r1, #0
        strh    r1, [r3, #8]
        ldr     r1, =s_song
        str     r0, [r1]
        mov     r7, r0
        ldr     r4, =s_voice            @ every voice, channel and the player cleared (the buffer
        mov     r1, #0                  @ playing kept), the trace from its start
        ldr     r2, =(NV * V_SIZE + MAXCH * C_SIZE + P_BUF) / 4
1:      str     r1, [r4], #4
        subs    r2, r2, #1
        bne     1b
        ldr     r5, =s_player
        str     r1, [r5, #P_SFX]
        ldr     r4, [r7, #SG_INIT]
        cmp     r4, #0
        bne     .Lrecorded
        ldr     r0, [r7, #SG_SPEED]     @ BrModReset: ticks 1, rows 0, order 0xFF
        mov     r1, #1
        mov     r2, #0
        mov     r3, #0xFF
        stmia   r5, {r0-r3}
        ldr     r5, =s_chan
        ldr     r4, =s_voice + 6 * V_SIZE
        ldr     r6, [r7, #SG_CHANS]
        mov     r0, #0x40
        mov     r1, #0x20
2:      strb    r0, [r5, #C_VOL]
        str     r1, [r4, #V_BASE]
        add     r5, r5, #C_SIZE
        add     r4, r4, #V_SIZE
        subs    r6, r6, #1
        bne     2b
        b       .Lplay_done
.Lrecorded:
        ldmia   r4!, {r0-r3, r12}       @ speed, ticks, rows, order, the row's offset
        stmia   r5, {r0-r3}
        ldr     r0, [r7, #SG_ORDER]
        ldrb    r0, [r0, r3]
        ldr     r1, [r7, #SG_PAT]
        ldr     r1, [r1, r0, lsl #2]
        add     r1, r1, r12
        str     r1, [r5, #P_ROW]
        ldr     r5, =s_chan             @ the channels: the same layout
        mov     r6, #6 * C_SIZE / 4
1:      ldr     r0, [r4], #4
        str     r0, [r5], #4
        subs    r6, r6, #1
        bne     1b
        ldr     r5, =s_voice + 6 * V_SIZE            @ the voices: their instrument's sample, place, step, level
        mov     r6, #6
2:      ldmia   r4!, {r0-r3}
        str     r3, [r5, #V_BASE]
        cmp     r0, #0
        beq     3f
        SMPHDR  r7, r0
        ldr     r12, [r7, #S_DATA]
        cmp     r12, #0
        beq     3f
        str     r12, [r5, #V_DATA]
        ldr     r12, [r7, #S_LEN]
        str     r12, [r5, #V_LEN]
        ldr     r12, [r7, #S_LOOP]
        str     r12, [r5, #V_LOOP]
        ldrb    r12, [r7, #S_LOOPS]
        str     r12, [r5, #V_LOOPS]
        str     r1, [r5, #V_POS]
        str     r2, [r5, #V_RATE]
3:      add     r5, r5, #V_SIZE
        subs    r6, r6, #1
        bne     2b
.Lplay_done:
        mov     r3, #0x04000000
        add     r3, r3, #0x200
        strh    r8, [r3, #8]
        ldmfd   sp!, {r4-r8, lr}
        bx      lr
        .ltorg

@ BrSfxFreeVoice and BrSfxVoiceStart: the sample r0 on the first silent effect voice, at
@ step r1 (Q12), levels r2 left, r3 right; none free: not played
snd_sfx:
        stmfd   sp!, {r4, r5, lr}
        ldr     r4, =s_voice
        mov     r5, #6
1:      ldr     r12, [r4, #V_RATE]
        cmp     r12, #0
        beq     2f
        add     r4, r4, #V_SIZE
        subs    r5, r5, #1
        bne     1b
        ldmfd   sp!, {r4, r5, lr}
        bx      lr
2:      ldr     r12, [r0, #S_DATA]
        str     r12, [r4, #V_DATA]
        ldr     r12, [r0, #S_LEN]
        str     r12, [r4, #V_LEN]
        ldr     r12, [r0, #S_LOOP]
        str     r12, [r4, #V_LOOP]
        ldrb    r12, [r0, #S_LOOPS]
        str     r12, [r4, #V_LOOPS]
        mov     r12, #0
        str     r12, [r4, #V_POS]
        orr     r2, r3, r2, lsl #16
        str     r2, [r4, #V_VOL]
        str     r1, [r4, #V_RATE]       @ (last: the mixer may run in between)
        ldmfd   sp!, {r4, r5, lr}
        bx      lr
        .ltorg


@ a retrace: play the buffer mixed last time, mix the next
snd_vblank:
        stmfd   sp!, {r4-r6, lr}
        ldr     r4, =s_player
        ldr     r0, [r4, #P_BUF]
        eor     r0, r0, #1
        str     r0, [r4, #P_BUF]
        mov     r1, #0x04000000
        mov     r2, #0
        strh    r2, [r1, #0xC6]
        strh    r2, [r1, #0xD2]
        ldr     r2, =s_out
        mov     r3, #N
        mla     r5, r0, r3, r2
        str     r5, [r1, #0xBC]
        add     r5, r5, #2 * N
        str     r5, [r1, #0xC8]
        ldr     r3, =0xB640
        strh    r3, [r1, #0xC6]
        strh    r3, [r1, #0xD2]
        ldr     r0, =s_song             @ no module yet (before the first snd_play): the buffers
        ldr     r0, [r0]                @ stay silent
        cmp     r0, #0
        beq     2f
        ldr     r0, =s_sfx_trace
        ldr     r0, [r0]
        cmp     r0, #0
        blne    sfx_frame
        ldr     r0, [r4, #P_BUF]
        eor     r0, r0, #1
        ldr     r2, =s_out
        mov     r3, #N
        mla     r5, r0, r3, r2          @ the other buffer's left
        mov     r6, #N / CHUNK
1:      mov     r0, r5
        add     r1, r5, #2 * N
        bl      mix_chunk
        add     r5, r5, #CHUNK
        subs    r6, r6, #1
        bne     1b
2:      ldmfd   sp!, {r4-r6, lr}
        bx      lr
        .ltorg
