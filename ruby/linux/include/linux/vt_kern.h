#ifndef _VT_KERN_H
#define _VT_KERN_H

/*
 * this really is an extension of the vc_cons structure in console.c, but
 * with information needed by the vt package
 */

#include <linux/config.h>
#include <linux/vt.h>
#include <linux/kbd_kern.h>

/*
 * Presently, a lot of graphics programs do not restore the contents of
 * the higher font pages.  Defining this flag will avoid use of them, but
 * will lose support for PIO_FONTRESET.  Note that many font operations are
 * not likely to work with these programs anyway; they need to be
 * fixed.  The linux/Documentation directory includes a code snippet
 * to save and restore the text font.
 */
#ifdef CONFIG_VGA_CONSOLE
#define BROKEN_GRAPHICS_PROGRAMS 1
#endif

/* scroll */
#define SM_UP       (1)
#define SM_DOWN     (2)

/* cursor */
#define CM_DRAW     (1)
#define CM_ERASE    (2)
#define CM_MOVE     (3)

int softcursor_original;
struct console_font_op; 

extern unsigned char color_table[];
extern int default_red[];
extern int default_grn[];
extern int default_blu[];      
                          
/*
 * this is what the terminal answers to a ESC-Z or csi0c query.
 */
#define VT100ID "\033[?1;2c"
#define VT102ID "\033[?6c"

/*
 * Data structure describing single virtual console except for data
 * used by vt.c.
 *
 * Fields marked with [#] must be set by the low-level driver.
 * Fields marked with [!] can be changed by the low-level driver
 * to achieve effects such as fast scrolling by changing the origin.
 */           

#define NPAR 16

struct vc_data {
        unsigned short  vc_num;                 /* Console number */
        unsigned int    vc_cols;                /* [#] Console size */
        unsigned int    vc_rows;
        unsigned int    vc_size_row;            /* Bytes per row */
        unsigned short  *vc_screenbuf;          /* In-memory character/attribute buffer */
        unsigned int    vc_screenbuf_size;
        unsigned char   vc_attr;                /* Current attributes */
        unsigned char   vc_def_color;           /* Default colors */
        unsigned char   vc_color;               /* Foreground & background */
        unsigned char   vc_s_color;             /* Saved foreground & background */
        unsigned char   vc_ulcolor;             /* Color for underline mode */
        unsigned char   vc_halfcolor;           /* Color for half intensity mode */
        unsigned short  vc_complement_mask;     /* [#] Xor mask for mouse pointer */
        unsigned short  vc_hi_font_mask;        /* [#] Attribute set for upper 256 chars of font or 0 if not supported */                                    
        unsigned short  vc_video_erase_char;    /* Background erase character */
        unsigned short  vc_s_complement_mask;   /* Saved mouse pointer mask */
        unsigned int    vc_x, vc_y;             /* Cursor position */
        unsigned int    vc_top, vc_bottom;      /* Scrolling region */
        unsigned int    vc_state;               /* Escape sequence parser state */
        unsigned int    vc_npar,vc_par[NPAR];   /* Parameters of current escape sequence */
        unsigned long   vc_origin;              /* [!] Start of real screen */
        unsigned long   vc_scr_end;             /* [!] End of real screen */
        unsigned long   vc_visible_origin;      /* [!] Top of visible window */
        unsigned long   vc_pos;                 /* Cursor address */
        unsigned int    vc_saved_x;
        unsigned int    vc_saved_y;
	struct kbd_struct kbd_table;		/* VC keyboard state */       
	/* data for manual vt switching */
	struct vt_mode  vt_mode;
        int             vt_pid;
        int             vt_newvt;
        /* mode flags */
        unsigned int    vc_charset      : 1;    /* Character set G0 / G1 */
        unsigned int    vc_s_charset    : 1;    /* Saved character set */
        unsigned int    vc_disp_ctrl    : 1;    /* Display chars < 32? */
        unsigned int    vc_toggle_meta  : 1;    /* Toggle high bit? */
        unsigned int    vc_decscnm      : 1;    /* Screen Mode */
        unsigned int    vc_decom        : 1;    /* Origin Mode */
        unsigned int    vc_decawm       : 1;    /* Autowrap Mode */
        unsigned int    vc_dectcem      : 1;    /* Text Cursor Enable */
        unsigned int    vc_irm          : 1;    /* Insert/Replace Mode */
        unsigned int    vc_deccolm      : 1;    /* 80/132 Column Mode */
        /* attribute flags */
        unsigned int    vc_intensity    : 2;    /* 0=half-bright, 1=normal, 2=bold */
        unsigned int    vc_underline    : 1;
        unsigned int    vc_blink        : 1;
        unsigned int    vc_reverse      : 1;
        unsigned int    vc_s_intensity  : 2;    /* saved rendition */
        unsigned int    vc_s_underline  : 1;
        unsigned int    vc_s_blink      : 1;
        unsigned int    vc_s_reverse    : 1;                        
        /* misc */
        unsigned int    vc_priv4        : 1;    /* indicating a private control
function (used to be called "ques") */
        unsigned int    vc_need_wrap    : 1;
        unsigned int    vc_can_do_color : 1;
        unsigned int    vc_report_mouse : 2;
        unsigned int    vc_kmalloced    : 1;
        unsigned char   vc_utf          : 1;    /* Unicode UTF-8 encoding */
        unsigned char   vc_utf_count;
                 int    vc_utf_char;
        unsigned int    vc_tab_stop[5];         /* Tab stops. 160 columns. */
        unsigned char   vc_palette[16*3];       /* Colour palette for VGA+ */
        unsigned short * vc_translate;
        unsigned char   vc_G0_charset;
        unsigned char   vc_G1_charset;
        unsigned char   vc_saved_G0;
        unsigned char   vc_saved_G1;
        unsigned int    vc_bell_pitch;          /* Console bell pitch */
        unsigned int    vc_bell_duration;       /* Console bell duration */
        unsigned int    vc_cursor_type;
        struct vt_struct *display_fg;		/* Ptr to display */
	unsigned long   vc_uni_pagedir;
        unsigned long   *vc_uni_pagedir_loc;  /* [!] Location of uni_pagedir var
iable for this console */
	wait_queue_head_t paste_wait;	        /* For selections */	
#ifdef CONFIG_VT_EXTENDED
        /* Internal flags */
        unsigned int    vc_decscl;              /* operating level */
        unsigned int    vc_c8bit        : 1;    /* 8-bit controls */
        unsigned int    vc_d8bit        : 1;    /* 8-bit data */
        unsigned int    vc_shift        : 1;    /* single shift */
        unsigned int    vc_priv1        : 1;    /* indicating a private control
                                                        function */
        unsigned int    vc_priv2        : 1;    /* indicating a private control
                                                        function */
        unsigned int    vc_priv3        : 1;    /* indicating a private control
function */
        /* Private modes */
        unsigned int    vc_decckm       : 1;    /* Cursor Keys */
        unsigned int    vc_decsclm      : 1;    /* Scrolling */
        unsigned int    vc_decarm       : 1;    /* Autorepeat */       
        unsigned int    vc_decnrcm      : 1;    /* National Replacement Characte
r Set */
        unsigned int    vc_decnkm       : 1;    /* Numeric Keypad */
        /* ANSI / ISO mode flags */
        unsigned int    vc_kam          : 1;    /* Keyboard Action */
        unsigned int    vc_crm          : 1;    /* Console Representation */
        unsigned int    vc_lnm          : 1;    /* Line feed/New line */
        /* Charset mappings */
        unsigned char   vc_GL_charset;
        unsigned char   vc_GR_charset;
        unsigned char   vc_G2_charset;
        unsigned char   vc_G3_charset;
        unsigned char   vc_GS_charset;
        unsigned char   vc_saved_G2;
        unsigned char   vc_saved_G3;
        unsigned char   vc_saved_GS;
#endif /* CONFIG_VT_EXTENDED */
};                                 

extern struct vc_data *vc_cons[MAX_NR_CONSOLES];

#define CUR_DEF         0
#define CUR_NONE        1
#define CUR_UNDERLINE   2
#define CUR_LOWER_THIRD 3
#define CUR_LOWER_HALF  4
#define CUR_TWO_THIRDS  5
#define CUR_BLOCK       6
#define CUR_HWMASK      0x0f
#define CUR_SWMASK      0xfff0

#define CUR_DEFAULT CUR_UNDERLINE

#define CON_IS_VISIBLE(vc) (vc->vc_num == fg_console)        

struct consw {
        const char *(*con_startup)(void);
        void    (*con_init)(struct vc_data *, int);
        void    (*con_deinit)(struct vc_data *);
        void    (*con_clear)(struct vc_data *, int, int, int, int);
        void    (*con_putc)(struct vc_data *, int, int, int);
        void    (*con_putcs)(struct vc_data *, const unsigned short *, int, int, int);
        void    (*con_cursor)(struct vc_data *, int);
        int     (*con_scroll)(struct vc_data *, int, int, int, int);
        void    (*con_bmove)(struct vc_data *, int, int, int, int, int, int);
        int     (*con_switch)(struct vc_data *);
        int     (*con_blank)(struct vc_data *, int);
        int     (*con_font_op)(struct vc_data *, struct console_font_op *);
        int     (*con_set_palette)(struct vc_data *, unsigned char *);
        int     (*con_scrolldelta)(struct vc_data *, int);
        int     (*con_set_origin)(struct vc_data *);
        void    (*con_save_screen)(struct vc_data *);
        u8      (*con_build_attr)(struct vc_data *, u8, u8, u8, u8, u8);
        void    (*con_invert_region)(struct vc_data *, u16 *, int);
        u16    *(*con_screen_pos)(struct vc_data *, int);
        unsigned long (*con_getxy)(struct vc_data *,unsigned long,int *,int *);
};

extern struct consw *conswitchp;

extern struct consw dummy_con;  /* dummy console buffer */
extern struct consw fb_con;     /* frame buffer based console */
extern struct consw vga_con;    /* VGA text console */
extern struct consw newport_con;        /* SGI Newport console  */
extern struct consw prom_con;   /* SPARC PROM console */

void take_over_console(struct consw *sw, int first, int last, int deflt);
void give_up_console(struct consw *sw);

extern struct vt_struct {
	unsigned char	vc_mode;		/* KD_TEXT, ... */
	struct consw	*sw;			/* Display driver for VT */
} *vt_cons;

void (*kd_mksound)(unsigned int hz, unsigned int ticks);

/* vt.c */

struct console_font_op;

int vc_allocate(unsigned int console);
int vc_cons_allocated(unsigned int console);
int vc_resize(unsigned int lines, unsigned int cols,
	      unsigned int first, unsigned int last);
#define vc_resize_all(l, c) vc_resize(l, c, 0, MAX_NR_CONSOLES-1)
#define vc_resize_con(l, c, x) vc_resize(l, c, x, x)
void vc_disallocate(unsigned int console);
void set_cursor(struct vc_data *vc);
void hide_cursor(struct vc_data *vc);
void add_softcursor(struct vc_data *vc);
void reset_palette(struct vc_data *vc);
void set_palette(struct vc_data *vc);
inline void save_screen(struct vc_data *vc);
void set_origin(struct vc_data *vc);
void unblank_screen(void);
void poke_blanked_console(void);
inline unsigned short *screenpos(struct vc_data *vc, int offset, int viewed);
void scrollback(int);
void scrollfront(int);
void gotoxy(struct vc_data *vc, int new_x, int new_y);
void update_region(struct vc_data *vc, unsigned long start, int count);
void redraw_screen(int new_console, int is_switch);
#define update_screen(x) redraw_screen(x, 0)
#define switch_screen(x) redraw_screen(x, 1)

struct tty_struct;
int tioclinux(struct tty_struct *tty, unsigned long arg);

/* consolemap.c */

struct unimapinit;
struct unipair;

void console_map_init(void);
int con_set_trans_old(unsigned char * table);
int con_get_trans_old(unsigned char * table);
int con_set_trans_new(unsigned short * table);
int con_get_trans_new(unsigned short * table);
int con_clear_unimap(struct vc_data *vc, struct unimapinit *ui);
int con_set_unimap(struct vc_data *vc, ushort ct, struct unipair *list);
int con_get_unimap(struct vc_data *vc, ushort ct, ushort *uct, struct unipair *list);
int con_set_default_unimap(struct vc_data *vc);
void con_free_unimap(struct vc_data *vc);
void con_protect_unimap(struct vc_data *vc, int rdonly);
int con_copy_unimap(struct vc_data *dstcons, struct vc_data *srccons);

/* vt_ioctl.c */

extern unsigned int video_font_height;
extern unsigned int default_font_height;
extern unsigned int video_scan_lines;

void change_console(unsigned int);
void complete_change_console(unsigned int);
int vt_waitactive(int vt);
void reset_vc(unsigned int new_console);
int con_font_op(int currcons, struct console_font_op *op);

#endif /* _VT_KERN_H */