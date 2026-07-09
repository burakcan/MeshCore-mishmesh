#pragma once

// Version of the mishmesh UI itself, independent of the meshcore FIRMWARE_VERSION
// it rides on. Same shape as the upstream FIRMWARE_VERSION define (fallback here,
// overridable with -D MISHMESH_VERSION=... in a build env). The meshcore version
// is FIRMWARE_VERSION itself - build with FIRMWARE_VERSION set to the meshcore
// release (e.g. v1.16.0); this UI version now carries the fork identity.
#ifndef MISHMESH_VERSION
#define MISHMESH_VERSION "v1.0.0"
#endif
