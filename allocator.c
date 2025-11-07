// student.c
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


CollegeRow *college_data = NULL;
int college_count = 0;

/* keep the same symbol so GUI and other code don't need changes */
StudentHeap student_heap;
char current_user_aadhaar[MAX_STR] = "";


/* ----------------------------
   Student-list (FCFS) helpers
   ---------------------------- */

void heap_init(void) {
    student_heap.size = 0;
}

/* Append student (no heap property) */
int heap_insert(Student s, Offer *offers, int offer_count) {
    if (student_heap.size >= MAX_HEAP_SIZE) return 0;
    HeapItem *it = &student_heap.items[student_heap.size];
    it->student = s;
    it->offers = offers;
    it->offer_count = offer_count;
    student_heap.size++;
    return 1;
}

/* Pop the earliest-inserted student (FIFO).
   We return the item in `out` (if non-NULL) and shift the array left.
   Returns 1 if popped, 0 if empty.
*/
int heap_pop_min(HeapItem *out) {
    if (student_heap.size == 0) return 0;
    if (out) *out = student_heap.items[0];
    /* shift left */
    for (int i = 1; i < student_heap.size; ++i) student_heap.items[i-1] = student_heap.items[i];
    student_heap.size--;
    return 1;
}

/* Find by student name (linear search) */
int heap_find_by_name(const char *name) {
    for (int i = 0; i < student_heap.size; ++i) {
        if (strcmp(student_heap.items[i].student.name, name) == 0) return i;
    }
    return -1;
}

/* Return indices in FCFS order (insertion order): 0..n-1 (up to max_out) */
int heap_peek_all_sorted(int *out_indices, int max_out) {
    int n = student_heap.size;
    if (n == 0) return 0;
    int fill = (n < max_out) ? n : max_out;
    for (int i = 0; i < fill; ++i) out_indices[i] = i;
    return fill;
}

/* Free per-student offers */
void heap_free_all_offers(void) {
    for (int i = 0; i < student_heap.size; ++i) {
        if (student_heap.items[i].offers) {
            free(student_heap.items[i].offers);
            student_heap.items[i].offers = NULL;
            student_heap.items[i].offer_count = 0;
        }
    }
}

/* ----------------------------
   Eligibility check (unchanged)
   ---------------------------- */

int is_eligible(const Student *s, const CollegeRow *r) {
    if (!r || !s) return 0;
    if (s->percentage12 < 75.0) return 0;

    int low = r->Opening_R <= r->Closing_R ? r->Opening_R : r->Closing_R;
    int high = r->Opening_R <= r->Closing_R ? r->Closing_R : r->Opening_R;
    if (low == 0 && high == 0) return 0;
    if (s->jee_rank < low || s->jee_rank > high) return 0;

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

/* --------------------------------------------
   build_offers(): collects eligible colleges then
   returns an array of offers sorted by Closing_R
   (min-heap used internally to produce sorted list)
   -------------------------------------------- */

Offer* build_offers(const Student *s, CollegeRow *rows, int nrows, int *out_offers) {
    if (!s || !rows || nrows <= 0) { *out_offers = 0; return NULL; }

    /* temp storage for eligible offers (unsorted) */
    Offer *temp = malloc(sizeof(Offer) * nrows);
    if (!temp) { *out_offers = 0; return NULL; }
    int c = 0;

    for (int i = 0; i < nrows; ++i) {
        if (rows[i].Seats_left <= 0) continue;
        if (is_eligible(s, &rows[i])) {
            temp[c].idx = i;
            temp[c].row = rows[i];
            temp[c].status = OFFER_PENDING;
            c++;
        }
    }

    if (c == 0) { free(temp); *out_offers = 0; return NULL; }

    /* Build a min-heap of Offers using Closing_R as key.
       We'll push temp[0..c-1] into heap_arr and then pop them to produce sorted array.
    */
    Offer *heap_arr = malloc(sizeof(Offer) * c);
    if (!heap_arr) { free(temp); *out_offers = 0; return NULL; }
    int heap_n = 0;

    auto void heap_push(const Offer *val) {
        int i = heap_n++;
        heap_arr[i] = *val;
        /* sift up */
        while (i > 0) {
            int p = (i - 1) / 2;
            if (heap_arr[p].row.Closing_R <= heap_arr[i].row.Closing_R) break;
            Offer t = heap_arr[p]; heap_arr[p] = heap_arr[i]; heap_arr[i] = t;
            i = p;
        }
    }

    auto void heapify_down_from(int idx) {
        int i = idx;
        while (1) {
            int l = 2*i + 1;
            int r = 2*i + 2;
            int smallest = i;
            if (l < heap_n && heap_arr[l].row.Closing_R < heap_arr[smallest].row.Closing_R) smallest = l;
            if (r < heap_n && heap_arr[r].row.Closing_R < heap_arr[smallest].row.Closing_R) smallest = r;
            if (smallest == i) break;
            Offer t = heap_arr[i]; heap_arr[i] = heap_arr[smallest]; heap_arr[smallest] = t;
            i = smallest;
        }
    }

    auto Offer heap_pop(void) {
        Offer out = heap_arr[0];
        heap_n--;
        if (heap_n > 0) {
            heap_arr[0] = heap_arr[heap_n];
            heapify_down_from(0);
        }
        return out;
    }

    /* push all */
    for (int i = 0; i < c; ++i) heap_push(&temp[i]);

    /* allocate final buffer and pop heap to get sorted offers (best = lowest Closing_R) */
    Offer *outbuf = malloc(sizeof(Offer) * c);
    if (!outbuf) { free(temp); free(heap_arr); *out_offers = 0; return NULL; }
    int outc = 0;
    while (heap_n > 0) {
        outbuf[outc++] = heap_pop();
    }

    free(temp);
    free(heap_arr);
    *out_offers = outc;
    return outbuf;
}

/* -----------------------
   User signup / login
   ----------------------- */

int user_signup(const char *users_txt, const char *name, const char *aadhaar, const char *password) {
    if (!users_txt || !name || !aadhaar || !password) return 0;

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
