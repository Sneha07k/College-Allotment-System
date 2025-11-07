#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Globals */
CollegeRow *college_data = NULL;
int college_count = 0;
StudentHeap student_heap;
char current_user_aadhaar[MAX_STR] = "";

/* ---------- HEAP (static array min-heap on jee_rank) ---------- */

static void swap_items(HeapItem *a, HeapItem *b) {
    HeapItem tmp = *a;
    *a = *b;
    *b = tmp;
}

/* heapify up from index i */
static void heapify_up(int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (student_heap.items[i].student.jee_rank < student_heap.items[parent].student.jee_rank) {
            swap_items(&student_heap.items[i], &student_heap.items[parent]);
            i = parent;
        } else break;
    }
}

/* heapify down from index i */
static void heapify_down(int i) {
    int n = student_heap.size;
    while (1) {
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        int smallest = i;
        if (l < n && student_heap.items[l].student.jee_rank < student_heap.items[smallest].student.jee_rank) smallest = l;
        if (r < n && student_heap.items[r].student.jee_rank < student_heap.items[smallest].student.jee_rank) smallest = r;
        if (smallest != i) {
            swap_items(&student_heap.items[i], &student_heap.items[smallest]);
            i = smallest;
        } else break;
    }
}

void heap_init(void) {
    student_heap.size = 0;
}

/* Return 1 on success, 0 on failure (full heap) */
int heap_insert(Student s, Offer *offers, int offer_count) {
    if (student_heap.size >= MAX_HEAP_SIZE) return 0;
    HeapItem *it = &student_heap.items[student_heap.size];
    it->student = s;
    it->offers = offers;
    it->offer_count = offer_count;
    student_heap.size++;
    heapify_up(student_heap.size - 1);
    return 1;
}

/* Pop min (best rank) into out; returns 1 if popped, 0 if empty */
int heap_pop_min(HeapItem *out) {
    if (student_heap.size == 0) return 0;
    if (out) *out = student_heap.items[0];
    /* move last to root */
    student_heap.items[0] = student_heap.items[student_heap.size - 1];
    student_heap.size--;
    heapify_down(0);
    return 1;
}

/* Find heap index by exact student name (linear search) */
int heap_find_by_name(const char *name) {
    for (int i = 0; i < student_heap.size; ++i) {
        if (strcmp(student_heap.items[i].student.name, name) == 0) return i;
    }
    return -1;
}

/* Return indices of heap items sorted by jee_rank (ascending).
   out_indices must have capacity max_out. Returns number filled. */
int heap_peek_all_sorted(int *out_indices, int max_out) {
    int n = student_heap.size;
    if (n == 0) return 0;
    /* make an array of indices and sort by corresponding rank */
    int *tmp = malloc(n * sizeof(int));
    for (int i = 0; i < n; ++i) tmp[i] = i;
    /* simple insertion sort (n small enough) */
    for (int i = 1; i < n; ++i) {
        int key = tmp[i];
        int j = i - 1;
        while (j >= 0 && student_heap.items[tmp[j]].student.jee_rank > student_heap.items[key].student.jee_rank) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }
    int fill = (n < max_out) ? n : max_out;
    for (int i = 0; i < fill; ++i) out_indices[i] = tmp[i];
    free(tmp);
    return fill;
}

/* Free dynamically allocated Offer arrays inside heap items */
void heap_free_all_offers(void) {
    for (int i = 0; i < student_heap.size; ++i) {
        if (student_heap.items[i].offers) {
            free(student_heap.items[i].offers);
            student_heap.items[i].offers = NULL;
            student_heap.items[i].offer_count = 0;
        }
    }
}

/* ---------- ELIGIBILITY & OFFERS ---------- */

int is_eligible(const Student *s, const CollegeRow *r) {
    if (!r || !s) return 0;
    if (s->percentage12 < 75.0) return 0;
    /* treat Opening_R and Closing_R as inclusive range */
    int low = r->Opening_R <= r->Closing_R ? r->Opening_R : r->Closing_R;
    int high = r->Opening_R <= r->Closing_R ? r->Closing_R : r->Opening_R;
    if (low == 0 && high == 0) return 0;
    if (s->jee_rank < low || s->jee_rank > high) return 0;

    /* seat type checks: OPEN or matching reservation or gender */
    char seat[MAX_STR]; char quota[MAX_STR];
    strncpy(seat, r->Seat_Type, sizeof(seat)-1); seat[sizeof(seat)-1]=0;
    strncpy(quota, s->reservation, sizeof(quota)-1); quota[sizeof(quota)-1]=0;
    for (int i=0; seat[i]; ++i) seat[i] = toupper((unsigned char)seat[i]);
    for (int i=0; quota[i]; ++i) quota[i] = toupper((unsigned char)quota[i]);
    char gender_upper[16];
    strncpy(gender_upper, s->gender, sizeof(gender_upper)-1); gender_upper[sizeof(gender_upper)-1]=0;
    for (int i=0; gender_upper[i]; ++i) gender_upper[i] = toupper((unsigned char)gender_upper[i]);

    if (strcmp(seat, "OPEN") == 0) return 1;
    if (strcmp(seat, quota) == 0) return 1;
    if (strcmp(seat, gender_upper) == 0) return 1;
    return 0;
}

/* Build offers: returns dynamically allocated Offer array (caller must free).
   Returns NULL and *out_offers=0 if none found. */
Offer* build_offers(const Student *s, CollegeRow *rows, int nrows, int *out_offers) {
    if (!s || !rows || nrows <= 0) { *out_offers = 0; return NULL; }
    Offer *buf = malloc(sizeof(Offer) * nrows);
    int c = 0;
    for (int i = 0; i < nrows; ++i) {
        if (rows[i].Seats_left <= 0) continue;
        if (is_eligible(s, &rows[i])) {
            buf[c].idx = i;
            buf[c].row = rows[i];
            buf[c].status = OFFER_PENDING;
            c++;
        }
    }
    if (c == 0) { free(buf); *out_offers = 0; return NULL; }
    /* sort by Opening_R ascending (better opening rank earlier) */
    for (int i=0;i<c-1;i++)
        for (int j=i+1;j<c;j++)
            if (buf[i].row.Opening_R > buf[j].row.Opening_R) {
                Offer tmp = buf[i]; buf[i]=buf[j]; buf[j]=tmp;
            }
    *out_offers = c;
    return buf;
}

/* ---------- AUTH (simple file-based) ---------- */

/* Sign up with given name, aadhaar, password */
int user_signup(const char *users_txt, const char *name, const char *aadhaar, const char *password) {
    if (!users_txt || !name || !aadhaar || !password) return 0;
    /* first check existence */
    FILE *f = fopen(users_txt, "r");
    char line[512];
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char an[MAX_STR], pw[MAX_STR], nm[MAX_STR];
            if (sscanf(line, "%127[^,],%127[^,],%127[^\n]", nm, an, pw) == 3) {
                if (strcmp(an, aadhaar) == 0) { fclose(f); return 0; }
            }
        }
        fclose(f);
    }
    f = fopen(users_txt, "a");
    if (!f) return 0;
    fprintf(f, "%s,%s,%s\n", name, aadhaar, password);
    fclose(f);
    return 1;
}

/* Login returns 1 if correct */
int user_login(const char *users_txt, const char *aadhaar, const char *password) {
    if (!users_txt || !aadhaar || !password) return 0;
    FILE *f = fopen(users_txt, "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char nm[MAX_STR], an[MAX_STR], pw[MAX_STR];
        if (sscanf(line, "%127[^,],%127[^,],%127[^\n]", nm, an, pw) == 3) {
            if (strcmp(an, aadhaar) == 0 && strcmp(pw, password) == 0) {
                strncpy(current_user_aadhaar, aadhaar, sizeof(current_user_aadhaar)-1);
                current_user_aadhaar[sizeof(current_user_aadhaar)-1] = 0;
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}
