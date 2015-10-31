# bluesky
BlueSky file system from Michael Vrable, Goeff Voelker, and Stephan Savage

This system was originally developed at the University of California, 
San Diego by the authors.


Required dependencies (from base Debian install):
    autoconf
    automake
    build-essential
    cmake
    git-core
    libasio-dev
    libboost-dev
    libboost-program-options-dev
    libboost-system-dev
    libboost-thread-dev
    libcurl4-gnutls-dev
    libdb-dev
    libgcrypt-dev
    libglib2.0-dev
    libprotobuf-dev
    libxml2-dev
    mercurial
    openjdk-6-jre
    protobuf-compiler
    python-all-dev
    s3cmd

Running under Valgrind:
  - Set G_SLICE=always-malloc environment variable to help with memory
    leak checking