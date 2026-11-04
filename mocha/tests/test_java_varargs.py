"""Exercise the production Mocha-to-Java variadic callbacks without a JVM."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

source = (Path(__file__).resolve().parents[1] / 'src/mo_java.c').read_text()
parts = []
for name in ('mocha_ExecuteJavaMethod_data', 'mocha_ConstructJava_data'):
    end = source.index('} '+name+';')
    start = source.rfind('typedef struct {', 0, end)
    parts.append(source[start:source.index('/***********************************************************************', end)])
setup = r'''
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
typedef void JRIEnv;
typedef void ExecEnv;
typedef struct { int value; } HObject;
typedef void ClassClass;
typedef void MochaContext;
typedef int MochaBoolean;
typedef int bool_t;
struct methodblock { int unused; };
#define TRUE 1
#define FALSE 0
#define MOCHA_TRUE 1
#define PRIVILEGED_EE ((ExecEnv *)1)
static int fail, privileged, callbacks, env;
static HObject object;
static JRIEnv *JRI_GetCurrentEnv(void) { return &env; }
static void check(va_list args)
{
    int i;
    for (i=0; i<10; ++i) assert(va_arg(args,int) == i-5);
    for (i=0; i<10; ++i) assert(va_arg(args,double) == i+0.5);
    assert(va_arg(args,HObject *) == &object);
}
static long do_execute_java_method_vararg(ExecEnv *ee, HObject *self,
        char *name, char *sig, struct methodblock *mb, bool_t is_static,
        va_list args, long *high, bool_t flag)
{
    assert(ee == &env);
    check(args);
    if (high) *high = 222;
    return 111;
}
static HObject *execute_java_constructor_vararg(ExecEnv *ee, char *name,
        ClassClass *cb, char *sig, va_list args)
{
    assert(ee == (privileged ? PRIVILEGED_EE : &env));
    check(args);
    return &object;
}
static MochaBoolean mocha_CallJava(MochaContext *mc, void (*callback)(void *),
                                   void *data, int synchronous)
{
    if (fail) return 0;
    *(JRIEnv **)data = &env;
    ++callbacks;
    callback(data);
    return 1;
}
'''
tests = r'''
#define ARGS -5,-4,-3,-2,-1,0,1,2,3,4,0.5,1.5,2.5,3.5,4.5,5.5,6.5,7.5,8.5,9.5,&object
int main(void)
{
    long result[2] = {0,0};
    assert(mocha_ExecuteJavaMethod(0,result,sizeof(result),0,0,0,0,0,ARGS));
    assert(result[0] == 111 && result[1] == 222);
    assert(mocha_ConstructJava(0,0,0,0,ARGS) == &object);
    privileged = 1;
    assert(mocha_ConstructJavaPrivileged(0,0,0,0,ARGS) == &object);
    assert(callbacks == 3);
    fail = 1;
    assert(!mocha_ExecuteJavaMethod(0,result,sizeof(result),0,0,0,0,0,ARGS));
    assert(!mocha_ConstructJava(0,0,0,0,ARGS));
    assert(!mocha_ConstructJavaPrivileged(0,0,0,0,ARGS));
    assert(callbacks == 3);
    puts("Mocha Java bridge varargs regression passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='mocha-varargs-') as directory:
    test = Path(directory) / 'test.c'
    program = Path(directory) / 'test'
    test.write_text(setup + '\n'.join(parts) + tests)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=gnu90', '-O2', str(test), '-o', str(program)]
    if '--sanitize' in sys.argv:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(command, check=True)
    subprocess.run([str(program)], check=True)
