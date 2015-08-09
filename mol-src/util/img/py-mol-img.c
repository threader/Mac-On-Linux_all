/*********************************************
 * Some Python -> C bindings for mol-img
 * 2007 - Joseph Jezak
 *********************************************/

#include <Python.h>
#include "mol-img.h"

/******************
 * Python Bindings
 ******************/

/* Create a qcow disk image */
static PyObject * create_qcow(PyObject *type, PyObject *args) {
	char * file;
	int size;

	if (!PyArg_ParseTuple(args, "sl", &file, &size))
		return NULL;
	
	return Py_BuildValue("i", create_img_qcow(file, size * SIZE_MB));
}

/* Create a raw disk image */
static PyObject * create_raw(PyObject *type, PyObject *args) {
	char * file;
	int size;

	if (!PyArg_ParseTuple(args, "sl", &file, &size))
		return NULL;
	
	return Py_BuildValue("i", create_img_raw(file, size * SIZE_MB));
}

/* Exported Methods */
static struct PyMethodDef molimg_methods[] = {
	{ "create_qcow", create_qcow, METH_VARARGS, "Create a QCOW disk image"},
	{ "create_raw", create_raw, METH_VARARGS, "Create a RAW disk image"},
	{ NULL, NULL, 0, NULL},
};

/* Init Module */
PyMODINIT_FUNC
initmolimg(void) {
	(void) Py_InitModule3("molimg", molimg_methods, "Create QCOW and RAW disk images");
}
