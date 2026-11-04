"""Run actual JRI marshalling/wrapper code without bootstrapping the JVM.

VM lookup/invocation is stubbed; production variadic entry points and argument
marshalling are extracted verbatim so this can run even while the VM is broken.
Usage: python3 sun-java/tests/test_jri_varargs.py [--sanitize]
"""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'sun-java/runtime/jrijdk.c').read_text()
start = source.index('typedef union JRI_PushArgsType')
marshalling = source[start:source.index('static char* JRI_CALLBACK\njri_PushArgumentsArray', start)]
start = source.index('static JavaObject* JRI_CALLBACK\njri_NewObjectA')
constructors = source[start:source.index('static struct java_lang_Class* JRI_CALLBACK\njri_GetObjectClass', start)]
start = source.index('#define DEFINE_CALLMETHOD(')
methods = source[start:source.index('/*******************************************************************************\n * Class Operations', start)]
setup = r'''
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "signature.h"
#define JRI_CALLBACK
#define FALSE 0
#define TRUE 1
typedef void ExecEnv;
typedef void JRIEnv;
typedef void JavaObject;
typedef void *jref;
typedef int bool_t;
typedef int jint;
typedef int JRIMethodID;
typedef unsigned char jbool;
typedef signed char jbyte;
typedef unsigned short jchar;
typedef short jshort;
typedef int64_t jlong;
typedef float jfloat;
typedef double jdouble;
struct java_lang_Class { int unused; };
typedef union { int32_t i; float f; void *h; void *p; } stack_item;
typedef struct { stack_item *optop; } JavaFrame;
typedef union { int64_t l; double d; } Java8;
typedef union { jref r; jbool z; jbyte b; jchar c; jshort s; jint i;
                jlong l; jfloat f; jdouble d; } JRIValue;
#define EE2NaEnv(ee) (ee)
#define NaEnv2EE(env) ((ExecEnv *)(env))
#define JRIEnv_deref(env, ref) (ref)
#define SET_INT64(t, s, value) do { (t).l=(value); memcpy((s), &(t).l, sizeof((t).l)); } while (0)
#define SET_DOUBLE(t, s, value) do { (t).d=(value); memcpy((s), &(t).d, sizeof((t).d)); } while (0)
'''
stubs = r'''
static stack_item slots[64];
static const char *signature;
static char *jri_PushArgumentsArray(ExecEnv *ee, char *name, char *sig,
                                  JavaFrame *frame, JRI_PushArgsType a)
{ return 0; }
static JRIValue jri_Invoke(ExecEnv *ee, jref self, JRIMethodID method,
                          JRI_PushArguments_t push, JRI_PushArgsType a,
                          bool_t is_static, bool_t constructor)
{
    JRIValue result;
    JavaFrame frame;
    memset(&result, 0, sizeof(result));
    if (method < 0) return result; /* lookup can fail before consuming args */
    memset(slots, 0, sizeof(slots));
    frame.optop = slots;
    result.i = push(ee, "test", (char *)signature, &frame, a) != 0;
    return result;
}
static JavaObject *jri_Construct(JRIEnv *env, struct java_lang_Class *clazz,
                                jint method, JRI_PushArguments_t push,
                                JRI_PushArgsType a)
{
    jri_Invoke(NaEnv2EE(env), 0, method, push, a, 0, 1);
    return 0;
}
'''
tests = r'''
#define ARGS -7,2,3,4,5,6,7,8, 1.25,2.25,3.25,4.25,5.25,6.25,7.25,8.25,9.25, INT64_C(-1234567890123),10.5,&object,&array
static int object, array;
static void check_slots(void)
{
    int i;
    int64_t l;
    double d;
    assert(slots[0].i == -7);
    for (i=1; i<8; ++i) assert(slots[i].i == i+1);
    for (i=0; i<9; ++i) assert(slots[8+i].f == (float)(i+1.25));
    memcpy(&l, &slots[17], sizeof(l));
    memcpy(&d, &slots[19], sizeof(d));
    assert(l == INT64_C(-1234567890123));
    assert(d == 10.5);
    assert(slots[21].h == &object);
    assert(slots[22].p == &array);
}
static void check_v(int unused, ...)
{
    va_list args;
    va_start(args, unused);
    assert(jri_CallMethod_intV(0,0,0,0,args)); check_slots();
    assert(jri_CallMethod_intV(0,0,0,0,args)); check_slots();
    assert(jri_CallStaticMethod_intV(0,0,0,0,args)); check_slots();
    jri_NewObjectV(0,0,0,0,args); check_slots();
    assert(!jri_CallMethod_intV(0,0,0,-1,args));
    assert(!jri_CallStaticMethod_intV(0,0,0,-1,args));
    jri_NewObjectV(0,0,0,-1,args);
    signature = "(IX)I"; /* marshalling may fail after lookup */
    assert(!jri_CallMethod_intV(0,0,0,0,args));
    assert(va_arg(args,int) == -7); /* V APIs preserve the caller cursor */
    va_end(args);
}
int main(void)
{
    signature = "(IIIIIIIIFFFFFFFFFJDLjava/lang/Object;[I)I";
    assert(jri_CallMethod_int(0,0,0,0,ARGS)); check_slots();
    assert(jri_CallStaticMethod_int(0,0,0,0,ARGS)); check_slots();
    jri_NewObject(0,0,0,0,ARGS); check_slots();
    check_v(0,ARGS);
    signature = "(ZBCSI)I";
    assert(jri_CallMethod_int(0,0,0,0,1,-2,65,-3,-4));
    assert(slots[0].i==1 && slots[1].i==-2 && slots[2].i==65);
    assert(slots[3].i==-3 && slots[4].i==-4);
    signature = "()I";
    assert(jri_CallMethod_int(0,0,0,0));
    puts("JRI variadic argument regression passed");
    return 0;
}
'''
compat = '#include "jri_va_compat.h"\n' if (root / 'sun-java/include/jri_va_compat.h').exists() else ''
with tempfile.TemporaryDirectory(prefix='jri-varargs-') as directory:
    test = Path(directory) / 'test.c'
    program = Path(directory) / 'test'
    test.write_text(setup + compat + marshalling + stubs + constructors + methods + tests)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=gnu90', '-O2', '-I'+str(root/'sun-java/include'), str(test), '-o', str(program)]
    if '--sanitize' in sys.argv:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(command, check=True)
    subprocess.run([str(program)], check=True)
