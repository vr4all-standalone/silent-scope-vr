plugins { id("com.android.application") }
providers.gradleProperty("vr4allBuildRoot").orNull?.let { layout.buildDirectory.set(file("$it/app")) }
android {
    namespace = "com.vrlightgunhub.silentscope"
    ndkVersion = "29.0.14206865"
    compileSdk { version = release(36) { minorApiLevel = 1 } }
    defaultConfig {
        applicationId = "com.vrlightgunhub.silentscope"
        minSdk = 29
        targetSdk = 36
        versionCode = 6
        versionName = rootProject.file("../VERSION").readText().trim()
        ndk { abiFilters += "arm64-v8a" }
    }
    signingConfigs { create("local") {
        storeFile = rootProject.file("../private/silent-scope.keystore")
        storePassword = "android"
        keyAlias = "silent-scope"
        keyPassword = "android"
    } }
    buildTypes { getByName("release") { signingConfig = signingConfigs.getByName("local"); isMinifyEnabled = false } }
    providers.gradleProperty("vr4allJniLibs").orNull?.let { sourceSets.getByName("main").jniLibs.setSrcDirs(listOf(it)) }
    packaging { jniLibs { useLegacyPackaging = true } }
    sourceSets.getByName("main").assets.srcDir(rootProject.file("../assets"))
}
