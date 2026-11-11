# Building Silent Scope VR

Use the named **silent-scope-vr-source.zip** from the release. Extract it into a short directory path. It contains the exact Flycast engine and expanded dependency sources, the adapter under `port/`, the audited Android framework, and the pinned OpenXR Android loader.

## Windows prerequisites for a Quest build

- Java 21.
- Android SDK platform 36 with the 36.1 minor API package used by Android Gradle Plugin 9.2.1.
- Android NDK **29.0.14206865**.
- Android SDK CMake **3.22.1**, including Ninja.
- Android build tools **36.0.0**.
- PowerShell 7.

Gradle **9.4.1** and the Maven dependencies are resolved by the included wrapper. The first Java build needs internet access or a populated Gradle cache.

Run from the extracted source root.

```powershell
./tools/build-quest-from-source.ps1 -AndroidSdk C:/Android/Sdk -JavaHome C:/Java/jdk-21
```

The default produces a debug-signed APK in `build/gradle/app/outputs/apk/debug/`. The debug key differs from the public release signing key. To make a release with your own identity, create `port/private/silent-scope.keystore` with the `silent-scope` alias and the passwords expected by `port/android-dreamcast/app/build.gradle.kts`, then run the script with `-Release`.

For native compilation only, add `-EngineOnly`. The main code, camera ownership, aiming, and renderer are in `flycast/core/`. The adapter snapshot and integration patch history are under `port/`. The source manifest records every exported file and the pinned engine and dependency revisions.

The scripts under `port/tools/` also preserve the original developer's build and diagnostic commands. The root build script resolves paths relative to the extracted source.

## Host checks

The adapter's CMake project supports the native and GPU regressions. On Windows with MinGW, CMake, Ninja and Python installed, pass those tools and the extracted Flycast source to `port/tools/build-host.ps1`. The public source omits private media fixtures and signing keys.

## Source identity

The upstream engine base is `8398fead532ede9d52c37addef3353611b6c5ce7`. The release includes the applied working-tree changes and expanded dependency files. `SOURCE-MANIFEST.json` records file hashes, dependency commits, and the hashes of the shipped APK and native library.
