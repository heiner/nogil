#ifndef Py_INTERNAL_OBJECT_H
#define Py_INTERNAL_OBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_gc.h"         // _PyObject_GC_IS_TRACKED()
#include "pycore_interp.h"     // PyInterpreterState.gc
#include "pycore_pystate.h"    // _PyThreadState_GET()

struct _PyWeakrefBase;

struct _PyWeakrefControl {
    struct _PyWeakrefBase base;

    _PyMutex mutex;

    /* The object to which this is a weak reference, or Py_None if none.
     * Note that this is a stealth reference:  wr_object's refcount is
     * not incremented to reflect this pointer.
     */
    PyObject *wr_object;
};

typedef struct _PyWeakrefControl PyWeakrefControl;
typedef struct _PyWeakrefBase PyWeakrefBase;

PyAPI_FUNC(int) _PyType_CheckConsistency(PyTypeObject *type);
PyAPI_FUNC(int) _PyDict_CheckConsistency(PyObject *mp, int check_content);
PyAPI_FUNC(void) _PyObject_Dealloc(PyObject *self);

/* Tell the GC to track this object.
 *
 * NB: While the object is tracked by the collector, it must be safe to call the
 * ob_traverse method.
 *
 * Internal note: interp->gc.generation0->_gc_prev doesn't have any bit flags
 * because it's not object header.  So we don't use _PyGCHead_PREV() and
 * _PyGCHead_SET_PREV() for it to avoid unnecessary bitwise operations.
 *
 * The PyObject_GC_Track() function is the public version of this macro.
 */
static inline void _PyObject_GC_TRACK_impl(const char *filename, int lineno,
                                           PyObject *op)
{
    _PyObject_ASSERT_FROM(op, !_PyObject_GC_IS_TRACKED(op),
                          "object already tracked by the garbage collector",
                          filename, lineno, "_PyObject_GC_TRACK");

    PyGC_Head *gc = _Py_AS_GC(op);
    gc->_gc_prev |= _PyGC_PREV_MASK_TRACKED;
}

#define _PyObject_GC_TRACK(op) \
    _PyObject_GC_TRACK_impl(__FILE__, __LINE__, _PyObject_CAST(op))

/* Tell the GC to stop tracking this object.
 *
 * Internal note: This may be called while GC. So _PyGC_PREV_MASK_COLLECTING
 * must be cleared. But _PyGC_PREV_MASK_FINALIZED bit is kept.
 *
 * The object must be tracked by the GC.
 *
 * The PyObject_GC_UnTrack() function is the public version of this macro.
 */
static inline void _PyObject_GC_UNTRACK_impl(const char *filename, int lineno,
                                             PyObject *op)
{
    _PyObject_ASSERT_FROM(op, _PyObject_GC_IS_TRACKED(op),
                          "object not tracked by the garbage collector",
                          filename, lineno, "_PyObject_GC_UNTRACK");

    PyGC_Head *gc = _Py_AS_GC(op);
    if (gc->_gc_next != 0) {
        PyGC_Head *prev = _PyGCHead_PREV(gc);
        PyGC_Head *next = _PyGCHead_NEXT(gc);

        _PyGCHead_SET_NEXT(prev, next);
        _PyGCHead_SET_PREV(next, prev);

        gc->_gc_next = 0;
    }

    gc->_gc_prev &= _PyGC_PREV_MASK_FINALIZED;
}

#define _PyObject_GC_UNTRACK(op) \
    _PyObject_GC_UNTRACK_impl(__FILE__, __LINE__, _PyObject_CAST(op))

#define _PyObject_FROM_GC(g) ((PyObject *)(((PyGC_Head *)g)+1))

#ifdef Py_REF_DEBUG
extern void _PyDebug_PrintTotalRefs(void);
#endif

#ifdef Py_TRACE_REFS
extern void _Py_AddToAllObjects(PyObject *op, int force);
extern void _Py_PrintReferences(FILE *);
extern void _Py_PrintReferenceAddresses(FILE *);
#endif

static inline PyObject **
_PyObject_GET_WEAKREFS_LISTPTR(PyObject *op)
{
    Py_ssize_t offset = Py_TYPE(op)->tp_weaklistoffset;
    return (PyObject **)((char *)op + offset);
}

static inline PyWeakrefControl **
_PyObject_GET_WEAKREFS_CONTROLPTR(PyObject *op)
{
    Py_ssize_t offset = Py_TYPE(op)->tp_weaklistoffset;
    return (PyWeakrefControl **)((char *)op + offset);
}

static inline PyWeakrefControl *
_PyObject_GET_WEAKREF_CONTROL(PyObject *op)
{
    return _Py_atomic_load_ptr(_PyObject_GET_WEAKREFS_CONTROLPTR(op));
}

// Fast inlined version of PyType_HasFeature()
static inline int
_PyType_HasFeature(PyTypeObject *type, unsigned long feature) {
    return ((type->tp_flags & feature) != 0);
}

// Fast inlined version of PyObject_IS_GC()
static inline int
_PyObject_IS_GC(PyObject *obj)
{
    return (PyType_IS_GC(Py_TYPE(obj))
            && (Py_TYPE(obj)->tp_is_gc == NULL
                || Py_TYPE(obj)->tp_is_gc(obj)));
}

// Fast inlined version of PyType_IS_GC()
#define _PyType_IS_GC(t) _PyType_HasFeature((t), Py_TPFLAGS_HAVE_GC)

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_OBJECT_H */
