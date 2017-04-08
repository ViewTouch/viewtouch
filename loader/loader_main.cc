/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * loader_main.cc - revision 8 (March 2006) implemented in GTK+3 in Septmber 2016
 * Implementation of system starting command
 */
#include <gtk/gtk.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "build_number.h"
#include "basic.hh"
#include "generic_char.hh"
#include "logger.hh"
#include "utility.hh"

/**** Defintions ****/
#define SOCKET_FILE    "/tmp/vt_main"
#define COMMAND_FILE   VIEWTOUCH_PATH "/bin/.vtpos_command"
#define WIN_WIDTH  640
#define WIN_HEIGHT 240

#ifdef DEBUG
int debug_mode = 1;
#else
int debug_mode = 0;
#endif

/**** Globals ****/
GdkDisplay     *display         = NULL;
GdkScreen      *screen          = NULL;
const gchar    *displayname     = NULL;
GtkCssProvider *provider        = NULL;
GIOChannel     *loadstatus      = NULL;

GtkWidget      *window          = NULL;
GtkWidget      *grid            = NULL;
GtkWidget      *label           = NULL;
GtkWidget      *space           = NULL;

int            SocketNo         = 0;
int            SocketNum        = 0;
int            GetInput         = 0;
char           KBInput[1024]    = "";
char           BuildNumber[]    = BUILD_NUMBER;

/**** Functions ****/
void ExitLoader()
{
    if (SocketNo)
        close(SocketNo);

    gtk_main_quit();

    exit(0);
}

void UpdateStatusBox(gchar *msg)
{
    gtk_label_set_text(GTK_LABEL(label), msg);
    gtk_widget_show(label);
}

void UpdateKeyboard(const char* str)
{
    genericChar prompt[256] = "Temporary Key: ";
    genericChar temp[4096]  = "";

    if (str)
        snprintf(KBInput, sizeof(KBInput), "%s_", str);

    snprintf(temp, 4096, "%s%s", prompt, KBInput);

    UpdateStatusBox(&temp[0]);
}

gboolean SocketInputCB(GIOChannel *source, GIOCondition condition, gpointer data)
{
    static char buffer[256];
    static char* c = buffer;

    while (read(SocketNo, c, 1) == 1)
    {
        switch (*c)
        {
            case '\0':
                c = buffer;
                if (strcmp(buffer, "done") == 0)
                    ExitLoader();
                else
                {
                    UpdateStatusBox(buffer);
                    return TRUE;
                }
                break;

            case '\r':
                GetInput = 1;
                *c = '\0';
                UpdateKeyboard("");
                break;

            default:
                c++;
                break;
        }
    }
    return TRUE;
}

int SetupConnection(const char* socket_file)
{
    struct sockaddr_un server_adr, client_adr;
    server_adr.sun_family = AF_UNIX;
    strcpy(server_adr.sun_path, socket_file);
    unlink(socket_file);

    char str[256];
    int dev = socket(AF_UNIX, SOCK_STREAM, 0);

    if (dev <= 0)
    {
        logmsg(LOG_ERR, "Failed to open socket '%s'", SOCKET_FILE);
    }
    else if (bind(dev, (struct sockaddr *) &server_adr, SUN_LEN(&server_adr)) < 0)
    {
        logmsg(LOG_ERR, "Failed to bind socket '%s'", SOCKET_FILE);
    }
    else
    {
        unsigned int len;

        sprintf(str, VIEWTOUCH_PATH "/bin/vt_main %s&", socket_file);
        system(str);
        listen(dev, 1);
        len = sizeof(client_adr);
        SocketNo = accept(dev, (struct sockaddr *) &client_adr, &len);
        if (SocketNo <= 0)
        {
            logmsg(LOG_ERR, "Failed to start main module");
            return 0;
        }
    }

    if (dev)
        close(dev);
    unlink(SOCKET_FILE);

    return SocketNo;
}

//Initial Widget setup
static void OpenStatusBox()
{
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), WIN_WIDTH, WIN_HEIGHT);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_set_name(window, "mywindow");

//  Grids are the preferred container since GTK+3 was released
    grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_widget_set_name(grid, "mygrid");

//  A blank label to help position the message label properly
    space = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid), space, 0, 0, 3, 1);
    gtk_widget_set_vexpand(space, FALSE);
    gtk_widget_set_hexpand(space, FALSE);

//  This is the label that status messages are displayed in
    label = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 2, 1);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_vexpand(label, FALSE);
    gtk_label_set_width_chars(GTK_LABEL(label), WIN_WIDTH/20);
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(label),0);
    gtk_label_set_yalign(GTK_LABEL(label),0.9);
    gtk_widget_set_name(label, "mylabel");

    gtk_widget_show_all(window);
}

void Css()
{
    provider = gtk_css_provider_new ();

    const char* vtpos_css = "/usr/viewtouch/css/vtpos.css";

    gtk_style_context_add_provider_for_screen (screen,
                                 GTK_STYLE_PROVIDER (provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

// CSS can be embedded in the code or imported from a file.  A demonstration
// of both methods are included.  The embedded method (gtk_css_provider_load_from_data)
// is commented out.  The import method (gtk_css_provider_load_from_file) with the
// associated vtpos.css file allows for styling changes without recompiling.
/*
    gtk_css_provider_load_from_data (GTK_CSS_PROVIDER(provider),
                                   " #mylabel {\n"
                                   "   background-color: rgba(0, 0, 0, 0);\n"
                                   "   font: Times Bold 10;\n"
                                   "   color: white;\n"
                                   "}\n"
                                   " #mygrid {\n"
                                   "   background-color: rgba(0, 0, 0, 0);\n"
                                   "}\n"
                                   " #mywindow {\n"
                                   "   background-color: black;\n"
                                   "   background: url('/usr/viewtouch/graphics/vtlogo.xpm');\n"
                                   "   background-size: 640px 240px;\n"
                                   "   background-position: center;\n"
                                   "   background-repeat: no-repeat;\n"
                                   "}\n", -1, NULL);
*/
//  Imports the CSS from file: /usr/viewtouch/dat/vtpos.css.
//  The viewtouch logo image (vtlogo.xpm) must also be located in /usr/viewtouch/dat/
    gtk_css_provider_load_from_file(GTK_CSS_PROVIDER(provider), g_file_new_for_path(vtpos_css), NULL);

    g_object_unref (provider);
}

int WriteArgList(int argc, genericChar* argv[])
{
    int retval = 0, argidx = 0, fd = -1;

    fd = open(COMMAND_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0700);

    if (fd >= 0)
    {
        for (argidx = 0; argidx < argc; argidx++)
        {
            write(fd, argv[argidx], strlen(argv[argidx]));
            write(fd, " ", 1);
        }
        close(fd);
        chmod(COMMAND_FILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        retval = 1;
    }
    return retval;
}

void SignalFn(int my_signal)
{
    logmsg(LOG_ERR, "Caught fatal signal (%d), exiting.\n", my_signal);
    ExitLoader();
}

void SetPerms()
{
    genericChar emp_data[] = VIEWTOUCH_PATH "/dat/employee.dat.bak";

    int fd = -1;

    if ((fd = open(emp_data, O_RDONLY)) >= 0) {
        /* chmod 666 file */
        fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        close(fd);
    }
}

int main(int argc, genericChar* argv[])
{
    SetPerms();

    signal(SIGINT,  SignalFn);

    int net_off = 0;
    int purge = 0;
    int notrace = 0;
    const char* data_path = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "help") == 0)
        {
            printf("Command line options:\n"
                   " path    or -p <dirname> specify data directory\n"
                   " help    or -h           display this help message\n"
                   " netoff  or -n           no network devices started\n"
#if DEBUG
                   " notrace or -t           disable FnTrace, debug mode only\n"
#endif
                   " version or -v           display the build number and exit\n\n");
            return 0;
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "path") == 0)
        {
            ++i;
            if (i >= argc)
            {
                logmsg(LOG_ERR, "No path name given");
                return 1;
            }
            data_path = argv[i];
        }
        else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "netoff") == 0)
        {
            net_off = 1;
        }
        else if (strcmp(argv[i], "purge") == 0)
        {
            purge = 1;
        }
#if DEBUG
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "notrace") == 0)
        {
            notrace = 1;
        }
#endif
        else if (strcmp(argv[i], "version") == 0)
        {
            // return version for vt_update
            printf("1\n");
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0)
        {
            printf("POS Version:  %s\n", BuildNumber);
            exit(1);
        }
    }

    // Write command line options to a file so that vt_main knows how to restart
    // the system
    WriteArgList(argc, argv);

    SocketNum = SetupConnection(SOCKET_FILE);

    gtk_init(&argc, &argv);

    display = gdk_display_get_default ();
    screen = gdk_display_get_default_screen (display);

    OpenStatusBox();

    Css();

    // Send Commands
    const genericChar* displaystr = gdk_display_get_name(display);
    int displaylen = strlen(displaystr);
    write(SocketNo, "display ", 8);
    write(SocketNo, displaystr, displaylen + 1);
    if (data_path)
    {
        write(SocketNo, "datapath ", 9);
        write(SocketNo, data_path, strlen(data_path)+1);
    }
    if (net_off)
        write(SocketNo, "netoff", 7);
    if (purge)
        write(SocketNo, "purge", 6);
    if (notrace)
        write(SocketNo, "notrace", 8);
    write(SocketNo, "done", 5);

    loadstatus = g_io_channel_unix_new(SocketNum);

    g_io_add_watch(loadstatus, G_IO_IN, SocketInputCB, NULL);

    gtk_main();

    return (0);
}
