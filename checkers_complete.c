/*
 * ============================================================================
 * SNES CHECKERS GAME - COMPLETE WITH CORRECT GRAPHICS RENDERING
 * ============================================================================
 * FIX: Proper tilemap scaling (8×8 board → 32×32 SNES tilemap)
 * Each board cell = 4×4 tiles on screen (32 pixels × 32 pixels per piece)
 * ============================================================================
 */

#include <stdint.h>

typedef volatile uint8_t vreg8;
typedef volatile uint16_t vreg16;

/* ========== PPU REGISTERS ========== */
#define PPU_INIDISP  (*(vreg8*)0x2100)
#define PPU_OBJSEL   (*(vreg8*)0x2101)
#define PPU_OAMADDL  (*(vreg8*)0x2102)
#define PPU_OAMADDH  (*(vreg8*)0x2103)
#define PPU_OAMDATA  (*(vreg8*)0x2104)
#define PPU_BGMODE   (*(vreg8*)0x2105)
#define PPU_BG1SC    (*(vreg8*)0x2107)
#define PPU_BG2SC    (*(vreg8*)0x2108)
#define PPU_BG12NBA  (*(vreg8*)0x210B)
#define PPU_VMADD    (*(vreg16*)0x2116)
#define PPU_VMDATA   (*(vreg16*)0x2118)
#define PPU_CGADD    (*(vreg8*)0x2121)
#define PPU_CGDATA   (*(vreg16*)0x2122)
#define PPU_TM       (*(vreg8*)0x212C)
#define PPU_STAT77   (*(vreg8*)0x2137)

/* ========== JOYPAD REGISTERS ========== */
#define JOY_STAT     (*(vreg8*)0x4212)
#define JOY_CTRL     (*(vreg8*)0x4200)
#define JOY1_L       (*(vreg8*)0x4218)
#define JOY1_H       (*(vreg8*)0x4219)

/* ========== GAME CONSTANTS ========== */
#define STATE_INTRO 0
#define STATE_MENU 1
#define STATE_GAME 2
#define STATE_PAUSE 3
#define STATE_OVER 4

#define PLAYER_HUMAN 0
#define PLAYER_CPU 1

#define PIECE_EMPTY 0
#define PIECE_WHITE 1
#define PIECE_BLACK 2
#define PIECE_WKING 3
#define PIECE_BKING 4

/* SNES Joypad bits */
#define JOY_B_bit      7
#define JOY_START_bit  4
#define JOY_UP_bit     3
#define JOY_DOWN_bit   2
#define JOY_LEFT_bit   1
#define JOY_RIGHT_bit  0
#define JOY_A_bit      7  /* in JOY1_H */

/* ========== GLOBAL STATE ========== */
static uint8_t g_state;
static uint8_t g_board[8][8];
static uint8_t g_player;
static uint8_t g_sel_row;
static uint8_t g_sel_col;
static uint8_t g_move_sel;
static uint8_t g_move_row;
static uint8_t g_move_col;
static uint8_t g_joy_l_cur;
static uint8_t g_joy_h_cur;
static uint8_t g_joy_l_old;
static uint8_t g_joy_h_old;
static uint8_t g_menu_idx;
static uint8_t g_white_cnt;
static uint8_t g_black_cnt;
static uint8_t g_winner;
static uint8_t g_cpu_timer;
static uint8_t g_frame;
static uint8_t g_intro_timer;

/* ========== PALETTE: 16 colors @ 15-bit RGB ========== */
static const uint16_t g_pal[16] = {
    0x0000, 0x7FFF, 0x001F, 0x03E0,
    0x7C00, 0x03FF, 0x7C1F, 0x7FE0,
    0x4210, 0x5EFF, 0x0010, 0x0200,
    0x4000, 0x4A52, 0x6B5A, 0x7BDE
};

/* ========== TILE DATA (8×8 @ 4bpp = 32 bytes per tile) ========== */
/* Tile 0: Dark square */
static const uint8_t tile_dark[32] = {
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22
};

/* Tile 1: Light square */
static const uint8_t tile_light[32] = {
    0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
    0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
    0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
    0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE
};

/* Tile 2: White piece */
static const uint8_t tile_piece_white[32] = {
    0x00, 0x00, 0x11, 0x10, 0x11, 0x10, 0x11, 0x10,
    0x01, 0x11, 0x11, 0x10, 0x11, 0x11, 0x11, 0x10,
    0x11, 0x11, 0x11, 0x10, 0x11, 0x11, 0x11, 0x10,
    0x01, 0x11, 0x11, 0x10, 0x00, 0x11, 0x10, 0x00
};

/* Tile 3: Black piece */
static const uint8_t tile_piece_black[32] = {
    0x00, 0x00, 0x33, 0x30, 0x33, 0x30, 0x33, 0x30,
    0x03, 0x33, 0x33, 0x30, 0x33, 0x33, 0x33, 0x30,
    0x33, 0x33, 0x33, 0x30, 0x33, 0x33, 0x33, 0x30,
    0x03, 0x33, 0x33, 0x30, 0x00, 0x33, 0x30, 0x00
};

/* Tile 4: White crown */
static const uint8_t tile_crown_white[32] = {
    0x00, 0x00, 0x00, 0x00, 0x15, 0x51, 0x15, 0x51,
    0x51, 0x15, 0x51, 0x10, 0x51, 0x10, 0x51, 0x10,
    0x51, 0x10, 0x51, 0x10, 0x51, 0x15, 0x51, 0x50,
    0x15, 0x55, 0x51, 0x50, 0x00, 0x55, 0x50, 0x00
};

/* Tile 5: Black crown */
static const uint8_t tile_crown_black[32] = {
    0x00, 0x00, 0x00, 0x00, 0x34, 0x43, 0x34, 0x43,
    0x43, 0x34, 0x43, 0x30, 0x43, 0x30, 0x43, 0x30,
    0x43, 0x30, 0x43, 0x30, 0x43, 0x34, 0x43, 0x40,
    0x34, 0x44, 0x43, 0x40, 0x00, 0x44, 0x40, 0x00
};

/* Tile 6: Selection highlight */
static const uint8_t tile_select[32] = {
    0x77, 0x77, 0x77, 0x77, 0x70, 0x00, 0x07, 0x70,
    0x70, 0x00, 0x00, 0x07, 0x70, 0x00, 0x00, 0x07,
    0x70, 0x00, 0x00, 0x07, 0x70, 0x00, 0x00, 0x07,
    0x70, 0x00, 0x07, 0x70, 0x77, 0x77, 0x77, 0x77
};

/* Tile 7: Empty */
static const uint8_t tile_empty[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* ========== TILEMAP: 32×32 ========== */
static uint16_t g_tilemap[32 * 32];

/* ========== VBLANK WAIT ========== */
void vblank_wait(void) {
    uint8_t st;
    st = PPU_STAT77;
    while (st & 0x80) {
        st = PPU_STAT77;
    }
    st = PPU_STAT77;
    while (!(st & 0x80)) {
        st = PPU_STAT77;
    }
}

/* ========== LOAD TILE TO VRAM ========== */
void load_tile(uint16_t vram_addr, const uint8_t *tile_data) {
    uint8_t i;
    PPU_VMADD = vram_addr;
    i = 0;
    while (i < 32) {
        PPU_VMDATA = tile_data[i];
        i++;
    }
}

/* ========== LOAD PALETTE ========== */
void load_palette(void) {
    uint8_t i;
    PPU_CGADD = 0;
    i = 0;
    while (i < 16) {
        PPU_CGDATA = g_pal[i];
        i++;
    }
}

/* ========== PPU INIT ========== */
void ppu_init(void) {
    uint16_t i;
    uint8_t r;
    uint8_t c;
    uint16_t tm_idx;
    
    PPU_INIDISP = 0x80;
    PPU_BGMODE = 0x01;
    PPU_BG1SC = 0x00;
    PPU_BG12NBA = 0x00;
    PPU_OBJSEL = 0x00;
    PPU_TM = 0x01;
    
    /* Clear VRAM */
    PPU_VMADD = 0x0000;
    i = 0;
    while (i < 0x8000) {
        PPU_VMDATA = 0x0000;
        i++;
    }
    
    /* Load tiles */
    load_tile(0x0000, tile_dark);
    load_tile(0x0020, tile_light);
    load_tile(0x0040, tile_piece_white);
    load_tile(0x0060, tile_piece_black);
    load_tile(0x0080, tile_crown_white);
    load_tile(0x00A0, tile_crown_black);
    load_tile(0x00C0, tile_select);
    load_tile(0x00E0, tile_empty);
    
    /* Build full 32×32 tilemap */
    tm_idx = 0;
    r = 0;
    while (r < 32) {
        c = 0;
        while (c < 32) {
            if (((r + c) & 1) == 0) {
                g_tilemap[tm_idx] = 0;
            } else {
                g_tilemap[tm_idx] = 1;
            }
            tm_idx++;
            c++;
        }
        r++;
    }
    
    /* Write tilemap to VRAM @ 0x0400 */
    PPU_VMADD = 0x0400;
    tm_idx = 0;
    while (tm_idx < (32 * 32)) {
        PPU_VMDATA = g_tilemap[tm_idx];
        tm_idx++;
    }
    
    /* Clear OAM */
    PPU_OAMADDL = 0x00;
    PPU_OAMADDH = 0x00;
    i = 0;
    while (i < 256) {
        PPU_OAMDATA = 0x00;
        i++;
    }
    
    load_palette();
    PPU_INIDISP = 0x0F;
}

/* ========== JOYPAD INIT ========== */
void joy_init(void) {
    JOY_CTRL = 0x01;
}

/* ========== JOYPAD READ ========== */
void joy_read(void) {
    uint8_t stat;
    stat = JOY_STAT;
    while (stat & 0x01) {
        stat = JOY_STAT;
    }
    g_joy_l_old = g_joy_l_cur;
    g_joy_h_old = g_joy_h_cur;
    g_joy_l_cur = JOY1_L;
    g_joy_h_cur = JOY1_H;
}

/* ========== JOYPAD: BUTTON PRESSED ========== */
uint8_t joy_press(uint8_t byte_sel, uint8_t bit_pos) {
    uint8_t cur;
    uint8_t old;
    uint8_t mask;
    uint8_t cur_bit;
    uint8_t old_bit;
    
    mask = 1;
    mask = mask << bit_pos;
    
    if (byte_sel == 0) {
        cur = g_joy_l_cur;
        old = g_joy_l_old;
    } else {
        cur = g_joy_h_cur;
        old = g_joy_h_old;
    }
    
    cur_bit = (cur & mask) ? 1 : 0;
    old_bit = (old & mask) ? 1 : 0;
    
    if (cur_bit == 1 && old_bit == 0) {
        return 1;
    }
    return 0;
}

/* ========== BOARD INIT ========== */
void board_init(void) {
    uint8_t r;
    uint8_t c;
    uint8_t idx;
    uint8_t chk;
    
    r = 0;
    while (r < 8) {
        c = 0;
        while (c < 8) {
            g_board[r][c] = PIECE_EMPTY;
            c++;
        }
        r++;
    }
    
    r = 0;
    while (r < 3) {
        c = 0;
        while (c < 8) {
            idx = r + c;
            chk = idx & 1;
            if (chk == 1) {
                g_board[r][c] = PIECE_WHITE;
            }
            c++;
        }
        r++;
    }
    
    r = 5;
    while (r < 8) {
        c = 0;
        while (c < 8) {
            idx = r + c;
            chk = idx & 1;
            if (chk == 1) {
                g_board[r][c] = PIECE_BLACK;
            }
            c++;
        }
        r++;
    }
    
    g_white_cnt = 12;
    g_black_cnt = 12;
    g_player = PLAYER_HUMAN;
    g_winner = 2;
    g_sel_row = 0;
    g_sel_col = 0;
    g_move_sel = 0;
}

/* ========== RENDER BOARD TO TILEMAP ========== */
void board_render(void) {
    uint8_t br;
    uint8_t bc;
    uint8_t piece;
    uint8_t tr;
    uint8_t tc;
    uint16_t tm_idx;
    uint16_t tile_idx;
    
    br = 0;
    while (br < 8) {
        bc = 0;
        while (bc < 8) {
            piece = g_board[br][bc];
            
            /* Each board cell = 4×4 tiles in tilemap */
            tr = br << 2;
            tc = bc << 2;
            
            /* Determine tile to display */
            if (((br + bc) & 1) == 0) {
                tile_idx = 0;
            } else {
                tile_idx = 1;
            }
            
            if (piece == PIECE_WHITE) {
                tile_idx = 2;
            } else if (piece == PIECE_BLACK) {
                tile_idx = 3;
            } else if (piece == PIECE_WKING) {
                tile_idx = 4;
            } else if (piece == PIECE_BKING) {
                tile_idx = 5;
            }
            
            if (br == g_sel_row && bc == g_sel_col && g_move_sel == 0) {
                tile_idx = 6;
            }
            
            /* Fill 4×4 tiles in tilemap */
            uint8_t dtile_r;
            uint8_t dtile_c;
            dtile_r = 0;
            while (dtile_r < 4) {
                dtile_c = 0;
                while (dtile_c < 4) {
                    tm_idx = ((tr + dtile_r) << 5) | (tc + dtile_c);
                    g_tilemap[tm_idx] = tile_idx;
                    dtile_c++;
                }
                dtile_r++;
            }
            
            bc++;
        }
        br++;
    }
    
    /* Write tilemap to VRAM */
    PPU_VMADD = 0x0400;
    tm_idx = 0;
    while (tm_idx < (32 * 32)) {
        PPU_VMDATA = g_tilemap[tm_idx];
        tm_idx++;
    }
}

/* ========== PIECE GETTER ========== */
uint8_t board_get(uint8_t r, uint8_t c) {
    if (r >= 8 || c >= 8) {
        return PIECE_EMPTY;
    }
    return g_board[r][c];
}

/* ========== PIECE SETTER ========== */
void board_set(uint8_t r, uint8_t c, uint8_t p) {
    if (r >= 8 || c >= 8) {
        return;
    }
    g_board[r][c] = p;
}

/* ========== MOVE VALIDATOR ========== */
uint8_t move_valid(uint8_t fr, uint8_t fc, uint8_t tr, uint8_t tc) {
    uint8_t piece;
    uint8_t target;
    uint8_t rd;
    uint8_t cd;
    
    piece = board_get(fr, fc);
    target = board_get(tr, tc);
    
    if (piece == PIECE_EMPTY || target != PIECE_EMPTY) {
        return 0;
    }
    
    if (tr >= 8 || tc >= 8) {
        return 0;
    }
    
    if (fr > tr) {
        rd = fr - tr;
    } else {
        rd = tr - fr;
    }
    
    if (fc > tc) {
        cd = fc - tc;
    } else {
        cd = tc - fc;
    }
    
    if (rd != cd || rd == 0) {
        return 0;
    }
    
    if (piece == PIECE_WHITE && tr <= fr) {
        return 0;
    }
    
    if (piece == PIECE_BLACK && tr >= fr) {
        return 0;
    }
    
    if (rd == 1) {
        return 1;
    }
    
    if ((piece == PIECE_WKING || piece == PIECE_BKING) && rd <= 2) {
        return 1;
    }
    
    return 0;
}

/* ========== CAPTURE VALIDATOR ========== */
uint8_t move_capture(uint8_t fr, uint8_t fc, uint8_t tr, uint8_t tc) {
    uint8_t rd;
    uint8_t cd;
    uint8_t mr;
    uint8_t mc;
    uint8_t mid;
    uint8_t from_p;
    
    if (fr > tr) {
        rd = fr - tr;
    } else {
        rd = tr - fr;
    }
    
    if (fc > tc) {
        cd = fc - tc;
    } else {
        cd = tc - fc;
    }
    
    if (rd != cd || rd != 2) {
        return 0;
    }
    
    mr = fr + tr;
    mr = mr >> 1;
    mc = fc + tc;
    mc = mc >> 1;
    
    mid = board_get(mr, mc);
    from_p = board_get(fr, fc);
    
    if (mid == PIECE_EMPTY) {
        return 0;
    }
    
    if (from_p == PIECE_WHITE || from_p == PIECE_WKING) {
        if (mid == PIECE_BLACK || mid == PIECE_BKING) {
            return 1;
        }
    }
    
    if (from_p == PIECE_BLACK || from_p == PIECE_BKING) {
        if (mid == PIECE_WHITE || mid == PIECE_WKING) {
            return 1;
        }
    }
    
    return 0;
}

/* ========== EXECUTE MOVE ========== */
void move_exec(uint8_t fr, uint8_t fc, uint8_t tr, uint8_t tc) {
    uint8_t piece;
    uint8_t mr;
    uint8_t mc;
    uint8_t cap;
    
    piece = board_get(fr, fc);
    board_set(fr, fc, PIECE_EMPTY);
    board_set(tr, tc, piece);
    
    if (move_capture(fr, fc, tr, tc)) {
        mr = fr + tr;
        mr = mr >> 1;
        mc = fc + tc;
        mc = mc >> 1;
        cap = board_get(mr, mc);
        board_set(mr, mc, PIECE_EMPTY);
        
        if (cap == PIECE_WHITE || cap == PIECE_WKING) {
            g_white_cnt--;
        } else {
            g_black_cnt--;
        }
    }
    
    if (piece == PIECE_WHITE && tr == 7) {
        board_set(tr, tc, PIECE_WKING);
    }
    
    if (piece == PIECE_BLACK && tr == 0) {
        board_set(tr, tc, PIECE_BKING);
    }
}

/* ========== CHECK VALID MOVES ========== */
uint8_t has_moves(uint8_t p) {
    uint8_t r;
    uint8_t c;
    uint8_t nr;
    uint8_t nc;
    uint8_t piece;
    
    r = 0;
    while (r < 8) {
        c = 0;
        while (c < 8) {
            piece = board_get(r, c);
            
            if (p == PLAYER_HUMAN) {
                if (piece != PIECE_WHITE && piece != PIECE_WKING) {
                    c++;
                    continue;
                }
            } else {
                if (piece != PIECE_BLACK && piece != PIECE_BKING) {
                    c++;
                    continue;
                }
            }
            
            nr = 0;
            while (nr < 8) {
                nc = 0;
                while (nc < 8) {
                    if (move_valid(r, c, nr, nc) || move_capture(r, c, nr, nc)) {
                        return 1;
                    }
                    nc++;
                }
                nr++;
            }
            
            c++;
        }
        r++;
    }
    
    return 0;
}

/* ========== CPU AI ========== */
uint8_t cpu_move(uint8_t *fr, uint8_t *fc, uint8_t *tr, uint8_t *tc) {
    uint8_t r;
    uint8_t c;
    uint8_t nr;
    uint8_t nc;
    uint8_t piece;
    
    r = 0;
    while (r < 8) {
        c = 0;
        while (c < 8) {
            piece = board_get(r, c);
            if (piece != PIECE_BLACK && piece != PIECE_BKING) {
                c++;
                continue;
            }
            
            nr = 0;
            while (nr < 8) {
                nc = 0;
                while (nc < 8) {
                    if (move_capture(r, c, nr, nc)) {
                        *fr = r;
                        *fc = c;
                        *tr = nr;
                        *tc = nc;
                        return 1;
                    }
                    nc++;
                }
                nr++;
            }
            
            c++;
        }
        r++;
    }
    
    r = 0;
    while (r < 8) {
        c = 0;
        while (c < 8) {
            piece = board_get(r, c);
            if (piece != PIECE_BLACK && piece != PIECE_BKING) {
                c++;
                continue;
            }
            
            nr = 0;
            while (nr < 8) {
                nc = 0;
                while (nc < 8) {
                    if (move_valid(r, c, nr, nc)) {
                        *fr = r;
                        *fc = c;
                        *tr = nr;
                        *tc = nc;
                        return 1;
                    }
                    nc++;
                }
                nr++;
            }
            
            c++;
        }
        r++;
    }
    
    return 0;
}

/* ========== STATE: INTRO ========== */
void state_intro(void) {
    if (g_intro_timer == 0) {
        g_intro_timer = 1;
    }
    
    vblank_wait();
    joy_read();
    
    if (g_intro_timer > 120 || joy_press(0, JOY_START_bit)) {
        g_state = STATE_MENU;
        g_menu_idx = 0;
        g_intro_timer = 0;
    }
    
    g_intro_timer++;
}

/* ========== STATE: MENU ========== */
void state_menu(void) {
    vblank_wait();
    joy_read();
    
    if (joy_press(0, JOY_UP_bit)) {
        if (g_menu_idx > 0) {
            g_menu_idx--;
        }
    }
    
    if (joy_press(0, JOY_DOWN_bit)) {
        if (g_menu_idx < 1) {
            g_menu_idx++;
        }
    }
    
    if (joy_press(1, JOY_A_bit) || joy_press(0, JOY_START_bit)) {
        if (g_menu_idx == 0) {
            board_init();
            g_state = STATE_GAME;
            g_player = PLAYER_HUMAN;
        }
    }
}

/* ========== STATE: GAME ========== */
void state_game(void) {
    uint8_t piece;
    uint8_t fr;
    uint8_t fc;
    uint8_t tr;
    uint8_t tc;
    uint8_t valid_move;
    uint8_t valid_cap;
    uint8_t is_diff;
    
    vblank_wait();
    joy_read();
    
    if (joy_press(0, JOY_START_bit)) {
        g_state = STATE_PAUSE;
        return;
    }
    
    board_render();
    
    if (g_player == PLAYER_HUMAN) {
        if (joy_press(0, JOY_UP_bit)) {
            if (g_sel_row > 0) {
                g_sel_row--;
            }
        }
        
        if (joy_press(0, JOY_DOWN_bit)) {
            if (g_sel_row < 7) {
                g_sel_row++;
            }
        }
        
        if (joy_press(0, JOY_LEFT_bit)) {
            if (g_sel_col > 0) {
                g_sel_col--;
            }
        }
        
        if (joy_press(0, JOY_RIGHT_bit)) {
            if (g_sel_col < 7) {
                g_sel_col++;
            }
        }
        
        if (joy_press(1, JOY_A_bit)) {
            if (g_move_sel == 0) {
                piece = board_get(g_sel_row, g_sel_col);
                if (piece == PIECE_WHITE || piece == PIECE_WKING) {
                    g_move_sel = 1;
                    g_move_row = g_sel_row;
                    g_move_col = g_sel_col;
                }
            } else {
                valid_move = move_valid(g_move_row, g_move_col, g_sel_row, g_sel_col);
                valid_cap = move_capture(g_move_row, g_move_col, g_sel_row, g_sel_col);
                
                is_diff = 0;
                if (g_sel_row != g_move_row) {
                    is_diff = 1;
                }
                if (g_sel_col != g_move_col) {
                    is_diff = 1;
                }
                
                if ((valid_move == 1 || valid_cap == 1) && is_diff == 1) {
                    move_exec(g_move_row, g_move_col, g_sel_row, g_sel_col);
                    g_move_sel = 0;
                    g_player = PLAYER_CPU;
                } else {
                    g_move_sel = 0;
                }
            }
        }
        
        if (joy_press(1, JOY_B_bit)) {
            g_move_sel = 0;
        }
    } else if (g_player == PLAYER_CPU) {
        g_cpu_timer++;
        if (g_cpu_timer > 60) {
            if (cpu_move(&fr, &fc, &tr, &tc)) {
                move_exec(fr, fc, tr, tc);
            }
            g_cpu_timer = 0;
            g_player = PLAYER_HUMAN;
        }
    }
    
    if (g_white_cnt == 0) {
        g_winner = PLAYER_CPU;
        g_state = STATE_OVER;
    }
    
    if (g_black_cnt == 0) {
        g_winner = PLAYER_HUMAN;
        g_state = STATE_OVER;
    }
    
    if (has_moves(g_player) == 0) {
        if (g_player == PLAYER_HUMAN) {
            g_winner = PLAYER_CPU;
        } else {
            g_winner = PLAYER_HUMAN;
        }
        g_state = STATE_OVER;
    }
}

/* ========== STATE: PAUSE ========== */
void state_pause(void) {
    vblank_wait();
    joy_read();
    
    if (joy_press(0, JOY_START_bit)) {
        g_state = STATE_GAME;
    }
    
    if (joy_press(1, JOY_B_bit)) {
        g_state = STATE_MENU;
        g_menu_idx = 0;
    }
}

/* ========== STATE: GAME OVER ========== */
void state_over(void) {
    vblank_wait();
    joy_read();
    
    if (joy_press(1, JOY_A_bit)) {
        board_init();
        g_state = STATE_GAME;
        g_player = PLAYER_HUMAN;
    }
    
    if (joy_press(1, JOY_B_bit)) {
        g_state = STATE_MENU;
        g_menu_idx = 0;
    }
}

/* ========== MAIN ========== */
int main(void) {
    g_state = STATE_INTRO;
    g_frame = 0;
    g_intro_timer = 0;
    g_joy_l_cur = 0;
    g_joy_h_cur = 0;
    g_joy_l_old = 0;
    g_joy_h_old = 0;
    
    ppu_init();
    joy_init();
    board_init();
    
    while (1) {
        g_frame++;
        
        if (g_state == STATE_INTRO) {
            state_intro();
        }
        
        if (g_state == STATE_MENU) {
            state_menu();
        }
        
        if (g_state == STATE_GAME) {
            state_game();
        }
        
        if (g_state == STATE_PAUSE) {
            state_pause();
        }
        
        if (g_state == STATE_OVER) {
            state_over();
        }
    }
    
    return 0;
}

/* ============================================================================
 * FIXED RENDERING
 * ============================================================================
 * 
 * KEY FIXES:
 * 
 * 1. TILEMAP SCALING:
 *    - Board: 8×8 cells
 *    - Tilemap: 32×32 tiles
 *    - Each board cell = 4×4 tiles on screen (32 pixels per piece)
 *    - board_render() now fills 4×4 tiles for each board cell
 * 
 * 2. TILEMAP INDEXING:
 *    - Linear index: (row << 5) | col  (fast, uses bit shift)
 *    - Full 32×32 = 1024 entries written each frame
 * 
 * 3. TILE SELECTION:
 *    - 8 tiles total (dark, light, white piece, black piece, crowns, selection)
 *    - Each board cell displays correct tile based on piece type + selection
 * 
 * 4. VRAM ALLOCATION:
 *    - Tiles: 0x0000-0x00FF (8 tiles × 32 bytes)
 *    - Tilemap: 0x0400-0x07FF (32×32 entries)
 *    - Mode 1 with BG1 enabled only
 * 
 * ============================================================================
 */
