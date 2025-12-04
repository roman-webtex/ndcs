/*
 * Copyright (C) 2025 Roman Dmytrenko <roman.webtex@gmail.com>
 *
 *  NDCS GTK+ client. v.0.0.2
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <gtk/gtk.h>
#include <unistd.h>
#include <wchar.h>
#include <glib/gprintf.h>
#include <locale.h>
#include <gtk/gtkwidget.h>

#if defined(__WIN32)
#include <winsock2.h> 
#include <ws2tcpip.h>
#else
#include <arpa/inet.h> 
#include <sys/socket.h> 
#endif

#define version 0.0.2
#define INI_FILE "ndcsclient.conf"

struct _app_data {
    GtkWidget *m_win;
    GtkWidget *pgb;
    GtkWidget *flist;
};

struct search_config {
    gchar *search_directory;
    gchar *file_mask;
    gchar *search_mask;
};

struct client_config {
    gchar server_address[20];
    int   server_port;
    gchar base_path[255];
    gchar runner[16];
};

struct search_config app_search_config, *pointer_app_search_config = &app_search_config;
struct client_config app_config, *pointer_client_config = &app_config;
struct _app_data app_data, *p_app_data = &app_data;

/*-----------------------------------------------------------------------------------------*/
int read_ini_file() {
    FILE *fp;
    char buffer[256]={ 0 };
    char *pb=buffer;
    char *lex;

    if ((fp=fopen(INI_FILE, "r")) == NULL ) {
        return -1;
    }
    
    while((*pb = fgetc(fp)) != '\n' || *pb != EOF) {
        if (*pb == '\n') {
            *pb = '\0';
            lex = strtok(buffer, "=");
            while(lex != NULL) {
                if(strcmp(lex, "server_port") == 0) {
                    lex = strtok(NULL, "=");
                    pointer_client_config->server_port = atoi(lex);
                }
                if(strcmp(lex, "server_address") == 0) {
                    lex = strtok(NULL, "=");
                    strcpy(pointer_client_config->server_address, lex);
                }
                if(strcmp(lex, "base_path") == 0) {
                    lex = strtok(NULL, "=");
                    strcpy(pointer_client_config->base_path, lex);
                }
                if(strcmp(lex, "explorer") == 0) {
                    lex = strtok(NULL, "=");
                    strcpy(pointer_client_config->runner, lex);
                }
                lex = strtok(NULL, "=");
            }
            buffer[0]='\0';
            pb=buffer;
        } else if(*pb == EOF) {
            break;
        } else {
            *pb++;
        }
    }
    fclose(fp);
    return 0;
}

/*-----------------------------------------------------------------------------------------*/
static void exit_message (GtkWidget *widget, gpointer data) 
{ 
    g_print ("Network Document Content Search client. By!\n"); 
}

/*-----------------------------------------------------------------------------------------*/
static void  get_file_mask_exit(GtkWidget *widget, gpointer data) 
{ 
    pointer_app_search_config->file_mask=gtk_entry_get_text(GTK_ENTRY(widget));
}

/*-----------------------------------------------------------------------------------------*/
static void  get_text_mask_exit(GtkWidget *widget, gpointer data) 
{ 
    pointer_app_search_config->search_mask=gtk_entry_get_text(GTK_ENTRY(widget));
}

/*-----------------------------------------------------------------------------------------*/
static void on_search_directory_select(GtkWidget *dialog, gint response, gpointer data)
{
    if(response == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *dlg = GTK_FILE_CHOOSER(dialog);
        pointer_app_search_config->search_directory = gtk_file_chooser_get_filename(dlg);
        gtk_entry_set_text(GTK_ENTRY(data), pointer_app_search_config->search_directory);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

/*-----------------------------------------------------------------------------------------*/
static void get_search_directory (GtkWidget *widget, gpointer data)
{

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Виберіть каталог",
        NULL,
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "Відміна", GTK_RESPONSE_CANCEL,
        "Вибрати", GTK_RESPONSE_ACCEPT,
        NULL
    );

#if !defined(__WIN32)
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), pointer_client_config->base_path);
#endif
    g_signal_connect(dialog, "response", G_CALLBACK(on_search_directory_select), data);
    gtk_widget_show(dialog);
}

/*-----------------------------------------------------------------------------------------*/
static void container_remove_child(GtkWidget *widget, gpointer data) 
{
    gtk_widget_destroy(GTK_WIDGET(widget));
}

/*-----------------------------------------------------------------------------------------*/
static void insert_row(GtkWidget *list, char* text)
{
    GtkListBox *lbox = GTK_LIST_BOX(list);
    GtkWidget *list_box_row = gtk_list_box_row_new();
    GtkWidget *hbox_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *row_label = gtk_label_new(text);
    gtk_box_set_spacing(GTK_BOX(hbox_row),0);
    gtk_container_add(GTK_CONTAINER(list_box_row), GTK_WIDGET(hbox_row));
    gtk_box_pack_start(GTK_BOX(hbox_row), GTK_WIDGET(row_label), FALSE, TRUE,0);
    gtk_list_box_insert(lbox, list_box_row, -1);
    gtk_widget_show_all(GTK_WIDGET(lbox));
}

/*-----------------------------------------------------------------------------------------*/
static void search_loop(GtkWidget *widget, gpointer f_list) 
{ 
    struct sockaddr_in server_addr;
    char* directory = pointer_app_search_config->search_directory;
    gint path_length = strlen(pointer_client_config->base_path); 
    int client_fp, status, j=0, i=0, count;
    char  fif_message[256] = { 0 };
    char  buffer[1024] = { 0 };
    char  resp[256] = { 0 };
    char* p_buffer = buffer;
    char* p_resp = resp;
    char sep[] = ": "; 
    double all, value;
    gchar **lines;

    GdkWindow *window = gtk_widget_get_parent_window (GTK_WIDGET (f_list));
    GdkCursor *cursor = gdk_cursor_new_from_name(gdk_display_get_default(), "progress");
    gdk_window_set_cursor (window, cursor);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(p_app_data->pgb), 0.0);
    gtk_container_foreach(GTK_CONTAINER(f_list), container_remove_child, NULL);

    while (directory[path_length] != '\0') {
        fif_message[j++] = directory[path_length++];
    }
    strcat(fif_message, "<");
    strcat(fif_message, "{");
    strcat(fif_message, pointer_app_search_config->file_mask);
    strcat(fif_message, "} {");
    strcat(fif_message, pointer_app_search_config->search_mask);
    strcat(fif_message, "}");

    if((client_fp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        insert_row(GTK_WIDGET(f_list), "Unable to create socket\n");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(pointer_client_config->server_port);
    
    if(inet_pton(AF_INET, pointer_client_config->server_address, &server_addr.sin_addr) <= 0) {
        insert_row(GTK_WIDGET(f_list), "Invlid address\n");
        return;
    }

    if((status = connect(client_fp, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0 ) {
        insert_row(GTK_WIDGET(f_list), "Unable to connect\n");
        return;
    }

    send(client_fp, fif_message, strlen(fif_message), 0);

    while ((j = recv(client_fp, p_buffer, 1024-1, 0)) > 0) {
        i=0;
        while(i<=j) {
            *p_resp++ = *p_buffer++ ;
            if(*p_buffer == '\n') {
                *p_resp = '\0';
                lines = g_strsplit(resp, ":", -1);
                if(resp[0] != sep[0] && resp[0] != sep[1]) {
                    if(strcmp(lines[0],"Всього ")==0) {
                        all = (double) atoi(g_utf8_substring (lines[1], 1, strlen(lines[1])));
                    }
                    insert_row(f_list, resp);
                } else {
                    value = ((double)atoi(lines[1]))/all;
                    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(p_app_data->pgb), value);
                }
                memset(resp, 0, 256*sizeof(char));
                p_resp = resp;
                *p_buffer++;
            }
            i++;
            while (gtk_events_pending()) {
                gtk_main_iteration_do(FALSE);
            }
        }
        memset(resp, 0, 256*sizeof(char));
        p_resp = resp;
        memset(buffer, 0, 1024*sizeof(char));
        p_buffer=buffer;
    }
    close(client_fp);

    cursor = gdk_cursor_new_from_name(gdk_display_get_default(), "default");
    gdk_window_set_cursor (window, cursor);
}

/*-----------------------------------------------------------------------------------------*/
static void show_doc(GtkWidget *widget, gpointer f_list) 
{ 
    const char* filename;
    char  buffer[255]={ 0 };
    char  temp[1024]={ 0 };
    char  command[1024]={ 0 };
    g_autoptr (GdkAppLaunchContext) context = NULL;
    char* p_buffer=buffer;
    char* p_temp=temp;
    GAppInfo* appinfo = NULL;
    GList* applist = NULL;
    GFile* file;
    FILE *fp;

    GtkListBox *lbox = GTK_LIST_BOX(f_list);
    GtkListBoxRow *row = gtk_list_box_get_selected_row(lbox);
    int line_index = gtk_list_box_row_get_index(row);
    GList *child = gtk_container_get_children(GTK_CONTAINER(row));
    GList *label = gtk_container_get_children(GTK_CONTAINER(child->data));
    if(GTK_IS_LABEL(GTK_WIDGET(label->data))) {
        filename = gtk_label_get_text(GTK_LABEL(label->data));
    }

    while((*p_buffer++ = *filename++) != '\0' ) {
        if(*filename == '>') {
            *p_buffer-- ;
            *p_buffer = '\0';
            break;
        }
    }

    p_buffer=buffer;
    *p_buffer++;
    while((*p_temp++ = *p_buffer++) !='\0') {
        ;
    }
    *p_temp = '\0';

    strcat(command, pointer_app_search_config->search_directory);
    strcat(command,temp);
    
    file = g_file_new_for_path(command);

    command[0] = '\0';
    strcat(command, pointer_client_config->runner);
    strcat(command, " \"");
    strcat(command, g_file_get_parse_name(file));
    strcat(command, "\"");
    
    context = gdk_display_get_app_launch_context (gdk_display_get_default ());
    appinfo = g_app_info_create_from_commandline((gchar*) pointer_client_config->runner, NULL, G_APP_INFO_CREATE_NONE, NULL);
    applist = g_list_prepend(NULL, file);

#if !defined(__WIN32)
    g_app_info_launch(appinfo, applist, G_APP_LAUNCH_CONTEXT (context), NULL);
#else
    if((fp=fopen("start.cmd", "w")) != NULL) {
        fputs("chcp 65001\n", fp);
        fputs(command, fp);
        fclose(fp);
        system("start.cmd");
    }
#endif
}

/*-----------------------------------------------------------------------------------------*/
static void activate(GtkApplication * app, gpointer user_data)
{
    GtkWidget *win,
              *vbox_main, 
              *hbox_directory, 
              *hbox_file_mask,
              *hbox_text_mask, 
              *hbox_progress,
              *hbox_file_list, 
              *hbox_buttons,
              *btn_get_dir, 
              *lab_directory,
              *entry_directory,
              *lab_file_mask,
              *entry_file_mask,
              *lab_text_mask,
              *entry_text_mask,
              *search_progress,
              *scrolled,
              *file_list,
              *btn_start,
              *btn_view,
              *btn_exit; 

    win            = gtk_application_window_new(app);
    lab_directory  = gtk_label_new("Каталог для пошуку :");
    entry_directory= gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_directory), pointer_client_config->base_path);
    pointer_app_search_config->search_directory = pointer_client_config->base_path;

    lab_file_mask  = gtk_label_new("Шаблон файлів :");
    entry_file_mask= gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_file_mask), "*");
    app_search_config.file_mask = "*";

    lab_text_mask  = gtk_label_new("Шаблон для пошуку :");
    entry_text_mask= gtk_entry_new();
    
    search_progress= gtk_progress_bar_new();
    
    scrolled       = gtk_scrolled_window_new(NULL, NULL);
    file_list      = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(file_list), GTK_SELECTION_SINGLE);
    
    gtk_container_add(GTK_CONTAINER(scrolled), file_list);

    btn_get_dir    = gtk_button_new_with_mnemonic("_Вибрати"); 
    btn_start      = gtk_button_new_with_mnemonic("_Розпочати");
    btn_view      = gtk_button_new_with_mnemonic("_Переглянути");
    btn_exit       = gtk_button_new_with_mnemonic("Ви_хід");

    vbox_main      = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    hbox_directory = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    hbox_file_mask = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    hbox_text_mask = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    hbox_progress  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    hbox_file_list = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    hbox_buttons   = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    gtk_container_add(GTK_CONTAINER(win), vbox_main);
    gtk_box_pack_start(GTK_BOX(vbox_main), GTK_WIDGET(hbox_directory), FALSE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(vbox_main), GTK_WIDGET(hbox_file_mask), FALSE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(vbox_main), GTK_WIDGET(hbox_text_mask), FALSE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(vbox_main), GTK_WIDGET(hbox_progress), FALSE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox_main), GTK_WIDGET(hbox_file_list), TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(vbox_main), GTK_WIDGET(hbox_buttons), FALSE, TRUE, 1);

    gtk_box_pack_start(GTK_BOX(hbox_directory), GTK_WIDGET(lab_directory), FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(hbox_directory), GTK_WIDGET(entry_directory), TRUE, TRUE, 1);
    gtk_box_pack_end(GTK_BOX(hbox_directory), GTK_WIDGET(btn_get_dir), FALSE, FALSE, 1);

    gtk_box_pack_start(GTK_BOX(hbox_file_mask), GTK_WIDGET(lab_file_mask), FALSE, FALSE, 1);
    gtk_box_pack_end(GTK_BOX(hbox_file_mask), GTK_WIDGET(entry_file_mask), TRUE, TRUE, 1);

    gtk_box_pack_start(GTK_BOX(hbox_text_mask), GTK_WIDGET(lab_text_mask), FALSE, FALSE, 1);
    gtk_box_pack_end(GTK_BOX(hbox_text_mask), GTK_WIDGET(entry_text_mask), TRUE, TRUE, 1);

    gtk_box_pack_start(GTK_BOX(hbox_progress), GTK_WIDGET(search_progress), TRUE, TRUE, 1);
    
    gtk_box_pack_start(GTK_BOX(hbox_file_list), GTK_WIDGET(scrolled), TRUE, TRUE, 1);

    gtk_box_pack_start(GTK_BOX(hbox_buttons), GTK_WIDGET(btn_start), FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(hbox_buttons), GTK_WIDGET(btn_view), FALSE, FALSE, 3);
    gtk_box_pack_end(GTK_BOX(hbox_buttons), GTK_WIDGET(btn_exit), FALSE, FALSE, 1);

    gtk_container_set_border_width(GTK_CONTAINER(win),5);
    
    gtk_window_set_title (GTK_WINDOW (win), "Пошук в документах"); 
    gtk_window_set_default_size (GTK_WINDOW (win), 800, 600); 

    g_signal_connect (G_OBJECT(btn_get_dir), "clicked", G_CALLBACK (get_search_directory), entry_directory); 
    g_signal_connect (G_OBJECT(btn_start), "clicked", G_CALLBACK (search_loop), file_list); 
    g_signal_connect (G_OBJECT(btn_view), "clicked", G_CALLBACK (show_doc), file_list); 
    g_signal_connect (G_OBJECT(btn_exit), "clicked", G_CALLBACK (exit_message), NULL); 
    g_signal_connect_swapped (G_OBJECT(btn_exit), "clicked", G_CALLBACK (gtk_widget_destroy), win);
    g_signal_connect (G_OBJECT(entry_file_mask), "focus_out_event", G_CALLBACK (get_file_mask_exit), NULL); 
    g_signal_connect (G_OBJECT(entry_text_mask), "focus_out_event", G_CALLBACK (get_text_mask_exit), NULL); 
    
    p_app_data->m_win = win;
    p_app_data->pgb = search_progress;
    p_app_data->flist = file_list;
    
    gtk_widget_show_all (win);    
}

/*-----------------------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    GtkApplication * app;
    int status = -1;

    if (read_ini_file() >= 0) {
        app = gtk_application_new("com.borysych.ndcsclient", G_APPLICATION_DEFAULT_FLAGS);    
        g_signal_connect (app, "activate", G_CALLBACK (activate), NULL); 
        status = g_application_run (G_APPLICATION (app), argc, argv); 
        g_object_unref (app);    
    }
    return status;
}
/*-----------------------------------------------------------------------------------------*/

