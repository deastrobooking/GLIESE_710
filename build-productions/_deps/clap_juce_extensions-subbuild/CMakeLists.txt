# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 4.3.2)

# Reject any attempt to use a toolchain file. We must not use one because
# we could be downloading it here. If the CMAKE_TOOLCHAIN_FILE environment
# variable is set, the cache variable will have been initialized from it.
unset(CMAKE_TOOLCHAIN_FILE CACHE)
unset(ENV{CMAKE_TOOLCHAIN_FILE})

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(clap_juce_extensions-populate NONE)


# Pass through things we've already detected in the main project to avoid
# paying the cost of redetecting them again in ExternalProject_Add()
set(GIT_EXECUTABLE [==[/usr/bin/git]==])
set(Git_VERSION [==[2.39.3 (Apple Git-146)]==])
set_property(GLOBAL PROPERTY _CMAKE_FindGit_GIT_EXECUTABLE_VERSION
  [==[/usr/bin/git;2.39.3 (Apple Git-146)]==]
)


include(ExternalProject)
ExternalProject_Add(clap_juce_extensions-populate
                     "UPDATE_DISCONNECTED" "False" "GIT_REPOSITORY" "https://github.com/free-audio/clap-juce-extensions.git" "EXTERNALPROJECT_INTERNAL_ARGUMENT_SEPARATOR" "GIT_SUBMODULES_RECURSE" "TRUE" "GIT_TAG" "e8de9e8571626633b8541a54c2406fccc4272767"
                    SOURCE_DIR          "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-src"
                    BINARY_DIR          "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
                    USES_TERMINAL_PATCH     YES
)


