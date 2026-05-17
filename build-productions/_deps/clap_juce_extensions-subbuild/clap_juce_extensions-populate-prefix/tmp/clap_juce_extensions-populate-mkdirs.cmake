# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-src")
  file(MAKE_DIRECTORY "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-src")
endif()
file(MAKE_DIRECTORY
  "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-build"
  "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-subbuild/clap_juce_extensions-populate-prefix"
  "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-subbuild/clap_juce_extensions-populate-prefix/tmp"
  "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-subbuild/clap_juce_extensions-populate-prefix/src/clap_juce_extensions-populate-stamp"
  "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-subbuild/clap_juce_extensions-populate-prefix/src"
  "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-subbuild/clap_juce_extensions-populate-prefix/src/clap_juce_extensions-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-subbuild/clap_juce_extensions-populate-prefix/src/clap_juce_extensions-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/home/Desktop/VSTS/GLIESE_710/build-productions/_deps/clap_juce_extensions-subbuild/clap_juce_extensions-populate-prefix/src/clap_juce_extensions-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
