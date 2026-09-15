plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.zeronovel.gardroid"
    compileSdk {
        version = release(36)
    }

    defaultConfig {
        applicationId = "com.zeronovel.gardroid"
        minSdk = 30
        targetSdk = 36
        versionCode = 1
        versionName = "1.4"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    android {
        defaultConfig {
            externalNativeBuild {
                cmake {
                    cppFlags  ("-std=c++17 -frtti -fexceptions")
                    val vcpkgRoot = System.getenv("VCPKG_ROOT")?.replace('\\', '/')
                    val ndkDir = ndkDirectory.absolutePath.replace('\\', '/')
                    arguments(
                        "-DCMAKE_TOOLCHAIN_FILE=${project.projectDir.absolutePath.replace('\\', '/')}/vcpkg-android.cmake",
                        "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=${ndkDir}/build/cmake/android.toolchain.cmake",
                        "-DVCPKG_ROOT=${vcpkgRoot}"
                    )
                }
            }
        }
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildFeatures {
        viewBinding = true
    }
}

dependencies {
    implementation(libs.appcompat)
    implementation(libs.material)
    implementation(libs.constraintlayout)
    testImplementation(libs.junit)
    androidTestImplementation(libs.ext.junit)
    androidTestImplementation(libs.espresso.core)
    implementation("androidx.navigation:navigation-fragment:2.9.6")
    implementation("androidx.navigation:navigation-ui:2.9.6")
    implementation("com.google.android.material:material:1.11.0")

}