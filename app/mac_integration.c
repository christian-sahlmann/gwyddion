
#ifdef __APPLE__
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <file.h>
#include "ige-mac-menu.h"
#define USE_MAC_INTEGRATION
#define USED_ON_MAC /* */
#else
#define USED_ON_MAC G_GNUC_UNUSED
#endif

#include "mac_integration.h"

void
gwy_osx_get_menu_from_widget(USED_ON_MAC GtkWidget *container)
{
#ifdef USE_MAC_INTEGRATION
    GList *children;            //,*subchildren,*subsubchildren;
    GList *l, *ll, *lll;
    GtkWidget *menubar = gtk_menu_bar_new();

    gtk_widget_set_name(menubar, "toolboxmenubar");
    children = gtk_container_get_children(GTK_CONTAINER(container));
    for (l = children; l; l = l->next) {
        GtkWidget *widget = l->data;

        if (GTK_IS_CONTAINER(widget)) {
            children = gtk_container_get_children(GTK_CONTAINER(widget));
            for (ll = children; ll; ll = ll->next) {
                GtkWidget *subwidget = ll->data;

                if (GTK_IS_CONTAINER(subwidget)) {
                    children =
                        gtk_container_get_children(GTK_CONTAINER(subwidget));
                    for (lll = children; lll; lll = lll->next) {
                        GtkWidget *subsubwidget = lll->data;

                        if (GTK_IS_MENU_ITEM(subsubwidget)) {
                            gtk_widget_hide(widget);
                            gtk_menu_shell_append(GTK_MENU_SHELL(menubar),
                                                  subsubwidget);
                        }
                    }
                }
            }
        }
    }
    gtk_container_add(GTK_CONTAINER(container), menubar);
    gtk_widget_hide(menubar);
    ige_mac_menu_set_menu_bar(GTK_MENU_SHELL(menubar));
#endif
}


#ifdef USE_MAC_INTEGRATION
gchar *gwy_osx_find_dir_in_bundle(const gchar *dirname);
int fileModulesReady = 0;
GPtrArray *files_array = NULL;

static void
gwy_osx_open_file(gpointer data, gpointer user_data)
{
    GString *str = (GString *) data;

    gwy_app_file_load(str->str, str->str, NULL);
    g_string_free(str, TRUE);
}

static OSStatus
appleEventHandler(const AppleEvent * event, AppleEvent * event2, long param)
{
#define BUFLEN 1024
    AEDescList docs;

    if (AEGetParamDesc(event, keyDirectObject, typeAEList, &docs) == noErr) {
        long n = 0;
        int i;

        AECountItems(&docs, &n);
        static UInt8 strBuffer[BUFLEN];

        for (i = 0; i < n; i++) {
            FSRef ref;

            if (AEGetNthPtr(&docs, i + 1, typeFSRef, 0, 0, &ref, sizeof(ref), 0)
                != noErr)
                continue;
            if (FSRefMakePath(&ref, strBuffer, BUFLEN)
                == noErr) {
                GString *str = g_strdup(strBuffer);     //g_string_new((const gchar*)strBuffer);

                if (fileModulesReady)
                    gwy_osx_open_file(str, NULL);
                else {
                    if (!files_array)
                        files_array = g_ptr_array_new();
                    g_ptr_array_add(files_array, str);
                }
            }
        }
    }
    return noErr;
}

#endif
void
gwy_osx_init_handler(USED_ON_MAC int *argc)
{
#ifdef USE_MAC_INTEGRATION
    gchar *tmp = gwy_osx_find_dir_in_bundle("data");

    if (tmp)
        free(tmp);              // check if inside bundle
    else
        *argc = 1;              // command line options not available in app bundles

    AEInstallEventHandler(kCoreEventClass,      // handle open file events
                          kAEOpenDocuments,
                          (AEEventHandlerUPP) appleEventHandler, 0, false);
#endif
}

void
gwy_osx_remove_handler(void)
{
#ifdef USE_MAC_INTEGRATION
    AERemoveEventHandler(kCoreEventClass,
                         kAEOpenDocuments,
                         (AEEventHandlerUPP) appleEventHandler, false);
#endif
}

void
gwy_osx_open_files(void)
{
#ifdef USE_MAC_INTEGRATION
    if (files_array) {
        g_ptr_array_foreach(files_array, gwy_osx_open_file, NULL);
        g_ptr_array_free(files_array, FALSE);
        files_array = NULL;
    }
    fileModulesReady = 1;
#endif
}

void
gwy_osx_set_locale()
{
#ifdef USE_MAC_INTEGRATION
    static const struct {
        const gchar *locale;
        const gchar *lang;
    } locales[] = {
        {
        "cs_CS.UTF-8", "cs"}, {
        "de_DE.UTF-8", "de"}, {
        "", "en"}, {
        "fr_FR.UTF-8", "fr"}, {
        "it_IT.UTF-8", "it"}, {
        "ru_RU.UTF-8", "ru"}
    };

    CFTypeRef preferences = CFPreferencesCopyAppValue(CFSTR("AppleLanguages"),
                                                      kCFPreferencesCurrentApplication);

    if (preferences != NULL && CFGetTypeID(preferences) == CFArrayGetTypeID()) {
        CFArrayRef prefArray = (CFArrayRef) preferences;
        int n = CFArrayGetCount(prefArray);
        static char buf[256];
        int i;

        for (i = 0; i < n; i++) {
            CFTypeRef element = CFArrayGetValueAtIndex(prefArray, i);

            if (element != NULL && CFGetTypeID(element) == CFStringGetTypeID()
                && CFStringGetCString((CFStringRef) element,
                                      buf, sizeof(buf),
                                      kCFStringEncodingASCII)) {
                int j;

                for (j = 0; j < G_N_ELEMENTS(locales); j++) {
                    if (strcmp(locales[j].lang, buf) == 0) {
                        g_setenv("LANG", locales[j].locale, TRUE);
                        goto exit;
                    }
                }
            }
        }
      exit:
        CFRelease(preferences);
    }
#endif
}
