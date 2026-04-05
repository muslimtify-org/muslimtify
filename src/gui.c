#include <gio/gio.h>
#include <gtk/gtk.h>

static void activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Hello World");
  gtk_window_set_default_size(GTK_WINDOW(window), 320, 240);
  gtk_window_present(GTK_WINDOW(window));
}

int show_gui(int argc, char **argv) {
  GtkApplication *app =
      gtk_application_new("com.github.rizki.muslimtify", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
