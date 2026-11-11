plugins { id("com.android.application") }
val engine = providers.gradleProperty("vr4allEngineSource").get()
val framework = providers.gradleProperty("vr4allJavaFramework").get()
providers.gradleProperty("vr4allBuildRoot").orNull?.let { layout.buildDirectory.set(file("$it/app")) }
android {
    namespace = "com.flycast.emulator" // Native JNI class names, separate from app identity.
    compileSdk { version = release(36) { minorApiLevel = 1 } }
    defaultConfig {
        applicationId = "com.vrlightgunhub.silentscope"
        minSdk = 29; targetSdk = 36; versionCode = 70
        versionName = rootProject.file("../VERSION").readText().trim()
        ndk { abiFilters += "arm64-v8a" }
    }
    signingConfigs { create("local") {
        storeFile = rootProject.file("../private/silent-scope.keystore")
        storePassword = "android"; keyAlias = "silent-scope"; keyPassword = "android"
    } }
    buildTypes { getByName("release") { signingConfig = signingConfigs.getByName("local"); isMinifyEnabled = false } }
    sourceSets.getByName("main") {
        java.srcDir(framework)
        res.srcDir("$engine/shell/android-studio/flycast/src/main/res")
        assets.srcDir(rootProject.file("../assets"))
        providers.gradleProperty("vr4allJniLibs").orNull?.let { jniLibs.setSrcDirs(listOf(it)) }
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_11; targetCompatibility = JavaVersion.VERSION_11 }
    packaging { jniLibs { useLegacyPackaging = true }; resources.excludes += "META-INF/DEPENDENCIES" }
}
dependencies {
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.documentfile:documentfile:1.0.1")
    implementation("org.apache.commons:commons-lang3:3.12.0")
    implementation("org.apache.httpcomponents.client5:httpclient5:5.0.3")
    implementation("org.slf4j:slf4j-android:1.7.35")
}
