/**
 * Pixel Run C25K — Couch to 5K for Pebble
 * Targets: basalt, chalk, diorite, gabbro
 *
 * Week select → Day select → Run timer with vibration cues
 * Tracks completed sessions, auto-suggests next run on launch.
 * No phone/JS required — runs entirely on-watch.
 */

#include <pebble.h>
#include <stdlib.h>

// ============================================================================
// C25K PROGRAM DATA
// ============================================================================
enum { PH_WARM=0, PH_RUN=1, PH_WALK=2, PH_COOL=3 };

typedef struct { uint16_t dur; uint8_t type; } Phase;

// All session phases in one flat array (87 phases total)
static const Phase s_phases[] = {
  // S0: Week 1 — 60s run / 90s walk x8 (offset 0, count 18)
  {300,PH_WARM},
  {60,PH_RUN},{90,PH_WALK},{60,PH_RUN},{90,PH_WALK},
  {60,PH_RUN},{90,PH_WALK},{60,PH_RUN},{90,PH_WALK},
  {60,PH_RUN},{90,PH_WALK},{60,PH_RUN},{90,PH_WALK},
  {60,PH_RUN},{90,PH_WALK},{60,PH_RUN},{90,PH_WALK},
  {300,PH_COOL},
  // S1: Week 2 — 90s run / 2min walk x6 (offset 18, count 14)
  {300,PH_WARM},
  {90,PH_RUN},{120,PH_WALK},{90,PH_RUN},{120,PH_WALK},
  {90,PH_RUN},{120,PH_WALK},{90,PH_RUN},{120,PH_WALK},
  {90,PH_RUN},{120,PH_WALK},{90,PH_RUN},{120,PH_WALK},
  {300,PH_COOL},
  // S2: Week 3 — 90s/3min alternating (offset 32, count 10)
  {300,PH_WARM},
  {90,PH_RUN},{90,PH_WALK},{180,PH_RUN},{180,PH_WALK},
  {90,PH_RUN},{90,PH_WALK},{180,PH_RUN},{180,PH_WALK},
  {300,PH_COOL},
  // S3: Week 4 — 3min/5min runs (offset 42, count 9)
  {300,PH_WARM},
  {180,PH_RUN},{90,PH_WALK},{300,PH_RUN},{150,PH_WALK},
  {180,PH_RUN},{90,PH_WALK},{300,PH_RUN},
  {300,PH_COOL},
  // S4: Week 5 Day 1 — 3x 5min run (offset 51, count 7)
  {300,PH_WARM},
  {300,PH_RUN},{180,PH_WALK},{300,PH_RUN},{180,PH_WALK},{300,PH_RUN},
  {300,PH_COOL},
  // S5: Week 5 Day 2 — 2x 8min run (offset 58, count 5)
  {300,PH_WARM},
  {480,PH_RUN},{300,PH_WALK},{480,PH_RUN},
  {300,PH_COOL},
  // S6: Week 5 Day 3 — 20min continuous (offset 63, count 3)
  {300,PH_WARM},{1200,PH_RUN},{300,PH_COOL},
  // S7: Week 6 Day 1 — 5-8-5min runs (offset 66, count 7)
  {300,PH_WARM},
  {300,PH_RUN},{180,PH_WALK},{480,PH_RUN},{180,PH_WALK},{300,PH_RUN},
  {300,PH_COOL},
  // S8: Week 6 Day 2 — 2x 10min run (offset 73, count 5)
  {300,PH_WARM},
  {600,PH_RUN},{180,PH_WALK},{600,PH_RUN},
  {300,PH_COOL},
  // S9: Week 6 Day 3 / Week 7 — 25min continuous (offset 78, count 3)
  {300,PH_WARM},{1500,PH_RUN},{300,PH_COOL},
  // S10: Week 8 — 28min continuous (offset 81, count 3)
  {300,PH_WARM},{1680,PH_RUN},{300,PH_COOL},
  // S11: Week 9 — 30min continuous (offset 84, count 3)
  {300,PH_WARM},{1800,PH_RUN},{300,PH_COOL},
};

// Session index → {offset into s_phases, phase count}
static const uint8_t s_sess[][2] = {
  {0,18},{18,14},{32,10},{42,9},{51,7},{58,5},{63,3},
  {66,7},{73,5},{78,3},{81,3},{84,3}
};

// Week (0-8) × Day (0-2) → session index
static const uint8_t s_wk_sess[9][3] = {
  {0,0,0},{1,1,1},{2,2,2},{3,3,3},{4,5,6},
  {7,8,9},{9,9,9},{10,10,10},{11,11,11}
};

// Descriptions per week (for week menu)
static const char *s_wk_desc[] = {
  "1min run / 1:30 walk x8",
  "1:30 run / 2min walk x6",
  "90s & 3min run intervals",
  "3 & 5 min run intervals",
  "Building to 20min run",
  "Building to 25min run",
  "25 min continuous runs",
  "28 min continuous runs",
  "30 min runs - 5K ready!",
};

// Descriptions per session (for day menu)
static const char *s_sess_desc[] = {
  "8x 1min run, 1:30 walk",      // S0
  "6x 1:30 run, 2min walk",      // S1
  "90s & 3min alternating",       // S2
  "3 & 5 min mixed runs",         // S3
  "3x 5min run, 3min walk",       // S4
  "2x 8min run, 5min walk",       // S5
  "20 min continuous run!",        // S6
  "5-8-5 min run intervals",      // S7
  "2x 10min run, 3min walk",      // S8
  "25 min continuous run",         // S9
  "28 min continuous run",         // S10
  "30 min continuous run!",        // S11
};

static const char *s_ph_name[] = {"WARM UP", "RUN!", "WALK", "COOL DOWN"};

#define N_MOTIV 12
static const char *s_motiv[] = {
  "You've got this!",
  "One step at a time!",
  "Keep moving forward!",
  "Stay strong!",
  "You're a runner!",
  "Push through!",
  "Almost there!",
  "Don't quit now!",
  "Feel the progress!",
  "You can do it!",
  "Keep going!",
  "Finish strong!",
};

// Persist keys
#define P_WEEK 0
#define P_DAY  1
#define P_DONE 2   // uint32 bitmask: bit (week*3+day) = completed

// ============================================================================
// GLOBALS
// ============================================================================
static Window *s_wk_win, *s_day_win, *s_run_win;
static MenuLayer *s_wk_menu, *s_day_menu;
static Layer *s_run_layer;

static int s_wk=0, s_day=0;    // Selected week/day (0-indexed)
static int s_si;                // Session index
static int s_pi;                // Current phase index
static int s_ph_rem;            // Seconds remaining in phase
static int s_tot_rem;           // Seconds remaining total
static int s_tot_dur;           // Total session duration
static bool s_started, s_paused, s_done;
static bool s_half;             // Halfway alert fired
static int s_mi;                // Motivation string index
static uint32_t s_done_mask=0;  // Completion bitmask
static int s_pause_sel=0;       // Pause menu: 0=Resume, 1=Complete, 2=Quit

// Heart rate (emery only)
static int s_hr_bpm=0;
static int s_hr_peak=0;
static int s_hr_sum=0;
static int s_hr_count=0;
static bool s_sim_hr=false;     // Simulate HR on emery emulator
#define MAX_HR 190

// Step tracking (gabbro/chalk)
static int s_steps=0;
static bool s_step_high=false;
static int s_confetti=0;
static AppTimer *s_confetti_timer=NULL;

// ============================================================================
// COMPLETION TRACKING
// ============================================================================
static bool is_complete(int wk, int day) {
  return (s_done_mask >> (wk*3+day)) & 1;
}
static void mark_complete(int wk, int day) {
  s_done_mask |= (1u << (wk*3+day));
  persist_write_int(P_DONE, (int)s_done_mask);
}
static int wk_done_count(int wk) {
  int c=0;
  for(int d=0;d<3;d++) if(is_complete(wk,d)) c++;
  return c;
}
// Find first incomplete session AFTER the highest completed one
static void find_next(int *wk, int *day) {
  // Find highest completed session index
  int highest = -1;
  for(int i=26; i>=0; i--)
    if((s_done_mask >> i) & 1) { highest = i; break; }
  // First incomplete after that
  for(int i=highest+1; i<27; i++) {
    if(!((s_done_mask >> i) & 1)) { *wk=i/3; *day=i%3; return; }
  }
  // All done or end reached — stay at highest
  if(highest >= 0) { *wk=highest/3; *day=highest%3; }
  else { *wk=0; *day=0; }
}

// ============================================================================
// HELPERS
// ============================================================================
static int sess_dur(int si) {
  int o=s_sess[si][0], n=s_sess[si][1], t=0;
  for(int i=0;i<n;i++) t+=s_phases[o+i].dur;
  return t;
}
static void fmt_ms(char *b, int sz, int sec) {
  snprintf(b, sz, "%d:%02d", sec/60, sec%60);
}

// ============================================================================
// VIBRATIONS
// ============================================================================
static void vib_run(void) {
  static const uint32_t s[] = {200,100,200};
  vibes_enqueue_custom_pattern((VibePattern){.durations=s,.num_segments=3});
}
static void vib_walk(void) { vibes_long_pulse(); }
static void vib_half(void) {
  static const uint32_t s[] = {100,80,100,80,100};
  vibes_enqueue_custom_pattern((VibePattern){.durations=s,.num_segments=5});
}
static void vib_done(void) {
  static const uint32_t s[] = {100,80,100,80,100,80,400};
  vibes_enqueue_custom_pattern((VibePattern){.durations=s,.num_segments=7});
}

// Forward declarations
static void start_confetti(void);

// ============================================================================
// RUN LOGIC
// ============================================================================
static void next_phase(void) {
  int n=s_sess[s_si][1];
  s_pi++;
  if(s_pi >= n) {
    s_done = true;
    vib_done();
    mark_complete(s_wk, s_day);
    start_confetti();
    return;
  }
  int o=s_sess[s_si][0];
  s_ph_rem = s_phases[o+s_pi].dur;
  s_mi = rand() % N_MOTIV;
  switch(s_phases[o+s_pi].type) {
    case PH_RUN:  vib_run(); break;
    case PH_WALK: case PH_COOL: vib_walk(); break;
    default: break;
  }
}

static void init_session(void) {
  s_si = s_wk_sess[s_wk][s_day];
  s_pi = 0;
  s_ph_rem = s_phases[s_sess[s_si][0]].dur;
  s_tot_dur = sess_dur(s_si);
  s_tot_rem = s_tot_dur;
  s_started = false;
  s_paused = true;
  s_done = false;
  s_half = false;
  s_mi = rand() % N_MOTIV;
  s_hr_bpm=0; s_hr_peak=0; s_hr_sum=0; s_hr_count=0;
  s_steps=0; s_step_high=false;
  // Simulate HR on emery (Time 2) emulator when no real sensor
  #ifdef PBL_PLATFORM_EMERY
  s_sim_hr=true;  // Will be disabled if real HR data arrives
  #else
  s_sim_hr=false;
  #endif
}

// ============================================================================
// RUN SCREEN DRAWING
// ============================================================================

#ifdef PBL_PLATFORM_EMERY
// Heart rate zone color (based on % of max HR)
static GColor hr_zone_color(int bpm) {
  #ifdef PBL_COLOR
  int pct = (bpm * 100) / MAX_HR;
  if(pct >= 90) return GColorRed;           // Zone 5: max
  if(pct >= 80) return GColorOrange;        // Zone 4: hard
  if(pct >= 70) return GColorChromeYellow;  // Zone 3: cardio
  if(pct >= 60) return GColorGreen;         // Zone 2: fat burn
  return GColorPictonBlue;                  // Zone 1: light
  #else
  return GColorWhite;
  #endif
}

// Tiny pixel heart icon (7x6)
static void draw_heart(GContext *ctx, int x, int y, GColor c) {
  graphics_context_set_fill_color(ctx, c);
  // Top bumps
  graphics_fill_rect(ctx, GRect(x+1,y,2,2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x+4,y,2,2), 0, GCornerNone);
  // Middle wide
  graphics_fill_rect(ctx, GRect(x,y+1,7,2), 0, GCornerNone);
  // Taper
  graphics_fill_rect(ctx, GRect(x+1,y+3,5,1), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x+2,y+4,3,1), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x+3,y+5,1,1), 0, GCornerNone);
}
#endif

// Tiny shoe icon (7x5)
static void draw_shoe(GContext *ctx, int x, int y, GColor c) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, GRect(x,y,3,2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x-1,y+2,6,2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x+5,y+2,2,2), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(x-1,y+4,8,1), 0, GCornerNone);
}

// Phase color helper
static GColor phase_color(uint8_t type) {
  #ifdef PBL_COLOR
  switch(type) {
    case PH_RUN:  return GColorFromHEX(0xE04000);
    case PH_WALK: return GColorFromHEX(0x00AA55);
    default:      return GColorFromHEX(0x0055AA);
  }
  #else
  return GColorWhite;
  #endif
}

// Tiny pixel runner (facing right, running pose)
static void draw_car(GContext *ctx, int cx, int top_y) {
  // Black outline for visibility
  graphics_context_set_fill_color(ctx, GColorBlack);
  // Head shadow
  graphics_fill_rect(ctx, GRect(cx-1, top_y-1, 4, 4), 0, GCornerNone);
  // Body shadow
  graphics_fill_rect(ctx, GRect(cx-2, top_y+2, 5, 6), 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorWhite);
  // Head (2x2)
  graphics_fill_rect(ctx, GRect(cx, top_y, 2, 2), 0, GCornerNone);
  // Body (torso leaning forward)
  graphics_fill_rect(ctx, GRect(cx-1, top_y+2, 2, 3), 0, GCornerNone);
  // Front arm (reaching forward)
  graphics_fill_rect(ctx, GRect(cx+1, top_y+3, 2, 1), 0, GCornerNone);
  // Back arm
  graphics_fill_rect(ctx, GRect(cx-3, top_y+4, 2, 1), 0, GCornerNone);
  // Front leg (extended forward)
  graphics_fill_rect(ctx, GRect(cx, top_y+5, 1, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(cx+1, top_y+6, 1, 2), 0, GCornerNone);
  // Back leg (kicked back)
  graphics_fill_rect(ctx, GRect(cx-1, top_y+5, 1, 1), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(cx-2, top_y+6, 1, 2), 0, GCornerNone);
}

// Checkered finish flag
static void draw_flag(GContext *ctx, int fx, int fy) {
  for(int dy=0; dy<8; dy++)
    for(int dx=0; dx<4; dx++) {
      graphics_context_set_fill_color(ctx,
        ((dx+dy)%2==0) ? GColorWhite : GColorBlack);
      graphics_fill_rect(ctx, GRect(fx+dx, fy+dy, 1, 1), 0, GCornerNone);
    }
}

// Session progress bar: colored segments per phase, car marker, grayed completion
static void draw_session_bar(GContext *ctx, int bw, int bh, int bar_w, int bar_h, int bar_y) {
  int bar_x = (bw - bar_w) / 2;
  int o = s_sess[s_si][0], n = s_sess[s_si][1];

  // Elapsed seconds into current phase
  int phase_elapsed = s_phases[o+s_pi].dur - s_ph_rem;

  // Total elapsed seconds
  int total_elapsed = 0;
  for(int i = 0; i < s_pi; i++) total_elapsed += s_phases[o+i].dur;
  total_elapsed += phase_elapsed;
  if(s_done) total_elapsed = s_tot_dur;

  // Black background behind bar for contrast
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(bar_x-2, bar_y-2, bar_w+4, bar_h+4), 0, GCornerNone);

  // Draw each phase segment
  int x = bar_x;
  for(int i = 0; i < n; i++) {
    int seg_w = (s_phases[o+i].dur * bar_w) / s_tot_dur;
    if(i == n-1) seg_w = bar_x + bar_w - x;
    if(seg_w < 1) seg_w = 1;

    if(i < s_pi || s_done) {
      // Completed: gray
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, GRect(x, bar_y, seg_w, bar_h), 0, GCornerNone);
    } else if(i == s_pi) {
      // Current phase: gray for elapsed, color for remaining
      int dur = s_phases[o+i].dur;
      int gray_w = dur > 0 ? (phase_elapsed * seg_w) / dur : 0;
      if(gray_w > 0) {
        graphics_context_set_fill_color(ctx, GColorDarkGray);
        graphics_fill_rect(ctx, GRect(x, bar_y, gray_w, bar_h), 0, GCornerNone);
      }
      graphics_context_set_fill_color(ctx, phase_color(s_phases[o+i].type));
      graphics_fill_rect(ctx, GRect(x+gray_w, bar_y, seg_w-gray_w, bar_h), 0, GCornerNone);
    } else {
      // Future: full phase color
      graphics_context_set_fill_color(ctx, phase_color(s_phases[o+i].type));
      graphics_fill_rect(ctx, GRect(x, bar_y, seg_w, bar_h), 0, GCornerNone);
    }

    // Segment separator only between wide segments (avoids clutter on W1)
    if(i < n-1 && seg_w > 8) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, GRect(x+seg_w-1, bar_y, 1, bar_h), 0, GCornerNone);
    }
    x += seg_w;
  }

  // White border
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_rect(ctx, GRect(bar_x-1, bar_y-1, bar_w+2, bar_h+2));

  // Checkered flag above the right end of bar
  draw_flag(ctx, bar_x + bar_w - 4, bar_y - 12);

  // Race car marker above bar
  int car_x = s_tot_dur > 0
    ? bar_x + (total_elapsed * bar_w) / s_tot_dur
    : bar_x;
  if(car_x < bar_x) car_x = bar_x;
  if(car_x > bar_x + bar_w) car_x = bar_x + bar_w;
  draw_car(ctx, car_x, bar_y - 10);
}

static void run_draw(Layer *l, GContext *ctx) {
  GRect b = layer_get_bounds(l);
  int w=b.size.w, h=b.size.h;
  int o=s_sess[s_si][0];
  uint8_t pt = s_phases[o+s_pi].type;

  // Phase background color
  GColor bg, fg;
  #ifdef PBL_COLOR
    fg = GColorWhite;
    if(s_done)           bg = GColorIslamicGreen;
    else if(pt==PH_RUN)  bg = GColorFromHEX(0xE04000);
    else if(pt==PH_WALK) bg = GColorFromHEX(0x00AA55);
    else                 bg = GColorFromHEX(0x0055AA);
  #else
    if(pt==PH_RUN && !s_done) { bg = GColorWhite; fg = GColorBlack; }
    else                       { bg = GColorBlack; fg = GColorWhite; }
  #endif

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Adaptive sizing
  bool big = w >= 240;  // gabbro=260
  int bar_h = big ? 20 : 16;
  int bar_w;
  #ifdef PBL_ROUND
  bar_w = w * 82 / 100;
  #else
  bar_w = w * 90 / 100;
  #endif
  int bar_y = h * 52 / 100;

  // Fonts scaled by screen
  GFont f_title = fonts_get_system_font(big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD);
  GFont f_big   = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  GFont f_med   = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f_sm    = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f_info  = fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, fg);

  // Layout Y positions
  int y_phase = h * 6 / 100;
  int y_count = h * 18 / 100;
  int y_step  = h * 42 / 100;   // Steps above bar
  int y_rem   = h * 62 / 100;
  int y_extra = h * 74 / 100;

  // Session progress bar
  draw_session_bar(ctx, w, h, bar_w, bar_h, bar_y);

  if(s_done) {
    // Confetti! Random colored rectangles
    #ifdef PBL_COLOR
    {
      GColor confetti[] = {GColorRed,GColorYellow,GColorGreen,GColorCyan,
                           GColorOrange,GColorPurple,GColorMagenta,GColorPictonBlue};
      int seed = s_confetti * 31 + 7;
      for(int i=0; i<25; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        int cx = seed % w;
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        int cy = (seed % h + s_confetti * 4 + i * 17) % h;
        graphics_context_set_fill_color(ctx, confetti[i % 8]);
        int sz = 2 + (i % 3);
        graphics_fill_rect(ctx, GRect(cx, cy, sz, sz), 0, GCornerNone);
      }
    }
    #endif

    // DONE!
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, "DONE!", f_title,
      GRect(0, y_phase+2, w, 34), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    char buf[32];
    snprintf(buf, sizeof(buf), "Week %d Day %d", s_wk+1, s_day+1);
    graphics_draw_text(ctx, buf, f_info,
      GRect(0, y_count+6, w, 24), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Stats below bar
    char tbuf[8];
    fmt_ms(tbuf, sizeof(tbuf), s_tot_dur);

    #ifdef PBL_PLATFORM_EMERY
    // Emery: time completed + heart stats on separate lines
    snprintf(buf, sizeof(buf), "%s completed!", tbuf);
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, buf, f_med,
      GRect(0, y_rem-2, w, 22), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    if(s_hr_count > 0) {
      int avg = s_hr_sum / s_hr_count;
      int hr_y = y_rem + 22;
      // Avg heart
      draw_heart(ctx, w/2-55, hr_y+5, hr_zone_color(avg));
      char abuf[12]; snprintf(abuf,sizeof(abuf),"Avg %d",avg);
      graphics_context_set_text_color(ctx, fg);
      graphics_draw_text(ctx,abuf,f_med,GRect(w/2-46,hr_y,55,22),
        GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
      // Peak heart
      draw_heart(ctx, w/2+12, hr_y+5, hr_zone_color(s_hr_peak));
      char pbuf2[12]; snprintf(pbuf2,sizeof(pbuf2),"Pk %d",s_hr_peak);
      graphics_draw_text(ctx,pbuf2,f_med,GRect(w/2+21,hr_y,55,22),
        GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
    }
    #else
    // Round: time + steps in one line
    if(s_steps > 0)
      snprintf(buf, sizeof(buf), "%s | %d steps", tbuf, s_steps);
    else
      snprintf(buf, sizeof(buf), "%s completed!", tbuf);
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, buf, f_info,
      GRect(0, y_rem, w, 22), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    #endif

    // Motivation
    int mot_y = y_extra;
    #ifdef PBL_PLATFORM_EMERY
    mot_y = y_rem + 46;  // Below HR stats
    #endif
    graphics_draw_text(ctx, s_motiv[s_mi], f_med,
      GRect(10, mot_y, w-20, 40), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  // === ACTIVE RUN SCREEN ===
  // Phase name
  graphics_draw_text(ctx, s_ph_name[pt], f_title,
    GRect(0,y_phase,w,34), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Phase countdown
  char pbuf[8];
  fmt_ms(pbuf, sizeof(pbuf), s_ph_rem);
  graphics_draw_text(ctx, pbuf, f_big,
    GRect(0,y_count,w,50), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Steps/HR display
  #ifdef PBL_PLATFORM_EMERY
  // Emery: HR at bottom next to W/D
  #else
  if(s_steps > 0) {
    if(big) {
      // Gabbro: shoe + steps above bar
      draw_shoe(ctx, w/2-22, y_step+3, GColorWhite);
      char step_buf[12]; snprintf(step_buf,sizeof(step_buf),"%d",s_steps);
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx,step_buf,f_info,
        GRect(w/2-12,y_step-1,60,22), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      graphics_context_set_text_color(ctx, fg);
    } else {
      // Chalk: shoe + steps at bottom next to W/D
      draw_shoe(ctx, w/2+12, y_extra+4, GColorWhite);
      char step_buf[12]; snprintf(step_buf,sizeof(step_buf),"%d",s_steps);
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx,step_buf,f_sm,
        GRect(w/2+22,y_extra+1,40,18), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      graphics_context_set_text_color(ctx, fg);
    }
  }
  #endif

  // Remaining
  char obuf[24], tmp[8];
  fmt_ms(tmp, sizeof(tmp), s_tot_rem);
  snprintf(obuf, sizeof(obuf), "%s remaining", tmp);
  graphics_draw_text(ctx, obuf, f_info,
    GRect(0,y_rem,w,22), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // W/D at bottom
  char hdr[16];
  snprintf(hdr, sizeof(hdr), "W%d D%d", s_wk+1, s_day+1);

  #ifdef PBL_PLATFORM_EMERY
  // Emery: W/D on left, heart+BPM on right
  if(s_hr_bpm > 0) {
    graphics_draw_text(ctx, hdr, f_sm,
      GRect(20,y_extra,w/2-20,18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    GColor hc = hr_zone_color(s_hr_bpm);
    draw_heart(ctx, w/2+20, y_extra+5, hc);
    char bpm_buf[8]; snprintf(bpm_buf,sizeof(bpm_buf),"%d",s_hr_bpm);
    graphics_context_set_text_color(ctx, hc);
    graphics_draw_text(ctx,bpm_buf,f_med,
      GRect(w/2+30,y_extra-2,50,22), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, fg);
  } else {
    graphics_draw_text(ctx, hdr, f_sm,
      GRect(0,y_extra,w,18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
  #else
  graphics_draw_text(ctx, hdr, f_sm,
    GRect(0,y_extra,w,18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  #endif

  // Motivational saying (rect screens)
  #ifdef PBL_RECT
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, s_motiv[s_mi], f_sm,
    GRect(10, y_extra+18, w-20, 36), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  #endif

  // Paused overlay
  if(s_paused) {
    // Dark overlay box
    graphics_context_set_fill_color(ctx, GColorBlack);
    int bx = w/2-75, bxw = 150;
    #ifdef PBL_RECT
    bx = 4; bxw = w-8;
    #endif

    if(!s_started) {
      // Pre-start: simple "SELECT to Start"
      graphics_fill_rect(ctx, GRect(bx, h/2-18, bxw, 38), 8, GCornersAll);
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, "SELECT to Start", f_med,
        GRect(0, h/2-12, w, 26), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      // Pause menu with 3 options
      int menu_h = 80;
      int my = h/2 - menu_h/2;
      graphics_fill_rect(ctx, GRect(bx, my, bxw, menu_h), 8, GCornersAll);

      const char *opts[] = {"Resume", "Complete", "Quit"};
      int row_h = 24;
      int oy = my + 4;

      for(int i=0; i<3; i++) {
        if(i == s_pause_sel) {
          // Highlight selected option
          #ifdef PBL_COLOR
          GColor hl;
          if(i==0) hl = GColorFromHEX(0x0055AA);
          else if(i==1) hl = GColorIslamicGreen;
          else hl = GColorFromHEX(0x882200);
          graphics_context_set_fill_color(ctx, hl);
          #else
          graphics_context_set_fill_color(ctx, GColorWhite);
          #endif
          graphics_fill_rect(ctx, GRect(bx+4, oy, bxw-8, row_h), 4, GCornersAll);
          #ifdef PBL_COLOR
          graphics_context_set_text_color(ctx, GColorWhite);
          #else
          graphics_context_set_text_color(ctx, GColorBlack);
          #endif
        } else {
          graphics_context_set_text_color(ctx, GColorLightGray);
        }
        graphics_draw_text(ctx, opts[i], f_med,
          GRect(0, oy, w, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
        oy += row_h;
      }
    }
  }
}

// ============================================================================
// CONFETTI TIMER (fast animation on done screen)
// ============================================================================
static void confetti_cb(void *data) {
  s_confetti++;
  if(s_run_layer) layer_mark_dirty(s_run_layer);
  s_confetti_timer = app_timer_register(80, confetti_cb, NULL);
}
static void start_confetti(void) {
  s_confetti = 0;
  if(s_confetti_timer) { app_timer_cancel(s_confetti_timer); s_confetti_timer=NULL; }
  s_confetti_timer = app_timer_register(80, confetti_cb, NULL);
}

// ============================================================================
// RUN TICK
// ============================================================================
static void run_tick(struct tm *t, TimeUnits u) {
  if(s_done) return;
  if(!s_started || s_paused) return;
  s_ph_rem--;
  s_tot_rem--;
  if(!s_half && s_tot_rem <= s_tot_dur/2) {
    s_half = true;
    vib_half();
  }
  if(s_ph_rem <= 0) next_phase();
  // Platform split: emery gets HR, everything else gets steps
  #ifdef PBL_PLATFORM_EMERY
  {
    // Real HR from sensor
    #ifdef PBL_HEALTH
    {
      HealthValue hv = health_service_peek_current_value(HealthMetricHeartRateBPM);
      if((int)hv >= 30 && (int)hv <= 220) {
        s_hr_bpm = (int)hv;
        s_sim_hr = false;  // Real data, stop simulating
        if(s_hr_bpm > s_hr_peak) s_hr_peak = s_hr_bpm;
        s_hr_sum += s_hr_bpm;
        s_hr_count++;
      }
    }
    #endif
    // Simulate HR on emery emulator when no real data
    if(s_hr_count == 0 || s_sim_hr) {
      int o2 = s_sess[s_si][0];
      uint8_t pt2 = s_phases[o2+s_pi].type;
      int base = (pt2==PH_RUN) ? 155 : (pt2==PH_WALK) ? 120 : 95;
      s_hr_bpm = base + (rand() % 11) - 5;
      if(s_hr_bpm > s_hr_peak) s_hr_peak = s_hr_bpm;
      s_hr_sum += s_hr_bpm;
      s_hr_count++;
      s_sim_hr = true;
    }
  }
  #else
  {
    // Gabbro/chalk: accelerometer step counting
    AccelData accel = {0};
    accel_service_peek(&accel);
    int mag = abs(accel.x) + abs(accel.y) + abs(accel.z);
    if(mag > 1500 && !s_step_high) { s_steps++; s_step_high = true; }
    if(mag < 1300) s_step_high = false;
    // Simulate when stationary/emulator
    if(mag < 1200) {
      int o2 = s_sess[s_si][0];
      uint8_t pt2 = s_phases[o2+s_pi].type;
      if(pt2 == PH_RUN) s_steps += 2 + (rand() % 2);
      else s_steps += 1 + (rand() % 2);
    }
  }
  #endif
  if(s_run_layer) layer_mark_dirty(s_run_layer);
}

// ============================================================================
// RUN WINDOW
// ============================================================================
static void run_sel(ClickRecognizerRef ref, void *ctx) {
  if(s_done) { window_stack_pop(true); return; }
  if(!s_started) {
    // First press: start the run
    s_started = true; s_paused = false;
  } else if(!s_paused) {
    // Running: pause and show menu
    s_paused = true; s_pause_sel = 0;
  } else {
    // Paused: confirm selected option
    switch(s_pause_sel) {
      case 0: s_paused = false; break;  // Resume
      case 1: // Complete — mark done and exit
        s_done = true;
        mark_complete(s_wk, s_day);
        vib_done();
        start_confetti();
        break;
      case 2: // Quit — exit without completing
        window_stack_pop(true);
        return;
    }
  }
  if(s_run_layer) layer_mark_dirty(s_run_layer);
}
static void run_up(ClickRecognizerRef ref, void *ctx) {
  if(s_paused && s_started) {
    s_pause_sel = (s_pause_sel + 2) % 3;  // Wrap up
    if(s_run_layer) layer_mark_dirty(s_run_layer);
  }
}
static void run_down(ClickRecognizerRef ref, void *ctx) {
  if(s_paused && s_started) {
    s_pause_sel = (s_pause_sel + 1) % 3;  // Wrap down
    if(s_run_layer) layer_mark_dirty(s_run_layer);
  }
}
static void run_back(ClickRecognizerRef ref, void *ctx) {
  window_stack_pop(true);
}
static void run_click(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, run_sel);
  window_single_click_subscribe(BUTTON_ID_UP, run_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, run_down);
  window_single_click_subscribe(BUTTON_ID_BACK, run_back);
}
static void run_load(Window *w) {
  Layer *wl = window_get_root_layer(w);
  GRect b = layer_get_bounds(wl);
  s_run_layer = layer_create(b);
  layer_set_update_proc(s_run_layer, run_draw);
  layer_add_child(wl, s_run_layer);
  window_set_click_config_provider(w, run_click);
  init_session();
  accel_data_service_subscribe(0, NULL);  // Enable accel for peek
  tick_timer_service_subscribe(SECOND_UNIT, run_tick);
}
static void run_unload(Window *w) {
  tick_timer_service_unsubscribe();
  accel_data_service_unsubscribe();
  if(s_confetti_timer) { app_timer_cancel(s_confetti_timer); s_confetti_timer=NULL; }
  if(s_run_layer) { layer_destroy(s_run_layer); s_run_layer = NULL; }
}

// ============================================================================
// DAY MENU (custom drawn with completion status)
// ============================================================================
static uint16_t day_num_rows(MenuLayer *ml, uint16_t si, void *data) { return 3; }
static int16_t day_cell_h(MenuLayer *ml, MenuIndex *idx, void *data) {
  #if PBL_DISPLAY_HEIGHT >= 260
  return 72;
  #else
  return 56;
  #endif
}

static void day_draw(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  GRect cb = layer_get_bounds(cell);
  int row = idx->row;
  int si = s_wk_sess[s_wk][row];
  bool done = is_complete(s_wk, row);
  bool selected = menu_layer_get_selected_index(s_day_menu).row == row;

  // Background
  #ifdef PBL_COLOR
  if(selected) {
    graphics_context_set_fill_color(ctx, GColorFromHEX(0x0055AA));
    graphics_fill_rect(ctx, cb, 0, GCornerNone);
  }
  // Horizontal session preview bar at bottom of cell
  int o=s_sess[si][0], n=s_sess[si][1];
  int bar_h = 4;
  int bar_y = cb.size.h - bar_h;
  int bar_margin = 10;
  int bar_w = cb.size.w - bar_margin * 2;
  int bx = bar_margin;
  int total = sess_dur(si);
  for(int i=0; i<n; i++) {
    int seg_w = (s_phases[o+i].dur * bar_w) / total;
    if(i == n-1) seg_w = bar_margin + bar_w - bx;
    if(seg_w < 1) seg_w = 1;
    graphics_context_set_fill_color(ctx, done ? GColorDarkGray : phase_color(s_phases[o+i].type));
    graphics_fill_rect(ctx, GRect(bx, bar_y, seg_w, bar_h), 0, GCornerNone);
    bx += seg_w;
  }
  #endif

  // Text — extra padding on rect screens
  #ifdef PBL_RECT
  int tx = 22;
  #else
  int tx = 14;
  #endif
  graphics_context_set_text_color(ctx, selected ? GColorWhite :
    (done ? GColorLightGray : GColorWhite));

  char title[20];
  if(done)
    snprintf(title, sizeof(title), "Day %d - Done!", row+1);
  else
    snprintf(title, sizeof(title), "Day %d", row+1);

  GFont ft = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont fs = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_draw_text(ctx, title, ft,
    GRect(tx, 4, cb.size.w-tx-16, 28), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char sub[40];
  snprintf(sub, sizeof(sub), "%s (%d min)", s_sess_desc[si], sess_dur(si)/60);
  graphics_draw_text(ctx, sub, fs,
    GRect(tx, 30, cb.size.w-tx-16, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Green check circle for completed
  #ifdef PBL_COLOR
  if(done) {
    graphics_context_set_fill_color(ctx, GColorGreen);
    graphics_fill_circle(ctx, GPoint(cb.size.w-16, cb.size.h/2), 6);
    graphics_context_set_fill_color(ctx, GColorWhite);
    // Simple checkmark: two small rects
    graphics_fill_rect(ctx, GRect(cb.size.w-19, cb.size.h/2, 3, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(cb.size.w-17, cb.size.h/2-3, 2, 5), 0, GCornerNone);
  }
  #endif
}

static void day_sel(MenuLayer *ml, MenuIndex *idx, void *data) {
  s_day = idx->row;
  window_stack_push(s_run_win, true);
}

static void day_load(Window *w) {
  Layer *wl = window_get_root_layer(w);
  GRect b = layer_get_bounds(wl);
  s_day_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_day_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = day_num_rows,
    .get_cell_height = day_cell_h,
    .draw_row = day_draw,
    .select_click = day_sel,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_day_menu, GColorFromHEX(0x1a1a2e), GColorWhite);
  menu_layer_set_highlight_colors(s_day_menu, GColorFromHEX(0x0055AA), GColorWhite);
  #endif
  menu_layer_set_click_config_onto_window(s_day_menu, w);
  #ifdef PBL_ROUND
  menu_layer_set_center_focused(s_day_menu, true);
  #endif
  layer_add_child(wl, menu_layer_get_layer(s_day_menu));
  // Auto-select first incomplete day after highest completed in this week
  int highest_d = -1;
  for(int d=2; d>=0; d--)
    if(is_complete(s_wk, d)) { highest_d = d; break; }
  int sel_d = 0;
  for(int d=highest_d+1; d<3; d++) {
    if(!is_complete(s_wk, d)) { sel_d = d; break; }
  }
  // If all done or none after highest, default to last completed+1 or 0
  if(highest_d >= 0 && sel_d <= highest_d) sel_d = (highest_d < 2) ? highest_d+1 : 2;
  menu_layer_set_selected_index(s_day_menu,
    (MenuIndex){.section=0,.row=sel_d}, MenuRowAlignCenter, false);
}
static void day_unload(Window *w) {
  if(s_day_menu) { menu_layer_destroy(s_day_menu); s_day_menu = NULL; }
}

// ============================================================================
// WEEK MENU (custom drawn with progress dots)
// ============================================================================
static uint16_t wk_num_rows(MenuLayer *ml, uint16_t si, void *data) { return 9; }
static int16_t wk_cell_h(MenuLayer *ml, MenuIndex *idx, void *data) {
  #if PBL_DISPLAY_HEIGHT >= 260
  return 72;
  #else
  return 56;
  #endif
}

static void wk_draw(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  GRect cb = layer_get_bounds(cell);
  int row = idx->row;
  int done_cnt = wk_done_count(row);
  bool selected = menu_layer_get_selected_index(s_wk_menu).row == row;

  #ifdef PBL_COLOR
  // Background
  if(selected) {
    graphics_context_set_fill_color(ctx, GColorFromHEX(0x0055AA));
    graphics_fill_rect(ctx, cb, 0, GCornerNone);
  }
  // Horizontal color bar at bottom: color by week progression
  GColor accent;
  if(row < 3)      accent = GColorFromHEX(0x00AA55);  // Early: green
  else if(row < 6) accent = GColorFromHEX(0xE0A000);  // Mid: yellow
  else             accent = GColorFromHEX(0xE04000);  // Late: orange
  if(done_cnt == 3) accent = GColorDarkGray;           // All done: muted
  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, GRect(10, cb.size.h-4, cb.size.w-20, 4), 0, GCornerNone);
  #endif

  // Title
  char title[16];
  snprintf(title, sizeof(title), "Week %d", row+1);
  GFont ft = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont fs = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  GColor tc = (done_cnt==3) ? GColorLightGray : GColorWhite;
  #ifndef PBL_COLOR
  tc = GColorWhite;
  #endif
  graphics_context_set_text_color(ctx, selected ? GColorWhite : tc);

  #ifdef PBL_RECT
  int wx = 18;
  #else
  int wx = 10;
  #endif
  graphics_draw_text(ctx, title, ft,
    GRect(wx, 8, cb.size.w-wx-40, 28), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_wk_desc[row], fs,
    GRect(wx, 36, cb.size.w-wx-40, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Completion dots (3 circles on right side)
  int dx = cb.size.w - 40;
  int dy = cb.size.h / 2;
  for(int d=0; d<3; d++) {
    #ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx,
      is_complete(row, d) ? GColorGreen : GColorDarkGray);
    #else
    graphics_context_set_fill_color(ctx,
      is_complete(row, d) ? GColorWhite : GColorDarkGray);
    #endif
    graphics_fill_circle(ctx, GPoint(dx + d*10, dy), 4);
  }
}

static void wk_sel(MenuLayer *ml, MenuIndex *idx, void *data) {
  s_wk = idx->row;
  window_stack_push(s_day_win, true);
}

static void wk_load(Window *w) {
  Layer *wl = window_get_root_layer(w);
  GRect b = layer_get_bounds(wl);
  s_wk_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_wk_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = wk_num_rows,
    .get_cell_height = wk_cell_h,
    .draw_row = wk_draw,
    .select_click = wk_sel,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_wk_menu, GColorFromHEX(0x1a1a2e), GColorWhite);
  menu_layer_set_highlight_colors(s_wk_menu, GColorFromHEX(0x0055AA), GColorWhite);
  #endif
  menu_layer_set_click_config_onto_window(s_wk_menu, w);
  #ifdef PBL_ROUND
  menu_layer_set_center_focused(s_wk_menu, true);
  #endif
  layer_add_child(wl, menu_layer_get_layer(s_wk_menu));
  // Auto-scroll to next incomplete week
  int next_wk, next_day;
  find_next(&next_wk, &next_day);
  menu_layer_set_selected_index(s_wk_menu,
    (MenuIndex){.section=0,.row=next_wk}, MenuRowAlignCenter, false);
}
static void wk_unload(Window *w) {
  if(s_wk_menu) { menu_layer_destroy(s_wk_menu); s_wk_menu = NULL; }
}

// ============================================================================
// LIFECYCLE
// ============================================================================
static void init(void) {
  srand(time(NULL));
  // Load completion data
  if(persist_exists(P_DONE)) s_done_mask = (uint32_t)persist_read_int(P_DONE);

  s_wk_win = window_create();
  window_set_window_handlers(s_wk_win,
    (WindowHandlers){.load=wk_load,.unload=wk_unload});

  s_day_win = window_create();
  window_set_window_handlers(s_day_win,
    (WindowHandlers){.load=day_load,.unload=day_unload});

  s_run_win = window_create();
  window_set_window_handlers(s_run_win,
    (WindowHandlers){.load=run_load,.unload=run_unload});

  window_stack_push(s_wk_win, true);
}

static void deinit(void) {
  window_destroy(s_run_win);
  window_destroy(s_day_win);
  window_destroy(s_wk_win);
}

int main(void) { init(); app_event_loop(); deinit(); return 0; }
