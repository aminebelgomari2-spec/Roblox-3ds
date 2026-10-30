// Roblox 3DS (jeu de fan) - rendu 3D logiciel, sans lib supplementaire
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SW 400
#define SH 240
#define FOCAL 270.0f
#define NEAR_Z 0.2f
#define HALF 32.0f
#define MAXBOX 64
#define MAXITEM 220
#define MAXPROJ 8
#define MAXEXPL 12
#define STEP (1.0f/60.0f)
#define PR 0.5f
#define PH 2.7f
#define PI_F 3.1415927f

typedef struct { float x0,y0,z0,x1,y1,z1; u8 r,g,b,target,alive; float rt; } Box;
typedef struct { float x,y,z,vy,kx,kz,face,walk; int ground; } Player;
typedef struct { float x,y,z,vx,vy,vz,t; int type,active; } Proj;
typedef struct { float x,y,z,t,sz; int active; } Expl;
typedef struct { float x,y,z,hx,hy,hz,yaw,d; u8 r,g,b; } Item;

static u16 *fb;
static u16 skyrow[SH];
static Box B[MAXBOX];
static int nb, map_id, map_ground, ntargets;
static float spawn_x, spawn_z;
static const char *map_names[4] = {"BASEPLATE","OBBY","ARENE","VILLE"};
static const char *hat_names[6] = {"AUCUN","CASQUETTE","HAUT-DE-FORME","COURONNE","CHAPEAU DE FETE","CASQUE VIKING"};
static const char *item_names[3] = {"BOMBE","LANCE-ROQUETTE","EPEE"};
static const u8 PAL[8][3] = {{231,76,60},{243,156,18},{241,196,15},{46,204,113},{26,188,156},{52,152,219},{155,89,182},{236,240,241}};
static int hat = 1, slot = 0, destroyed = 0, state = 0;
static Player P;
static Proj PJ[MAXPROJ];
static Expl EX[MAXEXPL];
static Item items[MAXITEM];
static int nitems;
static float cam_yaw = 0, cam_pit = 0.45f;
static float cam_x, cam_y, cam_z, fwx, fwy, fwz, rtx, rtz, upx, upy, upz;
static float cd = 0, cd_max = 1, swing = 0, walk_amt = 0, in_mx = 0, in_my = 0, acc = 0, menu_t = 0;
static int in_jump = 0, touching = 0, last_tx = 0, last_ty = 0;
static float chx, chy, chz, chf, chs, chc;

static void play_print_dyn(void);

/* ---------- utilitaires ---------- */
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline u16 pack(int r, int g, int b) {
  if (r < 0) r = 0;
  if (r > 255) r = 255;
  if (g < 0) g = 0;
  if (g > 255) g = 255;
  if (b < 0) b = 0;
  if (b > 255) b = 255;
  return (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
static float wrapang(float a) {
  while (a > PI_F) a -= 2 * PI_F;
  while (a < -PI_F) a += 2 * PI_F;
  return a;
}
static inline void px_put(int x, int y, u16 c) {
  if ((unsigned)x < SW && (unsigned)y < SH) fb[x * 240 + 239 - y] = c;
}
static void px_rect(int x, int y, int w, int h, u16 c) {
  for (int i = 0; i < w; i++) {
    int xx = x + i;
    if ((unsigned)xx >= SW) continue;
    for (int j = 0; j < h; j++) {
      int yy = y + j;
      if ((unsigned)yy < SH) fb[xx * 240 + 239 - yy] = c;
    }
  }
}

/* ---------- camera ---------- */
static void cam_set(float tx, float ty, float tz, float dist) {
  float cyw = cosf(cam_yaw), syw = sinf(cam_yaw), cp = cosf(cam_pit), sp = sinf(cam_pit);
  fwx = syw * cp; fwy = -sp; fwz = cyw * cp;
  rtx = cyw; rtz = -syw;
  upx = syw * sp; upy = cp; upz = cyw * sp;
  cam_x = tx - fwx * dist; cam_y = ty - fwy * dist; cam_z = tz - fwz * dist;
  if (map_ground && cam_y < 0.5f) cam_y = 0.5f;
}
static void to_cam(float x, float y, float z, float *o) {
  float dx = x - cam_x, dy = y - cam_y, dz = z - cam_z;
  o[0] = dx * rtx + dz * rtz;
  o[1] = dx * upx + dy * upy + dz * upz;
  o[2] = dx * fwx + dy * fwy + dz * fwz;
}

/* ---------- rasterisation (colonnes, car l'ecran 3DS est tourne) ---------- */
static int col_min[SW], col_max[SW];
static inline void upd(int x, float y) {
  int lo = (int)floorf(y), hi = (int)ceilf(y);
  if (lo < col_min[x]) col_min[x] = lo;
  if (hi > col_max[x]) col_max[x] = hi;
}
static void fill_poly(const float *px, const float *py, int n, u16 col) {
  float minx = px[0], maxx = px[0];
  for (int i = 1; i < n; i++) {
    if (px[i] < minx) minx = px[i];
    if (px[i] > maxx) maxx = px[i];
  }
  if (maxx < 0 || minx > SW - 1) return;
  int x0 = (int)ceilf(minx), x1 = (int)floorf(maxx);
  if (x0 < 0) x0 = 0;
  if (x1 > SW - 1) x1 = SW - 1;
  if (x0 > x1) return;
  for (int x = x0; x <= x1; x++) { col_min[x] = 100000; col_max[x] = -100000; }
  for (int i = 0; i < n; i++) {
    int j = (i + 1 == n) ? 0 : i + 1;
    float ax = px[i], ay = py[i], bx = px[j], by = py[j];
    if (ax > bx) { float t = ax; ax = bx; bx = t; t = ay; ay = by; by = t; }
    int xa = (int)ceilf(ax), xb = (int)floorf(bx);
    if (xa < x0) xa = x0;
    if (xb > x1) xb = x1;
    float dx = bx - ax;
    for (int x = xa; x <= xb; x++) {
      if (dx < 1e-4f) { upd(x, ay); upd(x, by); continue; }
      upd(x, ay + (by - ay) * ((float)x - ax) / dx);
    }
  }
  for (int x = x0; x <= x1; x++) {
    int ys = col_min[x], ye = col_max[x];
    if (ys > ye) continue;
    if (ys < 0) ys = 0;
    if (ye > SH - 1) ye = SH - 1;
    if (ys > ye) continue;
    u16 *p = fb + x * 240 + (239 - ye);
    for (int y = ys; y <= ye; y++) *p++ = col;
  }
}
static int clip_near(float in[][3], int n, float out[][3]) {
  int m = 0;
  for (int i = 0; i < n; i++) {
    float *a = in[i], *b = in[(i + 1) % n];
    int ain = a[2] >= NEAR_Z, bin = b[2] >= NEAR_Z;
    if (ain) { out[m][0] = a[0]; out[m][1] = a[1]; out[m][2] = a[2]; m++; }
    if (ain != bin) {
      float t = (NEAR_Z - a[2]) / (b[2] - a[2]);
      out[m][0] = a[0] + (b[0] - a[0]) * t;
      out[m][1] = a[1] + (b[1] - a[1]) * t;
      out[m][2] = NEAR_Z;
      m++;
    }
  }
  return m;
}

/* ---------- boites orientees (tout est fait de boites) ---------- */
static void draw_item(const Item *it) {
  static const int F[6][4] = {{1,3,7,5},{0,4,6,2},{2,3,7,6},{0,4,5,1},{4,5,7,6},{0,2,3,1}};
  static const float N[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
  float s = sinf(it->yaw), c = cosf(it->yaw);
  float cv[8][3];
  for (int i = 0; i < 8; i++) {
    float lx = (i & 1) ? it->hx : -it->hx;
    float ly = (i & 2) ? it->hy : -it->hy;
    float lz = (i & 4) ? it->hz : -it->hz;
    to_cam(it->x + lx * c + lz * s, it->y + ly, it->z - lx * s + lz * c, cv[i]);
  }
  for (int f = 0; f < 6; f++) {
    float nx = N[f][0], ny = N[f][1], nz = N[f][2];
    float wnx = nx * c + nz * s, wnz = -nx * s + nz * c;
    float fcx = it->x + nx * it->hx * c + nz * it->hz * s;
    float fcy = it->y + ny * it->hy;
    float fcz = it->z - nx * it->hx * s + nz * it->hz * c;
    if ((fcx - cam_x) * wnx + (fcy - cam_y) * ny + (fcz - cam_z) * wnz >= 0.0f) continue;
    float l = wnx * 0.36f + ny * 0.80f + wnz * 0.48f;
    float sh = 0.55f + 0.5f * (l > 0 ? l : 0);
    if (sh > 1.0f) sh = 1.0f;
    float q[4][3], o[8][3];
    for (int k = 0; k < 4; k++) {
      q[k][0] = cv[F[f][k]][0]; q[k][1] = cv[F[f][k]][1]; q[k][2] = cv[F[f][k]][2];
    }
    int m = clip_near(q, 4, o);
    if (m < 3) continue;
    float sx[8], sy[8];
    for (int k = 0; k < m; k++) {
      sx[k] = 200.0f + o[k][0] / o[k][2] * FOCAL;
      sy[k] = 120.0f - o[k][1] / o[k][2] * FOCAL;
    }
    fill_poly(sx, sy, m, pack((int)(it->r * sh), (int)(it->g * sh), (int)(it->b * sh)));
  }
}
static float dist2(float x, float y, float z) {
  float dx = x - cam_x, dy = y - cam_y, dz = z - cam_z;
  return dx * dx + dy * dy + dz * dz;
}
static Item *add_item(float x, float y, float z, float hx, float hy, float hz, float yw, int r, int g, int b) {
  if (nitems >= MAXITEM) return NULL;
  float dz = (x - cam_x) * fwx + (y - cam_y) * fwy + (z - cam_z) * fwz;
  if (dz < -(hx + hy + hz)) return NULL;
  Item *it = &items[nitems++];
  it->x = x; it->y = y; it->z = z; it->hx = hx; it->hy = hy; it->hz = hz; it->yaw = yw;
  it->r = (u8)r; it->g = (u8)g; it->b = (u8)b; it->d = dist2(x, y, z);
  return it;
}
static int cmp_item(const void *a, const void *b) {
  float da = ((const Item *)a)->d, db = ((const Item *)b)->d;
  return (da < db) - (da > db);
}
static void flush_items(void) {
  qsort(items, nitems, sizeof(Item), cmp_item);
  for (int i = 0; i < nitems; i++) draw_item(&items[i]);
}

/* ---------- personnage ---------- */
static void part(float ox, float oy, float oz, float hx, float hy, float hz, int r, int g, int b) {
  add_item(chx + ox * chc + oz * chs, chy + oy, chz - ox * chs + oz * chc, hx, hy, hz, chf, r, g, b);
}
static void add_hat(int h) {
  if (h == 1) {
    part(0, 2.82f, 0, 0.39f, 0.12f, 0.39f, 200, 30, 30);
    part(0, 2.74f, 0.45f, 0.3f, 0.03f, 0.2f, 150, 20, 20);
  } else if (h == 2) {
    part(0, 2.74f, 0, 0.6f, 0.04f, 0.6f, 25, 25, 25);
    part(0, 3.1f, 0, 0.34f, 0.36f, 0.34f, 35, 35, 35);
    part(0, 2.9f, 0, 0.35f, 0.06f, 0.35f, 200, 40, 40);
  } else if (h == 3) {
    part(0, 2.85f, 0, 0.38f, 0.15f, 0.38f, 255, 200, 20);
    part(0.3f, 3.15f, 0.3f, 0.07f, 0.15f, 0.07f, 255, 200, 20);
    part(-0.3f, 3.15f, 0.3f, 0.07f, 0.15f, 0.07f, 255, 200, 20);
    part(0.3f, 3.15f, -0.3f, 0.07f, 0.15f, 0.07f, 255, 200, 20);
    part(-0.3f, 3.15f, -0.3f, 0.07f, 0.15f, 0.07f, 255, 200, 20);
  } else if (h == 4) {
    for (int i = 0; i < 4; i++) {
      float w = 0.34f - 0.08f * i;
      if (i & 1) part(0, 2.82f + 0.24f * i, 0, w, 0.12f, w, 250, 220, 40);
      else part(0, 2.82f + 0.24f * i, 0, w, 0.12f, w, 240, 60, 160);
    }
  } else if (h == 5) {
    part(0, 2.86f, 0, 0.4f, 0.16f, 0.4f, 150, 150, 155);
    part(0.48f, 3.05f, 0, 0.08f, 0.22f, 0.08f, 240, 235, 220);
    part(-0.48f, 3.05f, 0, 0.08f, 0.22f, 0.08f, 240, 235, 220);
    part(0.48f, 3.3f, 0, 0.05f, 0.05f, 0.05f, 240, 235, 220);
    part(-0.48f, 3.3f, 0, 0.05f, 0.05f, 0.05f, 240, 235, 220);
  }
}
static void add_character(int hatid, float wph, float amt, int holding) {
  chs = sinf(chf); chc = cosf(chf);
  float lg = sinf(wph) * 0.4f * amt;
  part(-0.25f, 0.5f, lg, 0.24f, 0.5f, 0.25f, 75, 151, 75);
  part(0.25f, 0.5f, -lg, 0.24f, 0.5f, 0.25f, 75, 151, 75);
  part(0, 1.5f, 0, 0.5f, 0.5f, 0.25f, 13, 105, 172);
  part(0, 2.35f, 0, 0.35f, 0.35f, 0.35f, 245, 205, 48);
  part(-0.12f, 2.42f, 0.36f, 0.05f, 0.05f, 0.01f, 30, 30, 30);
  part(0.12f, 2.42f, 0.36f, 0.05f, 0.05f, 0.01f, 30, 30, 30);
  part(0, 2.25f, 0.36f, 0.14f, 0.025f, 0.01f, 30, 30, 30);
  part(-0.7f, 1.5f, -lg, 0.2f, 0.5f, 0.2f, 245, 205, 48);
  if (holding) part(0.7f, 1.7f, 0.5f, 0.2f, 0.2f, 0.55f, 245, 205, 48);
  else part(0.7f, 1.5f, lg, 0.2f, 0.5f, 0.2f, 245, 205, 48);
  add_hat(hatid);
}
static void add_held(int sl) {
  if (sl == 0) {
    part(0.7f, 1.95f, 1.2f, 0.2f, 0.2f, 0.2f, 25, 25, 25);
    part(0.7f, 2.25f, 1.2f, 0.04f, 0.1f, 0.04f, 255, 140, 20);
  } else if (sl == 1) {
    part(0.7f, 2.0f, 1.05f, 0.17f, 0.17f, 0.75f, 90, 120, 70);
    part(0.7f, 2.0f, 1.85f, 0.2f, 0.2f, 0.08f, 230, 120, 30);
    part(0.7f, 1.82f, 0.7f, 0.06f, 0.12f, 0.06f, 60, 60, 60);
  } else {
    part(0.7f, 1.9f, 1.9f, 0.05f, 0.16f, 0.9f, 210, 215, 225);
    part(0.7f, 1.9f, 1.05f, 0.3f, 0.05f, 0.07f, 120, 70, 30);
  }
}

/* ---------- fond : ciel + sol a damier ---------- */
static void draw_background(int ground) {
  for (int px = 0; px < SW; px += 2) {
    float a = ((float)px + 1.0f - 200.0f) / FOCAL;
    float bx = fwx + a * rtx, bz = fwz + a * rtz;
    u16 *col0 = fb + px * 240;
    for (int py = 0; py < SH; py += 2) {
      float b = -((float)py + 1.0f - 120.0f) / FOCAL;
      float dx = bx + b * upx, dy = fwy + b * upy, dz = bz + b * upz;
      u16 c = skyrow[py];
      if (ground && dy < -0.0005f) {
        float t = -cam_y / dy;
        float hx = cam_x + dx * t, hz = cam_z + dz * t;
        if (fabsf(hx) < HALF && fabsf(hz) < HALF) {
          int ck = (((int)floorf(hx * 0.25f)) + ((int)floorf(hz * 0.25f))) & 1;
          int r = ck ? 92 : 76, g = ck ? 168 : 146, bl = ck ? 66 : 56;
          if (t > 35.0f) {
            float f = clampf((t - 35.0f) / 60.0f, 0, 1);
            r += (int)((170 - r) * f); g += (int)((215 - g) * f); bl += (int)((250 - bl) * f);
          }
          c = pack(r, g, bl);
        }
      }
      u16 *p = col0 + (239 - py);
      p[0] = c; p[-1] = c; p[240] = c; p[239] = c;
    }
  }
}

/* ---------- construction des maps ---------- */
static void addb(float x0, float y0, float z0, float x1, float y1, float z1, int r, int g, int b, int tg) {
  if (nb >= MAXBOX) return;
  Box *q = &B[nb++];
  q->x0 = x0; q->y0 = y0; q->z0 = z0; q->x1 = x1; q->y1 = y1; q->z1 = z1;
  q->r = (u8)r; q->g = (u8)g; q->b = (u8)b; q->target = (u8)tg; q->alive = 1; q->rt = 0;
  if (tg) ntargets++;
}
static void addp(float x0, float y0, float z0, float x1, float y1, float z1, int idx) {
  const u8 *c = PAL[idx & 7];
  addb(x0, y0, z0, x1, y1, z1, c[0], c[1], c[2], 0);
}
static void addt(float cx, float y0, float cz) {
  addb(cx - 0.7f, y0, cz - 0.7f, cx + 0.7f, y0 + 1.4f, cz + 0.7f, 235, 60, 45, 1);
}
static void build_map(int id) {
  nb = 0; ntargets = 0; spawn_x = 0; spawn_z = 0; map_ground = 1;
  if (id == 0) {
    for (int i = 0; i < 5; i++) addp(6 + i, (float)i, 6 + i, 18 - i, (float)i + 1, 18 - i, i + 1);
    addp(-10, 0, 8, -8, 6, 10, 5);
    addp(-6, 0, 3, -4, 1.5f, 5, 0);
    addp(-6, 0, 6, -4, 3, 8, 1);
    addp(-6, 0, 9, -4, 4.5f, 11, 2);
    addp(-22, 0, -4, -18, 3, 4, 6);
    addp(14, 0, -10, 20, 2, -6, 4);
    addt(-14, 0, 14); addt(0, 0, 20); addt(20, 0, -2); addt(-16, 0, -14);
    addt(10, 0, -14); addt(12, 5, 12); addt(-9, 6, 9);
  } else if (id == 1) {
    map_ground = 0;
    addp(-4, -1, -4, 4, 0, 4, 7);
    addp(-1.5f, -1, 7, 1.5f, 0.5f, 10, 0);
    addp(3, -1, 12, 6, 1, 15, 1);
    addp(8, -1, 14, 11, 2, 17, 2);
    addp(13, -1, 14, 16, 3, 17, 3);
    addp(18, -1, 14, 21, 3, 17, 4);
    addp(19, -1, 20, 22, 4, 23, 5);
    addp(19, -1, 25, 22, 5, 28, 6);
    addp(13, -1, 31, 25, 5, 41, 7);
    addt(4.5f, 1, 13.5f); addt(9.5f, 2, 15.5f); addt(15.2f, 3, 16.2f);
    addt(16, 5, 34); addt(22, 5, 34); addt(19, 5, 39);
  } else if (id == 2) {
    spawn_z = -17;
    for (int i = 0; i < 4; i++) {
      float x0 = -22.0f + i * 11.0f;
      addb(x0, 0, -23, x0 + 11, 4, -22, 120, 120, 130, 0);
      addb(x0, 0, 22, x0 + 11, 4, 23, 120, 120, 130, 0);
      addb(-23, 0, x0, -22, 4, x0 + 11, 120, 120, 130, 0);
      addb(22, 0, x0, 23, 4, x0 + 11, 120, 120, 130, 0);
    }
    for (int i = 0; i < 3; i++)
      addp(-7.0f + 2 * i, 1.2f * i, -7.0f + 2 * i, 7.0f - 2 * i, 1.2f * (i + 1), 7.0f - 2 * i, i + 5);
    addt(0, 3.6f, 0);
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2) {
        float cx = 12.0f * sx, cz = 12.0f * sz;
        addp(cx - 1, 0, cz - 1, cx + 1, 5, cz + 1, 2);
        addt(cx, 5, cz);
      }
    for (int k = 0; k < 8; k++) {
      float a = (k * 45.0f + 22.5f) * PI_F / 180.0f;
      addt(16.0f * cosf(a), 0, 16.0f * sinf(a));
    }
  } else {
    for (int gx = -2; gx <= 2; gx++)
      for (int gz = -2; gz <= 2; gz++) {
        if (gx == 0 && gz == 0) continue;
        int h = 4 + (((gx + 3) * 5 + (gz + 3) * 3) % 5) * 2;
        float cx = gx * 12.0f, cz = gz * 12.0f;
        addp(cx - 3.5f, 0, cz - 3.5f, cx + 3.5f, (float)h, cz + 3.5f, gx + gz + 4);
        if (((gx + gz) & 1) == 0) addt(cx, (float)h, cz);
      }
    addt(6, 0, 0); addt(-6, 0, 0); addt(0, 0, 6); addt(0, 0, -6);
  }
}

/* ---------- console du bas ---------- */
static void btn(int row, const char *txt) {
  char line[64];
  printf("\x1b[%d;1H+------------------------------------+", row + 1);
  snprintf(line, sizeof line, "| %-34s |", txt);
  printf("\x1b[%d;1H%s", row + 2, line);
  printf("\x1b[%d;1H+------------------------------------+", row + 3);
}
static void menu_print(void) {
  char t[64];
  consoleClear();
  printf("\x1b[1;1H        ROBLOX 3DS  -  fan game");
  snprintf(t, sizeof t, "CHAPEAU: %s", hat_names[hat]);
  btn(2, t);
  printf("\x1b[6;1H   (touche le bouton ou gauche/droite)");
  printf("\x1b[7;1H          CHOISIS UNE MAP :");
  btn(8, "1. BASEPLATE  (simple, pyramide)");
  btn(12, "2. OBBY  (parcours dans le ciel)");
  btn(16, "3. ARENE  (murs et piliers)");
  btn(20, "4. VILLE  (immeubles)");
  printf("\x1b[27;1H            START = quitter");
}
static void play_print_dyn(void) {
  printf("\x1b[14;1H OBJET : %-20s", item_names[slot]);
  printf("\x1b[15;1H SCORE : %-8d", destroyed);
}
static void play_print_static(void) {
  consoleClear();
  printf("\x1b[1;1H MAP : %s", map_names[map_id]);
  printf("\x1b[3;1H Glisse le doigt sur cet ecran");
  printf("\x1b[4;1H pour tourner la camera");
  printf("\x1b[6;1H Rond = bouger      B = sauter");
  printf("\x1b[7;1H A = utiliser l'objet");
  printf("\x1b[8;1H Gauche/Droite = changer d'objet");
  printf("\x1b[9;1H L / R = tourner la camera");
  printf("\x1b[10;1H START = retour au menu");
  play_print_dyn();
}

/* ---------- gameplay ---------- */
static void spawn_expl(float x, float y, float z, float sz) {
  for (int i = 0; i < MAXEXPL; i++)
    if (!EX[i].active) {
      EX[i].x = x; EX[i].y = y; EX[i].z = z; EX[i].t = 0; EX[i].sz = sz; EX[i].active = 1;
      return;
    }
}
static void kill_target(Box *b) {
  b->alive = 0; b->rt = 10.0f; destroyed++;
  spawn_expl((b->x0 + b->x1) * 0.5f, (b->y0 + b->y1) * 0.5f, (b->z0 + b->z1) * 0.5f, 0.8f);
  play_print_dyn();
}
static void explode(float x, float y, float z, float R) {
  spawn_expl(x, y, z, R * 0.5f);
  for (int i = 0; i < nb; i++) {
    Box *b = &B[i];
    if (!b->alive || !b->target) continue;
    float qx = clampf(x, b->x0, b->x1) - x, qy = clampf(y, b->y0, b->y1) - y, qz = clampf(z, b->z0, b->z1) - z;
    if (qx * qx + qy * qy + qz * qz <= R * R) kill_target(b);
  }
  float dx = P.x - x, dy = P.y + 1.3f - y, dz = P.z - z;
  float d = sqrtf(dx * dx + dy * dy + dz * dz);
  if (d < R + 1.0f) {
    if (d < 0.4f) d = 0.4f;
    float s = 1.0f - d / (R + 1.0f);
    P.kx = clampf(P.kx + dx / d * 24.0f * s, -26, 26);
    P.kz = clampf(P.kz + dz / d * 24.0f * s, -26, 26);
    P.vy += dy / d * 14.0f * s + 7.0f * s;
    if (P.vy > 17.0f) P.vy = 17.0f;
    P.ground = 0;
  }
}
static int pt_in_box(float x, float y, float z, float m) {
  for (int i = 0; i < nb; i++) {
    const Box *b = &B[i];
    if (!b->alive) continue;
    if (x > b->x0 - m && x < b->x1 + m && y > b->y0 - m && y < b->y1 + m && z > b->z0 - m && z < b->z1 + m) return 1;
  }
  return 0;
}
static int pl_hit(float x, float y, float z) {
  for (int i = 0; i < nb; i++) {
    const Box *b = &B[i];
    if (!b->alive) continue;
    if (x + PR > b->x0 && x - PR < b->x1 && z + PR > b->z0 && z - PR < b->z1 &&
        y < b->y1 - 0.001f && y + PH > b->y0 + 0.001f) return 1;
  }
  return 0;
}
static void respawn(void) {
  P.x = spawn_x; P.y = 0; P.z = spawn_z; P.vy = 0; P.kx = 0; P.kz = 0; P.ground = 1;
}
static void game_step(float dt) {
  float s = sinf(cam_yaw), c = cosf(cam_yaw);
  float dxw = in_my * s + in_mx * c, dzw = in_my * c - in_mx * s;
  float len = sqrtf(in_mx * in_mx + in_my * in_my);
  float vx = dxw * 6.5f + P.kx, vz = dzw * 6.5f + P.kz;
  float target_amt = 0;
  if (len > 0.1f) {
    float tf = atan2f(dxw, dzw);
    P.face += wrapang(tf - P.face) * clampf(14.0f * dt, 0, 1);
    P.walk += dt * 10.0f;
    if (P.ground) target_amt = 1;
  }
  walk_amt += (target_amt - walk_amt) * clampf(10.0f * dt, 0, 1);
  P.kx *= 1.0f - clampf(3.5f * dt, 0, 1);
  P.kz *= 1.0f - clampf(3.5f * dt, 0, 1);
  float nx = P.x + vx * dt;
  if (!pl_hit(nx, P.y, P.z)) P.x = nx;
  float nz = P.z + vz * dt;
  if (!pl_hit(P.x, P.y, nz)) P.z = nz;
  if (map_ground) {
    P.x = clampf(P.x, -HALF + 0.6f, HALF - 0.6f);
    P.z = clampf(P.z, -HALF + 0.6f, HALF - 0.6f);
  }
  if (in_jump && P.ground) P.vy = 11.5f;
  P.vy -= 30.0f * dt;
  if (P.vy < -32.0f) P.vy = -32.0f;
  float py0 = P.y;
  P.y += P.vy * dt;
  P.ground = 0;
  if (map_ground && P.y <= 0) { P.y = 0; if (P.vy < 0) P.vy = 0; P.ground = 1; }
  for (int i = 0; i < nb; i++) {
    const Box *b = &B[i];
    if (!b->alive) continue;
    if (P.x + PR > b->x0 && P.x - PR < b->x1 && P.z + PR > b->z0 && P.z - PR < b->z1 &&
        P.y < b->y1 - 0.001f && P.y + PH > b->y0 + 0.001f) {
      if (P.vy <= 0 && py0 >= b->y1 - 0.05f) { P.y = b->y1; P.vy = 0; P.ground = 1; }
      else if (P.vy > 0 && py0 + PH <= b->y0 + 0.05f) { P.y = b->y0 - PH; P.vy = 0; }
    }
  }
  if (P.y < -25.0f) respawn();
  /* cibles qui reapparaissent */
  for (int i = 0; i < nb; i++) {
    Box *b = &B[i];
    if (b->alive || !b->target) continue;
    b->rt -= dt;
    if (b->rt <= 0) {
      int ov = P.x + PR > b->x0 && P.x - PR < b->x1 && P.z + PR > b->z0 && P.z - PR < b->z1 &&
               P.y < b->y1 && P.y + PH > b->y0;
      if (!ov) b->alive = 1;
    }
  }
  /* projectiles */
  for (int i = 0; i < MAXPROJ; i++) {
    Proj *q = &PJ[i];
    if (!q->active) continue;
    q->t += dt;
    if (q->type == 0) {
      float ox = q->x, oy = q->y, oz = q->z;
      q->vy -= 25.0f * dt;
      q->x += q->vx * dt; q->y += q->vy * dt; q->z += q->vz * dt;
      if (map_ground && q->y < 0.25f) {
        q->y = 0.25f; q->vy = -q->vy * 0.35f; q->vx *= 0.85f; q->vz *= 0.85f;
        if (fabsf(q->vy) < 1.0f) q->vy = 0;
      }
      if (pt_in_box(q->x, q->y, q->z, 0.25f)) {
        q->x = ox; q->y = oy; q->z = oz;
        q->vx *= -0.35f; q->vz *= -0.35f; q->vy *= -0.35f;
        if (fabsf(q->vy) < 1.0f) q->vy = 0;
      }
      if (q->t >= 2.0f) { explode(q->x, q->y, q->z, 5.5f); q->active = 0; }
      else if (q->y < -30.0f) q->active = 0;
    } else {
      q->x += q->vx * dt; q->y += q->vy * dt; q->z += q->vz * dt;
      int hit = (map_ground && q->y < 0.1f) || pt_in_box(q->x, q->y, q->z, 0.15f) || q->t > 3.0f ||
                fabsf(q->x) > 80 || fabsf(q->z) > 80;
      if (hit) { explode(q->x, q->y, q->z, 5.0f); q->active = 0; }
      else if (q->y < -30.0f) q->active = 0;
    }
  }
}
static Proj *find_proj(void) {
  for (int i = 0; i < MAXPROJ; i++) if (!PJ[i].active) return &PJ[i];
  return NULL;
}
static void use_item(void) {
  if (cd > 0) return;
  float s = sinf(cam_yaw), c = cosf(cam_yaw);
  P.face = cam_yaw;
  if (slot == 0) {
    Proj *q = find_proj();
    if (!q) return;
    q->active = 1; q->type = 0; q->t = 0;
    q->x = P.x + s * 0.9f; q->y = P.y + 2.0f; q->z = P.z + c * 0.9f;
    q->vx = s * 9.0f + P.kx; q->vz = c * 9.0f + P.kz;
    q->vy = clampf(6.0f + fwy * 8.0f, 1.5f, 12.0f);
    cd = cd_max = 0.7f;
  } else if (slot == 1) {
    Proj *q = find_proj();
    if (!q) return;
    float mx = P.x + s * 1.2f, my = P.y + 1.9f, mz = P.z + c * 1.2f;
    float dx = cam_x + fwx * 40.0f - mx, dy = cam_y + fwy * 40.0f - my, dz = cam_z + fwz * 40.0f - mz;
    float l = sqrtf(dx * dx + dy * dy + dz * dz);
    if (l < 0.01f) l = 1.0f;
    q->active = 1; q->type = 1; q->t = 0;
    q->x = mx; q->y = my; q->z = mz;
    q->vx = dx / l * 28.0f; q->vy = dy / l * 28.0f; q->vz = dz / l * 28.0f;
    cd = cd_max = 1.0f;
  } else {
    swing = 0.3f; cd = cd_max = 0.45f;
    for (int i = 0; i < nb; i++) {
      Box *b = &B[i];
      if (!b->alive || !b->target) continue;
      float dx = (b->x0 + b->x1) * 0.5f - P.x, dz = (b->z0 + b->z1) * 0.5f - P.z;
      float dy = (b->y0 + b->y1) * 0.5f - (P.y + 1.3f);
      float d = sqrtf(dx * dx + dz * dz);
      if (d - 0.7f < 3.2f && fabsf(dy) < 2.2f && (d < 0.5f || (dx * s + dz * c) / d > 0.2f)) kill_target(b);
    }
  }
}

/* ---------- dessin du monde et HUD ---------- */
static void draw_world(void) {
  nitems = 0;
  for (int i = 0; i < nb; i++) {
    const Box *b = &B[i];
    if (!b->alive) continue;
    float cx = (b->x0 + b->x1) * 0.5f, cy = (b->y0 + b->y1) * 0.5f, cz = (b->z0 + b->z1) * 0.5f;
    add_item(cx, cy, cz, (b->x1 - b->x0) * 0.5f, (b->y1 - b->y0) * 0.5f, (b->z1 - b->z0) * 0.5f, 0, b->r, b->g, b->b);
    if (b->target) add_item(cx, b->y1 + 0.25f, cz, 0.25f, 0.25f, 0.25f, 0, 255, 220, 40);
  }
  /* ombre sous le joueur */
  float sy = -1e9f, sd = 1e12f;
  int found = 0;
  if (map_ground) { sy = 0; found = 1; }
  for (int i = 0; i < nb; i++) {
    const Box *b = &B[i];
    if (!b->alive) continue;
    if (P.x > b->x0 - PR && P.x < b->x1 + PR && P.z > b->z0 - PR && P.z < b->z1 + PR &&
        b->y1 <= P.y + 0.05f && b->y1 > sy) {
      sy = b->y1; found = 1;
      sd = dist2((b->x0 + b->x1) * 0.5f, (b->y0 + b->y1) * 0.5f, (b->z0 + b->z1) * 0.5f) * 0.999f;
    }
  }
  if (found && P.y - sy < 40.0f) {
    Item *it = add_item(P.x, sy + 0.02f, P.z, 0.55f, 0.01f, 0.55f, 0, 35, 55, 35);
    if (it) { if (sy <= 0.0001f && map_ground) it->d = 1e12f; else it->d = sd; }
  }
  chx = P.x; chy = P.y; chz = P.z; chf = P.face;
  add_character(hat, P.walk, walk_amt, 1);
  if (swing > 0) {
    float ph = 1.0f - swing / 0.3f;
    float a = chf + (-1.3f + 2.6f * ph);
    float sa = sinf(a), ca = cosf(a);
    add_item(chx + sa * 1.5f, chy + 1.8f, chz + ca * 1.5f, 0.06f, 0.16f, 0.9f, a, 235, 235, 245);
    add_item(chx + sa * 0.65f, chy + 1.8f, chz + ca * 0.65f, 0.3f, 0.05f, 0.07f, a, 120, 70, 30);
  } else add_held(slot);
  for (int i = 0; i < MAXPROJ; i++) {
    const Proj *q = &PJ[i];
    if (!q->active) continue;
    if (q->type == 0) {
      int blink = ((int)(q->t * (6.0f + q->t * 8.0f))) & 1;
      if (blink && q->t > 0.8f) add_item(q->x, q->y, q->z, 0.25f, 0.25f, 0.25f, 0, 230, 40, 30);
      else add_item(q->x, q->y, q->z, 0.25f, 0.25f, 0.25f, 0, 25, 25, 25);
      add_item(q->x, q->y + 0.3f, q->z, 0.04f, 0.08f, 0.04f, 0, 255, 140, 20);
    } else {
      float yw = atan2f(q->vx, q->vz);
      add_item(q->x, q->y, q->z, 0.15f, 0.15f, 0.45f, yw, 90, 100, 90);
      add_item(q->x - q->vx / 28.0f * 0.6f, q->y - q->vy / 28.0f * 0.6f, q->z - q->vz / 28.0f * 0.6f,
               0.1f, 0.1f, 0.25f, yw, 255, 160, 40);
    }
  }
  for (int i = 0; i < MAXEXPL; i++) {
    const Expl *e = &EX[i];
    if (!e->active) continue;
    float k = e->t / 0.45f;
    float s = e->sz * (0.4f + 1.3f * k);
    add_item(e->x, e->y, e->z, s, s, s, 0, 255, (int)(220 - 170 * k), (int)(60 * (1 - k)));
    add_item(e->x, e->y, e->z, s * 0.85f, s * 0.85f, s * 0.85f, 0.785f, 255, (int)(160 - 110 * k), 30);
  }
  flush_items();
}
static void draw_icon(int i, int x, int y) {
  if (i == 0) {
    u16 blk = pack(20, 20, 20);
    for (int dy = -9; dy <= 9; dy++)
      for (int dx = -9; dx <= 9; dx++)
        if (dx * dx + dy * dy <= 81) px_put(x + 18 + dx, y + 21 + dy, blk);
    px_rect(x + 12, y + 16, 4, 4, pack(110, 110, 110));
    px_rect(x + 21, y + 7, 3, 5, pack(200, 160, 80));
    px_rect(x + 22, y + 4, 3, 3, pack(255, 120, 20));
  } else if (i == 1) {
    px_rect(x + 4, y + 14, 24, 8, pack(90, 120, 70));
    px_rect(x + 28, y + 15, 4, 6, pack(160, 160, 160));
    px_rect(x + 2, y + 11, 4, 14, pack(230, 120, 30));
    px_rect(x + 10, y + 22, 5, 7, pack(70, 70, 70));
  } else {
    u16 sil = pack(220, 225, 235), br = pack(130, 80, 35);
    for (int t = 0; t < 21; t++) px_rect(x + 10 + t, y + 24 - t, 3, 3, sil);
    for (int t = -4; t <= 4; t++) px_rect(x + 9 + t, y + 25 + t, 3, 3, br);
    for (int t = 1; t <= 5; t++) px_rect(x + 9 - t, y + 25 + t, 3, 3, br);
  }
}
static void draw_hud(void) {
  u16 white = pack(255, 255, 255);
  px_rect(190, 119, 21, 3, white);
  px_rect(199, 110, 3, 21, white);
  for (int i = 0; i < 3; i++) {
    int x = 140 + i * 42, y = 194;
    if (i == slot) px_rect(x - 3, y - 3, 42, 42, pack(255, 230, 60));
    else px_rect(x - 1, y - 1, 38, 38, pack(0, 0, 0));
    px_rect(x, y, 36, 36, i == slot ? pack(70, 70, 80) : pack(40, 40, 45));
    draw_icon(i, x, y);
  }
  if (cd > 0) px_rect(137, 186, (int)(126.0f * cd / cd_max), 4, pack(230, 40, 40));
  int k = 0;
  for (int i = 0; i < nb; i++)
    if (B[i].target) {
      px_rect(8 + k * 12, 8, 9, 9, B[i].alive ? pack(235, 60, 45) : pack(120, 120, 120));
      k++;
    }
}
static void draw_logo(void) {
  static const u8 GL[5][7] = {
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}};
  static const int word[6] = {0, 1, 2, 3, 1, 4};
  px_rect(0, 0, 400, 52, pack(205, 30, 30));
  px_rect(0, 52, 400, 4, pack(120, 15, 15));
  for (int pass = 0; pass < 2; pass++) {
    u16 col = pass == 0 ? pack(110, 10, 10) : pack(255, 255, 255);
    int off = pass == 0 ? 2 : 0;
    for (int li = 0; li < 6; li++)
      for (int r = 0; r < 7; r++)
        for (int c = 0; c < 5; c++)
          if (GL[word[li]][r] & (0x10 >> c)) px_rect(112 + li * 30 + c * 5 + off, 9 + r * 5 + off, 5, 5, col);
  }
}

/* ---------- etats : menu / jeu ---------- */
static void enter_menu(void) {
  state = 0; map_ground = 1; menu_print();
}
static void start_map(int id) {
  map_id = id; destroyed = 0;
  build_map(id);
  memset(PJ, 0, sizeof PJ); memset(EX, 0, sizeof EX);
  P.face = 0; P.walk = 0; respawn();
  cam_yaw = 0; cam_pit = 0.45f; slot = 0; cd = 0; swing = 0; acc = 0; touching = 0; walk_amt = 0;
  state = 1;
  play_print_static();
}
static int menu_frame(u32 kd, float dt) {
  menu_t += dt;
  if (kd & KEY_DRIGHT) { hat = (hat + 1) % 6; menu_print(); }
  if (kd & KEY_DLEFT) { hat = (hat + 5) % 6; menu_print(); }
  if (kd & KEY_TOUCH) {
    touchPosition t;
    hidTouchRead(&t);
    int r = t.py / 8;
    if (r >= 2 && r <= 5) { hat = (hat + 1) % 6; menu_print(); }
    for (int i = 0; i < 4; i++)
      if (r >= 8 + 4 * i && r <= 10 + 4 * i) { start_map(i); return 0; }
  }
  if (kd & KEY_START) return 1;
  cam_yaw = 0; cam_pit = 0.12f; map_ground = 1;
  cam_set(0, 1.5f, 0, 6.5f);
  draw_background(1);
  nitems = 0;
  chx = 0; chy = 0; chz = 0; chf = menu_t * 1.3f;
  add_character(hat, 0, 0, 0);
  flush_items();
  draw_logo();
  return 0;
}
static void play_frame(u32 kd, u32 kh, float dt) {
  circlePosition cp;
  hidCircleRead(&cp);
  float mx = cp.dx / 140.0f, my = cp.dy / 140.0f;
  float len = sqrtf(mx * mx + my * my);
  if (len < 0.15f) { mx = 0; my = 0; }
  else if (len > 1.0f) { mx /= len; my /= len; }
  in_mx = mx; in_my = my; in_jump = (kh & KEY_B) ? 1 : 0;
  if (kd & KEY_DRIGHT) { slot = (slot + 1) % 3; play_print_dyn(); }
  if (kd & KEY_DLEFT) { slot = (slot + 2) % 3; play_print_dyn(); }
  if (kd & KEY_A) use_item();
  if (kd & KEY_START) { enter_menu(); return; }
  if (kh & KEY_TOUCH) {
    touchPosition t;
    hidTouchRead(&t);
    if (touching) {
      cam_yaw -= (float)(t.px - last_tx) * 0.010f;
      cam_pit = clampf(cam_pit - (float)(t.py - last_ty) * 0.010f, -0.3f, 1.3f);
    }
    last_tx = t.px; last_ty = t.py; touching = 1;
  } else touching = 0;
  if (kh & KEY_L) cam_yaw -= 2.0f * dt;
  if (kh & KEY_R) cam_yaw += 2.0f * dt;
  cd -= dt; if (cd < 0) cd = 0;
  if (swing > 0) swing -= dt;
  for (int i = 0; i < MAXEXPL; i++)
    if (EX[i].active) { EX[i].t += dt; if (EX[i].t > 0.45f) EX[i].active = 0; }
  acc += dt;
  int n = 0;
  while (acc >= STEP && n < 4) { game_step(STEP); acc -= STEP; n++; }
  if (acc > STEP) acc = 0;
  cam_set(P.x, P.y + 2.0f, P.z, 7.0f);
  draw_background(map_ground);
  draw_world();
  draw_hud();
}

int main(void) {
  gfxInitDefault();
  gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
  gfxSetDoubleBuffering(GFX_TOP, true);
  consoleInit(GFX_BOTTOM, NULL);
  for (int y = 0; y < SH; y++) skyrow[y] = pack(70 + 115 * y / SH, 130 + 95 * y / SH, 235 + 20 * y / SH);
  memset(&P, 0, sizeof P);
  enter_menu();
  u64 last = osGetTime();
  while (aptMainLoop()) {
    hidScanInput();
    u32 kd = hidKeysDown(), kh = hidKeysHeld();
    u64 now = osGetTime();
    float dt = (float)(now - last) / 1000.0f;
    last = now;
    if (dt > 0.1f) dt = 0.1f;
    fb = (u16 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    if (state == 0) { if (menu_frame(kd, dt)) break; }
    else play_frame(kd, kh, dt);
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
  }
  gfxExit();
  return 0;
}
