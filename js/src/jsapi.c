/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * JavaScript API.
 */
#include "jsstddef.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsclist.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdate.h"
#include "jsemit.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsregexp.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#if JS_HAS_FILE_OBJECT
#include "jsfile.h"
#endif

#ifdef HAVE_VA_LIST_AS_ARRAY
#define JS_ADDRESSOF_VA_LIST(ap) (ap)
#else
#define JS_ADDRESSOF_VA_LIST(ap) (&(ap))
#endif

#if defined(JS_PARANOID_REQUEST) && defined(JS_THREADSAFE)
#define CHECK_REQUEST(cx)	JS_ASSERT(cx->requestDepth)
#else
#define CHECK_REQUEST(cx)	((void)0)
#endif

JS_PUBLIC_API(jsval)
JS_GetNaNValue(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return DOUBLE_TO_JSVAL(cx->runtime->jsNaN);
}

JS_PUBLIC_API(jsval)
JS_GetNegativeInfinityValue(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return DOUBLE_TO_JSVAL(cx->runtime->jsNegativeInfinity);
}

JS_PUBLIC_API(jsval)
JS_GetPositiveInfinityValue(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return DOUBLE_TO_JSVAL(cx->runtime->jsPositiveInfinity);
}

JS_PUBLIC_API(jsval)
JS_GetEmptyStringValue(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return STRING_TO_JSVAL(cx->runtime->emptyString);
}

static JSBool
TryArgumentFormatter(JSContext *cx, const char **formatp, JSBool fromJS,
		     jsval **vpp, va_list *app)
{
    const char *format;
    JSArgumentFormatMap *map;

    format = *formatp;
    for (map = cx->argumentFormatMap; map; map = map->next) {
	if (!strncmp(format, map->format, map->length)) {
	    *formatp = format + map->length;
	    return map->formatter(cx, format, fromJS, vpp, app);
	}
    }
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_CHAR, format);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ConvertArguments(JSContext *cx, uintN argc, jsval *argv, const char *format,
		    ...)
{
    va_list ap;
    JSBool ok;

    va_start(ap, format);
    ok = JS_ConvertArgumentsVA(cx, argc, argv, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ConvertArgumentsVA(JSContext *cx, uintN argc, jsval *argv,
		      const char *format, va_list ap)
{
    jsval *sp;
    JSBool required;
    char c;
    JSFunction *fun;
    jsdouble d;
    JSString *str;
    JSObject *obj;

    CHECK_REQUEST(cx);
    sp = argv;
    required = JS_TRUE;
    while ((c = *format++) != '\0') {
	if (isspace(c))
	    continue;
	if (c == '/') {
	    required = JS_FALSE;
	    continue;
	}
	if (sp == argv + argc) {
	    if (required) {
		fun = js_ValueToFunction(cx, &argv[-2], JS_FALSE);
		if (fun) {
		    char numBuf[12];
		    JS_snprintf(numBuf, sizeof numBuf, "%u", argc);
		    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
					 JSMSG_MORE_ARGS_NEEDED,
					 JS_GetFunctionName(fun), numBuf,
					 (argc == 1) ? "" : "s");
		}
		return JS_FALSE;
	    }
	    break;
	}
	switch (c) {
	  case 'b':
	    if (!js_ValueToBoolean(cx, *sp, va_arg(ap, JSBool *)))
		return JS_FALSE;
	    break;
	  case 'c':
	    if (!js_ValueToUint16(cx, *sp, va_arg(ap, uint16 *)))
		return JS_FALSE;
	    break;
	  case 'i':
	    if (!js_ValueToECMAInt32(cx, *sp, va_arg(ap, int32 *)))
		return JS_FALSE;
	    break;
	  case 'u':
	    if (!js_ValueToECMAUint32(cx, *sp, va_arg(ap, uint32 *)))
		return JS_FALSE;
	    break;
	  case 'j':
	    if (!js_ValueToInt32(cx, *sp, va_arg(ap, int32 *)))
		return JS_FALSE;
	    break;
	  case 'd':
	    if (!js_ValueToNumber(cx, *sp, va_arg(ap, jsdouble *)))
		return JS_FALSE;
	    break;
	  case 'I':
	    if (!js_ValueToNumber(cx, *sp, &d))
		return JS_FALSE;
	    *va_arg(ap, jsdouble *) = js_DoubleToInteger(d);
	    break;
	  case 's':
	  case 'S':
	  case 'W':
	    str = js_ValueToString(cx, *sp);
	    if (!str)
		return JS_FALSE;
	    *sp = STRING_TO_JSVAL(str);
	    if (c == 's')
		*va_arg(ap, char **) = JS_GetStringBytes(str);
	    else if (c == 'W')
		*va_arg(ap, jschar **) = str->chars;
	    else
		*va_arg(ap, JSString **) = str;
	    break;
	  case 'o':
	    if (!js_ValueToObject(cx, *sp, &obj))
		return JS_FALSE;
	    *sp = OBJECT_TO_JSVAL(obj);
	    *va_arg(ap, JSObject **) = obj;
	    break;
	  case 'f':
	    fun = js_ValueToFunction(cx, sp, JS_FALSE);
	    if (!fun)
		return JS_FALSE;
	    *sp = OBJECT_TO_JSVAL(fun->object);
	    *va_arg(ap, JSFunction **) = fun;
	    break;
	  case 'v':
	    *va_arg(ap, jsval *) = *sp;
	    break;
	  case '*':
	    break;
	  default:
	    format--;
	    if (!TryArgumentFormatter(cx, &format, JS_TRUE, &sp, JS_ADDRESSOF_VA_LIST(ap)))
		return JS_FALSE;
	    /* NB: the formatter already updated sp, so we continue here. */
	    continue;
	}
	sp++;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(jsval *)
JS_PushArguments(JSContext *cx, void **markp, const char *format, ...)
{
    va_list ap;
    jsval *argv;

    va_start(ap, format);
    argv = JS_PushArgumentsVA(cx, markp, format, ap);
    va_end(ap);
    return argv;
}

JS_PUBLIC_API(jsval *)
JS_PushArgumentsVA(JSContext *cx, void **markp, const char *format, va_list ap)
{
    uintN argc;
    jsval *argv, *sp;
    char c;
    const char *cp;
    JSString *str;
    JSFunction *fun;

    CHECK_REQUEST(cx);
    *markp = NULL;
    argc = 0;
    for (cp = format; (c = *cp) != '\0'; cp++) {
	/*
	 * Count non-space non-star characters as individual jsval arguments.
	 * This may over-allocate stack, but we'll fix below.
	 */
	if (isspace(c) || c == '*')
	    continue;
	argc++;
    }
    sp = js_AllocStack(cx, argc, markp);
    if (!sp)
	return NULL;
    argv = sp;
    while ((c = *format++) != '\0') {
	if (isspace(c) || c == '*')
	    continue;
	switch (c) {
	  case 'b':
	    *sp = BOOLEAN_TO_JSVAL((JSBool) va_arg(ap, int));
	    break;
	  case 'c':
	    *sp = INT_TO_JSVAL((uint16) va_arg(ap, unsigned int));
	    break;
	  case 'i':
	  case 'j':
	    if (!js_NewNumberValue(cx, (jsdouble) va_arg(ap, int32), sp))
		goto bad;
	    break;
	  case 'u':
	    if (!js_NewNumberValue(cx, (jsdouble) va_arg(ap, uint32), sp))
		goto bad;
	    break;
	  case 'd':
	  case 'I':
	    if (!js_NewDoubleValue(cx, va_arg(ap, jsdouble), sp))
		goto bad;
	    break;
	  case 's':
	    str = JS_NewStringCopyZ(cx, va_arg(ap, char *));
	    if (!str)
		goto bad;
	    *sp = STRING_TO_JSVAL(str);
	    break;
	  case 'W':
	    str = JS_NewUCStringCopyZ(cx, va_arg(ap, jschar *));
	    if (!str)
		goto bad;
	    *sp = STRING_TO_JSVAL(str);
	    break;
	  case 'S':
	    str = va_arg(ap, JSString *);
	    *sp = STRING_TO_JSVAL(str);
	    break;
	  case 'o':
	    *sp = OBJECT_TO_JSVAL(va_arg(ap, JSObject *));
	    break;
	  case 'f':
	    fun = va_arg(ap, JSFunction *);
	    *sp = fun ? OBJECT_TO_JSVAL(fun->object) : JSVAL_NULL;
	    break;
	  case 'v':
	    *sp = va_arg(ap, jsval);
	    break;
	  default:
	    format--;
	    if (!TryArgumentFormatter(cx, &format, JS_FALSE, &sp, JS_ADDRESSOF_VA_LIST(ap)))
		goto bad;
	    /* NB: the formatter already updated sp, so we continue here. */
	    continue;
	}
	sp++;
    }

    /*
     * We may have overallocated stack due to a multi-character format code
     * handled by a JSArgumentFormatter.  Give back that stack space!
     */
    JS_ASSERT(sp <= argv + argc);
    if (sp < argv + argc)
	cx->stackPool.current->avail = (jsuword)sp;
    return argv;

bad:
    js_FreeStack(cx, *markp);
    return NULL;
}

JS_PUBLIC_API(void)
JS_PopArguments(JSContext *cx, void *mark)
{
    CHECK_REQUEST(cx);
    js_FreeStack(cx, mark);
}

JS_PUBLIC_API(JSBool)
JS_AddArgumentFormatter(JSContext *cx, const char *format,
			JSArgumentFormatter formatter)
{
    size_t length;
    JSArgumentFormatMap **mpp, *map;

    length = strlen(format);
    mpp = &cx->argumentFormatMap;
    while ((map = *mpp) != NULL) {
	/* Insert before any shorter string to match before prefixes. */
	if (map->length < length)
	    break;
	if (map->length == length && !strcmp(map->format, format))
	    goto out;
	mpp = &map->next;
    }
    map = JS_malloc(cx, sizeof *map);
    if (!map)
	return JS_FALSE;
    map->format = format;
    map->length = length;
    map->next = *mpp;
    *mpp = map;
out:
    map->formatter = formatter;
    return JS_TRUE;
}

JS_PUBLIC_API(void)
JS_RemoveArgumentFormatter(JSContext *cx, const char *format)
{
    size_t length;
    JSArgumentFormatMap **mpp, *map;

    length = strlen(format);
    mpp = &cx->argumentFormatMap;
    while ((map = *mpp) != NULL) {
	if (map->length == length && !strcmp(map->format, format)) {
	    *mpp = map->next;
	    JS_free(cx, map);
	    return;
	}
	mpp = &map->next;
    }
}

JS_PUBLIC_API(JSBool)
JS_ConvertValue(JSContext *cx, jsval v, JSType type, jsval *vp)
{
    JSBool ok = JS_FALSE, b;
    JSObject *obj;
    JSFunction *fun;
    JSString *str;
    jsdouble d, *dp;

    CHECK_REQUEST(cx);
    switch (type) {
      case JSTYPE_VOID:
	*vp = JSVAL_VOID;
	break;
      case JSTYPE_OBJECT:
	ok = js_ValueToObject(cx, v, &obj);
	if (ok)
	    *vp = OBJECT_TO_JSVAL(obj);
	break;
      case JSTYPE_FUNCTION:
	fun = js_ValueToFunction(cx, &v, JS_FALSE);
	ok = (fun != NULL);
	if (ok)
	    *vp = OBJECT_TO_JSVAL(fun->object);
	break;
      case JSTYPE_STRING:
	str = js_ValueToString(cx, v);
	ok = (str != NULL);
	if (ok)
	    *vp = STRING_TO_JSVAL(str);
	break;
      case JSTYPE_NUMBER:
	ok = js_ValueToNumber(cx, v, &d);
	if (ok) {
	    dp = js_NewDouble(cx, d);
	    ok = (dp != NULL);
	    if (ok)
		*vp = DOUBLE_TO_JSVAL(dp);
	}
	break;
      case JSTYPE_BOOLEAN:
	ok = js_ValueToBoolean(cx, v, &b);
	if (ok)
	    *vp = BOOLEAN_TO_JSVAL(b);
	break;
      default: {
	char numBuf[12];
	JS_snprintf(numBuf, sizeof numBuf, "%d", (int)type);
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_TYPE,
			     numBuf);
	ok = JS_FALSE;
	break;
      }
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    CHECK_REQUEST(cx);
    return js_ValueToObject(cx, v, objp);
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToFunction(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return js_ValueToFunction(cx, &v, JS_FALSE);
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToConstructor(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return js_ValueToFunction(cx, &v, JS_TRUE);
}

JS_PUBLIC_API(JSString *)
JS_ValueToString(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return js_ValueToString(cx, v);
}

JS_PUBLIC_API(JSBool)
JS_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp)
{
    CHECK_REQUEST(cx);
    return js_ValueToNumber(cx, v, dp);
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAInt32(JSContext *cx, jsval v, int32 *ip)
{
    CHECK_REQUEST(cx);
    return js_ValueToECMAInt32(cx, v, ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAUint32(JSContext *cx, jsval v, uint32 *ip)
{
    CHECK_REQUEST(cx);
    return js_ValueToECMAUint32(cx, v, ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToInt32(JSContext *cx, jsval v, int32 *ip)
{
    CHECK_REQUEST(cx);
    return js_ValueToInt32(cx, v, ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToUint16(JSContext *cx, jsval v, uint16 *ip)
{
    CHECK_REQUEST(cx);
    return js_ValueToUint16(cx, v, ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp)
{
    CHECK_REQUEST(cx);
    return js_ValueToBoolean(cx, v, bp);
}

JS_PUBLIC_API(JSType)
JS_TypeOfValue(JSContext *cx, jsval v)
{
    JSType type = JSTYPE_VOID;
    JSObject *obj;
    JSObjectOps *ops;
    JSClass *clasp;

    CHECK_REQUEST(cx);
    if (JSVAL_IS_VOID(v)) {
	type = JSTYPE_VOID;
    } else if (JSVAL_IS_OBJECT(v)) {
	obj = JSVAL_TO_OBJECT(v);
	if (obj &&
	    (ops = obj->map->ops,
	     ops == &js_ObjectOps
	     ? (clasp = OBJ_GET_CLASS(cx, obj),
		clasp->call || clasp == &js_FunctionClass)
	     : ops->call != 0)) {
	    type = JSTYPE_FUNCTION;
	} else {
	    type = JSTYPE_OBJECT;
	}
    } else if (JSVAL_IS_NUMBER(v)) {
	type = JSTYPE_NUMBER;
    } else if (JSVAL_IS_STRING(v)) {
	type = JSTYPE_STRING;
    } else if (JSVAL_IS_BOOLEAN(v)) {
	type = JSTYPE_BOOLEAN;
    }
    return type;
}

JS_PUBLIC_API(const char *)
JS_GetTypeName(JSContext *cx, JSType type)
{
    CHECK_REQUEST(cx);
    if ((uintN)type >= (uintN)JSTYPE_LIMIT)
	return NULL;
    return js_type_str[type];
}

/************************************************************************/

JS_PUBLIC_API(JSRuntime *)
JS_NewRuntime(uint32 maxbytes)
{
    JSRuntime *rt;

#ifdef DEBUG
    JS_BEGIN_MACRO
    /*
     * This code asserts that the numbers associated with the error names in
     * jsmsg.def are monotonically increasing.  It uses values for the error
     * names enumerated in jscntxt.c.  It's not a compiletime check, but it's
     * better than nothing.
     */
    int errorNumber = 0;
#define MSG_DEF(name, number, count, exception, format) \
    JS_ASSERT(name == errorNumber++);
#include "js.msg"
#undef MSG_DEF
    JS_END_MACRO;
#endif /* DEBUG */

    if (!js_InitStringGlobals())
	return NULL;
    rt = malloc(sizeof(JSRuntime));
    if (!rt)
	return NULL;
    memset(rt, 0, sizeof(JSRuntime));
    if (!js_InitGC(rt, maxbytes))
	goto bad;
#ifdef JS_THREADSAFE
    rt->gcLock = JS_NEW_LOCK();
    if (!rt->gcLock)
	goto bad;
    rt->gcDone = JS_NEW_CONDVAR(rt->gcLock);
    if (!rt->gcDone)
	goto bad;
    rt->requestDone = JS_NEW_CONDVAR(rt->gcLock);
    if (!rt->requestDone)
	goto bad;
    js_SetupLocks(20,20);		/* this is asymmetric with JS_ShutDown. */
    rt->rtLock = JS_NEW_LOCK();
    rt->stateChange = JS_NEW_CONDVAR(rt->rtLock);
#endif
    rt->propertyCache.empty = JS_TRUE;
    JS_INIT_CLIST(&rt->contextList);
    JS_INIT_CLIST(&rt->trapList);
    JS_INIT_CLIST(&rt->watchPointList);
    return rt;

bad:
    JS_DestroyRuntime(rt);
    return NULL;
}

JS_PUBLIC_API(void)
JS_DestroyRuntime(JSRuntime *rt)
{
    js_FinishGC(rt);
#ifdef JS_THREADSAFE
    if (rt->gcLock)
	JS_DESTROY_LOCK(rt->gcLock);
    if (rt->gcDone)
	JS_DESTROY_CONDVAR(rt->gcDone);
    if (rt->requestDone)
	JS_DESTROY_CONDVAR(rt->requestDone);
    JS_DESTROY_LOCK(rt->rtLock);
    JS_DESTROY_CONDVAR(rt->stateChange);
#endif
    free(rt);
}

JS_PUBLIC_API(void)
JS_ShutDown(void)
{
    js_FreeStringGlobals();
#ifdef JS_THREADSAFE
    js_CleanupLocks();
#endif
}

#ifdef JS_THREADSAFE

JS_PUBLIC_API(void)
JS_BeginRequest(JSContext *cx)
{
    JSRuntime *rt;

    if (!cx->requestDepth) {
	/* Wait until the GC is finished. */
	rt = cx->runtime;
	JS_LOCK_GC(rt);
	while (rt->gcLevel > 0)
	    JS_AWAIT_GC_DONE(rt);

	/* Indicate that a request is running. */
	rt->requestCount++;
	JS_UNLOCK_GC(rt);
    }
    cx->requestDepth++;
}

JS_PUBLIC_API(void)
JS_EndRequest(JSContext *cx)
{
    JSRuntime *rt;

    CHECK_REQUEST(cx);
    cx->requestDepth--;
    if (!cx->requestDepth) {
	rt = cx->runtime;
	JS_LOCK_GC(rt);
	JS_ASSERT(rt->requestCount > 0);
	rt->requestCount--;
        if (rt->requestCount == 0)
	    JS_NOTIFY_REQUEST_DONE(rt);
	JS_UNLOCK_GC(rt);
    }
}

/* Yield to pending GC operations, regardless of request depth */
JS_PUBLIC_API(void)
JS_YieldRequest(JSContext *cx)
{
    JSRuntime *rt;

    CHECK_REQUEST(cx);
    rt = cx->runtime;
    JS_LOCK_GC(rt);
    JS_ASSERT(rt->requestCount > 0);
    rt->requestCount--;
    if (rt->requestCount == 0)
        JS_NOTIFY_REQUEST_DONE(rt);
    JS_UNLOCK_GC(rt);
    /* XXXbe give the GC or another request calling it a chance to run here?
             Assumes FIFO scheduling */
    JS_LOCK_GC(rt);
    rt->requestCount++;
    JS_UNLOCK_GC(rt);
}

/* Like JS_EndRequest, but don't notify any GC waiting in the wings. */
JS_PUBLIC_API(void)
JS_SuspendRequest(JSContext *cx)
{
    JSRuntime *rt;

    CHECK_REQUEST(cx);
    cx->requestDepth--;
    if (!cx->requestDepth) {
	rt = cx->runtime;
	JS_LOCK_GC(rt);
	JS_ASSERT(rt->requestCount > 0);
	rt->requestCount--;
	JS_UNLOCK_GC(rt);
    }
}

JS_PUBLIC_API(void)
JS_ResumeRequest(JSContext *cx)
{
    JSRuntime *rt;

    if (!cx->requestDepth) {
	/* Wait until the GC is finished. */
	rt = cx->runtime;
	JS_LOCK_GC(rt);
	while (rt->gcLevel > 0)
	    JS_AWAIT_GC_DONE(rt);

	/* Indicate that a request is running. */
	rt->requestCount++;
	JS_UNLOCK_GC(rt);
    }
    cx->requestDepth++;
}

#endif /* JS_THREADSAFE */

JS_PUBLIC_API(void)
JS_Lock(JSRuntime *rt)
{
    JS_LOCK_RUNTIME(rt);
}

JS_PUBLIC_API(void)
JS_Unlock(JSRuntime *rt)
{
    JS_UNLOCK_RUNTIME(rt);
}

JS_PUBLIC_API(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stacksize)
{
    return js_NewContext(rt, stacksize);
}

JS_PUBLIC_API(void)
JS_DestroyContext(JSContext *cx)
{
    js_DestroyContext(cx, JS_FORCE_GC);
}

JS_PUBLIC_API(void)
JS_DestroyContextNoGC(JSContext *cx)
{
    js_DestroyContext(cx, JS_NO_GC);
}

JS_PUBLIC_API(void)
JS_DestroyContextMaybeGC(JSContext *cx)
{
    js_DestroyContext(cx, JS_MAYBE_GC);
}

JS_PUBLIC_API(void*)
JS_GetContextPrivate(JSContext *cx)
{
    return cx->data;
}

JS_PUBLIC_API(void)
JS_SetContextPrivate(JSContext *cx, void *data)
{
    cx->data = data;
}

JS_PUBLIC_API(JSRuntime *)
JS_GetRuntime(JSContext *cx)
{
    return cx->runtime;
}

JS_PUBLIC_API(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp)
{
    return js_ContextIterator(rt, iterp);
}

JS_PUBLIC_API(JSVersion)
JS_GetVersion(JSContext *cx)
{
    return cx->version;
}

JS_PUBLIC_API(JSVersion)
JS_SetVersion(JSContext *cx, JSVersion version)
{
    JSVersion oldVersion;

    CHECK_REQUEST(cx);
    oldVersion = cx->version;
    if (version == oldVersion)
        return oldVersion;

    cx->version = version;

#if !JS_BUG_FALLIBLE_EQOPS
    if (cx->version == JSVERSION_1_2) {
	cx->jsop_eq = JSOP_NEW_EQ;
	cx->jsop_ne = JSOP_NEW_NE;
    } else {
	cx->jsop_eq = JSOP_EQ;
	cx->jsop_ne = JSOP_NE;
    }
#endif /* !JS_BUG_FALLIBLE_EQOPS */

#if JS_HAS_EXPORT_IMPORT
    /* XXX this might fail due to low memory */
    js_InitScanner(cx);
#endif /* JS_HAS_EXPORT_IMPORT */

    return oldVersion;
}

static struct v2smap {
    JSVersion   version;
    const char  *string;
} v2smap[] = {
    {JSVERSION_1_0,     "1.0"},
    {JSVERSION_1_1,     "1.1"},
    {JSVERSION_1_2,     "1.2"},
    {JSVERSION_1_3,     "1.3"},
    {JSVERSION_1_4,     "1.4"},
    {JSVERSION_1_5,     "1.5"},
    {JSVERSION_DEFAULT, "default"},
    {JSVERSION_UNKNOWN, NULL},          /* must be last, NULL is sentinel */
};

JS_PUBLIC_API(const char *)
JS_VersionToString(JSVersion version)
{
    int i;

    for (i = 0; v2smap[i].string; i++)
        if (v2smap[i].version == version)
            return v2smap[i].string;
    return "unknown";
}

JS_PUBLIC_API(JSVersion)
JS_StringToVersion(const char *string)
{
    int i;

    for (i = 0; v2smap[i].string; i++)
        if (strcmp(v2smap[i].string, string) == 0)
            return v2smap[i].version;
    return JSVERSION_UNKNOWN;
}

JS_PUBLIC_API(uint32)
JS_GetOptions(JSContext *cx)
{
    return cx->options;
}

JS_PUBLIC_API(uint32)
JS_SetOptions(JSContext *cx, uint32 options)
{
    uint32 oldopts = cx->options;
    cx->options = options;
    return oldopts;
}

JS_PUBLIC_API(uint32)
JS_ToggleOptions(JSContext *cx, uint32 options)
{
    uint32 oldopts = cx->options;
    cx->options ^= options;
    return oldopts;
}

JS_PUBLIC_API(const char *)
JS_GetImplementationVersion(void)
{
    return "JavaScript-C 1.5 pre-release 1 1999 10 31";
}


JS_PUBLIC_API(JSObject *)
JS_GetGlobalObject(JSContext *cx)
{
    return cx->globalObject;
}

JS_PUBLIC_API(void)
JS_SetGlobalObject(JSContext *cx, JSObject *obj)
{
    cx->globalObject = obj;
}

JS_PUBLIC_API(JSBool)
JS_InitStandardClasses(JSContext *cx, JSObject *obj)
{
    JSObject *fun_proto, *obj_proto, *array_proto;

    CHECK_REQUEST(cx);
    /* If cx has no global object, use obj so prototypes can be found. */
    if (!cx->globalObject)
	cx->globalObject = obj;

#if JS_HAS_UNDEFINED
    /*
     * Define a top-level property 'undefined' with the undefined value.
     * (proposed ECMA v2, now in ECMA ed3?)
     */
    if (!OBJ_DEFINE_PROPERTY(cx, obj,
			     (jsid)cx->runtime->atomState.typeAtoms[JSTYPE_VOID],
			     JSVAL_VOID, NULL, NULL, 0, NULL))
	return JS_FALSE;
#endif

    /* Initialize the function class first so constructors can be made. */
    fun_proto = js_InitFunctionClass(cx, obj);
    if (!fun_proto)
	return JS_FALSE;

    /* Initialize the object class next so Object.prototype works. */
    obj_proto = js_InitObjectClass(cx, obj);
    if (!obj_proto)
	return JS_FALSE;

    /* Function.prototype and the global object delegate to Object.prototype. */
    OBJ_SET_PROTO(cx, fun_proto, obj_proto);
    if (!OBJ_GET_PROTO(cx, obj))
	OBJ_SET_PROTO(cx, obj, obj_proto);

    /* Initialize the rest of the standard objects and functions. */
    return (array_proto = js_InitArrayClass(cx, obj)) != NULL &&
	   js_InitArgsAndCallClasses(cx, obj, obj_proto) &&
	   js_InitBooleanClass(cx, obj) &&
	   js_InitMathClass(cx, obj) &&
	   js_InitNumberClass(cx, obj) &&
	   js_InitStringClass(cx, obj) &&
#if JS_HAS_REGEXPS
	   js_InitRegExpClass(cx, obj) &&
#endif
#if JS_HAS_SCRIPT_OBJECT
	   js_InitScriptClass(cx, obj) &&
#endif
#if JS_HAS_ERROR_EXCEPTIONS
	   js_InitExceptionClasses(cx, obj) &&
#endif
#if JS_HAS_FILE_OBJECT
           js_InitFileClass(cx, obj, JS_TRUE) &&
#endif
	   js_InitDateClass(cx, obj);
}

JS_PUBLIC_API(JSObject *)
JS_GetScopeChain(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return cx->fp ? cx->fp->scopeChain : NULL;
}

JS_PUBLIC_API(void *)
JS_malloc(JSContext *cx, size_t nbytes)
{
    void *p;

    cx->runtime->gcMallocBytes += nbytes;

#if defined(XP_OS2) || defined(XP_MAC) || defined(AIX) || defined(OSF1) || defined(__MWERKS__)
    if (nbytes == 0) /*DSR072897 - Windows allows this, OS/2 & Mac don't*/
	nbytes = 1;
#endif
    p = malloc(nbytes);
    if (!p)
	JS_ReportOutOfMemory(cx);
    return p;
}

JS_PUBLIC_API(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes)
{
    p = realloc(p, nbytes);
    if (!p)
	JS_ReportOutOfMemory(cx);
    return p;
}

JS_PUBLIC_API(void)
JS_free(JSContext *cx, void *p)
{
    if (p)
	free(p);
}

JS_PUBLIC_API(char *)
JS_strdup(JSContext *cx, const char *s)
{
    char *p = JS_malloc(cx, strlen(s) + 1);
    if (!p)
	return NULL;
    return strcpy(p, s);
}

JS_PUBLIC_API(jsdouble *)
JS_NewDouble(JSContext *cx, jsdouble d)
{
    CHECK_REQUEST(cx);
    return js_NewDouble(cx, d);
}

JS_PUBLIC_API(JSBool)
JS_NewDoubleValue(JSContext *cx, jsdouble d, jsval *rval)
{
    CHECK_REQUEST(cx);
    return js_NewDoubleValue(cx, d, rval);
}

JS_PUBLIC_API(JSBool)
JS_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval)
{
    CHECK_REQUEST(cx);
    return js_NewNumberValue(cx, d, rval);
}

JS_PUBLIC_API(JSBool)
JS_AddRoot(JSContext *cx, void *rp)
{
    CHECK_REQUEST(cx);
    return js_AddRoot(cx, rp, NULL);
}

JS_PUBLIC_API(JSBool)
JS_RemoveRoot(JSContext *cx, void *rp)
{
    CHECK_REQUEST(cx);
    return js_RemoveRoot(cx->runtime, rp);
}

JS_PUBLIC_API(JSBool)
JS_RemoveRootRT(JSRuntime *rt, void *rp)
{
    return js_RemoveRoot(rt, rp);
}

JS_PUBLIC_API(JSBool)
JS_AddNamedRoot(JSContext *cx, void *rp, const char *name)
{
    CHECK_REQUEST(cx);
    return js_AddRoot(cx, rp, name);
}

#ifdef DEBUG

#include "jshash.h" /* Added by JSIFY */

typedef struct NamedRootDumpArgs {
    void (*dump)(const char *name, void *rp, void *data);
    void *data;
} NamedRootDumpArgs;

JS_STATIC_DLL_CALLBACK(intN)
js_named_root_dumper(JSHashEntry *he, intN i, void *arg)
{
    NamedRootDumpArgs *args = arg;

    if (he->value)
	args->dump(he->value, (void *)he->key, args->data);
    return HT_ENUMERATE_NEXT;
}

JS_PUBLIC_API(void)
JS_DumpNamedRoots(JSRuntime *rt,
		  void (*dump)(const char *name, void *rp, void *data),
		  void *data)
{
    NamedRootDumpArgs args;

    args.dump = dump;
    args.data = data;
    JS_HashTableEnumerateEntries(rt->gcRootsHash, js_named_root_dumper, &args);
}

#endif /* DEBUG */

JS_PUBLIC_API(JSBool)
JS_LockGCThing(JSContext *cx, void *thing)
{
    JSBool ok;

    CHECK_REQUEST(cx);
    ok = js_LockGCThing(cx, thing);
    if (!ok)
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_LOCK);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_UnlockGCThing(JSContext *cx, void *thing)
{
    JSBool ok;

    CHECK_REQUEST(cx);
    ok = js_UnlockGCThing(cx, thing);
    if (!ok)
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_UNLOCK);
    return ok;
}

JS_PUBLIC_API(void)
JS_GC(JSContext *cx)
{
    if (!cx->fp)
	JS_FinishArenaPool(&cx->stackPool);
    JS_FinishArenaPool(&cx->codePool);
    JS_FinishArenaPool(&cx->tempPool);
    js_ForceGC(cx);
}

JS_PUBLIC_API(void)
JS_MaybeGC(JSContext *cx)
{
    JSRuntime *rt;
    uint32 bytes, lastBytes;

    rt = cx->runtime;
    bytes = rt->gcBytes;
    lastBytes = rt->gcLastBytes;
    if ((bytes > 8192 && bytes > lastBytes + lastBytes / 2)
#ifdef NES40
/*
    This is the other side of the fix in jsgc.c, allocGCThing where we stopped
    doing a gc when the allocation fails. It turned out that the server branch-
    callback wasn't providing for enough gc to prevent certain string concatenations
    from exhausting the heap - with large strings the number of JSObjects remains
    small but the amount of malloc'd space can be huge. We re-instate a test of the
    malloc'd space here to help trigger a gc. (The server changed the frequency of
    issuing calls to MaybeGC as well).
*/
            || (rt->gcMallocBytes > rt->gcMaxBytes)
#endif /* NES40 */
            )
	JS_GC(cx);
}

JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallback(JSContext *cx, JSGCCallback cb)
{
    JSRuntime *rt;
    JSGCCallback oldcb;

    rt = cx->runtime;
    oldcb = rt->gcCallback;
    rt->gcCallback = cb;
    return oldcb;
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_DestroyIdArray(JSContext *cx, JSIdArray *ida)
{
    JS_free(cx, ida);
}

JS_PUBLIC_API(JSBool)
JS_ValueToId(JSContext *cx, jsval v, jsid *idp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    if (JSVAL_IS_INT(v)) {
	*idp = v;
    } else {
	atom = js_ValueToStringAtom(cx, v);
	if (!atom)
	    return JS_FALSE;
	*idp = (jsid)atom;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_IdToValue(JSContext *cx, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    *vp = js_IdToValue(id);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_PropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    CHECK_REQUEST(cx);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_EnumerateStub(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ResolveStub(JSContext *cx, JSObject *obj, jsval id)
{
    CHECK_REQUEST(cx);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ConvertStub(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    CHECK_REQUEST(cx);
#if JS_BUG_EAGER_TOSTRING
    if (type == JSTYPE_STRING)
	return JS_TRUE;
#endif
    return js_TryValueOf(cx, obj, type, vp);
}

JS_PUBLIC_API(void)
JS_FinalizeStub(JSContext *cx, JSObject *obj)
{
}

JS_PUBLIC_API(JSObject *)
JS_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
	     JSClass *clasp, JSNative constructor, uintN nargs,
	     JSPropertySpec *ps, JSFunctionSpec *fs,
	     JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    JSAtom *atom;
    JSObject *proto, *ctor;
    JSBool named;
    JSFunction *fun;
    jsval junk;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, clasp->name, strlen(clasp->name), 0);
    if (!atom)
	return NULL;

    /* Create a prototype object for this class. */
    proto = js_NewObject(cx, clasp, parent_proto, obj);
    if (!proto)
	return NULL;

    if (!constructor) {
	/* Lacking a constructor, name the prototype (e.g., Math). */
	named = OBJ_DEFINE_PROPERTY(cx, obj, (jsid)atom, OBJECT_TO_JSVAL(proto),
				    NULL, NULL, 0, NULL);
	if (!named)
	    goto bad;
	ctor = proto;
    } else {
	/* Define the constructor function in obj's scope. */
	fun = js_DefineFunction(cx, obj, atom, constructor, nargs, 0);
	named = (fun != NULL);
	if (!fun)
	    goto bad;

        /*
         * Remember the class this function is a constructor for so that
         * we know to create an object of this class when we call the
         * constructor.
         */
        fun->clasp = clasp;

	/* Connect constructor and prototype by named properties. */
	ctor = fun->object;
	if (!js_SetClassPrototype(cx, ctor, proto,
				  JSPROP_READONLY | JSPROP_PERMANENT)) {
	    goto bad;
	}

	/* Bootstrap Function.prototype (see also JS_InitStandardClasses). */
	if (OBJ_GET_CLASS(cx, ctor) == clasp) {
	    /* XXXMLM - this fails in framesets that are writing over
	     *           themselves!
	     * JS_ASSERT(!OBJ_GET_PROTO(cx, ctor));
	     */
	    OBJ_SET_PROTO(cx, ctor, proto);
	}
    }

    /* Add properties and methods to the prototype and the constructor. */
    if ((ps && !JS_DefineProperties(cx, proto, ps)) ||
	(fs && !JS_DefineFunctions(cx, proto, fs)) ||
	(static_ps && !JS_DefineProperties(cx, ctor, static_ps)) ||
	(static_fs && !JS_DefineFunctions(cx, ctor, static_fs))) {
	goto bad;
    }
    return proto;

bad:
    if (named)
	(void) OBJ_DELETE_PROPERTY(cx, obj, (jsid)atom, &junk);
    cx->newborn[GCX_OBJECT] = NULL;
    return NULL;
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    return OBJ_GET_CLASS(cx, obj);
}
#else
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSObject *obj)
{
    CHECK_REQUEST(cx);
    return LOCKED_OBJ_GET_CLASS(obj);
}
#endif

JS_PUBLIC_API(JSBool)
JS_InstanceOf(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv)
{
    JSFunction *fun;

    CHECK_REQUEST(cx);
    if (OBJ_GET_CLASS(cx, obj) == clasp)
	return JS_TRUE;
    if (argv) {
	fun = js_ValueToFunction(cx, &argv[-2], JS_FALSE);
	if (fun) {
	    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				 JSMSG_INCOMPATIBLE_PROTO,
				 clasp->name, JS_GetFunctionName(fun),
				 OBJ_GET_CLASS(cx, obj)->name);
	}
    }
    return JS_FALSE;
}

JS_PUBLIC_API(void *)
JS_GetPrivate(JSContext *cx, JSObject *obj)
{
    jsval v;

    CHECK_REQUEST(cx);
    JS_ASSERT(OBJ_GET_CLASS(cx, obj)->flags & JSCLASS_HAS_PRIVATE);
    v = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    if (!JSVAL_IS_INT(v))
	return NULL;
    return JSVAL_TO_PRIVATE(v);
}

JS_PUBLIC_API(JSBool)
JS_SetPrivate(JSContext *cx, JSObject *obj, void *data)
{
    CHECK_REQUEST(cx);
    JS_ASSERT(OBJ_GET_CLASS(cx, obj)->flags & JSCLASS_HAS_PRIVATE);
    OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, PRIVATE_TO_JSVAL(data));
    return JS_TRUE;
}

JS_PUBLIC_API(void *)
JS_GetInstancePrivate(JSContext *cx, JSObject *obj, JSClass *clasp,
		      jsval *argv)
{
    CHECK_REQUEST(cx);
    if (!JS_InstanceOf(cx, obj, clasp, argv))
	return NULL;
    return JS_GetPrivate(cx, obj);
}

JS_PUBLIC_API(JSObject *)
JS_GetPrototype(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    CHECK_REQUEST(cx);
    proto = JSVAL_TO_OBJECT(OBJ_GET_SLOT(cx, obj, JSSLOT_PROTO));

    /* Beware ref to dead object (we may be called from obj's finalizer). */
    return proto && proto->map ? proto : NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetPrototype(JSContext *cx, JSObject *obj, JSObject *proto)
{
    CHECK_REQUEST(cx);
    OBJ_SET_SLOT(cx, obj, JSSLOT_PROTO, OBJECT_TO_JSVAL(proto));
    return JS_TRUE;
}

JS_PUBLIC_API(JSObject *)
JS_GetParent(JSContext *cx, JSObject *obj)
{
    JSObject *parent;

    CHECK_REQUEST(cx);
    parent = JSVAL_TO_OBJECT(OBJ_GET_SLOT(cx, obj, JSSLOT_PARENT));

    /* Beware ref to dead object (we may be called from obj's finalizer). */
    return parent && parent->map ? parent : NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetParent(JSContext *cx, JSObject *obj, JSObject *parent)
{
    CHECK_REQUEST(cx);
    OBJ_SET_SLOT(cx, obj, JSSLOT_PARENT, OBJECT_TO_JSVAL(parent));
    return JS_TRUE;
}

JS_PUBLIC_API(JSObject *)
JS_GetConstructor(JSContext *cx, JSObject *proto)
{
    JSBool ok;
    jsval cval;

    CHECK_REQUEST(cx);
    ok = OBJ_GET_PROPERTY(cx, proto,
			  (jsid)cx->runtime->atomState.constructorAtom,
			  &cval);
    if (!ok)
	return NULL;
    if (!JSVAL_IS_FUNCTION(cx, cval)) {
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR,
			     OBJ_GET_CLASS(cx, proto)->name);
	return NULL;
    }
    return JSVAL_TO_OBJECT(cval);
}

JS_PUBLIC_API(JSObject *)
JS_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    CHECK_REQUEST(cx);
    return js_NewObject(cx, clasp, proto, parent);
}

JS_PUBLIC_API(JSObject *)
JS_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
		   JSObject *parent)
{
    CHECK_REQUEST(cx);
    return js_ConstructObject(cx, clasp, proto, parent);
}

static JSBool
DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
	       JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
	       JSProperty **propp)
{
    jsid id;
    JSAtom *atom;

    if (attrs & JSPROP_INDEX) {
	id = INT_TO_JSVAL((jsint)name);
	atom = NULL;
    } else {
	atom = js_Atomize(cx, name, strlen(name), 0);
	if (!atom)
	    return JS_FALSE;
	id = (jsid)atom;
    }
    return OBJ_DEFINE_PROPERTY(cx, obj, id, value, getter, setter, attrs,
			       propp);
}

#define AUTO_NAMELEN(s,n)   (((n) == (size_t)-1) ? js_strlen(s) : (n))

static JSBool
DefineUCProperty(JSContext *cx, JSObject *obj,
		 const jschar *name, size_t namelen, jsval value,
		 JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
		 JSProperty **propp)
{
    JSAtom *atom;

    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name,namelen), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_DEFINE_PROPERTY(cx, obj, (jsid)atom, value, getter, setter,
			       attrs, propp);
}

JS_PUBLIC_API(JSObject *)
JS_DefineObject(JSContext *cx, JSObject *obj, const char *name, JSClass *clasp,
		JSObject *proto, uintN attrs)
{
    JSObject *nobj;

    CHECK_REQUEST(cx);
    nobj = js_NewObject(cx, clasp, proto, obj);
    if (!nobj)
	return NULL;
    if (!DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(nobj), NULL, NULL, attrs,
			NULL)) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    return nobj;
}

JS_PUBLIC_API(JSBool)
JS_DefineConstDoubles(JSContext *cx, JSObject *obj, JSConstDoubleSpec *cds)
{
    JSBool ok;
    jsval value;
    uintN flags;

    CHECK_REQUEST(cx);
    for (ok = JS_TRUE; cds->name; cds++) {
#if JS_ALIGN_OF_DOUBLE == 8
	/*
	 * The GC ignores references outside its pool such as &cds->dval,
	 * so we don't need to GC-alloc constant doubles.
	 */
	jsdouble d = cds->dval;
	jsint i;

	value = (JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i))
		? INT_TO_JSVAL(i)
		: DOUBLE_TO_JSVAL(&cds->dval);
#else
	ok = js_NewNumberValue(cx, cds->dval, &value);
	if (!ok)
	    break;
#endif
	flags = cds->flags;
	if (!flags)
	    flags = JSPROP_READONLY | JSPROP_PERMANENT;
	ok = DefineProperty(cx, obj, cds->name, value, NULL, NULL, flags, NULL);
	if (!ok)
	    break;
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_DefineProperties(JSContext *cx, JSObject *obj, JSPropertySpec *ps)
{
    JSBool ok;
    JSProperty *prop;
    JSScopeProperty *sprop;

    CHECK_REQUEST(cx);
    for (ok = JS_TRUE; ps->name; ps++) {
	ok = DefineProperty(cx, obj, ps->name, JSVAL_VOID,
			    ps->getter, ps->setter, ps->flags,
			    &prop);
	if (!ok)
	    break;
	if (prop) {
	    if (OBJ_IS_NATIVE(obj)) {
		sprop = (JSScopeProperty *)prop;
		sprop->id = INT_TO_JSVAL(ps->tinyid);
	    }
	    OBJ_DROP_PROPERTY(cx, obj, prop);
	}
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
		  JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    CHECK_REQUEST(cx);
    return DefineProperty(cx, obj, name, value, getter, setter, attrs, NULL);
}

JS_PUBLIC_API(JSBool)
JS_DefinePropertyWithTinyId(JSContext *cx, JSObject *obj, const char *name,
			    int8 tinyid, jsval value,
			    JSPropertyOp getter, JSPropertyOp setter,
			    uintN attrs)
{
    JSBool ok;
    JSProperty *prop;
    JSScopeProperty *sprop;

    CHECK_REQUEST(cx);
    ok = DefineProperty(cx, obj, name, value, getter, setter, attrs, &prop);
    if (ok && prop) {
	if (OBJ_IS_NATIVE(obj)) {
	    sprop = (JSScopeProperty *)prop;
	    sprop->id = INT_TO_JSVAL(tinyid);
	}
	OBJ_DROP_PROPERTY(cx, obj, prop);
    }
    return ok;
}

static JSBool
LookupProperty(JSContext *cx, JSObject *obj, const char *name, JSObject **objp,
	       JSProperty **propp)
{
    JSAtom *atom;

    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, objp, propp);
}

static JSBool
LookupUCProperty(JSContext *cx, JSObject *obj,
		 const jschar *name, size_t namelen,
		 JSObject **objp, JSProperty **propp)
{
    JSAtom *atom;

    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name,namelen), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, objp, propp);
}

JS_PUBLIC_API(JSBool)
JS_AliasProperty(JSContext *cx, JSObject *obj, const char *name,
		 const char *alias)
{
    JSObject *obj2;
    JSProperty *prop;
    JSAtom *atom;
    JSScope *scope;
    JSBool ok;

    CHECK_REQUEST(cx);
    /* XXXbe push this into jsobj.c or jsscope.c */
    if (!LookupProperty(cx, obj, name, &obj2, &prop))
	return JS_FALSE;
    if (!prop) {
	js_ReportIsNotDefined(cx, name);
	return JS_FALSE;
    }
    if (obj2 != obj || !OBJ_IS_NATIVE(obj2)) {
	OBJ_DROP_PROPERTY(cx, obj2, prop);
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_ALIAS,
			     alias, name, OBJ_GET_CLASS(cx, obj2)->name);
	return JS_FALSE;
    }
    atom = js_Atomize(cx, alias, strlen(alias), 0);
    if (!atom) {
	ok = JS_FALSE;
    } else {
	scope = OBJ_SCOPE(obj);
	ok = (scope->ops->add(cx, scope, (jsid)atom, (JSScopeProperty *)prop)
	      != NULL);
    }
    OBJ_DROP_PROPERTY(cx, obj, prop);
    return ok;
}

static jsval
LookupResult(JSContext *cx, JSObject *obj, JSObject *obj2, JSProperty *prop)
{
    JSScopeProperty *sprop;
    jsval rval;

    if (!prop) {
	/* XXX bad API: no way to tell "not defined" from "void value" */
	return JSVAL_VOID;
    }
    if (OBJ_IS_NATIVE(obj2)) {
	/* Peek at the native property's slot value, without doing a Get. */
	sprop = (JSScopeProperty *)prop;
	rval = LOCKED_OBJ_GET_SLOT(obj2, sprop->slot);
    } else {
	/* XXX bad API: no way to return "defined but value unknown" */
	rval = JSVAL_TRUE;
    }
    OBJ_DROP_PROPERTY(cx, obj2, prop);
    return rval;
}

static JSBool
GetPropertyAttributes(JSContext *cx, JSObject *obj, JSAtom *atom,
		      uintN *attrsp, JSBool *foundp)
{
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok;

    if (!atom)
	return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &obj2, &prop))
	return JS_FALSE;
    if (!prop || obj != obj2) {
	*foundp = JS_FALSE;
	if (prop)
	    OBJ_DROP_PROPERTY(cx, obj2, prop);
	return JS_TRUE;
    }

    *foundp = JS_TRUE;
    ok = OBJ_GET_ATTRIBUTES(cx, obj, (jsid)atom, prop, attrsp);
    OBJ_DROP_PROPERTY(cx, obj, prop);
    return ok;
}

static JSBool
SetPropertyAttributes(JSContext *cx, JSObject *obj, JSAtom *atom,
		      uintN attrs, JSBool *foundp)
{
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok;

    if (!atom)
	return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &obj2, &prop))
	return JS_FALSE;
    if (!prop || obj != obj2) {
	*foundp = JS_FALSE;
	if (prop)
	    OBJ_DROP_PROPERTY(cx, obj2, prop);
	return JS_TRUE;
    }

    *foundp = JS_TRUE;
    ok = OBJ_SET_ATTRIBUTES(cx, obj, (jsid)atom, prop, &attrs);
    OBJ_DROP_PROPERTY(cx, obj, prop);
    return ok;
}


JS_PUBLIC_API(JSBool)
JS_GetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
			 uintN *attrsp, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return GetPropertyAttributes(cx, obj,
				 js_Atomize(cx, name, strlen(name), 0),
				 attrsp, foundp);
}

JS_PUBLIC_API(JSBool)
JS_SetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
			 uintN attrs, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return SetPropertyAttributes(cx, obj,
				 js_Atomize(cx, name, strlen(name), 0),
				 attrs, foundp);
}

JS_PUBLIC_API(JSBool)
JS_LookupProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSBool ok;
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    ok = LookupProperty(cx, obj, name, &obj2, &prop);
    if (ok)
	*vp = LookupResult(cx, obj, obj2, prop);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_GetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_GET_PROPERTY(cx, obj, (jsid)atom, vp);
}

JS_PUBLIC_API(JSBool)
JS_SetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_SET_PROPERTY(cx, obj, (jsid)atom, vp);
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty(JSContext *cx, JSObject *obj, const char *name)
{
    jsval junk;

    CHECK_REQUEST(cx);
    return JS_DeleteProperty2(cx, obj, name, &junk);
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty2(JSContext *cx, JSObject *obj, const char *name,
		   jsval *rval)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_DELETE_PROPERTY(cx, obj, (jsid)atom, rval);
}

JS_PUBLIC_API(JSBool)
JS_DefineUCProperty(JSContext *cx, JSObject *obj,
		    const jschar *name, size_t namelen, jsval value,
		    JSPropertyOp getter, JSPropertyOp setter,
		    uintN attrs)
{
    CHECK_REQUEST(cx);
    return DefineUCProperty(cx, obj, name, namelen, value, getter, setter,
			    attrs, NULL);
}

JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttributes(JSContext *cx, JSObject *obj,
			   const jschar *name, size_t namelen,
			   uintN *attrsp, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return GetPropertyAttributes(cx, obj,
		    js_AtomizeChars(cx, name, AUTO_NAMELEN(name,namelen), 0),
		    attrsp, foundp);
}

JS_PUBLIC_API(JSBool)
JS_SetUCPropertyAttributes(JSContext *cx, JSObject *obj,
			   const jschar *name, size_t namelen,
			   uintN attrs, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return SetPropertyAttributes(cx, obj,
		    js_AtomizeChars(cx, name, AUTO_NAMELEN(name,namelen), 0),
		    attrs, foundp);
}

JS_PUBLIC_API(JSBool)
JS_DefineUCPropertyWithTinyId(JSContext *cx, JSObject *obj,
			      const jschar *name, size_t namelen,
			      int8 tinyid, jsval value,
			      JSPropertyOp getter, JSPropertyOp setter,
			      uintN attrs)
{
    JSBool ok;
    JSProperty *prop;
    JSScopeProperty *sprop;

    CHECK_REQUEST(cx);
    ok = DefineUCProperty(cx, obj, name, namelen, value, getter, setter, attrs,
			  &prop);
    if (ok && prop) {
	if (OBJ_IS_NATIVE(obj)) {
	    sprop = (JSScopeProperty *)prop;
	    sprop->id = INT_TO_JSVAL(tinyid);
	}
	OBJ_DROP_PROPERTY(cx, obj, prop);
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_LookupUCProperty(JSContext *cx, JSObject *obj,
		    const jschar *name, size_t namelen,
		    jsval *vp)
{
    JSBool ok;
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    ok = LookupUCProperty(cx, obj, name, namelen, &obj2, &prop);
    if (ok)
	*vp = LookupResult(cx, obj, obj2, prop);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_GetUCProperty(JSContext *cx, JSObject *obj,
		 const jschar *name, size_t namelen,
		 jsval *vp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name,namelen), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_GET_PROPERTY(cx, obj, (jsid)atom, vp);
}

JS_PUBLIC_API(JSBool)
JS_SetUCProperty(JSContext *cx, JSObject *obj,
		 const jschar *name, size_t namelen,
		 jsval *vp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name,namelen), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_SET_PROPERTY(cx, obj, (jsid)atom, vp);
}

JS_PUBLIC_API(JSBool)
JS_DeleteUCProperty2(JSContext *cx, JSObject *obj,
		     const jschar *name, size_t namelen,
		     jsval *rval)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name,namelen), 0);
    if (!atom)
	return JS_FALSE;
    return OBJ_DELETE_PROPERTY(cx, obj, (jsid)atom, rval);
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, jsint length, jsval *vector)
{
    CHECK_REQUEST(cx);
    /* jsuint cast does ToUint32 */
    return js_NewArrayObject(cx, (jsuint)length, vector);
}

JS_PUBLIC_API(JSBool)
JS_IsArrayObject(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    return OBJ_GET_CLASS(cx, obj) == &js_ArrayClass;
}

JS_PUBLIC_API(JSBool)
JS_GetArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    CHECK_REQUEST(cx);
    return js_GetLengthProperty(cx, obj, lengthp);
}

JS_PUBLIC_API(JSBool)
JS_SetArrayLength(JSContext *cx, JSObject *obj, jsuint length)
{
    CHECK_REQUEST(cx);
    return js_SetLengthProperty(cx, obj, length);
}

JS_PUBLIC_API(JSBool)
JS_HasArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    CHECK_REQUEST(cx);
    return js_HasLengthProperty(cx, obj, lengthp);
}

JS_PUBLIC_API(JSBool)
JS_DefineElement(JSContext *cx, JSObject *obj, jsint index, jsval value,
		 JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    CHECK_REQUEST(cx);
    return OBJ_DEFINE_PROPERTY(cx, obj, INT_TO_JSVAL(index), value,
			       getter, setter, attrs, NULL);
}

JS_PUBLIC_API(JSBool)
JS_AliasElement(JSContext *cx, JSObject *obj, const char *name, jsint alias)
{
    JSObject *obj2;
    JSProperty *prop;
    JSScope *scope;
    JSBool ok;

    CHECK_REQUEST(cx);
    /* XXXbe push this into jsobj.c or jsscope.c */
    if (!LookupProperty(cx, obj, name, &obj2, &prop))
	return JS_FALSE;
    if (!prop) {
	js_ReportIsNotDefined(cx, name);
	return JS_FALSE;
    }
    if (obj2 != obj || !OBJ_IS_NATIVE(obj2)) {
	char numBuf[12];
	OBJ_DROP_PROPERTY(cx, obj2, prop);
	JS_snprintf(numBuf, sizeof numBuf, "%ld", (long)alias);
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_ALIAS,
			     numBuf, name, OBJ_GET_CLASS(cx, obj2)->name);
	return JS_FALSE;
    }
    scope = OBJ_SCOPE(obj);
    ok = (scope->ops->add(cx, scope, INT_TO_JSVAL(alias),
			  (JSScopeProperty *)prop)
	  != NULL);
    OBJ_DROP_PROPERTY(cx, obj, prop);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_LookupElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    JSBool ok;
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    ok = OBJ_LOOKUP_PROPERTY(cx, obj, INT_TO_JSVAL(index), &obj2, &prop);
    if (ok)
	*vp = LookupResult(cx, obj, obj2, prop);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_GetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    CHECK_REQUEST(cx);
    return OBJ_GET_PROPERTY(cx, obj, INT_TO_JSVAL(index), vp);
}

JS_PUBLIC_API(JSBool)
JS_SetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    CHECK_REQUEST(cx);
    return OBJ_SET_PROPERTY(cx, obj, INT_TO_JSVAL(index), vp);
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement(JSContext *cx, JSObject *obj, jsint index)
{
    jsval junk;

    CHECK_REQUEST(cx);
    return JS_DeleteElement2(cx, obj, index, &junk);
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement2(JSContext *cx, JSObject *obj, jsint index, jsval *rval)
{
    CHECK_REQUEST(cx);
    return OBJ_DELETE_PROPERTY(cx, obj, INT_TO_JSVAL(index), rval);
}

JS_PUBLIC_API(void)
JS_ClearScope(JSContext *cx, JSObject *obj)
{
    JSObjectMap *map;
    JSScope *scope;

    CHECK_REQUEST(cx);
    /* XXXbe push this into jsobj.c or jsscope.c */
    JS_LOCK_OBJ(cx, obj);
    map = obj->map;
    if (MAP_IS_NATIVE(map)) {
	scope = (JSScope *)map;
	scope->ops->clear(cx, scope);
    }

    /* Reset freeslot so we're consistent. */
    map->freeslot = JSSLOT_FREE(OBJ_GET_CLASS(cx, obj));
    JS_UNLOCK_OBJ(cx, obj);
}

JS_PUBLIC_API(JSIdArray *)
JS_Enumerate(JSContext *cx, JSObject *obj)
{
    jsint i, n;
    jsval iter_state, num_properties;
    jsid id;
    JSIdArray *ida;
    jsval *vector;

    CHECK_REQUEST(cx);

    ida = NULL;
    iter_state = JSVAL_NULL;

    /* Get the number of properties to enumerate. */
    if (!OBJ_ENUMERATE(cx, obj, JSENUMERATE_INIT, &iter_state, &num_properties))
	goto error;
    if (!JSVAL_IS_INT(num_properties)) {
	JS_ASSERT(0);
	goto error;
    }

    /* Grow as needed if we don't know the exact amount ahead of time. */
    n = JSVAL_TO_INT(num_properties);
    if (n <= 0)
        n = 8;

    /* Create an array of jsids large enough to hold all the properties */
    ida = js_NewIdArray(cx, n);
    if (!ida)
    	goto error;

    i = 0;
    vector = &ida->vector[0];
    while (1) {
	if (i == ida->length) {
	    /* Grow length by factor of 1.5 instead of doubling. */
	    jsint newlen = ida->length + (((jsuint)ida->length + 1) >> 1);
	    ida = js_GrowIdArray(cx, ida, newlen);
	    if (!ida)
		goto error;
	    vector = &ida->vector[0];
	}

	if (!OBJ_ENUMERATE(cx, obj, JSENUMERATE_NEXT, &iter_state, &id))
	    goto error;

	/* No more jsid's to enumerate ? */
	if (iter_state == JSVAL_NULL)
	    break;
	vector[i++] = id;
    }
    ida->length = i;
    return ida;

error:
    if (iter_state != JSVAL_NULL)
	OBJ_ENUMERATE(cx, obj, JSENUMERATE_DESTROY, &iter_state, 0);
    if (ida)
	JS_DestroyIdArray(cx, ida);
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
	       jsval *vp, uintN *attrsp)
{
    CHECK_REQUEST(cx);
    return OBJ_CHECK_ACCESS(cx, obj, id, mode, vp, attrsp);
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative call, uintN nargs, uintN flags,
	       JSObject *parent, const char *name)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);

    if (!name) {
    	atom = NULL;
    } else {
	atom = js_Atomize(cx, name, strlen(name), 0);
	if (!atom)
	    return NULL;
    }
    return js_NewFunction(cx, NULL, call, nargs, flags, parent, atom);
}

JS_PUBLIC_API(JSObject *)
JS_CloneFunctionObject(JSContext *cx, JSObject *funobj, JSObject *parent)
{
    CHECK_REQUEST(cx);
    if (OBJ_GET_CLASS(cx, funobj) != &js_FunctionClass) {
        /* Indicate we cannot clone this object */
	return funobj;
    }
    return js_CloneFunctionObject(cx, funobj, parent);
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionObject(JSFunction *fun)
{
    return fun->object;
}

JS_PUBLIC_API(const char *)
JS_GetFunctionName(JSFunction *fun)
{
    return fun->atom
	   ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom))
	   : js_anonymous_str;
}

JS_PUBLIC_API(JSBool)
JS_DefineFunctions(JSContext *cx, JSObject *obj, JSFunctionSpec *fs)
{
    JSFunction *fun;

    CHECK_REQUEST(cx);
    for (; fs->name; fs++) {
	fun = JS_DefineFunction(cx, obj, fs->name, fs->call, fs->nargs,
				fs->flags);
	if (!fun)
	    return JS_FALSE;
	fun->extra = fs->extra;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSFunction *)
JS_DefineFunction(JSContext *cx, JSObject *obj, const char *name, JSNative call,
		  uintN nargs, uintN attrs)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
	return NULL;
    return js_DefineFunction(cx, obj, atom, call, nargs, attrs);
}

static JSScript *
CompileTokenStream(JSContext *cx, JSObject *obj, JSTokenStream *ts,
		   void *tempMark, JSBool *eofp)
{
    JSBool eof;
    JSCodeGenerator cg;
    JSScript *script;

    CHECK_REQUEST(cx);
    eof = JS_FALSE;
    if (!js_InitCodeGenerator(cx, &cg, ts->filename, ts->lineno,
			      ts->principals)) {
	script = NULL;
	goto out;
    }
    if (!js_CompileTokenStream(cx, obj, ts, &cg)) {
	script = NULL;
        eof = (ts->flags & TSF_EOF) != 0;
	goto out;
    }
    script = js_NewScriptFromCG(cx, &cg, NULL);
out:
    if (eofp)
        *eofp = eof;
    if (!js_CloseTokenStream(cx, ts)) {
        if (script)
	    js_DestroyScript(cx, script);
	script = NULL;
    }
    cg.tempMark = tempMark;
    js_FinishCodeGenerator(cx, &cg);
    return script;
}

JS_PUBLIC_API(JSScript *)
JS_CompileScript(JSContext *cx, JSObject *obj,
		 const char *bytes, size_t length,
		 const char *filename, uintN lineno)
{
    jschar *chars;
    JSScript *script;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    script = JS_CompileUCScript(cx, obj, chars, length, filename, lineno);
    JS_free(cx, chars);
    return script;
}

JS_PUBLIC_API(JSScript *)
JS_CompileScriptForPrincipals(JSContext *cx, JSObject *obj,
			      JSPrincipals *principals,
			      const char *bytes, size_t length,
			      const char *filename, uintN lineno)
{
    jschar *chars;
    JSScript *script;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    script = JS_CompileUCScriptForPrincipals(cx, obj, principals,
					     chars, length, filename, lineno);
    JS_free(cx, chars);
    return script;
}

JS_PUBLIC_API(JSScript *)
JS_CompileUCScript(JSContext *cx, JSObject *obj,
		   const jschar *chars, size_t length,
		   const char *filename, uintN lineno)
{
    CHECK_REQUEST(cx);
    return JS_CompileUCScriptForPrincipals(cx, obj, NULL, chars, length,
					   filename, lineno);
}

JS_PUBLIC_API(JSScript *)
JS_CompileUCScriptForPrincipals(JSContext *cx, JSObject *obj,
				JSPrincipals *principals,
				const jschar *chars, size_t length,
				const char *filename, uintN lineno)
{
    void *mark;
    JSTokenStream *ts;

    CHECK_REQUEST(cx);
    mark = JS_ARENA_MARK(&cx->tempPool);
    ts = js_NewTokenStream(cx, chars, length, filename, lineno, principals);
    if (!ts)
	return NULL;
    return CompileTokenStream(cx, obj, ts, mark, NULL);
}

extern JS_PUBLIC_API(JSBool)
JS_BufferIsCompilableUnit(JSContext *cx, JSObject *obj,
                          const char *bytes, size_t length)
{
    jschar *chars;
    JSScript *script;
    void *mark;
    JSTokenStream *ts;
    JSErrorReporter older;
    JSBool hitEOF, result;
    JSExceptionState *exnState;

    CHECK_REQUEST(cx);
    mark = JS_ARENA_MARK(&cx->tempPool);
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return JS_TRUE;
    exnState = JS_SaveExceptionState(cx);
    ts = js_NewTokenStream(cx, chars, length, NULL, 0, NULL);
    if (!ts) {
        result = JS_TRUE;
        goto out;
    }

    older = JS_SetErrorReporter(cx, NULL);
    script = CompileTokenStream(cx, obj, ts, mark, &hitEOF);
    JS_SetErrorReporter(cx, older);

    if (script == NULL) {
        /*
         * We ran into an error, but it was because we ran out of source,
         * and not for some other reason.  For this case (and this case
         * only) we return false, so the calling function knows to try to
         * collect more source.
         */
        result = hitEOF ? JS_FALSE : JS_TRUE;
    } else {
        result = JS_TRUE;
        js_DestroyScript(cx, script);
    }

out:
    JS_free(cx, chars);
    JS_RestoreExceptionState(cx, exnState);
    return result;
}

JS_PUBLIC_API(JSScript *)
JS_CompileFile(JSContext *cx, JSObject *obj, const char *filename)
{
    void *mark;
    JSTokenStream *ts;

    CHECK_REQUEST(cx);
    mark = JS_ARENA_MARK(&cx->tempPool);
    ts = js_NewFileTokenStream(cx, filename, stdin);
    if (!ts)
	return NULL;
    return CompileTokenStream(cx, obj, ts, mark, NULL);
}

JS_PUBLIC_API(JSScript *)
JS_CompileFileHandle(JSContext *cx, JSObject *obj, const char *filename,
                     FILE *fh)
{
    return JS_CompileFileHandleForPrincipals(cx, obj, filename, fh, NULL);
}

JS_PUBLIC_API(JSScript *)
JS_CompileFileHandleForPrincipals(JSContext *cx, JSObject *obj,
                                  const char *filename, FILE *fh,
                                  JSPrincipals *principals)
{
    void *mark;
    JSTokenStream *ts;

    CHECK_REQUEST(cx);
    mark = JS_ARENA_MARK(&cx->tempPool);
    ts = js_NewFileTokenStream(cx, NULL, fh);
    if (!ts)
	return NULL;
    ts->filename = filename;
    if (principals) {
        ts->principals = principals;
        JSPRINCIPALS_HOLD(cx, ts->principals);
    }
    return CompileTokenStream(cx, obj, ts, mark, NULL);
}

JS_PUBLIC_API(JSObject *)
JS_NewScriptObject(JSContext *cx, JSScript *script)
{
    JSObject *obj;

    CHECK_REQUEST(cx);
    obj = js_NewObject(cx, &js_ScriptClass, NULL, NULL);
    if (!obj)
	return NULL;
    if (script) {
	if (!JS_SetPrivate(cx, obj, script))
	    return NULL;
	script->object = obj;
    }
    return obj;
}

JS_PUBLIC_API(void)
JS_DestroyScript(JSContext *cx, JSScript *script)
{
    CHECK_REQUEST(cx);
    js_DestroyScript(cx, script);
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunction(JSContext *cx, JSObject *obj, const char *name,
		   uintN nargs, const char **argnames,
		   const char *bytes, size_t length,
		   const char *filename, uintN lineno)
{
    jschar *chars;
    JSFunction *fun;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    fun = JS_CompileUCFunction(cx, obj, name, nargs, argnames, chars, length,
			       filename, lineno);
    JS_free(cx, chars);
    return fun;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunctionForPrincipals(JSContext *cx, JSObject *obj,
				JSPrincipals *principals, const char *name,
				uintN nargs, const char **argnames,
				const char *bytes, size_t length,
				const char *filename, uintN lineno)
{
    jschar *chars;
    JSFunction *fun;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    fun = JS_CompileUCFunctionForPrincipals(cx, obj, principals, name,
					    nargs, argnames, chars, length,
					    filename, lineno);
    JS_free(cx, chars);
    return fun;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JSObject *obj, const char *name,
		     uintN nargs, const char **argnames,
		     const jschar *chars, size_t length,
		     const char *filename, uintN lineno)
{
    CHECK_REQUEST(cx);
    return JS_CompileUCFunctionForPrincipals(cx, obj, NULL, name,
					     nargs, argnames,
					     chars, length,
					     filename, lineno);
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunctionForPrincipals(JSContext *cx, JSObject *obj,
				  JSPrincipals *principals, const char *name,
				  uintN nargs, const char **argnames,
				  const jschar *chars, size_t length,
				  const char *filename, uintN lineno)
{
    void *mark;
    JSTokenStream *ts;
    JSFunction *fun;
    JSAtom *funAtom, *argAtom;
    uintN i;
    JSScopeProperty *sprop;
    jsval junk;

    CHECK_REQUEST(cx);
    mark = JS_ARENA_MARK(&cx->tempPool);
    ts = js_NewTokenStream(cx, chars, length, filename, lineno, principals);
    if (!ts) {
	fun = NULL;
	funAtom = NULL;
	goto out;
    }
    if (!name) {
    	funAtom = NULL;
    } else {
	funAtom = js_Atomize(cx, name, strlen(name), 0);
	if (!funAtom) {
	    fun = NULL;
	    goto out;
	}
    }
/* XXXbe new-function, bind name only on success */
    fun = js_DefineFunction(cx, obj, funAtom, NULL, nargs, 0);
    if (!fun)
	goto out;
    if (nargs) {
	for (i = 0; i < nargs; i++) {
	    argAtom = js_Atomize(cx, argnames[i], strlen(argnames[i]), 0);
	    if (!argAtom)
		break;
	    if (!js_DefineProperty(cx, fun->object, (jsid)argAtom,
				   JSVAL_VOID, js_GetArgument, js_SetArgument,
				   JSPROP_ENUMERATE|JSPROP_PERMANENT,
				   (JSProperty **)&sprop)) {
		break;
	    }
	    JS_ASSERT(sprop);
	    sprop->id = INT_TO_JSVAL(i);
	    OBJ_DROP_PROPERTY(cx, fun->object, (JSProperty *)sprop);
	}
	if (i < nargs) {
	    (void) OBJ_DELETE_PROPERTY(cx, obj, (jsid)funAtom, &junk);
	    fun = NULL;
	    goto out;
	}
    }
    if (!js_CompileFunctionBody(cx, ts, fun)) {
	(void) OBJ_DELETE_PROPERTY(cx, obj, (jsid)funAtom, &junk);
	fun = NULL;
    }
out:
    if (ts)
	js_CloseTokenStream(cx, ts);
    JS_ARENA_RELEASE(&cx->tempPool, mark);
    return fun;
}

JS_PUBLIC_API(JSString *)
JS_DecompileScript(JSContext *cx, JSScript *script, const char *name,
		   uintN indent)
{
    JSPrinter *jp;
    JSString *str;

    CHECK_REQUEST(cx);
    jp = js_NewPrinter(cx, name,
                       indent & ~JS_DONT_PRETTY_PRINT,
                       !(indent & JS_DONT_PRETTY_PRINT));
    if (!jp)
	return NULL;
    if (js_DecompileScript(jp, script))
	str = js_GetPrinterOutput(jp);
    else
	str = NULL;
    js_DestroyPrinter(jp);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunction(JSContext *cx, JSFunction *fun, uintN indent)
{
    JSPrinter *jp;
    JSString *str;

    CHECK_REQUEST(cx);
    jp = js_NewPrinter(cx, JS_GetFunctionName(fun),
                       indent & ~JS_DONT_PRETTY_PRINT,
                       !(indent & JS_DONT_PRETTY_PRINT));
    if (!jp)
	return NULL;
    if (js_DecompileFunction(jp, fun))
	str = js_GetPrinterOutput(jp);
    else
	str = NULL;
    js_DestroyPrinter(jp);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunctionBody(JSContext *cx, JSFunction *fun, uintN indent)
{
    JSPrinter *jp;
    JSString *str;

    CHECK_REQUEST(cx);
    jp = js_NewPrinter(cx, JS_GetFunctionName(fun),
                       indent & ~JS_DONT_PRETTY_PRINT,
                       !(indent & JS_DONT_PRETTY_PRINT));
    if (!jp)
	return NULL;
    if (js_DecompileFunctionBody(jp, fun))
	str = js_GetPrinterOutput(jp);
    else
	str = NULL;
    js_DestroyPrinter(jp);
    return str;
}

JS_PUBLIC_API(JSBool)
JS_ExecuteScript(JSContext *cx, JSObject *obj, JSScript *script, jsval *rval)
{
    CHECK_REQUEST(cx);
    if (!js_Execute(cx, obj, script, NULL, NULL, 0, rval)) {
#if JS_HAS_EXCEPTIONS
        js_ReportUncaughtException(cx);
#endif
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateScript(JSContext *cx, JSObject *obj,
		  const char *bytes, uintN length,
		  const char *filename, uintN lineno,
		  jsval *rval)
{
    jschar *chars;
    JSBool ok;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return JS_FALSE;
    ok = JS_EvaluateUCScript(cx, obj, chars, length, filename, lineno, rval);
    JS_free(cx, chars);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateScriptForPrincipals(JSContext *cx, JSObject *obj,
			       JSPrincipals *principals,
			       const char *bytes, uintN length,
			       const char *filename, uintN lineno,
			       jsval *rval)
{
    jschar *chars;
    JSBool ok;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return JS_FALSE;
    ok = JS_EvaluateUCScriptForPrincipals(cx, obj, principals, chars, length,
					  filename, lineno, rval);
    JS_free(cx, chars);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScript(JSContext *cx, JSObject *obj,
		    const jschar *chars, uintN length,
		    const char *filename, uintN lineno,
		    jsval *rval)
{
    CHECK_REQUEST(cx);
    return JS_EvaluateUCScriptForPrincipals(cx, obj, NULL, chars, length,
					    filename, lineno, rval);
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScriptForPrincipals(JSContext *cx, JSObject *obj,
				 JSPrincipals *principals,
				 const jschar *chars, uintN length,
				 const char *filename, uintN lineno,
				 jsval *rval)
{
    JSScript *script;
    JSBool ok;

    CHECK_REQUEST(cx);
    script = JS_CompileUCScriptForPrincipals(cx, obj, principals, chars, length,
					     filename, lineno);
    if (!script)
	return JS_FALSE;
    ok = js_Execute(cx, obj, script, NULL, NULL, 0, rval);
#if JS_HAS_EXCEPTIONS
    if (!ok)
        js_ReportUncaughtException(cx);
#endif
    JS_DestroyScript(cx, script);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_CallFunction(JSContext *cx, JSObject *obj, JSFunction *fun, uintN argc,
		jsval *argv, jsval *rval)
{
    CHECK_REQUEST(cx);
    if (!js_InternalCall(cx, obj, OBJECT_TO_JSVAL(fun->object), argc, argv,
			 rval)) {
#if JS_HAS_EXCEPTIONS
        js_ReportUncaughtException(cx);
#endif
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionName(JSContext *cx, JSObject *obj, const char *name, uintN argc,
		    jsval *argv, jsval *rval)
{
    jsval fval;

    CHECK_REQUEST(cx);
    if (!JS_GetProperty(cx, obj, name, &fval))
	return JS_FALSE;
    if (!js_InternalCall(cx, obj, fval, argc, argv, rval)) {
#if JS_HAS_EXCEPTIONS
        js_ReportUncaughtException(cx);
#endif
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval, uintN argc,
		     jsval *argv, jsval *rval)
{
    CHECK_REQUEST(cx);
    if (!js_InternalCall(cx, obj, fval, argc, argv, rval)) {
#if JS_HAS_EXCEPTIONS
        js_ReportUncaughtException(cx);
#endif
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBranchCallback)
JS_SetBranchCallback(JSContext *cx, JSBranchCallback cb)
{
    JSBranchCallback oldcb;

    CHECK_REQUEST(cx);
    oldcb = cx->branchCallback;
    cx->branchCallback = cb;
    return oldcb;
}

JS_PUBLIC_API(JSBool)
JS_IsRunning(JSContext *cx)
{
    return cx->fp != NULL;
}

JS_PUBLIC_API(JSBool)
JS_IsConstructing(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return cx->fp && cx->fp->constructing;
}

/************************************************************************/

JS_PUBLIC_API(JSString *)
JS_NewString(JSContext *cx, char *bytes, size_t length)
{
    jschar *chars;
    JSString *str;

    CHECK_REQUEST(cx);
    /* Make a Unicode vector from the 8-bit char codes in bytes. */
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;

    /* Free chars (but not bytes, which caller frees on error) if we fail. */
    str = js_NewString(cx, chars, length, 0);
    if (!str) {
	JS_free(cx, chars);
	return NULL;
    }

    /* Hand off bytes to the deflated string cache, if possible. */
    if (!js_SetStringBytes(str, bytes, length))
	JS_free(cx, bytes);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n)
{
    jschar *js;
    JSString *str;

    CHECK_REQUEST(cx);
    js = js_InflateString(cx, s, n);
    if (!js)
	return NULL;
    str = js_NewString(cx, js, n, 0);
    if (!str)
	JS_free(cx, js);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s)
{
    size_t n;
    jschar *js;
    JSString *str;

    CHECK_REQUEST(cx);
    if (!s)
	return cx->runtime->emptyString;
    n = strlen(s);
    js = js_InflateString(cx, s, n);
    if (!js)
	return NULL;
    str = js_NewString(cx, js, n, 0);
    if (!str)
	JS_free(cx, js);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_InternString(JSContext *cx, const char *s)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, s, strlen(s), ATOM_PINNED);
    if (!atom)
	return NULL;
    return ATOM_TO_STRING(atom);
}

JS_PUBLIC_API(JSString *)
JS_NewUCString(JSContext *cx, jschar *chars, size_t length)
{
    CHECK_REQUEST(cx);
    return js_NewString(cx, chars, length, 0);
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const jschar *s, size_t n)
{
    CHECK_REQUEST(cx);
    return js_NewStringCopyN(cx, s, n, 0);
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const jschar *s)
{
    CHECK_REQUEST(cx);
    if (!s)
	return cx->runtime->emptyString;
    return js_NewStringCopyZ(cx, s, 0);
}

JS_PUBLIC_API(JSString *)
JS_InternUCStringN(JSContext *cx, const jschar *s, size_t length)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, s, length, ATOM_PINNED);
    if (!atom)
	return NULL;
    return ATOM_TO_STRING(atom);
}

JS_PUBLIC_API(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s)
{
    return JS_InternUCStringN(cx, s, js_strlen(s));
}

JS_PUBLIC_API(char *)
JS_GetStringBytes(JSString *str)
{
    char *bytes = js_GetStringBytes(str);
    return bytes ? bytes : "";
}

JS_PUBLIC_API(jschar *)
JS_GetStringChars(JSString *str)
{
    return str->chars;
}

JS_PUBLIC_API(size_t)
JS_GetStringLength(JSString *str)
{
    return str->length;
}

JS_PUBLIC_API(intN)
JS_CompareStrings(JSString *str1, JSString *str2)
{
    return js_CompareStrings(str1, str2);
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_ReportError(JSContext *cx, const char *format, ...)
{
    va_list ap;

    CHECK_REQUEST(cx);
    va_start(ap, format);
    js_ReportErrorVA(cx, JSREPORT_ERROR, format, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumber(JSContext *cx, JSErrorCallback errorCallback,
		     void *userRef, const uintN errorNumber, ...)
{
    va_list ap;

    CHECK_REQUEST(cx);
    va_start(ap, errorNumber);
    js_ReportErrorNumberVA(cx, JSREPORT_ERROR, errorCallback, userRef,
			   errorNumber, JS_TRUE, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberUC(JSContext *cx, JSErrorCallback errorCallback,
		     void *userRef, const uintN errorNumber, ...)
{
    va_list ap;

    CHECK_REQUEST(cx);
    va_start(ap, errorNumber);
    js_ReportErrorNumberVA(cx, JSREPORT_ERROR, errorCallback, userRef,
			   errorNumber, JS_FALSE, ap);
    va_end(ap);
}

JS_PUBLIC_API(JSBool)
JS_ReportWarning(JSContext *cx, const char *format, ...)
{
    va_list ap;
    JSBool ok;

    va_start(ap, format);
    ok = js_ReportErrorVA(cx, JSREPORT_WARNING, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumber(JSContext *cx, uintN flags,
                             JSErrorCallback errorCallback, void *userRef,
                             const uintN errorNumber, ...)
{
    va_list ap;
    JSBool ok;

    CHECK_REQUEST(cx);
    va_start(ap, errorNumber);
    ok = js_ReportErrorNumberVA(cx, flags, errorCallback, userRef,
                                errorNumber, JS_TRUE, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumberUC(JSContext *cx, uintN flags,
                               JSErrorCallback errorCallback, void *userRef,
                               const uintN errorNumber, ...)
{
    va_list ap;
    JSBool ok;

    CHECK_REQUEST(cx);
    va_start(ap, errorNumber);
    ok = js_ReportErrorNumberVA(cx, flags, errorCallback, userRef,
                                errorNumber, JS_FALSE, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(void)
JS_ReportOutOfMemory(JSContext *cx)
{
    JS_ReportError(cx, "out of memory");
}

JS_PUBLIC_API(JSErrorReporter)
JS_SetErrorReporter(JSContext *cx, JSErrorReporter er)
{
    JSErrorReporter older;

    CHECK_REQUEST(cx);
    older = cx->errorReporter;
    cx->errorReporter = er;
    return older;
}

/************************************************************************/

/*
 * Regular Expressions.
 */
JS_PUBLIC_API(JSObject *)
JS_NewRegExpObject(JSContext *cx, char *bytes, size_t length, uintN flags)
{
#if JS_HAS_REGEXPS
    jschar *chars;
    JSObject *obj;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    obj = js_NewRegExpObject(cx, chars, length, flags);
    JS_free(cx, chars);
    return obj;
#else
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_REG_EXPS);
    return NULL;
#endif
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, jschar *chars, size_t length, uintN flags)
{
    CHECK_REQUEST(cx);
#if JS_HAS_REGEXPS
    return js_NewRegExpObject(cx, chars, length, flags);
#else
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_REG_EXPS);
    return NULL;
#endif
}

JS_PUBLIC_API(void)
JS_SetRegExpInput(JSContext *cx, JSString *input, JSBool multiline)
{
    JSRegExpStatics *res;

    CHECK_REQUEST(cx);
    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = input;
    res->multiline = multiline;
    cx->runtime->gcPoke = JS_TRUE;
}

JS_PUBLIC_API(void)
JS_ClearRegExpStatics(JSContext *cx)
{
    JSRegExpStatics *res;

    CHECK_REQUEST(cx);
    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = NULL;
    res->multiline = JS_FALSE;
    res->parenCount = 0;
    res->lastMatch = res->lastParen = js_EmptySubString;
    res->leftContext = res->rightContext = js_EmptySubString;
    cx->runtime->gcPoke = JS_TRUE;
}

JS_PUBLIC_API(void)
JS_ClearRegExpRoots(JSContext *cx)
{
    JSRegExpStatics *res;

    CHECK_REQUEST(cx);
    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = NULL;
    cx->runtime->gcPoke = JS_TRUE;
}

/* TODO: compile, execute, get/set other statics... */

/************************************************************************/

JS_PUBLIC_API(JSBool)
JS_IsExceptionPending(JSContext *cx)
{
    CHECK_REQUEST(cx);
#if JS_HAS_EXCEPTIONS
    return (JSBool) cx->throwing;
#else
    return JS_FALSE;
#endif
}

JS_PUBLIC_API(JSBool)
JS_GetPendingException(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
#if JS_HAS_EXCEPTIONS
    if (!cx->throwing)
	return JS_FALSE;
    *vp = cx->exception;
    return JS_TRUE;
#else
    return JS_FALSE;
#endif
}

JS_PUBLIC_API(void)
JS_SetPendingException(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
#if JS_HAS_EXCEPTIONS
    cx->throwing = JS_TRUE;
    cx->exception = v;
#endif
}

JS_PUBLIC_API(void)
JS_ClearPendingException(JSContext *cx)
{
    CHECK_REQUEST(cx);
#if JS_HAS_EXCEPTIONS
    cx->throwing = JS_FALSE;
#endif
}

#if JS_HAS_EXCEPTIONS
struct JSExceptionState
{
    JSBool throwing;
    jsval  exception;
};
#endif

JS_PUBLIC_API(JSExceptionState *)
JS_SaveExceptionState(JSContext *cx)
{
#if JS_HAS_EXCEPTIONS
    JSExceptionState *state;
    CHECK_REQUEST(cx);
    state = (JSExceptionState*) JS_malloc(cx, sizeof(JSExceptionState));
    if (state) {
        state->throwing = JS_GetPendingException(cx, &state->exception);
        if (state->throwing && JSVAL_IS_GCTHING(state->exception))
            JS_AddRoot(cx, &state->exception);
    }
    return state;
#else
    return NULL;
#endif
}

JS_PUBLIC_API(void)
JS_RestoreExceptionState(JSContext *cx, JSExceptionState *state)
{
#if JS_HAS_EXCEPTIONS
    CHECK_REQUEST(cx);
    if (state) {
        if (state->throwing)
            JS_SetPendingException(cx, state->exception);
        else
            JS_ClearPendingException(cx);
        JS_DropExceptionState(cx, state);
    }
#endif
}

JS_PUBLIC_API(void)
JS_DropExceptionState(JSContext *cx, JSExceptionState *state)
{
#if JS_HAS_EXCEPTIONS
    CHECK_REQUEST(cx);
    if (state) {
        if (state->throwing && JSVAL_IS_GCTHING(state->exception))
            JS_RemoveRoot(cx, &state->exception);
        JS_free(cx, state);
    }
#endif
}

JS_PUBLIC_API(JSErrorReport *)
JS_ErrorFromException(JSContext *cx, jsval v)
{
#if JS_HAS_EXCEPTIONS
    CHECK_REQUEST(cx);
    return js_ErrorFromException(cx, v);
#else
    return NULL;
#endif
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(intN)
JS_GetContextThread(JSContext *cx)
{
    return cx->thread;
}

JS_PUBLIC_API(intN)
JS_SetContextThread(JSContext *cx)
{
    intN old = cx->thread;
    cx->thread = js_CurrentThreadId();
    return old;
}

JS_PUBLIC_API(intN)
JS_ClearContextThread(JSContext *cx)
{
    intN old = cx->thread;
    cx->thread = 0;
    return old;
}
#endif

/************************************************************************/

JS_FRIEND_API(JSBool)
JS_IsAssigning(JSContext *cx)
{
    JSStackFrame *fp;
    jsbytecode *pc;

    if (!(fp = cx->fp) || !(pc = fp->pc))
	return JS_FALSE;
    return (js_CodeSpec[*pc].format & JOF_SET) != 0;
}

/************************************************************************/

#ifdef XP_PC
#include <windows.h>
#if defined(XP_OS2_HACK)
/*DSR031297 - the OS/2 equiv is dll_InitTerm, but I don't see the need for it*/
#else
/*
 * Initialization routine for the JS DLL...
 */

/*
 * Global Instance handle...
 * In Win32 this is the module handle of the DLL.
 *
 * In Win16 this is the instance handle of the application
 * which loaded the DLL.
 */

#ifdef _WIN32
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    return TRUE;
}

#else  /* !_WIN32 */

int CALLBACK LibMain( HINSTANCE hInst, WORD wDataSeg,
		      WORD cbHeapSize, LPSTR lpszCmdLine )
{
    return TRUE;
}

BOOL CALLBACK __loadds WEP(BOOL fSystemExit)
{
    return TRUE;
}

#endif /* !_WIN32 */
#endif /* XP_OS2 */
#endif /* XP_PC */
