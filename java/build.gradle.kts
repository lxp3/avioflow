plugins {
    `java-library`
    `maven-publish`
    signing
    id("com.vanniktech.maven.publish") version "0.29.0"
}

group = "io.github.lxp3"
version = providers.fileContents(layout.projectDirectory.file("../version.txt")).asText.get().trim()

val nativeClassifier = providers.gradleProperty("avioflow.nativeClassifier").orNull
val nativeLibraryDir = providers.gradleProperty("avioflow.nativeLibraryDir").orNull
val nativeRootDir = providers.gradleProperty("avioflow.nativeRootDir")
    .map { file(it) }
    .orElse(layout.buildDirectory.dir("native").map { it.asFile })

java {
    withSourcesJar()
}

tasks.withType<JavaCompile>().configureEach {
    options.release.set(8)
}

tasks.test {
    useJUnitPlatform()
    systemProperty("avioflow.test.audio", layout.projectDirectory.file("../public/wavs/TownTheme.mp3").asFile.absolutePath)
    if (nativeLibraryDir != null) {
        val libraryName = System.mapLibraryName("avioflow_jni")
        systemProperty("avioflow.native.path", file(nativeLibraryDir).resolve(libraryName).absolutePath)
    }
}

dependencies {
    testImplementation("org.junit.jupiter:junit-jupiter:5.11.4")
}

val nativeJar = tasks.register<Jar>("nativeJar") {
    archiveClassifier.set(nativeClassifier ?: "native")
    if (nativeLibraryDir != null) {
        from(nativeLibraryDir) {
            into("io/github/lxp3/avioflow/native/${nativeClassifier ?: "unknown"}")
        }
    }
}

publishing {
    publications {
        withType<MavenPublication>().configureEach {
            if (nativeClassifier != null && nativeLibraryDir != null) {
                artifact(nativeJar)
            } else {
                val root = nativeRootDir.get()
                if (root.exists()) {
                    root.listFiles()
                        ?.filter { it.isDirectory }
                        ?.sortedBy { it.name }
                        ?.forEach { platformDir ->
                            val jarTask = tasks.register<Jar>("nativeJar${platformDir.name.replace(Regex("[^A-Za-z0-9]"), "")}") {
                                archiveClassifier.set(platformDir.name)
                                from(platformDir) {
                                    into("io/github/lxp3/avioflow/native/${platformDir.name}")
                                }
                            }
                            artifact(jarTask)
                        }
                }
            }
            pom {
                name.set("avioflow")
                description.set("High-performance audio decoding and encoding library powered by FFmpeg")
                url.set("https://github.com/lxp3/avioflow")
                licenses {
                    license {
                        name.set("MIT License")
                        url.set("https://opensource.org/licenses/MIT")
                    }
                }
                developers {
                    developer {
                        id.set("lxp3")
                        name.set("lxp3")
                    }
                }
                scm {
                    connection.set("scm:git:git://github.com/lxp3/avioflow.git")
                    developerConnection.set("scm:git:ssh://github.com/lxp3/avioflow.git")
                    url.set("https://github.com/lxp3/avioflow")
                }
            }
        }
    }
}

mavenPublishing {
    publishToMavenCentral(com.vanniktech.maven.publish.SonatypeHost.CENTRAL_PORTAL)
    signAllPublications()
}
