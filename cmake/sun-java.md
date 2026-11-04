# Sun Java CMake targets

The `sun-java` Makefiles are represented by `targets/sun-java.cmake` and individual manifests under `targets/sun-java/`. This is the original pre-JNI VM and class library, not a modern JDK. The Linux source configuration selects X/Motif AWT and the original stub multimedia implementation; Mac, Windows, IRIX and Solaris backends are not selected.

## Configure

Build the Java components independently of browser Java integration:

```sh
cmake -S . -B out/java -DNETSCAPE_BUILD_BROWSER=OFF \
  -DNETSCAPE_BUILD_LEGACY=ON -DNETSCAPE_BUILD_SUN_JAVA=ON
cmake --build out/java --target netscape_sun_java --parallel
```

To integrate the source runtime with the browser, use `-DNETSCAPE_ENABLE_JAVA=ON -DNETSCAPE_JAVA_PROVIDER=SOURCE`. The source provider is the default when browser Java is enabled. The prior prebuilt-archive mode remains available with `-DNETSCAPE_JAVA_PROVIDER=IMPORTED -DNETSCAPE_JAVA_ROOT=/path/to/legacy/dist`.

The source runtime retains vintage ABI assumptions. Its JRI argument bridge now handles array-based `va_list` types using explicit ownership and `va_copy`/`va_end`. The complete `netscape_sun_java` aggregate, including the VM executable, compiles and links on x64 GCC 15. The Java-enabled browser with Mocha also compiles and links. Java execution has not been verified; remaining pointer-width warnings mean this is not a certified x64 JVM port.

## Targets

| Target | Source area / output |
| --- | --- |
| `netscape_java_javart` | `runtime/`, including the long-name sources; `javart.a` |
| `netscape_java_sunmd` | `md/`; `sunmd.a` |
| `netscape_java_javai` | Interpreter entry point; `javai.a` |
| `netscape_java_awt` | `awt/x/` plus `ustring.c`; `libawt.a` |
| `netscape_java_mmedia` | `mmedia/stub/`; `libmmedia.a` |
| `netscape_java_nsa` | `netscape/applet/`; `libnsa.a` |
| `netscape_java_nsn` | `netscape/net/`; `libnsn.a` |
| `netscape_java_njs` | `netscape/javascript/`; `libnjs.a`; selected when Mocha is enabled |
| `netscape_java_vm` | `java/` interpreter launcher; `java` |
| `netscape_java_vm_static` | Explicit `STATIC_JAVA` AWT/multimedia launcher; `java_s` |
| `netscape_javah`, `netscape_javap` | Standalone tools with their reduced runtime/platform source sets |
| `netscape_java_genconst`, `netscape_java_constants` | Constant generator and `RuntimeConstants.java` |
| `netscape_java_opcodes` | Four opcode tables generated from `include/opcodes.list` |
| `netscape_java_classes` | Versioned runtime class archive |
| `netscape_javac`, `netscape_javadoc` | Launcher scripts under the build's `bin/`; execution needs rebuilt `javac.zip` |
| `netscape_java_regenerate_natives` | Explicit legacy header/JRI/stub regeneration using the built `javah` |
| `netscape_sun_java` | Aggregate of the default source archives, VM, tools, constants, classes and compiler launchers |

`netscape_java_jri` declares the old `libjri.a` glue as an explicit target, matching the commented-out default in the original javah Makefile. It currently fails against the checked-in JRI headers because `JRIFieldThunk`/`JRIMethodThunk` are unavailable. It is excluded from the aggregate and default build. The JavaScript bridge needs Mocha's context fields and is excluded from the aggregate when Mocha is disabled. The `java_s` variant is also explicit, as in the original Makefile.

## Generated files and bootstrap

Native sources and checked-in `_gen`/`_stubs` interfaces are staged under `generated/sun-java/` in the build directory, preserving relative includes. Opcode tables and runtime constants are generated there. Normal builds use the shipped `classsrc/java_<version>.x` runtime archive and the checked-in native interfaces, avoiding the original compiler/header bootstrap cycle.

To rebuild runtime and compiler classes, supply a compatible vintage compiler as a CMake command list:

```sh
cmake -S . -B out/java -DNETSCAPE_BUILD_BROWSER=OFF \
  -DNETSCAPE_BUILD_LEGACY=ON -DNETSCAPE_BUILD_SUN_JAVA=ON \
  -DNETSCAPE_REBUILD_JAVA_CLASSES=ON \
  -DNETSCAPE_JAVA_BOOTSTRAP_COMMAND=/opt/legacy-jdk/bin/javac
cmake --build out/java --target netscape_java_classes
```

Compilation follows the original runtime/compiler package lists. Python packages the resulting classes into the versioned runtime archive and `javac.zip`. A modern compiler is not assumed to produce compatible classes. Bootstrap compilation has not been verified because no vintage compiler is available.

`netscape_java_regenerate_natives` uses the built legacy javah, the class archive (or `NETSCAPE_JAVA_CLASSPATH`), and checked-in class manifests to regenerate normal and JRI headers/stubs in the staged tree. Reconfiguration refreshes that tree from the checkout; rerun the explicit regeneration target after reconfiguration if using regenerated interfaces. Source files are never overwritten by these build rules.

## Verification

On Linux x86_64 with GCC 15, CMake configured the source target graph and successfully built opcode generation, `javah`, `javap`, the constant generator/Java constants, Linux platform, AWT, multimedia, interpreter-entry, applet and networking archives. A small standalone console adapter supplies javap's verifier diagnostics without linking the full VM. After the JRI `va_list` ownership fix, the runtime archive and complete `netscape_sun_java` aggregate also compiled and linked. A focused regression extracts the production JRI marshaler and constructor/instance/static wrappers, with VM lookup stubbed; it passes under address and undefined-behavior sanitizers. It covers register/stack overflow arguments, promoted primitive arguments, references, repeated `V` calls, and lookup/marshalling failures while preserving the caller cursor.

```sh
python3 sun-java/tests/test_jri_varargs.py --sanitize
```

The Java-enabled browser with Mocha also compiled and linked after correcting three borrowed `va_list` pointers in the Mocha-to-Java callbacks. A second isolated regression exercises those production callbacks, including register/stack overflow arguments and failed callbacks, under sanitizers:

```sh
python3 mocha/tests/test_java_varargs.py --sanitize
```

Java execution, bootstrap class compilation and native-interface regeneration remain unverified.
