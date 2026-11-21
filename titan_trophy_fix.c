/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * titan_trophy_fix - PS5 payload
 *
 * Makes trophies work for games installed as PKG by TITAN CORE.
 *
 * In PKG mode the game's /app0 is the TITAN title cache
 * (/data/titan-core/caches/<ID>). TITAN only puts part of sce_sys there: no
 * nptitle.dat, trophy2/ or uds/. Without them the game never calls the system
 * trophy and UDS services, so trophies are never registered or unlocked.
 *
 * For every cache with "kind":"pkg" this payload copies, when missing:
 *   /system_data/priv/appmeta/<ID>/nptitle.dat          -> <cache>/sce_sys/nptitle.dat
 *   /system_data/priv/appmeta/<ID>/trophy2/npbind.dat   -> <cache>/sce_sys/trophy2/npbind.dat
 *   /system_data/priv/appmeta/<ID>/uds/npbind.dat       -> <cache>/sce_sys/uds/npbind.dat
 *   CNT entry "trophy2/trophy00.ucp" of the app.pkg     -> <cache>/sce_sys/trophy2/trophy00.ucp
 *   CNT entry "uds/uds00.ucp" of the app.pkg            -> <cache>/sce_sys/uds/uds00.ucp
 * The .ucp entries are stored in plaintext in the PKG; the system already keeps
 * decrypted npbind/nptitle copies in appmeta. No keys are needed.
 *
 * It is safe to run with TITAN open: writing to the cache does not affect it,
 * and a forced cache rebuild in TITAN keeps these files. The payload never
 * writes to /user/app/<ID> (changing that folder while TITAN runs crashes it).
 *
 * Notifications follow the console language (English by default, Portuguese
 * for pt-BR/pt-PT). The log is always in English: /data/titan-core/trophyfix.log
 *
 * Build (ps5-payload-sdk):  make
 * Host test:  gcc -DHOST_TEST -o titan_trophy_fix.exe titan_trophy_fix.c
 *             titan_trophy_fix.exe <simulated_root>     (TTF_LANG=17 for pt-BR)
 */
#ifndef TTF_VERSION
#  define TTF_VERSION "1.0"
#endif
/* readable marker in the binary (e.g. strings titan_trophy_fix_v1.0.elf | grep TTF_VERSION) */
const char ttf_version_tag[] = "TTF_VERSION=" TTF_VERSION;

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef HOST_TEST
#  include <io.h>
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
typedef long long off64;
static FILE *pkg_open(const char *p) { return fopen(p, "rb"); }
static int pkg_read(FILE *f, off64 off, void *buf, size_t n) {
    if (fseeko64(f, off, SEEK_SET) != 0) return -1;
    return fread(buf, 1, n, f) == n ? 0 : -1;
}
static int titan_running(void) { return 0; }
static void notify(const char *msg) { printf("[NOTIFY] %s\n", msg); }
static int system_lang(void) { const char *e = getenv("TTF_LANG"); return e ? atoi(e) : 1; }
#else
#  include <unistd.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  define MKDIR(p) mkdir(p, 0777)
typedef off_t off64;
static FILE *pkg_open(const char *p) { return fopen(p, "rb"); }
static int pkg_read(FILE *f, off64 off, void *buf, size_t n) {
    if (fseeko(f, off, SEEK_SET) != 0) return -1;
    return fread(buf, 1, n, f) == n ? 0 : -1;
}

typedef struct notify_request {
    char useless1[45];
    char message[3075];
} notify_request_t;
int sceKernelSendNotificationRequest(int, notify_request_t *, size_t, int);

static void notify(const char *msg) {
    notify_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.message, msg, sizeof(req.message) - 1);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

/* libSceSystemService: SCE_SYSTEM_SERVICE_PARAM_ID_LANG = 1 */
int sceSystemServiceParamGetInt(int param_id, int *value);
static int system_lang(void) {
    int v = -1;
    if (sceSystemServiceParamGetInt(1, &v) != 0) return -1;
    return v;
}

/* TITAN is open when 127.0.0.1:8097 accepts connections */
static int titan_running(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return 0;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(8097);
    a.sin_addr.s_addr = htonl(0x7f000001);
    int ok = connect(s, (struct sockaddr *)&a, sizeof(a)) == 0;
    close(s);
    return ok;
}
#endif

/* ------------------------------------------------------------------ */
/* Notification texts                                                  */
/* ------------------------------------------------------------------ */
#define LANG_PT_PT 7    /* SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT */
#define LANG_PT_BR 17   /* SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR */

enum { L_EN = 0, L_PT = 1, L_COUNT };
static int g_lang = L_EN;

enum {
    M_NO_CACHE,
    M_FIXED,
    M_FIXED_WITH_ERRORS,
    M_FAILED,
    M_NOTHING,
    M_COUNT
};

static const char *const MSG[M_COUNT][L_COUNT] = {
    [M_NO_CACHE] = {
        "TITAN CORE cache folder not found.",
        "pasta de cache do TITAN CORE não encontrada.",
    },
    [M_FIXED] = {
        "%d game(s) fixed: %s",
        "%d jogo(s) corrigido(s): %s",
    },
    [M_FIXED_WITH_ERRORS] = {
        " (some errors, see the log)",
        " (houve erros, veja o log)",
    },
    [M_FAILED] = {
        "%d game(s) failed. See /data/titan-core/trophyfix.log",
        "%d jogo(s) com erro. Veja /data/titan-core/trophyfix.log",
    },
    [M_NOTHING] = {
        "nothing to fix (%d installed PKG game(s) already OK).",
        "nada a corrigir (%d jogo(s) PKG instalado(s) já ok).",
    },
};

#define TR(id) (MSG[id][g_lang])

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
#define PATHMAX 1024
#define NPBIND_MAGIC 0xD294A018u
#define ENTRY_NAMES 0x200u

static char ROOT[PATHMAX] = "";   /* path prefix, only used by the host test */
static FILE *g_log;

static void logf_(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    if (g_log) {
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fflush(g_log);
    }
}

static void rp(char *out, const char *path) { snprintf(out, PATHMAX, "%s%s", ROOT, path); }

static int file_size(const char *p, long long *sz) {
    struct stat st;
    if (stat(p, &st) != 0 || !S_ISREG(st.st_mode)) return -1;
    if (sz) *sz = (long long)st.st_size;
    return 0;
}

static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static void mkdirs(const char *dir) {
    char tmp[PATHMAX];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
            MKDIR(tmp);
            *p = c;
        }
    }
    MKDIR(tmp);
}

/* writes buf to dst atomically (tmp file + rename) */
static int write_file(const char *dst, const void *buf, size_t n) {
    char dir[PATHMAX], tmp[PATHMAX];
    snprintf(dir, sizeof(dir), "%s", dst);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = 0; mkdirs(dir); }
    snprintf(tmp, sizeof(tmp), "%s.tmp", dst);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(buf, 1, n, f) == n;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return -1; }
    remove(dst);
    if (rename(tmp, dst) != 0) { remove(tmp); return -1; }
    return 0;
}

static unsigned char *read_all(const char *p, size_t *n) {
    long long sz;
    if (file_size(p, &sz) != 0 || sz <= 0 || sz > 64 * 1024 * 1024) return NULL;
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    unsigned char *b = malloc((size_t)sz);
    if (b && fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); b = NULL; }
    fclose(f);
    if (b) *n = (size_t)sz;
    return b;
}

static uint32_t be32(const unsigned char *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* ------------------------------------------------------------------ */
/* Reading entries of the PKG \x7FCNT container                         */
/* ------------------------------------------------------------------ */
typedef struct {
    FILE *f;
    off64 base;              /* file offset of \x7FCNT */
    uint32_t count;
    unsigned char *table;    /* count * 32 bytes */
    char *names;             /* name table (entry 0x200) */
    uint32_t names_len;
} cnt_t;

static int is_cnt(FILE *f, off64 off) {
    unsigned char m[4];
    return pkg_read(f, off, m, 4) == 0 && memcmp(m, "\x7f" "CNT", 4) == 0;
}

static int cnt_open(cnt_t *c, const char *path) {
    memset(c, 0, sizeof(*c));
    c->f = pkg_open(path);
    if (!c->f) return -1;
    if (is_cnt(c->f, 0)) {
        c->base = 0;
    } else {
        /* PS5 PKGs start with \x7FFIH; one of the LE u64 header fields points to the CNT */
        unsigned char hdr[0x1000];
        if (pkg_read(c->f, 0, hdr, sizeof(hdr)) != 0) return -2;
        c->base = -1;
        for (int i = 0; i + 8 <= (int)sizeof(hdr); i += 8) {
            uint64_t v = 0;
            for (int k = 7; k >= 0; k--) v = (v << 8) | hdr[i + k];
            if (v > 0 && (v & 0xF) == 0 && is_cnt(c->f, (off64)v)) { c->base = (off64)v; break; }
        }
        if (c->base < 0) return -3;
    }
    unsigned char h[0x20];
    if (pkg_read(c->f, c->base, h, sizeof(h)) != 0) return -4;
    c->count = be32(h + 0x10);
    uint32_t table = be32(h + 0x18);
    if (c->count == 0 || c->count > 4096) return -5;
    c->table = malloc((size_t)c->count * 32);
    if (!c->table || pkg_read(c->f, c->base + table, c->table, (size_t)c->count * 32) != 0) return -6;
    for (uint32_t i = 0; i < c->count; i++) {
        const unsigned char *e = c->table + i * 32;
        if (be32(e) == ENTRY_NAMES) {
            c->names_len = be32(e + 20);
            if (c->names_len == 0 || c->names_len > 1024 * 1024) return -7;
            c->names = malloc(c->names_len + 1);
            if (!c->names || pkg_read(c->f, c->base + be32(e + 16), c->names, c->names_len) != 0) return -8;
            c->names[c->names_len] = 0;
        }
    }
    return 0;
}

static void cnt_close(cnt_t *c) {
    if (c->f) fclose(c->f);
    free(c->table);
    free(c->names);
    memset(c, 0, sizeof(*c));
}

/* finds an entry by name; returns a pointer to its 32-byte record */
static const unsigned char *cnt_find(cnt_t *c, const char *name) {
    for (uint32_t i = 0; i < c->count; i++) {
        const unsigned char *e = c->table + i * 32;
        uint32_t no = be32(e + 4);
        if (no && c->names && no < c->names_len && strcmp(c->names + no, name) == 0) return e;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Fixing one cache                                                    */
/* ------------------------------------------------------------------ */
enum { R_OK_ALREADY = 0, R_FIXED = 1, R_SKIP = 2, R_FAIL = 3, R_NOT_INSTALLED = 4 };

/* extracts the value of "key":"value" from a simple JSON document */
static int json_str(const char *js, const char *key, char *out, size_t outn) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(js, pat);
    if (!p) return -1;
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < outn) {
        if (*p == '\\' && p[1]) p++;
        out[i++] = *p++;
    }
    out[i] = 0;
    return *p == '"' ? 0 : -1;
}

static int copy_small(const char *src, const char *dst, int (*check)(const unsigned char *, size_t)) {
    size_t n;
    unsigned char *b = read_all(src, &n);
    if (!b) return -1;
    if (check && !check(b, n)) { free(b); return -2; }
    int r = write_file(dst, b, n);
    free(b);
    return r;
}

static int chk_npbind(const unsigned char *b, size_t n) { return n >= 4 && be32(b) == NPBIND_MAGIC; }
static int chk_nptitle(const unsigned char *b, size_t n) { return n >= 28 && memcmp(b, "NPTD", 4) == 0; }

static int extract_entry(cnt_t *c, const char *name, const char *dst) {
    const unsigned char *e = cnt_find(c, name);
    if (!e) return -1;
    if (be32(e + 8) & 0x80000000u) return -2;      /* encrypted: not expected for .ucp */
    uint32_t off = be32(e + 16), sz = be32(e + 20);
    if (sz == 0 || sz > 256u * 1024 * 1024) return -3;
    unsigned char *b = malloc(sz);
    if (!b) return -4;
    int r = pkg_read(c->f, c->base + off, b, sz);
    if (r == 0) r = write_file(dst, b, sz);
    free(b);
    return r;
}

static int fix_title(const char *tid, int *copied) {
    char cache[PATHMAX], p[PATHMAX], src[PATHMAX], dst[PATHMAX];
    *copied = 0;

    snprintf(p, sizeof(p), "/data/titan-core/caches/%s", tid);
    rp(cache, p);

    char done_path[PATHMAX];
    snprintf(done_path, sizeof(done_path), "%s/.done", cache);
    size_t n;
    char *done = (char *)read_all(done_path, &n);
    if (!done) return R_SKIP;                             /* incomplete cache */
    done = realloc(done, n + 1);
    done[n] = 0;
    char kind[32] = "", source[PATHMAX] = "";
    json_str(done, "kind", kind, sizeof(kind));
    json_str(done, "source", source, sizeof(source));
    free(done);
    if (strcmp(kind, "pkg") != 0) return R_SKIP;          /* folder: sce_sys is already complete */

    /* Installed = the system has the title's appmeta AND the source PKG exists.
     * Deleted titles lose /user/app/<ID> and appmeta, but the TITAN cache stays. */
    char appmeta[PATHMAX];
    snprintf(p, sizeof(p), "/system_data/priv/appmeta/%s", tid);
    rp(appmeta, p);
    if (!is_dir(appmeta)) { logf_("  %s: not installed (no appmeta), skipped\n", tid); return R_NOT_INSTALLED; }

    char pkgp[PATHMAX];
    rp(pkgp, source);
    if (!source[0] || file_size(pkgp, NULL) != 0) {
        logf_("  %s: source PKG not accessible (%s), skipped\n", tid, source[0] ? source : "?");
        return R_NOT_INSTALLED;
    }

    char sce[PATHMAX];
    snprintf(sce, sizeof(sce), "%s/sce_sys", cache);
    if (!is_dir(sce)) { logf_("  %s: cache has no sce_sys, skipped\n", tid); return R_SKIP; }

    /* 1) files the system already keeps decrypted */
    static const struct { const char *rel; int (*chk)(const unsigned char *, size_t); } meta[] = {
        { "nptitle.dat", chk_nptitle },
        { "trophy2/npbind.dat", chk_npbind },
        { "uds/npbind.dat", chk_npbind },
    };
    int missing_src = 0, fail = 0;
    for (size_t i = 0; i < sizeof(meta) / sizeof(meta[0]); i++) {
        snprintf(dst, sizeof(dst), "%s/%s", sce, meta[i].rel);
        if (file_size(dst, NULL) == 0) continue;
        snprintf(src, sizeof(src), "%s/%s", appmeta, meta[i].rel);
        if (file_size(src, NULL) != 0) { missing_src++; continue; }
        int r = copy_small(src, dst, meta[i].chk);
        if (r == 0) { (*copied)++; logf_("  %s: copied %s\n", tid, meta[i].rel); }
        else { fail++; logf_("  %s: ERROR copying %s (%d)\n", tid, meta[i].rel, r); }
    }

    /* 2) .ucp files from the PKG CNT container (plaintext) */
    static const char *ucp[] = { "trophy2/trophy00.ucp", "uds/uds00.ucp" };
    int need_ucp = 0;
    for (size_t i = 0; i < 2; i++) {
        snprintf(dst, sizeof(dst), "%s/%s", sce, ucp[i]);
        if (file_size(dst, NULL) != 0) need_ucp = 1;
    }
    if (need_ucp) {
        cnt_t c;
        int r = cnt_open(&c, pkgp);
        if (r != 0) {
            logf_("  %s: could not read PKG %s (%d)\n", tid, source, r);
            fail++;
        } else {
            for (size_t i = 0; i < 2; i++) {
                snprintf(dst, sizeof(dst), "%s/%s", sce, ucp[i]);
                if (file_size(dst, NULL) == 0) continue;
                int er = extract_entry(&c, ucp[i], dst);
                if (er == 0) { (*copied)++; logf_("  %s: extracted %s from the PKG\n", tid, ucp[i]); }
                else if (er == -1) logf_("  %s: the PKG has no %s\n", tid, ucp[i]);
                else { fail++; logf_("  %s: ERROR extracting %s (%d)\n", tid, ucp[i], er); }
            }
        }
        cnt_close(&c);
    }

    if (missing_src)
        logf_("  %s: %d file(s) not yet in /system_data/priv/appmeta/%s "
              "(start the game once and run again)\n", tid, missing_src, tid);
    if (fail) return R_FAIL;
    return *copied ? R_FIXED : R_OK_ALREADY;
}

int main(int argc, char **argv) {
#ifdef HOST_TEST
    if (argc > 1) snprintf(ROOT, sizeof(ROOT), "%s", argv[1]);
#else
    (void)argc; (void)argv;
#endif
    int lang = system_lang();
    g_lang = (lang == LANG_PT_BR || lang == LANG_PT_PT) ? L_PT : L_EN;

    char logp[PATHMAX];
    rp(logp, "/data/titan-core/trophyfix.log");
    g_log = fopen(logp, "a");
    logf_("=== titan_trophy_fix v%s ===\n", TTF_VERSION);
    logf_("System language code: %d (notifications in %s)\n", lang, g_lang == L_PT ? "Portuguese" : "English");

    /* Writing to the cache while TITAN is open was tested (v1.0): TITAN does not
     * react and does not rebuild the cache. We only record its state here.
     * (The payload never writes to /user/app/<ID>, which crashes TITAN if changed.) */
    logf_("TITAN %s\n", titan_running() ? "open" : "closed");

    char caches[PATHMAX];
    rp(caches, "/data/titan-core/caches");
    DIR *d = opendir(caches);
    if (!d) {
        char msg[256];
        logf_("Folder %s not found.\n", caches);
        snprintf(msg, sizeof(msg), "Trophy Fix v" TTF_VERSION ": %s", TR(M_NO_CACHE));
        notify(msg);
        if (g_log) fclose(g_log);
        return 1;
    }

    int fixed = 0, ok = 0, failed = 0, files = 0, gone = 0;
    char fixed_ids[512] = "";
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (strlen(de->d_name) != 9) continue;            /* PPSA12345 */
        int c = 0;
        int r = fix_title(de->d_name, &c);
        files += c;
        if (r == R_FIXED) {
            fixed++;
            if (strlen(fixed_ids) + 12 < sizeof(fixed_ids)) {
                if (fixed_ids[0]) strcat(fixed_ids, ", ");
                strcat(fixed_ids, de->d_name);
            }
        } else if (r == R_OK_ALREADY) ok++;
        else if (r == R_FAIL) failed++;
        else if (r == R_NOT_INSTALLED) gone++;
    }
    closedir(d);
    logf_("Summary: %d fixed (%d file(s) written), %d already OK, %d failed, %d not installed\n",
          fixed, files, ok, failed, gone);

    char body[600], msg[700];
    if (fixed) {
        snprintf(body, sizeof(body), TR(M_FIXED), fixed, fixed_ids);
        if (failed) strncat(body, TR(M_FIXED_WITH_ERRORS), sizeof(body) - strlen(body) - 1);
    } else if (failed) {
        snprintf(body, sizeof(body), TR(M_FAILED), failed);
    } else {
        snprintf(body, sizeof(body), TR(M_NOTHING), ok);
    }
    snprintf(msg, sizeof(msg), "Trophy Fix v" TTF_VERSION ": %s", body);
    logf_("Notification: %s\n", msg);
    notify(msg);
    if (g_log) fclose(g_log);
    return failed ? 2 : 0;
}
