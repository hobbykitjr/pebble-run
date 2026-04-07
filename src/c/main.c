/**
 * Pixel Run C25K — Couch to 5K for Pebble Round 2
 * Target: gabbro (260x260, round, 64-color)
 *
 * Week select → Day select → Run timer with vibration cues
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

// ============================================================================
// RUN LOGIC
// ============================================================================
static void next_phase(void) {
  int n=s_sess[s_si][1];
  s_pi++;
  if(s_pi >= n) {
    s_done = true;
    vib_done();
    persist_write_int(P_WEEK, s_wk);
    persist_write_int(P_DAY, s_day);
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
}

// ============================================================================
// RUN SCREEN DRAWING
// ============================================================================
static void run_draw(Layer *l, GContext *ctx) {
  GRect b = layer_get_bounds(l);
  int w=b.size.w;
  int o=s_sess[s_si][0];
  uint8_t pt = s_phases[o+s_pi].type;

  // Phase background color
  GColor bg;
  if(s_done)           bg = GColorIslamicGreen;
  else if(pt==PH_RUN)  bg = GColorFromHEX(0xE04000);
  else if(pt==PH_WALK) bg = GColorFromHEX(0x00AA55);
  else                 bg = GColorFromHEX(0x0055AA);

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Progress ring
  GRect ring = grect_inset(b, GEdgeInsets(3));
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, 8, 0, TRIG_MAX_ANGLE);
  if(s_tot_dur > 0) {
    int elapsed = s_tot_dur - s_tot_rem;
    int prog_deg = (elapsed * 360) / s_tot_dur;
    if(s_done) prog_deg = 360;
    if(prog_deg > 0) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, 8,
        DEG_TO_TRIGANGLE(-90), DEG_TO_TRIGANGLE(-90 + prog_deg));
    }
  }

  // Fonts
  GFont f28 = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f42 = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  GFont f18 = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, GColorWhite);

  if(s_done) {
    // Completion screen
    graphics_draw_text(ctx, "DONE!", f28,
      GRect(0,55,w,34), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    char buf[32];
    snprintf(buf, sizeof(buf), "Week %d Day %d", s_wk+1, s_day+1);
    graphics_draw_text(ctx, buf, f18,
      GRect(0,95,w,24), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    char tbuf[8];
    fmt_ms(tbuf, sizeof(tbuf), s_tot_dur);
    snprintf(buf, sizeof(buf), "%s completed!", tbuf);
    graphics_draw_text(ctx, buf, f18,
      GRect(0,120,w,24), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, s_motiv[s_mi], f18,
      GRect(20,158,w-40,40), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  // Week/Day header
  char hdr[16];
  snprintf(hdr, sizeof(hdr), "W%d D%d", s_wk+1, s_day+1);
  graphics_draw_text(ctx, hdr, f14,
    GRect(0,30,w,18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Phase name
  graphics_draw_text(ctx, s_ph_name[pt], f28,
    GRect(0,50,w,34), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Phase countdown (big)
  char pbuf[8];
  fmt_ms(pbuf, sizeof(pbuf), s_ph_rem);
  graphics_draw_text(ctx, pbuf, f42,
    GRect(0,88,w,50), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Total remaining
  char obuf[24], tmp[8];
  fmt_ms(tmp, sizeof(tmp), s_tot_rem);
  snprintf(obuf, sizeof(obuf), "%s remaining", tmp);
  graphics_draw_text(ctx, obuf, f14,
    GRect(0,145,w,18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Motivation
  graphics_draw_text(ctx, s_motiv[s_mi], f18,
    GRect(20,170,w-40,40), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Paused overlay
  if(s_paused) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(w/2-75, b.size.h/2-18, 150, 38), 8, GCornersAll);
    graphics_context_set_text_color(ctx, GColorWhite);
    const char *ptxt = s_started ? "PAUSED" : "SELECT to Start";
    GFont pf = s_started ? f28 : f18;
    int py = s_started ? b.size.h/2-16 : b.size.h/2-12;
    graphics_draw_text(ctx, ptxt, pf,
      GRect(0,py,w,34), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// ============================================================================
// RUN TICK
// ============================================================================
static void run_tick(struct tm *t, TimeUnits u) {
  if(!s_started || s_paused || s_done) return;
  s_ph_rem--;
  s_tot_rem--;
  if(!s_half && s_tot_rem <= s_tot_dur/2) {
    s_half = true;
    vib_half();
  }
  if(s_ph_rem <= 0) next_phase();
  if(s_run_layer) layer_mark_dirty(s_run_layer);
}

// ============================================================================
// RUN WINDOW
// ============================================================================
static void run_sel(ClickRecognizerRef ref, void *ctx) {
  if(s_done) { window_stack_pop(true); return; }
  if(!s_started) { s_started = true; s_paused = false; }
  else s_paused = !s_paused;
  if(s_run_layer) layer_mark_dirty(s_run_layer);
}
static void run_back(ClickRecognizerRef ref, void *ctx) {
  window_stack_pop(true);
}
static void run_click(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, run_sel);
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
  tick_timer_service_subscribe(SECOND_UNIT, run_tick);
}
static void run_unload(Window *w) {
  tick_timer_service_unsubscribe();
  if(s_run_layer) { layer_destroy(s_run_layer); s_run_layer = NULL; }
}

// ============================================================================
// DAY MENU
// ============================================================================
static uint16_t day_num_rows(MenuLayer *ml, uint16_t si, void *data) { return 3; }
static int16_t day_cell_h(MenuLayer *ml, MenuIndex *idx, void *data) { return 52; }

static void day_draw(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  char title[8];
  snprintf(title, sizeof(title), "Day %d", idx->row + 1);
  int si = s_wk_sess[s_wk][idx->row];
  // Show total time in subtitle
  char sub[40];
  int dur = sess_dur(si);
  snprintf(sub, sizeof(sub), "%s (%d min)", s_sess_desc[si], dur/60);
  menu_cell_basic_draw(ctx, cell, title, sub, NULL);
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
  menu_layer_set_click_config_onto_window(s_day_menu, w);
  #ifdef PBL_ROUND
  menu_layer_set_center_focused(s_day_menu, true);
  #endif
  layer_add_child(wl, menu_layer_get_layer(s_day_menu));
}
static void day_unload(Window *w) {
  if(s_day_menu) { menu_layer_destroy(s_day_menu); s_day_menu = NULL; }
}

// ============================================================================
// WEEK MENU
// ============================================================================
static uint16_t wk_num_rows(MenuLayer *ml, uint16_t si, void *data) { return 9; }
static int16_t wk_cell_h(MenuLayer *ml, MenuIndex *idx, void *data) { return 52; }

static void wk_draw(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  char title[12];
  snprintf(title, sizeof(title), "Week %d", idx->row + 1);
  menu_cell_basic_draw(ctx, cell, title, s_wk_desc[idx->row], NULL);
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
  menu_layer_set_click_config_onto_window(s_wk_menu, w);
  #ifdef PBL_ROUND
  menu_layer_set_center_focused(s_wk_menu, true);
  #endif
  layer_add_child(wl, menu_layer_get_layer(s_wk_menu));
  // Scroll to last completed week
  int last_wk = persist_exists(P_WEEK) ? persist_read_int(P_WEEK) : 0;
  if(last_wk >= 0 && last_wk < 9) {
    menu_layer_set_selected_index(s_wk_menu,
      (MenuIndex){.section=0,.row=last_wk}, MenuRowAlignCenter, false);
  }
}
static void wk_unload(Window *w) {
  if(s_wk_menu) { menu_layer_destroy(s_wk_menu); s_wk_menu = NULL; }
}

// ============================================================================
// LIFECYCLE
// ============================================================================
static void init(void) {
  srand(time(NULL));

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
