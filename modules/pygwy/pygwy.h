#ifndef _PYGWY_H
#define _PYGWY_H

void destroy_environment(PyObject *d, gboolean show_errors);
PyObject* create_environment(const gchar *filename, gboolean show_errors);
void pygwy_initialize(void);
PyObject* pygwy_run_string(const char *cmd, int type, PyObject *g, PyObject *l);
#endif
