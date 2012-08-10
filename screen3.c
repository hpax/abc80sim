/*
 * Screen and keyboard management for the ABC80 emulator.
 */

#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

#include "screen.h"
#include "z80.h"


#define FRAMEWIDTH    9
#define FRAMEHEIGHT  10

#define EVENT_MASK (KeyPressMask | KeyReleaseMask | FocusChangeMask\
                    | EnterWindowMask | LeaveWindowMask\
                    | ButtonPressMask | ButtonReleaseMask | PointerMotionMask\
                    | StructureNotifyMask)

#define NORMAL  0
#define GRAPHIC 1


/*
 * The bitmap for the error list.
 */
#include "errlist.xbm"
static Pixmap errlist_pm;


static Display  *x_display;
static Window    x_window;
static Window    err_window;
static int       err_y;
static int       x_screen;
static GC        norm_gc;
static GC        inv_gc;
static GC        norm_graph_gc;
static GC        inv_graph_gc;
static int       flushcount;
static int       winwidth, winheight;

Screen_pos  screenpos[24][40];  /* Logical screen */
char        screenbuf[24][40];  /* Actual contents of screen */
Screen_pos *addr_to_sp[1024];   /* Map "address" (0 - 1023) to screen pos. */
    
/*
 * State of the screen buffer.
 */
static int buf_row = 0;
static int buf_start = 0;
static int buf_end = 0;
static int buf_len = 0;
static int buf_mode = NORMAL;
static int buf_inv = 0;
    
extern int events_in_queue;
extern int event_pending;
int xfd;


/*
 * Get an event from the X server.
 */
static int dragging;
static short last_y;
void
get_event(void)
{
    static int   keycount = 0;
    static uchar portval;
    XEvent       event;
    KeySym       keysym;
    char         key;
    
    event_pending = 0;

    if (!XCheckMaskEvent(x_display, EVENT_MASK, &event)) {
        events_in_queue = 0;
        return;
    }
    /*    x_flushed = 1;
          flushcount=0;*/
    
    switch (event.type) {
      case FocusIn:
      case EnterNotify:
        XAutoRepeatOff(x_display);
        XFlush(x_display);
        break;
        
      case FocusOut:
      case LeaveNotify:
        XAutoRepeatOn(x_display);
        XFlush(x_display);
        break;
        
      case KeyPress:
        if (XLookupString(&event.xkey, &key, 1, &keysym, NULL)) {
            switch (key) {
              case 'Å': key=']'; break;
              case 'Ä': key='['; break;
              case 'Ö': key='\\'; break;
              case 'å': key='}'; break;
              case 'ä': key='{'; break;
              case 'ö': key='|'; break;
            }
            keycount++;
            portval = 0x80 | key;
            set_in_port(56, portval);
            z80_state.i_vector = 26;
            z80_state.interrupt = 1;
        }
        break;
        
      case KeyRelease:
        if (!XLookupString(&event.xkey, &key, 1, NULL, NULL)) {
            return;
        }
        if (!(--keycount)) {
            portval &= 0x7f;
            set_in_port(56, portval);
        }
        break;

      case ButtonPress:
        {
            XButtonPressedEvent *ev = (XButtonPressedEvent *)&event;

            if (ev->window == err_window) {
                dragging = 1;
                last_y = ev->y;
            }
        }
        break;

      case ButtonRelease:
        dragging = 0;
        break;

      case MotionNotify:
        {
            XPointerMovedEvent *ev = (XPointerMovedEvent *)&event;
            Window tmp;
            int x, y;
            unsigned int button_status;

            XQueryPointer(x_display, err_window, &tmp, &tmp, &x, &y, &x, &y,
                          &button_status);
            button_status &= (Button1Mask | Button2Mask | Button3Mask);
            if (button_status == 0) {
                dragging = 0;
            }

            if (dragging) {
                if (err_y + ev->y - last_y < winheight - errlist_height) {
                    last_y += (ev->y - last_y -  winheight + errlist_height 
                               + err_y);
                    err_y = winheight - errlist_height;
                } else if (err_y + ev->y - last_y > winheight) {
                    last_y += (ev->y - last_y -  winheight + err_y);
                    err_y = winheight;
                } else {
                    err_y += (ev->y - last_y);
                }

                XTranslateCoordinates(x_display, x_window,
                                      RootWindow(x_display, x_screen), 
                                      20, err_y, 
                                      &x, &y, &tmp);
                XMoveWindow(x_display, err_window, x, y);
                XSync(x_display, True);
            }
        }
        break;

      case ConfigureNotify:
        {
            XConfigureEvent *ev = (XConfigureEvent *)&event;
            Window tmp;
            int x, y;

            XTranslateCoordinates(x_display, x_window,
                                  RootWindow(x_display, x_screen), 
                                  20, err_y, 
                                  &x, &y, &tmp);
            XMoveWindow(x_display, err_window, x, y);
        }
        break;

      case MapNotify:
        {
            XConfigureEvent *ev = (XConfigureEvent *)&event;

            if (ev->window == err_window) {
                XRaiseWindow(x_display, x_window);
            }
        }
        break;

    }
}



/*
 * Load the ABC80 font.
 */
static XFontStruct *
load_font(char *fontname)
{
    XFontStruct  *fontstruct;
    char        **old_fontpath;
    char        **new_fontpath;
    int           nfont;
    int           i;
    
    if ((fontstruct = XLoadQueryFont(x_display, fontname)) == NULL) {
        old_fontpath = XGetFontPath(x_display, &nfont);
        new_fontpath = (char **)malloc((nfont + 1) * sizeof(char*));
        if (!new_fontpath) {
            perror("Out of memory");
            exit(1);
        }

        for (i = 0; i < nfont; i++) {
            new_fontpath[i] = (char *)malloc(strlen(old_fontpath[i]) + 1);
            if (!new_fontpath[i]) {
                perror("Out of memory");
                exit(1);
            }
            strcpy(new_fontpath[i], old_fontpath[i]);
        }

        new_fontpath[i] = (char *)malloc(256 * sizeof(char));
        if (!new_fontpath[i]) {
            perror("Out of memory");
            exit(1);
        }

        sprintf(new_fontpath[i], "%s/%s/", ABCDIR, "font");
        XSetFontPath(x_display, new_fontpath, nfont + 1);

        XFreeFontPath(old_fontpath);
        for (i = 0; i < nfont + 1; i++) {
            free(new_fontpath[i]);
        }
        free(new_fontpath);

        if ((fontstruct = XLoadQueryFont(x_display, fontname)) == NULL) {
            fprintf(stderr, "ABC80: Can't load font: %s\n", fontname);
            exit(1);
        }
    }

    return fontstruct;
}



/*
 * Initialize the ABC80 screen and IO handling.
 */
void
screen_init()
{
    XSetWindowAttributes  win_attr;    /* storage for "window attributes" */
    XSizeHints            hints;
    XGCValues             gcvalues;
    int                   r, c;
    int                   rc;
    XFontStruct           *tfont, *gfont;
    int                   charwidth, charheight;
XEvent ev;
    
    x_display = XOpenDisplay(NULL);
    if (!x_display) {
        perror("window_new: Can't open display.");
        exit(1);
    }
    
    x_screen = DefaultScreen(x_display);
    
    tfont = load_font("abc80");
    gfont = load_font("abc80graph");
    charwidth = tfont->max_bounds.width;
    charheight = tfont->ascent + tfont->descent;
    winwidth = charwidth * 40 + FRAMEWIDTH * 2;
    winheight = charheight * 24 + FRAMEHEIGHT * 2;

    x_window = XCreateSimpleWindow(x_display, RootWindow(x_display, x_screen),
                                   100, 100, winwidth, winheight, 0, 
                                   WhitePixel(x_display, x_screen), 
                                   BlackPixel(x_display, x_screen));
    
    /* 
     * Set up "window hints" so that we won't be allowed to
     * resize the window while it's running
     */
    hints.flags = PSize | PMinSize | PMaxSize;
    hints.width = hints.min_width = hints.max_width = winwidth;
    hints.height = hints.min_height = hints.max_height = winheight;
    XSetStandardProperties(x_display, x_window, "ABC80", "ABC80", 
                           None, NULL, 0, &hints);
    
    /* 
     * Tell X which events we'd like to be aware of
     */
    win_attr.backing_store = Always;
    win_attr.event_mask = EVENT_MASK;
    XChangeWindowAttributes(x_display, x_window, 
                            CWBackingStore | CWEventMask, &win_attr);
    
    /*
     * Create the Graphic Contexts needed for writing 
     * text and graphics in both normal and inverted video.
     */
    gcvalues.plane_mask = 1;
    gcvalues.background = BlackPixel(x_display, x_screen);
    gcvalues.foreground = WhitePixel(x_display, x_screen);
    gcvalues.font = tfont->fid;
    norm_gc = XCreateGC(x_display, x_window, GCPlaneMask | GCFont |
                        GCForeground | GCBackground, &gcvalues);
    gcvalues.font = gfont->fid;
    norm_graph_gc = XCreateGC(x_display, x_window, GCPlaneMask | GCFont |
                              GCForeground | GCBackground, &gcvalues);
    gcvalues.background = WhitePixel(x_display, x_screen);
    gcvalues.foreground = BlackPixel(x_display, x_screen);
    inv_graph_gc = XCreateGC(x_display, x_window, GCPlaneMask | GCFont |
                             GCForeground | GCBackground, &gcvalues);
    gcvalues.font = tfont->fid;
    inv_gc = XCreateGC(x_display, x_window, GCPlaneMask | GCFont |
                       GCForeground | GCBackground, &gcvalues);
     
    /*
     * Initialize the "logical" screen"
     */
    memset((char *)addr_to_sp, 0, 1024 * sizeof(Screen_pos *));
    for (r = 0; r < 24; r++) {
        for (c = 0; c < 40; c++) {
            int addr;
            screenpos[r][c].r = r;
            screenpos[r][c].c = c;
            screenpos[r][c].x = c * charwidth + FRAMEWIDTH;
            screenpos[r][c].y = r * charheight + FRAMEHEIGHT + 14;
            screenpos[r][c].graph_mode = 0;            
            addr = ((r & 7) << 7) + (r >> 3) * 40 + c;
            addr_to_sp[addr] = &screenpos[r][c];
        }
    }
    
    errlist_pm = XCreateBitmapFromData(x_display, x_window, errlist_bits,
                                       errlist_width, errlist_height);
    err_window = XCreateSimpleWindow(x_display, 
                                     RootWindow(x_display, x_screen),
                                     0, 400, errlist_width,
                                     errlist_height + 20, 0, 
                                     BlackPixel(x_display, x_screen), 
                                     WhitePixel(x_display, x_screen));
    XChangeWindowAttributes(x_display, err_window, 
                            CWBackingStore | CWEventMask, &win_attr);
    XSetTransientForHint(x_display, err_window, x_window);

    XMapWindow(x_display, x_window);
    XWindowEvent(x_display, x_window, StructureNotifyMask, &ev);

    {
        Window tmp;
        int x, y;

        err_y = winheight - errlist_height;
        XTranslateCoordinates(x_display, x_window,
                              RootWindow(x_display, x_screen), 
                              20, err_y, 
                              &x, &y, &tmp);
        XMoveWindow(x_display, err_window, x, y);
    }
    XMapWindow(x_display, err_window);
#ifdef ARRRRGHHH
    {
        Window w[2];

        w[0] = x_window;
        w[1] = err_window;
        XRestackWindows(x_display, w, 2);
    }
#endif 
    XRaiseWindow(x_display, x_window);
    XSync(x_display, FALSE);

    XCopyPlane(x_display, errlist_pm, err_window, inv_gc, 0, 0,
               errlist_width, errlist_height, 
               0, 0, 1);

    /*
     * Set up handling of the keyboard.
     */
    XAutoRepeatOff(x_display);
    XSync(x_display, FALSE);
    
    xfd = ConnectionNumber(x_display);
}



/*
 * Clean up before exiting.
 */
void
screen_reset(void)
{
    XAutoRepeatOn(x_display);
    XFlush(x_display);
}



/*
 * Flush the screen buffer.
 */
static void
buf_flush(int r, int c, int length, int graph_mode, int inv_mode)
{
    if (inv_mode) {        
        if (graph_mode == GRAPHIC) {
            XDrawImageString(x_display, x_window, inv_graph_gc,
                             screenpos[r][c].x, screenpos[r][c].y, 
                             &screenbuf[r][c], length);
        } else {
            XDrawImageString(x_display, x_window, inv_gc,
                             screenpos[r][c].x, screenpos[r][c].y, 
                             &screenbuf[r][c], length);
        }
    } else {
        if (graph_mode == GRAPHIC) {
            XDrawImageString(x_display, x_window, norm_graph_gc,
                             screenpos[r][c].x, screenpos[r][c].y, 
                             &screenbuf[r][c], length);
        } else {
            XDrawImageString(x_display, x_window, norm_gc,
                             screenpos[r][c].x, screenpos[r][c].y, 
                             &screenbuf[r][c], length);
        }
    }
}


/*
 * This function is called whenever something is written in the
 * image memory. We try to buffer writes as long as they are
 * creating a contiguous string to minimize the calls to the X server.
 * We also update screen appearence according to the graphic and/or
 * inverted video.
 */
void
screen_write(int addr, int value)
{
    Screen_pos *sp;
    int c, r;
    int graph_mode;
    int inv_mode;
    int update;
    
    if ((sp = addr_to_sp[addr]) != 0) { /* Is the addr on the sreen? */
        r = sp->r;
        c = sp->c;
        
        /*
         * If the new character is not right after the last
         * written character we need to flush the buffer.
         */
        if (buf_len != 0 && (r != buf_row || c != buf_end)) {
            buf_flush(buf_row, buf_start, buf_len, buf_mode, buf_inv);
            buf_len = 0;
        }
        
        if (buf_len == 0) {            
            buf_row = r;
            buf_start = buf_end = c;
        }
        
        /*
         * Write the character in the buffer.
         */
        screenpos[r][c].value = (uchar)value;
        
        /*
         * Update graphics/normal mode on the line in 
         * which the character was written.
         */
        if (c > 0) {
            graph_mode = screenpos[r][c - 1].graph_mode;
        } else {
            graph_mode = NORMAL;
        }
        update = 1;
        while (c < 40 && update) {
            if (((screenpos[r][c].value & 0x7f) >= 17) 
                && ((screenpos[r][c].value & 0x7f) <= 23))
            {
                graph_mode = GRAPHIC;
                   
            } else if (((screenpos[r][c].value & 0x7f) >= 1) 
                       && ((screenpos[r][c].value & 0x7f) <= 7))
            {
                graph_mode = NORMAL;
            }
            
            inv_mode = ((screenpos[r][c].value & 0x80) != 0);
            
            if (buf_len != 0 && (buf_mode != graph_mode 
                                 || buf_inv != inv_mode)) {
                buf_flush(buf_row, buf_start, buf_len, buf_mode, buf_inv);
                buf_row = r;
                buf_start = buf_end = c;
                buf_len = 0;
            }
            
            if (c == sp->c) {
                screenpos[r][c].graph_mode = graph_mode;
                screenbuf[r][c] = value & 0x7f;
                if (screenbuf[r][c] < 32) {
                    screenbuf[r][c] = 32;
                }
                buf_len++;
                buf_end++;
                buf_mode = graph_mode;
                buf_inv = ((screenpos[r][c].value & 0x80) != 0);
                
            } else if (screenpos[r][c].graph_mode != graph_mode) {
                screenpos[r][c].graph_mode = graph_mode;
                buf_len++;
                buf_end++;
                buf_mode = graph_mode;
                buf_inv = ((screenpos[r][c].value & 0x80) != 0);
                
            } else {
                update = 0;
            }
            c++;
        }
    }
}


void
screen_flush(void)
{
    extern int flush_pending;
    
    if ((++flushcount) == 5) {        
        flushcount = 0;
        buf_flush(buf_row, buf_start, buf_len, buf_mode, buf_inv);
        buf_len = 0;
        XFlush(x_display);
    }
    flush_pending = 0;
}
