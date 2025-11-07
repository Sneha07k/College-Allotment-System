
#include <ctype.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"


static const char *CSV_PATH = "C:\\Users\\sneha\\OneDrive\\Desktop\\csas\\colleges.csv";
static const char *ALLOC_PATH = "C:\\Users\\sneha\\OneDrive\\Desktop\\csas\\allocations.txt";
static const char *USERS_PATH = "C:\\Users\\sneha\\OneDrive\\Desktop\\csas\\users.csv";


static GtkWidget *main_window = NULL;
static GtkWidget *tree_students = NULL;
static GtkListStore *student_store = NULL;


static void trim_local(char *s) {
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    int l = strlen(s);
    while (l > 0 && isspace((unsigned char)s[l-1])) s[--l] = '\0';
}


static void refresh_student_list() {
    if (!student_store) return;
    gtk_list_store_clear(student_store);
    int idxs[MAX_HEAP_SIZE];
    int n = heap_peek_all_sorted(idxs, MAX_HEAP_SIZE);
    for (int i=0;i<n;i++) {
        int hi = idxs[i];
        HeapItem *h = &student_heap.items[hi];
        const char *inst = "None";
        const char *course = "None";
        const char *status = "PENDING";
        if (h->offer_count > 0 && h->offers) {
            inst = h->offers[0].row.Institute;
            course = h->offers[0].row.Academic;
            status = (h->offers[0].status == OFFER_FROZEN) ? "FROZEN" : "PENDING";
        }
        GtkTreeIter iter;
        gtk_list_store_append(student_store, &iter);
        gtk_list_store_set(student_store, &iter,
                           0, h->student.name,
                           1, inst,
                           2, course,
                           3, status,
                           -1);
    }
}


static void show_message(GtkWindow *parent, const char *title, const char *msg) {
    GtkWidget *d = gtk_message_dialog_new(parent,
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_OK,
                                         "%s", msg);
    gtk_window_set_title(GTK_WINDOW(d), title);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}


static void on_add_student(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWindow *parent = GTK_WINDOW(user_data);
    GtkWidget *dlg = gtk_dialog_new_with_buttons("Add Student", parent,
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 "_Cancel", GTK_RESPONSE_CANCEL,
                                                 "_Add", GTK_RESPONSE_ACCEPT,
                                                 NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_container_add(GTK_CONTAINER(content), grid);

    const char *labels[] = {"Name", "12th %", "JEE Rank", "Age", "Gender", "Aadhaar", "Email", "Phone", "12th Exam No", "Reservation"};
    GtkWidget *entries[10];
    for (int i=0;i<10;i++) {
        GtkWidget *lbl = gtk_label_new(labels[i]);
        entries[i] = gtk_entry_new();
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), entries[i], 1, i, 1, 1);
    }
    if (current_user_aadhaar[0]) gtk_entry_set_text(GTK_ENTRY(entries[5]), current_user_aadhaar);

    gtk_widget_show_all(dlg);
    int r = gtk_dialog_run(GTK_DIALOG(dlg));
    if (r == GTK_RESPONSE_ACCEPT) {
        Student s; memset(&s, 0, sizeof(s));
        strncpy(s.name, gtk_entry_get_text(GTK_ENTRY(entries[0])), MAX_STR-1);
        s.percentage12 = atof(gtk_entry_get_text(GTK_ENTRY(entries[1])));
        s.jee_rank = atoi(gtk_entry_get_text(GTK_ENTRY(entries[2])));
        s.age = atoi(gtk_entry_get_text(GTK_ENTRY(entries[3])));
        strncpy(s.gender, gtk_entry_get_text(GTK_ENTRY(entries[4])), sizeof(s.gender)-1);
        strncpy(s.aadhaar, gtk_entry_get_text(GTK_ENTRY(entries[5])), sizeof(s.aadhaar)-1);
        strncpy(s.email, gtk_entry_get_text(GTK_ENTRY(entries[6])), sizeof(s.email)-1);
        strncpy(s.phone, gtk_entry_get_text(GTK_ENTRY(entries[7])), sizeof(s.phone)-1);
        strncpy(s.exam_number, gtk_entry_get_text(GTK_ENTRY(entries[8])), sizeof(s.exam_number)-1);
        strncpy(s.reservation, gtk_entry_get_text(GTK_ENTRY(entries[9])), sizeof(s.reservation)-1);

        int offer_count = 0;
        Offer *offers = build_offers(&s, college_data, college_count, &offer_count);
        if (!heap_insert(s, offers, offer_count)) {
            show_message(parent, "Heap full", "Cannot add student: heap is full.");
            free(offers);
        } else {
            refresh_student_list();
        }
    }
    gtk_widget_destroy(dlg);
}


static void on_allocate_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWindow *parent = GTK_WINDOW(user_data);
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_students));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
        show_message(parent, "Select", "Please select a student from the list.");
        return;
    }
    gchar *student_name;
    gtk_tree_model_get(model, &iter, 0, &student_name, -1);
    if (!student_name) return;
    int idx = heap_find_by_name(student_name);
    g_free(student_name);
    if (idx < 0) {
        show_message(parent, "Not found", "Selected student no longer exists in heap.");
        return;
    }
    HeapItem *h = &student_heap.items[idx];
    if (h->offer_count == 0 || !h->offers) {
        show_message(parent, "No offers", "This student has no offers.");
        return;
    }


    GtkWidget *dlg = gtk_dialog_new_with_buttons("Select Offer to Freeze", parent,
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 "_Cancel", GTK_RESPONSE_CANCEL,
                                                 "_Freeze", GTK_RESPONSE_ACCEPT,
                                                 NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    GSList *group = NULL;
    GtkWidget *radio_buttons[h->offer_count];
    for (int i=0;i<h->offer_count;i++) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s - %s (Seats: %d) [%s]",
                 h->offers[i].row.Institute, h->offers[i].row.Academic, h->offers[i].row.Seats_left,
                 (h->offers[i].status==OFFER_FROZEN) ? "FROZEN" : "PENDING");
        if (i==0) radio_buttons[i] = gtk_radio_button_new_with_label(NULL, buf);
        else radio_buttons[i] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_buttons[0]), buf);
        group = g_slist_append(group, radio_buttons[i]);
        gtk_box_pack_start(GTK_BOX(vbox), radio_buttons[i], FALSE, FALSE, 0);
    }
    gtk_widget_show_all(dlg);
    int resp = gtk_dialog_run(GTK_DIALOG(dlg));
    if (resp == GTK_RESPONSE_ACCEPT) {
        int chosen = -1;
        for (int i=0;i<h->offer_count;i++) {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_buttons[i]))) { chosen = i; break; }
        }
        if (chosen >= 0) {
            Offer *o = &h->offers[chosen];
            if (o->row.Seats_left <= 0) {
                show_message(parent, "Unavailable", "Selected seat not available.");
            } else {
                o->status = OFFER_FROZEN;

                int cidx = o->idx;
                if (cidx >= 0 && cidx < college_count) {
                    if (college_data[cidx].Seats_left > 0) college_data[cidx].Seats_left--;
                    if (college_data[cidx].Seats_left == 0) {

                        for (int k=cidx;k<college_count-1;k++) college_data[k]=college_data[k+1];
                        college_count--;
                    }

                    write_college_csv(CSV_PATH, college_data, college_count);
                }
                append_allocation_txt(ALLOC_PATH, (current_user_aadhaar[0]?current_user_aadhaar:h->student.aadhaar), h->student.name, o);
                show_message(parent, "Frozen", "Offer frozen and allocation recorded.");
                refresh_student_list();
            }
        }
    }
    gtk_widget_destroy(dlg);
}


static void on_view_allocations(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWindow *parent = GTK_WINDOW(user_data);
    FILE *f = fopen(ALLOC_PATH, "r");
    if (!f) {
        show_message(parent, "Allocations", "No allocations found.");
        return;
    }
    GtkWidget *dlg = gtk_dialog_new_with_buttons("My Allocations", parent,
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 "_Close", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled, 600, 400);
    gtk_container_add(GTK_CONTAINER(content), scrolled);
    GtkWidget *text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled), text);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));

    char line[1024];
    char aad[128] = "", student[128] = "", inst[128] = "", course[128] = "", status[128] = "";
    GString *acc = g_string_new("");
    int any = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Aadhaar:", 8) == 0) { strncpy(aad, line+8, sizeof(aad)-1); trim_local(aad); }
        else if (strncmp(line, "Student:", 8) == 0) { strncpy(student, line+8, sizeof(student)-1); trim_local(student); }
        else if (strncmp(line, "Institute:", 10) == 0) { strncpy(inst, line+10, sizeof(inst)-1); trim_local(inst); }
        else if (strncmp(line, "Course:", 7) == 0) { strncpy(course, line+7, sizeof(course)-1); trim_local(course); }
        else if (strncmp(line, "Status:", 7) == 0) { strncpy(status, line+7, sizeof(status)-1); trim_local(status); }
        else if (strncmp(line, "---", 3) == 0) {
            if (current_user_aadhaar[0] == 0 || strcmp(current_user_aadhaar, aad) == 0) {
                g_string_append_printf(acc, "Student: %s\nInstitute: %s\nCourse: %s\nStatus: %s\n\n", student, inst, course, status);
                any = 1;
            }
            aad[0]=student[0]=inst[0]=course[0]=status[0]=0;
        }
    }
    fclose(f);
    if (!any) g_string_append(acc, "No allocations found.\n");
    gtk_text_buffer_set_text(buf, acc->str, -1);
    g_string_free(acc, TRUE);

    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}


static int run_auth_dialog(GtkWindow *parent) {
    GtkWidget *dlg = gtk_dialog_new_with_buttons("Login / Signup", parent,
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget *lbl_aad = gtk_label_new("Aadhaar:");
    GtkWidget *ent_aad = gtk_entry_new();
    GtkWidget *lbl_pwd = gtk_label_new("Password:");
    GtkWidget *ent_pwd = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(ent_pwd), FALSE);
    GtkWidget *btn_login = gtk_button_new_with_label("Login");
    GtkWidget *btn_signup = gtk_button_new_with_label("Signup");

    gtk_grid_attach(GTK_GRID(grid), lbl_aad, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ent_aad, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_pwd, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ent_pwd, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), btn_login, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), btn_signup, 1, 2, 1, 1);

    
    g_signal_connect_swapped(btn_login, "clicked", G_CALLBACK(gtk_widget_hide), dlg);
    g_signal_connect_swapped(btn_signup, "clicked", G_CALLBACK(gtk_widget_hide), dlg);

    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    const char *aad = gtk_entry_get_text(GTK_ENTRY(ent_aad));
    const char *pwd = gtk_entry_get_text(GTK_ENTRY(ent_pwd));
    int ok = 0;
    if (aad && aad[0] && pwd && pwd[0]) {
        if (user_login(USERS_PATH, aad, pwd)) {
            strncpy(current_user_aadhaar, aad, sizeof(current_user_aadhaar)-1);
            ok = 1;
        } else {

            GtkWidget *sd = gtk_dialog_new_with_buttons("Signup - Enter Name", parent,
                                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                                        "_OK", GTK_RESPONSE_ACCEPT,
                                                        NULL);
            GtkWidget *c2 = gtk_dialog_get_content_area(GTK_DIALOG(sd));
            GtkWidget *e_name = gtk_entry_new();
            gtk_container_add(GTK_CONTAINER(c2), e_name);
            gtk_widget_show_all(sd);
            if (gtk_dialog_run(GTK_DIALOG(sd)) == GTK_RESPONSE_ACCEPT) {
                const char *name = gtk_entry_get_text(GTK_ENTRY(e_name));
                if (name && name[0]) {
                    if (user_signup(USERS_PATH, name, aad, pwd)) {
                        strncpy(current_user_aadhaar, aad, sizeof(current_user_aadhaar)-1);
                        ok = 1;
                    } else {
                        show_message(parent, "Signup failed", "Signup failed (aadhaar may already exist).");
                    }
                }
            }
            gtk_widget_destroy(sd);
        }
    }
    gtk_widget_destroy(dlg);
    return ok;
}


int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    college_data = read_college_csv(CSV_PATH, &college_count);
    if (!college_data) {
        fprintf(stderr, "Failed to read %s\n", CSV_PATH);
        return 1;
    }


    heap_init();

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 900, 600);
    gtk_window_set_title(GTK_WINDOW(main_window), "College Seat Allocation");
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);


    if (!run_auth_dialog(GTK_WINDOW(main_window))) {
        fprintf(stderr, "Auth failed\n");
        return 1;
    }

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *vleft = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *vright = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(main_window), hbox);
    gtk_box_pack_start(GTK_BOX(hbox), vleft, TRUE, TRUE, 6);
    gtk_box_pack_start(GTK_BOX(hbox), vright, FALSE, FALSE, 6);


    student_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    tree_students = gtk_tree_view_new_with_model(GTK_TREE_MODEL(student_store));
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;
    const char *titles[] = {"Student", "Top Institute", "Top Course", "Status"};
    for (int i=0;i<4;i++) {
        renderer = gtk_cell_renderer_text_new();
        col = gtk_tree_view_column_new_with_attributes(titles[i], renderer, "text", i, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree_students), col);
    }
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled, 600, 600);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_students);
    gtk_box_pack_start(GTK_BOX(vleft), scrolled, TRUE, TRUE, 0);


    GtkWidget *btn_add = gtk_button_new_with_label("Add Student");
    GtkWidget *btn_alloc = gtk_button_new_with_label("Allocate / Freeze Offer");
    GtkWidget *btn_view = gtk_button_new_with_label("View My Allocations");
    gtk_box_pack_start(GTK_BOX(vright), btn_add, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vright), btn_alloc, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vright), btn_view, FALSE, FALSE, 6);

    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_student), main_window);
    g_signal_connect(btn_alloc, "clicked", G_CALLBACK(on_allocate_clicked), main_window);
    g_signal_connect(btn_view, "clicked", G_CALLBACK(on_view_allocations), main_window);

    refresh_student_list();

    gtk_widget_show_all(main_window);
    gtk_main();


    heap_free_all_offers();
    if (college_data) free(college_data);
    return 0;
}
