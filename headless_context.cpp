#include <Python.h>
#include <Windows.h>

struct Context {
    PyObject_HEAD
    int reusable;
    int destroyed;
    int enabled;
    HMODULE opengl32;
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    HDC restore_hdc;
    HGLRC restore_hglrc;
};

static PyTypeObject * Context_type;

static void Context_dealloc(Context * self) {
    if (self->enabled) {
        wglMakeCurrent(self->restore_hdc, self->restore_hglrc);
        self->enabled = false;
    }
    if (!self->destroyed) {
        wglDeleteContext(self->hglrc);
        ReleaseDC(self->hwnd, self->hdc);
        DestroyWindow(self->hwnd);
        self->hwnd = NULL;
        self->hdc = NULL;
        self->hglrc = NULL;
        self->destroyed = true;
    }
    PyObject_Del(self);
}

static int Context_init(Context * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"reusable", NULL};

    int reusable = false;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|p", (char **)keywords, &reusable)) {
        return -1;
    }

    HINSTANCE hinst = GetModuleHandle(NULL);
    WNDCLASS wndclass = {CS_OWNDC, DefWindowProc, 0, 0, hinst, NULL, NULL, NULL, NULL, "headless_context"};
    RegisterClass(&wndclass);
    self->opengl32 = GetModuleHandle("opengl32");
    self->hwnd = CreateWindow("headless_context", NULL, 0, 0, 0, 0, 0, NULL, NULL, hinst, NULL);
    self->hdc = GetDC(self->hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    DescribePixelFormat(self->hdc, 1, sizeof(pfd), &pfd);
    SetPixelFormat(self->hdc, 1, &pfd);
    self->hglrc = wglCreateContext(self->hdc);
    self->reusable = reusable;
    self->destroyed = false;
    self->enabled = false;
    self->restore_hdc = NULL;
    self->restore_hglrc = NULL;
    return 0;
}

static PyObject * Context_meth_enter(Context * self, PyObject * args) {
    if (self->enabled) {
        PyErr_Format(PyExc_RuntimeError, "Context is already enabled");
        return NULL;
    }
    if (self->destroyed) {
        PyErr_Format(PyExc_RuntimeError, "Context is destroyed");
        return NULL;
    }
    self->restore_hdc = wglGetCurrentDC();
    self->restore_hglrc = wglGetCurrentContext();
    wglMakeCurrent(self->hdc, self->hglrc);
    self->enabled = true;
    Py_RETURN_NONE;
}

static PyObject * Context_meth_exit(Context * self, PyObject * args, PyObject * kwargs) {
    if (!self->enabled) {
        PyErr_Format(PyExc_RuntimeError, "Context is not enabled");
        return NULL;
    }
    if (!self->reusable) {
        wglDeleteContext(self->hglrc);
        ReleaseDC(self->hwnd, self->hdc);
        DestroyWindow(self->hwnd);
        self->hwnd = NULL;
        self->hdc = NULL;
        self->hglrc = NULL;
        self->destroyed = true;
    }
    wglMakeCurrent(self->restore_hdc, self->restore_hglrc);
    self->restore_hdc = NULL;
    self->restore_hglrc = NULL;
    self->enabled = false;
    Py_RETURN_NONE;
}

PyObject * Context_meth_load_opengl_function(Context * self, PyObject * arg) {
    if (!self->enabled) {
        PyErr_Format(PyExc_RuntimeError, "Context is not enabled");
        return NULL;
    }
    if (!PyUnicode_CheckExact(arg)) {
        return NULL;
    }
    const char * name = PyUnicode_AsUTF8(arg);
    void * proc = (void *)GetProcAddress(self->opengl32, name);
    if (!proc) {
        proc = (void *)wglGetProcAddress(name);
    }
    return PyLong_FromVoidPtr(proc);
}

static PyMethodDef Context_methods[] = {
    {"__enter__", (PyCFunction)Context_meth_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)Context_meth_exit, METH_VARARGS | METH_KEYWORDS},
    {"load_opengl_function", (PyCFunction)Context_meth_exit, METH_O},
    {},
};

static PyType_Slot Context_slots[] = {
    {Py_tp_init, Context_init},
    {Py_tp_methods, Context_methods},
    {Py_tp_dealloc, Context_dealloc},
    {},
};

static PyType_Spec Context_spec = {"Context", sizeof(Context), 0, Py_TPFLAGS_DEFAULT, Context_slots};

static PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "headless_context", NULL, -1, NULL};

extern "C" PyObject * PyInit_headless_context() {
    PyObject * module = PyModule_Create(&module_def);
    Context_type = (PyTypeObject *)PyType_FromSpec(&Context_spec);
    PyModule_AddObject(module, "Context", (PyObject *)Context_type);
    return module;
}
