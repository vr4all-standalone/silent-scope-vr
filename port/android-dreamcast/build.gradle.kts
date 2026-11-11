plugins { id("com.android.application") version "9.2.1" apply false }
providers.gradleProperty("vr4allBuildRoot").orNull?.let { layout.buildDirectory.set(file("$it/root")) }
