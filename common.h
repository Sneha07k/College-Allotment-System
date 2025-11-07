#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

#define MAX_STR 128
#define MAX_HEAP_SIZE 1000

typedef struct {
    char Institute[MAX_STR];
    char Quota[MAX_STR];
    char Gender[MAX_STR];
    int Year;
    char Academic[MAX_STR];
    int Closing_R;
    int Opening_R;
    char Seat_Type[MAX_STR];
    int Seats_left;
} CollegeRow;


typedef struct {
    char name[MAX_STR];
    float percentage12;
    int jee_rank;
    int age;
    char gender[16];
    char aadhaar[32];
    char email[64];
    char phone[16];
    char exam_number[32];
    char reservation[32];
} Student;

typedef enum { OFFER_PENDING = 0, OFFER_FROZEN = 1 } OfferStatus;

typedef struct {
    int idx;            /* index into college_data */
    CollegeRow row;     /* snapshot of the college row at time of offer building */
    OfferStatus status;
} Offer;


typedef struct {
    Student student;
    Offer *offers;     /* dynamically allocated by build_offers() */
    int offer_count;
} HeapItem;


/* We keep the StudentHeap struct name and fixed array for minimal change.
   Internally we will treat it as a simple list (FCFS). */
typedef struct {
    HeapItem items[MAX_HEAP_SIZE];
    int size;
} StudentHeap;


extern CollegeRow *college_data;
extern int college_count;
extern StudentHeap student_heap;  
extern char current_user_aadhaar[MAX_STR];


CollegeRow* read_college_csv(const char *path, int *out_count);
int write_college_csv(const char *path, CollegeRow *rows, int count);
int append_allocation_txt(const char *path, const char *aadhaar, const char *student_name, const Offer *o);


/* Student-list APIs (kept names same so GUI remain unchanged) */
void heap_init(void);
int heap_insert(Student s, Offer *offers, int offer_count); 
int heap_pop_min(HeapItem *out); 
int heap_peek_all_sorted(int *out_indices, int max_out); 
int heap_find_by_name(const char *name); 
void heap_free_all_offers(void);


int is_eligible(const Student *s, const CollegeRow *r);
Offer* build_offers(const Student *s, CollegeRow *rows, int nrows, int *out_offers);

int user_signup(const char *users_txt, const char *name, const char *aadhaar, const char *password);
int user_login(const char *users_txt, const char *aadhaar, const char *password);

#endif
