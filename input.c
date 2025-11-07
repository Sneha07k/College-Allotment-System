#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void trim_inplace(char *s) {
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    int l = strlen(s);
    while (l > 0 && isspace((unsigned char)s[l-1])) s[--l] = '\0';
}


CollegeRow* read_college_csv(const char *path, int *out_count) {
    *out_count = 0;
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("read_college_csv: fopen");
        return NULL;
    }
    char line[8192];

    /* read and ignore header */
    if (!fgets(line, sizeof(line), f)) { fclose(f); return NULL; }

    int cap = 256;
    CollegeRow *rows = malloc(sizeof(CollegeRow) * cap);
    if (!rows) { fclose(f); return NULL; }
    int rc = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strlen(line) <= 1) continue;

        char *cells[32]; int cc = 0;
        char *p = strtok(line, ",\n\r");
        while (p && cc < 32) { trim_inplace(p); cells[cc++] = p; p = strtok(NULL, ",\n\r"); }
        CollegeRow r; memset(&r, 0, sizeof(r));
        if (cc > 0) strncpy(r.Institute, cells[0], MAX_STR-1);
        if (cc > 1) strncpy(r.Quota, cells[1], MAX_STR-1);
        if (cc > 2) strncpy(r.Gender, cells[2], MAX_STR-1);
        if (cc > 3) r.Year = atoi(cells[3]);
        if (cc > 4) strncpy(r.Academic, cells[4], MAX_STR-1);
        if (cc > 5) r.Closing_R = atoi(cells[5]);
        if (cc > 6) r.Opening_R = atoi(cells[6]);
        if (cc > 7) strncpy(r.Seat_Type, cells[7], MAX_STR-1);
        if (cc > 8) r.Seats_left = atoi(cells[8]);
        if (rc >= cap) {
            cap *= 2;
            CollegeRow *tmp = realloc(rows, sizeof(CollegeRow) * cap);
            if (!tmp) { free(rows); fclose(f); return NULL; }
            rows = tmp;
        }
        rows[rc++] = r;
    }
    fclose(f);
    *out_count = rc;
    return rows;
}


int write_college_csv(const char *path, CollegeRow *rows, int count) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "Institute,Quota,Gender,Year,Academic_Program_Name,Closing_Rank,Opening_Rank,Seat_Type,Seats_left\n");
    for (int i=0;i<count;i++) {
        fprintf(f, "%s,%s,%s,%d,%s,%d,%d,%s,%d\n",
                rows[i].Institute, rows[i].Quota, rows[i].Gender, rows[i].Year,
                rows[i].Academic, rows[i].Closing_R, rows[i].Opening_R,
                rows[i].Seat_Type, rows[i].Seats_left);
    }
    fclose(f);
    return 1;
}


int append_allocation_txt(const char *path, const char *aadhaar, const char *student_name, const Offer *o) {
    FILE *f = fopen(path, "a");
    if (!f) return 0;
    const char *status = (o->status == OFFER_FROZEN) ? "FROZEN" : "PENDING";
    fprintf(f, "Aadhaar:%s\nStudent:%s\nInstitute:%s\nCourse:%s\nStatus:%s\n---\n",
            aadhaar ? aadhaar : "", student_name ? student_name : "",
            o->row.Institute, o->row.Academic, status);
    fclose(f);
    return 1;
}
